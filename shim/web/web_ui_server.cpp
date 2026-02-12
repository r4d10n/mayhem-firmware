/*
 * WebSocket-based Web UI server implementation.
 *
 * Zero external dependencies — uses raw POSIX sockets, includes
 * minimal SHA-1 and Base64 implementations for the WebSocket handshake.
 *
 * Protocol:
 *   GET /        → serves embedded HTML page
 *   GET /ws      → WebSocket upgrade
 *   WS binary    → framebuffer data (server → client)
 *   WS text      → JSON input events (client → server)
 *
 * Frame format (binary):
 *   Byte 0:      0x01 = full frame, 0x02 = delta frame
 *   Bytes 1-2:   width  (uint16 LE)
 *   Bytes 3-4:   height (uint16 LE)
 *   Full frame:  raw RGB565 pixels (w*h*2 bytes)
 *   Delta frame: runs of (offset:u32_LE, count:u16_LE, pixels:u16_LE[count])
 */

#include "web_ui_server.hpp"
#include "sdl2_backend.hpp"   /* shim::g_input, InputState */
#include "framebuffer.hpp"
#include "event_m0.hpp"       /* EventDispatcher::events_flag, EVT_MASK_* */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace shim {

/* ================================================================
 * Minimal SHA-1 (RFC 3174)
 * ================================================================ */
namespace {

struct SHA1Context {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
};

static inline uint32_t sha1_rol(uint32_t v, int bits) {
    return (v << bits) | (v >> (32 - bits));
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = sha1_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = sha1_rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1_rol(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_init(SHA1Context& ctx) {
    ctx.state[0] = 0x67452301;
    ctx.state[1] = 0xEFCDAB89;
    ctx.state[2] = 0x98BADCFE;
    ctx.state[3] = 0x10325476;
    ctx.state[4] = 0xC3D2E1F0;
    ctx.count = 0;
    memset(ctx.buffer, 0, sizeof(ctx.buffer));
}

static void sha1_update(SHA1Context& ctx, const uint8_t* data, size_t len) {
    size_t buf_idx = (size_t)(ctx.count & 63);
    ctx.count += len;

    while (len > 0) {
        size_t copy = 64 - buf_idx;
        if (copy > len) copy = len;
        memcpy(ctx.buffer + buf_idx, data, copy);
        buf_idx += copy;
        data += copy;
        len -= copy;
        if (buf_idx == 64) {
            sha1_transform(ctx.state, ctx.buffer);
            buf_idx = 0;
        }
    }
}

static void sha1_final(SHA1Context& ctx, uint8_t digest[20]) {
    uint64_t bits = ctx.count * 8;
    uint8_t pad = 0x80;
    sha1_update(ctx, &pad, 1);
    pad = 0;
    while ((ctx.count & 63) != 56) {
        sha1_update(ctx, &pad, 1);
    }
    uint8_t len_be[8];
    for (int i = 7; i >= 0; i--) {
        len_be[i] = (uint8_t)(bits & 0xFF);
        bits >>= 8;
    }
    sha1_update(ctx, len_be, 8);

    for (int i = 0; i < 5; i++) {
        digest[i * 4] = (uint8_t)(ctx.state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx.state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx.state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx.state[i]);
    }
}

/* ================================================================
 * Minimal Base64 encode
 * ================================================================ */

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::vector<char> base64_encode(const uint8_t* data, size_t len) {
    std::vector<char> out;
    out.reserve(((len + 2) / 3) * 4 + 1);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) n |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < len) n |= ((uint32_t)data[i + 2]);

        out.push_back(b64_table[(n >> 18) & 0x3F]);
        out.push_back(b64_table[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? b64_table[n & 0x3F] : '=');
    }
    out.push_back('\0');
    return out;
}

/* ================================================================
 * Simple JSON value extraction (no allocations, no exceptions)
 * ================================================================ */

/* Find "key":"value" or "key":number in a JSON string.
 * Returns pointer to the value start and sets len.
 * For strings, returns the content without quotes. */
static const char* json_find_string(const char* json, const char* key,
                                     size_t* out_len) {
    /* Look for "key":" */
    char search[128];
    int slen = snprintf(search, sizeof(search), "\"%s\":\"", key);
    if (slen <= 0) return nullptr;

    const char* p = strstr(json, search);
    if (!p) return nullptr;
    p += slen;
    const char* end = strchr(p, '"');
    if (!end) return nullptr;
    *out_len = (size_t)(end - p);
    return p;
}

static bool json_find_bool(const char* json, const char* key, bool* out) {
    char search_true[128], search_false[128];
    snprintf(search_true, sizeof(search_true), "\"%s\":true", key);
    snprintf(search_false, sizeof(search_false), "\"%s\":false", key);

    if (strstr(json, search_true)) {
        *out = true;
        return true;
    }
    if (strstr(json, search_false)) {
        *out = false;
        return true;
    }
    return false;
}

static bool json_find_int(const char* json, const char* key, int* out) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"' || *p == '{' || *p == '[') return false;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p) return false;
    *out = (int)v;
    return true;
}

