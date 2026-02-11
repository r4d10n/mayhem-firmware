/*
 * Processor Factory — creates BasebandProcessor instances by image tag
 *
 * Each entry maps a portapack::spi_flash::image_tag_t (4-char ID) to
 * the corresponding processor class. On real hardware these are separate
 * M4 binaries; here they're all compiled into one binary.
 */

#include "processor_factory.hpp"
#include "spi_image.hpp"

/* Include all processor headers */
#include "proc_acars.hpp"
#include "proc_adsbrx.hpp"
#include "proc_adsbtx.hpp"
#include "proc_afsk.hpp"
#include "proc_afskrx.hpp"
#include "proc_ais.hpp"
#include "proc_am_audio.hpp"
// proc_am_tv.hpp excluded — redefines WidebandFMAudio class
#include "proc_aprsrx.hpp"
#include "proc_audio_beep.hpp"
#include "proc_audiotx.hpp"
#include "proc_ble_tx.hpp"
#include "proc_bint_stream_tx.hpp"
#include "proc_btlerx.hpp"
#include "proc_capture.hpp"
#include "proc_epirb.hpp"
#include "proc_ert.hpp"
#include "proc_flex.hpp"
#include "proc_fsk.hpp"
#include "proc_fsk_rx.hpp"
#include "proc_gps_sim.hpp"
#include "proc_jammer.hpp"
#include "proc_mictx.hpp"
#include "proc_morse.hpp"
#include "proc_morsetx.hpp"
#include "proc_nfm_audio.hpp"
#include "proc_noaaapt_rx.hpp"
#include "proc_nrfrx.hpp"
#include "proc_ook.hpp"
#include "proc_pocsag2.hpp"
#include "proc_protoview.hpp"
#include "proc_rds.hpp"
#include "proc_replay.hpp"
#include "proc_siggen.hpp"
#include "proc_sonde.hpp"
#include "proc_spectrum_painter.hpp"
#include "proc_sstvrx.hpp"
#include "proc_sstvtx.hpp"
#include "proc_subcar.hpp"
#include "proc_subghzd.hpp"
#include "proc_test.hpp"
#include "proc_tones.hpp"
#include "proc_tpms.hpp"
#include "proc_weather.hpp"
#include "proc_wefaxrx.hpp"
#include "proc_wfm_audio.hpp"
#include "proc_wideband_spectrum.hpp"

#include <cstdio>
#include <cstring>

using namespace portapack::spi_flash;

