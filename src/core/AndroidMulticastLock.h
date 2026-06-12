#pragma once

namespace AetherSDR {

// Holds the two WifiManager locks the app needs for radio traffic:
//
// - MulticastLock: the Wi-Fi driver filters broadcast/multicast frames by
//   default to save power; without it the UDP discovery broadcasts
//   FlexRadio sends on :4992 silently never arrive.
// - WifiLock (low-latency): Wi-Fi power save naps the chip between
//   beacons, so the AP buffers-then-drops the radio's high-rate VITA-49
//   unicast streams (audio, FFT, waterfall) while TCP limps along on
//   retransmissions. The lock disables power save while the app runs.
//
// Compiled on Android only (see the ANDROID block in CMakeLists.txt).
class AndroidMulticastLock {
public:
    // Acquire the process-wide lock. Safe to call more than once.
    static void acquire();
    // Release it (not called in normal operation — the lock is held for
    // the app's lifetime; battery cost is acceptable for an SDR client
    // that streams continuously anyway).
    static void release();
};

} // namespace AetherSDR