/* ================================================================
 * Socket helpers
 * ================================================================ */

static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

static bool send_all(int fd, const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += (size_t)n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* Non-blocking socket: wait for it to become writable */
            struct pollfd pfd = {fd, POLLOUT, 0};
            int ret = poll(&pfd, 1, 200 /* 200ms timeout */);
            if (ret <= 0) return false; /* Timeout or error — drop client */
            /* Socket writable again, retry send */
        } else if (n < 0 && errno == EINTR) {
            continue; /* Interrupted by signal, retry */
        } else {
            return false; /* Real error or connection closed */
        }
    }
    return true;
}

}  // anonymous namespace

/* ================================================================
 * WebUIServer implementation
 * ================================================================ */

WebUIServer::~WebUIServer() {
    stop();
}

bool WebUIServer::start(int port) {
    if (running_.load()) return true;

    port_ = port;

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        fprintf(stderr, "[WebUI] socket() failed\n");
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[WebUI] bind() failed on port %d\n", port_);
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 4) < 0) {
        fprintf(stderr, "[WebUI] listen() failed\n");
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    set_nonblocking(server_fd_);

    running_ = true;
    client_count_ = 0;
    server_thread_ = std::thread(&WebUIServer::server_thread_func, this);

    fprintf(stderr, "[WebUI] Server started on http://0.0.0.0:%d/\n", port_);
    return true;
}

void WebUIServer::stop() {
    if (!running_.load()) return;
    running_ = false;

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int i = 0; i < client_count_; i++) {
        if (clients_[i].fd >= 0) {
            close(clients_[i].fd);
            clients_[i].fd = -1;
        }
    }
    client_count_ = 0;

    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }

    fprintf(stderr, "[WebUI] Server stopped.\n");
}

void WebUIServer::server_thread_func() {
    struct pollfd fds[MAX_CLIENTS + 1];

    while (running_.load()) {
        /* Build poll set */
        int nfds = 0;
        fds[0].fd = server_fd_;
        fds[0].events = POLLIN;
        nfds = 1;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (int i = 0; i < client_count_; i++) {
                if (clients_[i].fd >= 0) {
                    fds[nfds].fd = clients_[i].fd;
                    fds[nfds].events = POLLIN;
                    nfds++;
                }
            }
        }

        int ret = poll(fds, (nfds_t)nfds, 50 /* 50ms timeout */);
        if (ret < 0) continue;

        /* Check for new connections */
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd >= 0) {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                if (client_count_ < MAX_CLIENTS) {
                    set_nonblocking(client_fd);
                    int nodelay = 1;
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                    Client& c = clients_[client_count_];
                    c.fd = client_fd;
                    c.websocket_ready = false;
                    c.needs_full_frame = true;
                    c.recv_buffer.clear();
                    client_count_++;

                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
                    fprintf(stderr, "[WebUI] Client connected from %s (%d total)\n",
                            ip, client_count_);
                } else {
                    close(client_fd);
                }
            }
        }

        /* Check client data */
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            int poll_idx = 1;
            for (int i = 0; i < client_count_; i++) {
                if (clients_[i].fd >= 0 && poll_idx < nfds) {
                    if (fds[poll_idx].revents & (POLLIN | POLLHUP | POLLERR)) {
                        handle_client_data(i);
                    }
                    poll_idx++;
                }
            }
        }
    }
}