namespace shim {

std::unique_ptr<BasebandProcessor> create_processor(const image_tag_t& tag) {
    /* RX processors */
    if (tag == image_tag_nfm_audio)          return std::make_unique<NarrowbandFMAudio>();
    if (tag == image_tag_wfm_audio)          return std::make_unique<WidebandFMAudio>();
    if (tag == image_tag_am_audio)           return std::make_unique<NarrowbandAMAudio>();
    if (tag == image_tag_wideband_spectrum)   return std::make_unique<WidebandSpectrum>();
    if (tag == image_tag_adsb_rx)            return std::make_unique<ADSBRXProcessor>();
    if (tag == image_tag_ais)                return std::make_unique<AISProcessor>();
    if (tag == image_tag_acars)              return std::make_unique<ACARSProcessor>();
    if (tag == image_tag_afsk_rx)            return std::make_unique<AFSKRxProcessor>();
    if (tag == image_tag_aprs_rx)            return std::make_unique<APRSRxProcessor>();
    if (tag == image_tag_btle_rx)            return std::make_unique<BTLERxProcessor>();
    if (tag == image_tag_nrf_rx)             return std::make_unique<NRFRxProcessor>();
    if (tag == image_tag_capture)            return std::make_unique<CaptureProcessor>();
    if (tag == image_tag_ert)                return std::make_unique<ERTProcessor>();
    if (tag == image_tag_epirb_rx)           return std::make_unique<EPIRBProcessor>();
    if (tag == image_tag_fskrx)              return std::make_unique<FSKRxProcessor>();
    if (tag == image_tag_pocsag2)            return std::make_unique<POCSAGProcessor>();
    if (tag == image_tag_pocsag)             return std::make_unique<POCSAGProcessor>();
    if (tag == image_tag_flex)               return std::make_unique<FlexProcessor>();
    if (tag == image_tag_sonde)              return std::make_unique<SondeProcessor>();
    if (tag == image_tag_tpms)               return std::make_unique<TPMSProcessor>();
    if (tag == image_tag_weather)            return std::make_unique<WeatherProcessor>();
    if (tag == image_tag_subghzd)            return std::make_unique<SubGhzDProcessor>();
    if (tag == image_tag_subcar)             return std::make_unique<SubCarProcessor>();
    if (tag == image_tag_protoview)          return std::make_unique<ProtoViewProcessor>();
    if (tag == image_tag_wefaxrx)            return std::make_unique<WeFaxRx>();
    if (tag == image_tag_noaaapt_rx)         return std::make_unique<NoaaAptRx>();
    if (tag == image_tag_sstv_rx)            return std::make_unique<SSTVRXProcessor>();
    if (tag == image_tag_morse)              return std::make_unique<MorseProcessor>();

    /* TX processors */
    if (tag == image_tag_adsb_tx)            return std::make_unique<ADSBTXProcessor>();
    if (tag == image_tag_afsk)               return std::make_unique<AFSKProcessor>();
    if (tag == image_tag_audio_tx)           return std::make_unique<AudioTXProcessor>();
    if (tag == image_tag_btle_tx)            return std::make_unique<BTLETxProcessor>();
    if (tag == image_tag_fsktx)              return std::make_unique<FSKProcessor>();
    if (tag == image_tag_gps)                return std::make_unique<GPSReplayProcessor>();
    if (tag == image_tag_jammer)             return std::make_unique<JammerProcessor>();
    if (tag == image_tag_mic_tx)             return std::make_unique<MicTXProcessor>();
    if (tag == image_tag_morsetx)            return std::make_unique<MorseTXProcessor>();
    if (tag == image_tag_ook)                return std::make_unique<OOKProcessor>();
    if (tag == image_tag_ookstream)          return std::make_unique<BinaryTimedProcessorStreamed>();
    if (tag == image_tag_rds)                return std::make_unique<RDSProcessor>();
    if (tag == image_tag_replay)             return std::make_unique<ReplayProcessor>();
    if (tag == image_tag_siggen)             return std::make_unique<SigGenProcessor>();
    if (tag == image_tag_spectrum_painter)   return std::make_unique<SpectrumPainterProcessor>();
    if (tag == image_tag_sstv_tx)            return std::make_unique<SSTVTXProcessor>();
    if (tag == image_tag_tones)              return std::make_unique<TonesProcessor>();

    /* Utility / other */
    if (tag == image_tag_audio_beep)         return std::make_unique<AudioBeepProcessor>();
    if (tag == image_tag_test)               return std::make_unique<TestProcessor>();

    /* Unknown tag */
    char t[5] = {};
    std::memcpy(t, &tag, 4);
    fprintf(stderr, "[Factory] Unknown processor tag: '%s'\n", t);
    return nullptr;
}

const char* processor_name(const image_tag_t& tag) {
    if (tag == image_tag_nfm_audio)          return "NFM Audio";
    if (tag == image_tag_wfm_audio)          return "WFM Audio";
    if (tag == image_tag_am_audio)           return "AM Audio";
    if (tag == image_tag_wideband_spectrum)   return "Wideband Spectrum";
    if (tag == image_tag_adsb_rx)            return "ADS-B RX";
    if (tag == image_tag_ais)                return "AIS";
    if (tag == image_tag_acars)              return "ACARS";
    if (tag == image_tag_capture)            return "Capture";
    if (tag == image_tag_nfm_audio)          return "NFM Audio";
    if (tag == image_tag_pocsag2)            return "POCSAG";
    if (tag == image_tag_flex)               return "FLEX";
    if (tag == image_tag_sonde)              return "Sonde";
    if (tag == image_tag_tpms)               return "TPMS";
    if (tag == image_tag_weather)            return "Weather";
    if (tag == image_tag_audio_beep)         return "Audio Beep";
    if (tag == image_tag_test)               return "Test";
    if (tag == image_tag_replay)             return "Replay";
    if (tag == image_tag_siggen)             return "SigGen";
    return "Unknown";
}

} // namespace shim
