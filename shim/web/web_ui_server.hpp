/*
 * WebSocket-based Web UI server for the Linux shim.
 *
 * Zero-dependency implementation using raw POSIX sockets.
 * Serves an embedded HTML page and streams the RGB565 framebuffer
 * to connected browsers via WebSocket binary frames.
 * Receives input events (keyboard, touch, encoder) via WebSocket
 * text frames and maps them to shim::g_input atomics.
 *
 * Runs in a dedicated thread alongside SDL2 (both can be active).
 */

#ifndef __SHIM_WEB_UI_SERVER_HPP__
#define __SHIM_WEB_UI_SERVER_HPP__

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>

namespace shim {

class WebUIServer {
   public:
    static WebUIServer& get() {
        static WebUIServer instance;
        return instance;
    }

    bool start(int port = 8080);
    void stop();
    bool is_running() const { return running_.load(std::memory_order_relaxed); }

    /* Called from the display refresh loop to push new frames.
     * Compares against the previous frame and sends deltas. */
    void push_frame(const uint16_t* rgb565, int width, int height);

   private:
    WebUIServer() = default;
    ~WebUIServer();

    WebUIServer(const WebUIServer&) = delete;
    WebUIServer& operator=(const WebUIServer&) = delete;

    void server_thread_func();
    void handle_client_data(int idx);
    bool websocket_handshake(int fd);
    void send_websocket_frame(int fd, const uint8_t* data, size_t len, uint8_t opcode);
    void send_http_response(int fd, const char* content_type, const char* body, size_t body_len);
    void process_websocket_message(int fd, const uint8_t* data, size_t len, uint8_t opcode);
    void remove_client(int idx);
    void broadcast_frame(const uint8_t* data, size_t len);

    /* Frame compression: RLE delta encoding on RGB565 */
    std::vector<uint8_t> encode_full_frame(const uint16_t* pixels, int width, int height);
    std::vector<uint8_t> encode_delta_frame(const uint16_t* current, int width, int height);

    int server_fd_ = -1;
    int port_ = 8080;
    std::atomic<bool> running_{false};
    std::thread server_thread_;

    /* Connected clients */
    struct Client {
        int fd = -1;
        bool websocket_ready = false;
        bool needs_full_frame = true;
        std::vector<uint8_t> recv_buffer;
    };
    static constexpr int MAX_CLIENTS = 8;
    Client clients_[MAX_CLIENTS];
    int client_count_ = 0;
    std::mutex clients_mutex_;

    /* Previous frame for delta encoding */
    std::vector<uint16_t> prev_frame_;
    std::mutex frame_mutex_;

    /* Embedded HTML page content */
    static const char* get_html_page();
    static size_t get_html_page_len();
};

}  // namespace shim

#endif /* __SHIM_WEB_UI_SERVER_HPP__ */