void WebUIServer::handle_client_data(int idx) {
    /* Called with clients_mutex_ held */
    Client& c = clients_[idx];
    uint8_t buf[4096];
    ssize_t n = recv(c.fd, buf, sizeof(buf), 0);

    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return; /* No data right now, try again on next poll */
    }
    if (n <= 0) {
        remove_client(idx);
        return;
    }

    c.recv_buffer.insert(c.recv_buffer.end(), buf, buf + n);

    if (!c.websocket_ready) {
        /* Check if we have a complete HTTP request (ends with \r\n\r\n) */
        auto& rb = c.recv_buffer;
        if (rb.size() >= 4) {
            bool complete = false;
            for (size_t i = 0; i <= rb.size() - 4; i++) {
                if (rb[i] == '\r' && rb[i + 1] == '\n' &&
                    rb[i + 2] == '\r' && rb[i + 3] == '\n') {
                    complete = true;
                    break;
                }
            }
            if (complete) {
                /* Null-terminate for string operations */
                rb.push_back(0);
                const char* req = (const char*)rb.data();

                /* Check if this is a WebSocket upgrade */
                if (strstr(req, "Upgrade: websocket") || strstr(req, "Upgrade: Websocket")) {
                    if (websocket_handshake(c.fd)) {
                        c.websocket_ready = true;
                        c.needs_full_frame = true;
                    } else {
                        remove_client(idx);
                        return;
                    }
                } else {
                    /* Serve HTTP — check if requesting favicon */
                    if (strstr(req, "GET /favicon.ico")) {
                        const char* resp = "HTTP/1.1 404 Not Found\r\n"
                                          "Content-Length: 0\r\n"
                                          "Connection: close\r\n\r\n";
                        send_all(c.fd, resp, strlen(resp));
                    } else {
                        /* Serve the HTML page */
                        const char* html = get_html_page();
                        size_t html_len = get_html_page_len();
                        send_http_response(c.fd, "text/html; charset=utf-8", html, html_len);
                    }
                    remove_client(idx);
                    return;
                }
                rb.clear();
            }
        }
    } else {
        /* Parse WebSocket frames */
        auto& rb = c.recv_buffer;
        while (rb.size() >= 2) {
            uint8_t byte0 = rb[0];
            uint8_t byte1 = rb[1];
            bool masked = (byte1 & 0x80) != 0;
            uint64_t payload_len = byte1 & 0x7F;
            size_t header_size = 2;

            if (payload_len == 126) {
                if (rb.size() < 4) break;
                payload_len = ((uint64_t)rb[2] << 8) | rb[3];
                header_size = 4;
            } else if (payload_len == 127) {
                if (rb.size() < 10) break;
                payload_len = 0;
                for (int i = 0; i < 8; i++) {
                    payload_len = (payload_len << 8) | rb[2 + i];
                }
                header_size = 10;
            }

            size_t mask_size = masked ? 4 : 0;
            size_t total = header_size + mask_size + payload_len;

            if (rb.size() < total) break;

            /* Unmask payload */
            uint8_t mask_key[4] = {0, 0, 0, 0};
            if (masked) {
                memcpy(mask_key, rb.data() + header_size, 4);
            }

            std::vector<uint8_t> payload(payload_len);
            for (uint64_t i = 0; i < payload_len; i++) {
                payload[i] = rb[header_size + mask_size + i] ^ mask_key[i & 3];
            }

            uint8_t opcode = byte0 & 0x0F;

            if (opcode == 0x08) {
                /* Close frame */
                uint8_t close_frame[2] = {0x88, 0x00};
                send_all(c.fd, close_frame, 2);
                remove_client(idx);
                return;
            } else if (opcode == 0x09) {
                /* Ping — send pong */
                send_websocket_frame(c.fd, payload.data(), payload.size(), 0x0A);
            } else if (opcode == 0x01 || opcode == 0x02) {
                /* Text or binary message */
                process_websocket_message(c.fd, payload.data(), payload.size(), opcode);
            }

            /* Remove processed frame from buffer */
            rb.erase(rb.begin(), rb.begin() + (ptrdiff_t)total);
        }
    }
}

bool WebUIServer::websocket_handshake(int fd) {
    /* Called with clients_mutex_ held, recv_buffer contains the HTTP request */
    int ci = -1;
    for (int i = 0; i < client_count_; i++) {
        if (clients_[i].fd == fd) { ci = i; break; }
    }
    if (ci < 0) return false;

    const char* req = (const char*)clients_[ci].recv_buffer.data();

    /* Find Sec-WebSocket-Key header */
    const char* key_header = strstr(req, "Sec-WebSocket-Key: ");
    if (!key_header) key_header = strstr(req, "Sec-Websocket-Key: ");
    if (!key_header) return false;

    key_header += strlen("Sec-WebSocket-Key: ");
    const char* key_end = strstr(key_header, "\r\n");
    if (!key_end) return false;

    size_t key_len = (size_t)(key_end - key_header);
    if (key_len > 128) return false;

    /* Concatenate key + magic string */
    static const char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char concat[256];
    memcpy(concat, key_header, key_len);
    memcpy(concat + key_len, magic, strlen(magic));
    size_t concat_len = key_len + strlen(magic);

    /* SHA-1 hash */
    SHA1Context sha;
    sha1_init(sha);
    sha1_update(sha, (const uint8_t*)concat, concat_len);
    uint8_t digest[20];
    sha1_final(sha, digest);

    /* Base64 encode */
    auto accept_key = base64_encode(digest, 20);

    /* Send response */
    char response[512];
    int resp_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key.data());

    return send_all(fd, response, (size_t)resp_len);
}

