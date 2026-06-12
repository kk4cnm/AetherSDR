#pragma once

namespace AetherSDR {

// Holds a WifiManager.MulticastLock so the Wi-Fi stack delivers the UDP
// broadcast discovery packets FlexRadio transmits on port 4992. Android
// filters broadcast/multicast frames in the Wi-Fi driver by default to
// save power; without this lock discovery silently finds nothing.
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
