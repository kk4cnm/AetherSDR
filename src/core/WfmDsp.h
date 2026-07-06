#pragma once

#include <array>
#include <memory>
#include <vector>

namespace AetherSDR {

class Resampler;

// ---------------------------------------------------------------------------
// WfmDsp — pure DSP core of the WFM data demodulator.
//
// No Qt audio, no radio I/O, fully unit-testable (tests/wfm_dsp_test.cpp).
// Replicates the receive chain of SkyRoof's Slicer (VE3NEA), the reference
// implementation whose output HS-SoundModem decodes flawlessly:
//
//   IQ @ native device rate
//     → [1] NCO mix-down   (phase-continuous offset/Doppler correction)
//     → [2] twin linear-phase resamplers → exactly 48 kHz
//     → [3] phase-difference FM discriminator (atan2, amplitude-invariant)
//     → [4] FIR low-pass, 95 taps, fc = 20 kHz, Hamming, linear phase
//     → mono float audio @ 48 kHz
//
// Why the NCO matters: the panadapter (and with it the DAX IQ centre) stays
// FIXED during a satellite pass while external Doppler software steps the
// slice frequency. An FM signal offset Δf from the IQ centre demodulates
// with a DC term of 2·Δf·kGain/fs; at kGain = 3 that hard-clips downstream
// beyond ≈8 kHz offset — half the UHF Doppler swing. Mixing the offset down
// BEFORE the discriminator removes the DC term, and because only the NCO
// frequency changes (never its phase) each Doppler step is click-free, so
// the modem never loses lock. SkyRoof does exactly this (Slicer.SetOffset).
//
// Why native-rate input matters: capturing the DAX IQ endpoint at a forced
// 48 kHz lets the OS mixer silently resample whatever rate DAX is really
// set to (24/48/96/192 k). A 24 k stream upsampled by Windows has no energy
// above ±12 kHz — the "10–13.5 kHz waterfall notch" previously chased with
// EQ compensators. Feed this class the true device rate instead; it
// resamples to exactly 48 kHz with r8brain, flat to 0.95·Nyquist — the same
// useful-bandwidth ratio SkyRoof uses for its Kaiser decimators.
//
// Deliberately NO de-emphasis, squelch, AGC or EQ anywhere in the chain:
// the output feeds data modems (G3RUH 9600 bd needs a flat, linear-phase
// discriminator response). The rising f² noise floor on a waterfall is the
// normal signature of an FM discriminator — do not "fix" it here.
//
// Threading: single-threaded by design. All calls — including
// setFreqOffsetHz() — must come from the same thread (the main thread today).
// ---------------------------------------------------------------------------
class WfmDsp
{
public:
    static constexpr int   kAudioRate  = 48000;  // output rate, always exact
    static constexpr int   kLpCutoffHz = 20000;  // post-demod FIR fc
    static constexpr float kGain       = 3.0f;   // G3RUH ±3 kHz dev → ≈±0.4

    static constexpr int kFirOrder = 94;             // even → odd taps → Type-I
    static constexpr int kFirTaps  = kFirOrder + 1;  // 95

    explicit WfmDsp(int iqRateHz);
    ~WfmDsp();

    int iqRateHz() const { return m_iqRate; }

    // Largest |signal − IQ centre| the chain can absorb: the resampler
    // passband edge (0.95 · min(iqRate, 48 k)/2) minus an 8 kHz guard for
    // the signal's own width (G3RUH Carson bandwidth ≈ ±8 kHz).
    float maxFreqOffsetHz() const;

    // offsetHz = signal frequency − IQ centre frequency. Phase-continuous:
    // only the NCO step changes, the accumulated phase never jumps.
    void setFreqOffsetHz(float offsetHz);

    // Demodulate `frames` interleaved IQ pairs at iqRateHz(). Replaces
    // `audioOut` with mono 48 kHz audio. Output is unclamped, nominal
    // ±2·dev·kGain/48000 — the caller applies volume/limiting.
    void process(const float* iqInterleaved, int frames, std::vector<float>& audioOut);

private:
    const int m_iqRate;

    // [1] NCO (SkyRoof FirstMixer): θ advances by m_ncoStep per input sample
    double m_ncoPhase{0.0};
    double m_ncoStep{0.0};

    // [2] identical twin resamplers keep I and Q phase-matched (same
    // deterministic linear-phase filter, same latency, same output length);
    // null when iqRate == 48 kHz
    std::unique_ptr<Resampler> m_resampleI;
    std::unique_ptr<Resampler> m_resampleQ;
    std::vector<float> m_workI, m_workQ;

    // [3] discriminator state
    float m_prevI{0.0f};
    float m_prevQ{0.0f};

    // [4] FIR delay line (circular)
    std::array<float, kFirTaps> m_firBuf{};
    int m_firIdx{0};
};

} // namespace AetherSDR