bool WebUIServer::send_websocket_frame(int fd, const uint8_t* data, size_t len,
                                        uint8_t opcode) {
    uint8_t header[10];
    size_t header_len = 0;

    header[0] = 0x80 | opcode; /* FIN + opcode */
    header_len = 1;

    if (len < 126) {
        header[1] = (uint8_t)len;
        header_len = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (uint8_t)(len >> 8);
        header[3] = (uint8_t)(len & 0xFF);
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) {
            header[9 - i] = (uint8_t)(len >> (i * 8));
        }
        header_len = 10;
    }

    /* Send header then payload — both must succeed */
    if (!send_all(fd, header, header_len)) return false;
    if (len > 0 && !send_all(fd, data, len)) return false;
    return true;
}

void WebUIServer::send_http_response(int fd, const char* content_type,
                                      const char* body, size_t body_len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n",
        content_type, body_len);

    send_all(fd, header, (size_t)hlen);
    send_all(fd, body, body_len);
}

void WebUIServer::process_websocket_message(int fd, const uint8_t* data,
                                             size_t len, uint8_t opcode) {
    if (opcode != 0x01) return; /* Only handle text messages */
    if (len == 0) return;

    /* Null-terminate the JSON */
    char json_buf[512];
    size_t copy_len = (len < sizeof(json_buf) - 1) ? len : sizeof(json_buf) - 1;
    memcpy(json_buf, data, copy_len);
    json_buf[copy_len] = '\0';

    /* Parse type field */
    size_t type_len = 0;
    const char* type = json_find_string(json_buf, "type", &type_len);
    if (!type) return;

    if (type_len == 3 && memcmp(type, "key", 3) == 0) {
        /* Keyboard input: {"type":"key","code":"ArrowUp","down":true} */
        size_t code_len = 0;
        const char* code = json_find_string(json_buf, "code", &code_len);
        bool down = false;
        if (!code || !json_find_bool(json_buf, "down", &down)) return;

        uint8_t bit = 0xFF;
        if (code_len == 7 && memcmp(code, "ArrowUp", 7) == 0) bit = 3;
        else if (code_len == 9 && memcmp(code, "ArrowDown", 9) == 0) bit = 2;
        else if (code_len == 9 && memcmp(code, "ArrowLeft", 9) == 0) bit = 1;
        else if (code_len == 10 && memcmp(code, "ArrowRight", 10) == 0) bit = 0;
        else if (code_len == 5 && memcmp(code, "Enter", 5) == 0) bit = 4;
        else if (code_len == 6 && memcmp(code, "Escape", 6) == 0) bit = 5;
        else if (code_len == 9 && memcmp(code, "Backspace", 9) == 0) bit = 5;
        /* Also accept KeyW/A/S/D for WASD navigation */
        else if (code_len == 4 && memcmp(code, "KeyW", 4) == 0) bit = 3;
        else if (code_len == 4 && memcmp(code, "KeyS", 4) == 0) bit = 2;
        else if (code_len == 4 && memcmp(code, "KeyA", 4) == 0) bit = 1;
        else if (code_len == 4 && memcmp(code, "KeyD", 4) == 0) bit = 0;
        else if (code_len == 5 && memcmp(code, "Space", 5) == 0) bit = 4;

        if (bit != 0xFF) {
            uint8_t mask = 1u << bit;
            uint8_t current = g_input.switches.load(std::memory_order_relaxed);
            if (down) {
                g_input.switches.store(current | mask, std::memory_order_relaxed);
            } else {
                g_input.switches.store(current & ~mask, std::memory_order_relaxed);
            }
            EventDispatcher::events_flag(EVT_MASK_SWITCHES);
        }

    } else if (type_len == 5 && memcmp(type, "touch", 5) == 0) {
        /* Touch input: {"type":"touch","x":120,"y":160,"down":true} */
        int x = 0, y = 0;
        bool down = false;
        if (!json_find_int(json_buf, "x", &x)) return;
        if (!json_find_int(json_buf, "y", &y)) return;
        if (!json_find_bool(json_buf, "down", &down)) return;

        if (down) {
            g_input.touch_x.store((int16_t)x, std::memory_order_relaxed);
            g_input.touch_y.store((int16_t)y, std::memory_order_relaxed);
            g_input.touch_active.store(true, std::memory_order_relaxed);
        } else {
            g_input.touch_active.store(false, std::memory_order_relaxed);
        }
        EventDispatcher::events_flag(EVT_MASK_TOUCH);

    } else if (type_len == 7 && memcmp(type, "encoder", 7) == 0) {
        /* Encoder input: {"type":"encoder","delta":1} */
        int delta = 0;
        if (!json_find_int(json_buf, "delta", &delta)) return;

        if (delta > 0) {
            g_input.encoder.fetch_add((uint32_t)delta, std::memory_order_relaxed);
        } else if (delta < 0) {
            g_input.encoder.fetch_sub((uint32_t)(-delta), std::memory_order_relaxed);
        }
        EventDispatcher::events_flag(EVT_MASK_ENCODER);
    }
}

void WebUIServer::remove_client(int idx) {
    /* Called with clients_mutex_ held */
    if (idx < 0 || idx >= client_count_) return;
    if (clients_[idx].fd >= 0) {
        close(clients_[idx].fd);
    }
    /* Swap with last */
    if (idx < client_count_ - 1) {
        clients_[idx] = std::move(clients_[client_count_ - 1]);
    }
    client_count_--;
    fprintf(stderr, "[WebUI] Client disconnected (%d remaining)\n", client_count_);
}

void WebUIServer::push_frame(const uint16_t* rgb565, int width, int height) {
    if (!running_.load()) return;

    std::lock_guard<std::mutex> lock(clients_mutex_);

    /* Check if any WebSocket clients exist */
    bool has_ws_clients = false;
    for (int i = 0; i < client_count_; i++) {
        if (clients_[i].websocket_ready) {
            has_ws_clients = true;
            break;
        }
    }
    if (!has_ws_clients) return;

    /* Check if any client needs a full frame */
    bool any_needs_full = false;
    for (int i = 0; i < client_count_; i++) {
        if (clients_[i].websocket_ready && clients_[i].needs_full_frame) {
            any_needs_full = true;
            break;
        }
    }

    /* Build delta frame for existing clients */
    std::vector<uint8_t> delta;
    {
        std::lock_guard<std::mutex> flock(frame_mutex_);
        if (prev_frame_.size() == (size_t)(width * height)) {
            delta = encode_delta_frame(rgb565, width, height);
        }
    }

    /* Build full frame if needed */
    std::vector<uint8_t> full;
    if (any_needs_full || delta.empty()) {
        full = encode_full_frame(rgb565, width, height);
    }

    /* Send to each client, track failures for cleanup */
    int dead[MAX_CLIENTS];
    int dead_count = 0;

    for (int i = 0; i < client_count_; i++) {
        if (!clients_[i].websocket_ready) continue;

        bool ok = true;
        if (clients_[i].needs_full_frame || delta.empty()) {
            ok = send_websocket_frame(clients_[i].fd, full.data(), full.size(), 0x02);
            if (ok) clients_[i].needs_full_frame = false;
        } else if (delta.size() > 5) {
            /* Only send delta if there are actual changes (header is 5 bytes) */
            ok = send_websocket_frame(clients_[i].fd, delta.data(), delta.size(), 0x02);
        }
        if (!ok) dead[dead_count++] = i;
    }

    /* Remove dead clients in reverse order (remove_client swaps with last) */
    for (int d = dead_count - 1; d >= 0; d--) {
        remove_client(dead[d]);
    }

    /* Update previous frame */
    {
        std::lock_guard<std::mutex> flock(frame_mutex_);
        size_t num_pixels = (size_t)(width * height);
        prev_frame_.resize(num_pixels);
        memcpy(prev_frame_.data(), rgb565, num_pixels * sizeof(uint16_t));
    }
}

std::vector<uint8_t> WebUIServer::encode_full_frame(const uint16_t* pixels,
                                                     int width, int height) {
    size_t num_pixels = (size_t)(width * height);
    std::vector<uint8_t> out;
    out.reserve(5 + num_pixels * 2);

    /* Header */
    out.push_back(0x01); /* Full frame */
    out.push_back((uint8_t)(width & 0xFF));
    out.push_back((uint8_t)((width >> 8) & 0xFF));
    out.push_back((uint8_t)(height & 0xFF));
    out.push_back((uint8_t)((height >> 8) & 0xFF));

    /* Raw RGB565 pixels (little-endian, matches x86 native order) */
    const uint8_t* p = reinterpret_cast<const uint8_t*>(pixels);
    out.insert(out.end(), p, p + num_pixels * 2);

    return out;
}

std::vector<uint8_t> WebUIServer::encode_delta_frame(const uint16_t* current,
                                                      int width, int height) {
    size_t num_pixels = (size_t)(width * height);
    std::vector<uint8_t> out;
    out.reserve(5 + 1024); /* Typically small */

    /* Header */
    out.push_back(0x02); /* Delta frame */
    out.push_back((uint8_t)(width & 0xFF));
    out.push_back((uint8_t)((width >> 8) & 0xFF));
    out.push_back((uint8_t)(height & 0xFF));
    out.push_back((uint8_t)((height >> 8) & 0xFF));

    /* Find runs of changed pixels.
     * Format: offset (uint32 LE) + count (uint16 LE) + pixels (uint16 LE each)
     * Using uint32 for offset since 240*320 = 76800 > 65535. */
    size_t i = 0;
    while (i < num_pixels) {
        /* Skip unchanged pixels */
        while (i < num_pixels && current[i] == prev_frame_[i]) i++;
        if (i >= num_pixels) break;

        /* Start of a changed run */
        size_t run_start = i;
        /* Extend run while pixels differ (or small gap of same pixels) */
        while (i < num_pixels && i - run_start < 65535) {
            if (current[i] != prev_frame_[i]) {
                i++;
            } else {
                /* Allow up to 4 unchanged pixels in a run to avoid fragmentation */
                size_t gap = 0;
                size_t j = i;
                while (j < num_pixels && j - run_start < 65535 &&
                       current[j] == prev_frame_[j] && gap < 4) {
                    j++;
                    gap++;
                }
                if (j < num_pixels && j - run_start < 65535 &&
                    current[j] != prev_frame_[j]) {
                    i = j + 1; /* Include the gap */
                } else {
                    break; /* End the run here */
                }
            }
        }

        size_t run_len = i - run_start;

        /* Encode: offset (4 bytes LE) + count (2 bytes LE) + pixels */
        uint32_t offset = (uint32_t)run_start;
        uint16_t count = (uint16_t)run_len;

        out.push_back((uint8_t)(offset & 0xFF));
        out.push_back((uint8_t)((offset >> 8) & 0xFF));
        out.push_back((uint8_t)((offset >> 16) & 0xFF));
        out.push_back((uint8_t)((offset >> 24) & 0xFF));
        out.push_back((uint8_t)(count & 0xFF));
        out.push_back((uint8_t)((count >> 8) & 0xFF));

        const uint8_t* p = reinterpret_cast<const uint8_t*>(current + run_start);
        out.insert(out.end(), p, p + run_len * 2);

        /* If delta is getting bigger than a full frame, just send full */
        if (out.size() > num_pixels * 2) {
            return {}; /* Signal: use full frame instead */
        }
    }

    return out;
}

void WebUIServer::broadcast_frame(const uint8_t* data, size_t len) {
    /* Called with clients_mutex_ held */
    for (int i = 0; i < client_count_; i++) {
        if (clients_[i].websocket_ready) {
            send_websocket_frame(clients_[i].fd, data, len, 0x02);
        }
    }
}

/* ================================================================
 * Embedded HTML page
 * ================================================================ */

static const char HTML_PAGE[] = R"HTMLRAW(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>PortaPack Remote</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{background:#1a1a2e;display:flex;flex-direction:column;justify-content:center;align-items:center;min-height:100vh;margin:0;font-family:'Courier New',monospace;color:#ccc;user-select:none;-webkit-user-select:none}
h1{color:#e94560;font-size:18px;margin:8px 0}
#screen{border:2px solid #555;cursor:crosshair;image-rendering:pixelated;image-rendering:-moz-crisp-edges;touch-action:none}
#status{color:#0f0;margin:6px;font-size:13px;min-height:18px}
#status.disconnected{color:#f55}
.dpad{display:grid;grid-template-columns:60px 60px 60px;grid-template-rows:60px 60px 60px;gap:4px;margin:8px auto}
.dpad .btn{background:#2a2a4a;color:#fff;border:1px solid #555;border-radius:8px;font-size:20px;display:flex;align-items:center;justify-content:center;cursor:pointer;touch-action:manipulation}
.dpad .btn:active,.dpad .btn.pressed{background:#e94560;border-color:#e94560}
.dpad .empty{background:transparent;border:none}
.row{display:flex;gap:6px;margin:4px}
.row .btn{background:#2a2a4a;color:#fff;border:1px solid #555;border-radius:6px;padding:10px 20px;font-size:14px;font-family:inherit;cursor:pointer;touch-action:manipulation}
.row .btn:active,.row .btn.pressed{background:#0a7e8c;border-color:#0a7e8c}
#info{color:#666;font-size:11px;margin-top:10px;text-align:center}
</style>
</head>
<body>
<h1>PortaPack Mayhem</h1>
<canvas id="screen" width="240" height="320"></canvas>
<div id="status">Connecting...</div>
<div class="dpad">
 <div class="empty"></div>
 <div class="btn" data-key="ArrowUp" id="btn-up">&#9650;</div>
 <div class="empty"></div>
 <div class="btn" data-key="ArrowLeft" id="btn-left">&#9664;</div>
 <div class="btn" data-key="Enter" id="btn-sel">OK</div>
 <div class="btn" data-key="ArrowRight" id="btn-right">&#9654;</div>
 <div class="empty"></div>
 <div class="btn" data-key="ArrowDown" id="btn-down">&#9660;</div>
 <div class="empty"></div>
</div>
<div class="row">
 <div class="btn" data-key="Escape" id="btn-back">BACK</div>
 <div class="btn" id="btn-enc-left" data-enc="-1">&#8634; ENC</div>
 <div class="btn" id="btn-enc-right" data-enc="1">ENC &#8635;</div>
</div>
<div id="info">Keyboard: Arrows/WASD=DPad, Enter=Select, Esc=Back, Scroll=Encoder<br>Click/tap screen for touch</div>
<script>
(function(){
'use strict';
var canvas=document.getElementById('screen');
var ctx=canvas.getContext('2d');
var statusEl=document.getElementById('status');
var ws=null;
var imageData=ctx.createImageData(240,320);
var connected=false;
var frameCount=0;
var lastFps=0;
var fpsTimer=0;

/* Scale canvas for visibility */
var scale=2;
if(window.innerWidth<500) scale=Math.floor(window.innerWidth/244)||1;
canvas.style.width=(240*scale)+'px';
canvas.style.height=(320*scale)+'px';

/* Fill black initially */
for(var i=0;i<240*320*4;i+=4){
 imageData.data[i]=0;imageData.data[i+1]=0;imageData.data[i+2]=0;imageData.data[i+3]=255;
}
ctx.putImageData(imageData,0,0);

function setStatus(msg,ok){
 statusEl.textContent=msg;
 statusEl.className=ok?'':'disconnected';
}

function connect(){
 if(ws&&(ws.readyState===0||ws.readyState===1)) return;
 setStatus('Connecting...',false);
 ws=new WebSocket('ws://'+location.host+'/ws');
 ws.binaryType='arraybuffer';

 ws.onopen=function(){
  connected=true;
  setStatus('Connected',true);
  frameCount=0;
  fpsTimer=performance.now();
 };

 ws.onmessage=function(evt){
  if(evt.data instanceof ArrayBuffer){
   handleFrame(new DataView(evt.data));
   frameCount++;
   var now=performance.now();
   if(now-fpsTimer>=1000){
    lastFps=Math.round(frameCount*1000/(now-fpsTimer));
    frameCount=0;
    fpsTimer=now;
    setStatus('Connected - '+lastFps+' fps',true);
   }
  }
 };

 ws.onclose=function(){
  connected=false;
  setStatus('Disconnected - reconnecting...',false);
  setTimeout(connect,1500);
 };

 ws.onerror=function(){
  ws.close();
 };
}

function handleFrame(view){
 var type=view.getUint8(0);
 var w=view.getUint16(1,true);
 var h=view.getUint16(3,true);
 var d=imageData.data;

 if(type===0x01){
  /* Full frame */
  for(var i=0;i<w*h;i++){
   var rgb565=view.getUint16(5+i*2,true);
   var idx=i*4;
   d[idx]=((rgb565>>11)&0x1F)*255/31|0;
   d[idx+1]=((rgb565>>5)&0x3F)*255/63|0;
   d[idx+2]=(rgb565&0x1F)*255/31|0;
   d[idx+3]=255;
  }
 } else if(type===0x02){
  /* Delta frame: runs of (offset:u32LE, count:u16LE, pixels) */
  var pos=5;
  while(pos+6<=view.byteLength){
   var offset=view.getUint32(pos,true);pos+=4;
   var count=view.getUint16(pos,true);pos+=2;
   if(pos+count*2>view.byteLength) break;
   for(var j=0;j<count;j++){
    var rgb565=view.getUint16(pos,true);pos+=2;
    var idx=(offset+j)*4;
    d[idx]=((rgb565>>11)&0x1F)*255/31|0;
    d[idx+1]=((rgb565>>5)&0x3F)*255/63|0;
    d[idx+2]=(rgb565&0x1F)*255/31|0;
    d[idx+3]=255;
   }
  }
 }
 ctx.putImageData(imageData,0,0);
}

function sendKey(code,down){
 if(ws&&ws.readyState===1){
  ws.send(JSON.stringify({type:'key',code:code,down:down}));
 }
}

function sendTouch(x,y,down){
 if(ws&&ws.readyState===1){
  ws.send(JSON.stringify({type:'touch',x:x,y:y,down:down}));
 }
}

function sendEncoder(delta){
 if(ws&&ws.readyState===1){
  ws.send(JSON.stringify({type:'encoder',delta:delta}));
 }
}

/* Keyboard */
document.addEventListener('keydown',function(e){
 if(e.target.tagName==='INPUT') return;
 e.preventDefault();
 sendKey(e.code,true);
});
document.addEventListener('keyup',function(e){
 if(e.target.tagName==='INPUT') return;
 e.preventDefault();
 sendKey(e.code,false);
});

/* Canvas touch/click */
function canvasCoords(e){
 var rect=canvas.getBoundingClientRect();
 return{
  x:Math.floor((e.clientX-rect.left)/(rect.width/240)),
  y:Math.floor((e.clientY-rect.top)/(rect.height/320))
 };
}

canvas.addEventListener('mousedown',function(e){
 var c=canvasCoords(e);
 sendTouch(c.x,c.y,true);
});
canvas.addEventListener('mousemove',function(e){
 if(e.buttons&1){
  var c=canvasCoords(e);
  sendTouch(c.x,c.y,true);
 }
});
canvas.addEventListener('mouseup',function(){
 sendTouch(0,0,false);
});

/* Touch events for mobile */
canvas.addEventListener('touchstart',function(e){
 e.preventDefault();
 var t=e.touches[0];
 var c=canvasCoords(t);
 sendTouch(c.x,c.y,true);
},{passive:false});
canvas.addEventListener('touchmove',function(e){
 e.preventDefault();
 var t=e.touches[0];
 var c=canvasCoords(t);
 sendTouch(c.x,c.y,true);
},{passive:false});
canvas.addEventListener('touchend',function(e){
 e.preventDefault();
 sendTouch(0,0,false);
},{passive:false});

/* Mouse wheel = encoder */
canvas.addEventListener('wheel',function(e){
 e.preventDefault();
 sendEncoder(e.deltaY>0?1:-1);
},{passive:false});

/* DPad buttons (touch + mouse) */
var activeButtons={};

function btnDown(el){
 var key=el.getAttribute('data-key');
 var enc=el.getAttribute('data-enc');
 el.classList.add('pressed');
 if(key){
  activeButtons[key]=true;
  sendKey(key,true);
 }
 if(enc){
  sendEncoder(parseInt(enc));
 }
}

function btnUp(el){
 var key=el.getAttribute('data-key');
 el.classList.remove('pressed');
 if(key&&activeButtons[key]){
  delete activeButtons[key];
  sendKey(key,false);
 }
}

document.querySelectorAll('.btn').forEach(function(el){
 el.addEventListener('mousedown',function(e){e.preventDefault();btnDown(el);});
 el.addEventListener('mouseup',function(e){e.preventDefault();btnUp(el);});
 el.addEventListener('mouseleave',function(e){btnUp(el);});
 el.addEventListener('touchstart',function(e){e.preventDefault();btnDown(el);},{passive:false});
 el.addEventListener('touchend',function(e){e.preventDefault();btnUp(el);},{passive:false});
 el.addEventListener('touchcancel',function(e){btnUp(el);});
});

connect();
})();
</script>
</body>
</html>)HTMLRAW";

const char* WebUIServer::get_html_page() {
    return HTML_PAGE;
}

size_t WebUIServer::get_html_page_len() {
    return sizeof(HTML_PAGE) - 1; /* Exclude null terminator */
}

}  // namespace shim
