# AetherSDR Android Port

This fork targets Android tablets (and later phones). Upstream:
[aethersdr/AetherSDR](https://github.com/aethersdr/AetherSDR).

## Strategy

**Phase 1 (this branch): the existing Qt Widgets UI on Android tablets.**
Validates the whole stack — SmartSDR protocol, VITA-49 UDP streaming,
QAudioSink/QAudioSource over AAudio, QRhi GPU spectrum on OpenGL ES —
with minimal UI work. Tablets in landscape are the target form factor;
the desktop layout is dense but usable at 10"+ with Qt's DPI scaling.

**Phase 2: touch-first QML UI for phones**, reusing `src/core` +
`src/models` unchanged. The Widgets GUI stays for tablets until QML
reaches parity.

### Phase 1 feature scope

| Feature | Status |
|---|---|
| LAN connect by IP | working (verified on hardware) |
| LAN discovery | needs `MulticastLock` + WifiLock (see below); may still be filtered by some Wi-Fi chipsets |
| Panadapter / waterfall | working via **CPU renderer** (QPainter). QRhiWidget GPU path composites black on Android (Qt 6.8.2 / Mali-G610) — forced off, see `AETHER_GPU_SPECTRUM_ANDROID_FORCE` |
| RX audio (remote audio via QAudioSink) | working, uncompressed streams (needs WifiLock — see below) |
| TX: device mic + on-screen PTT | ported (`RECORD_AUDIO` manifest + runtime permission requested at startup) |
| SmartLink (WAN) | deferred — needs Opus for Android + Qt6Keychain replacement |
| Client DSP strip / NR2 / RNNoise | compiles; FFTW absent → fallback FFT |
| DFNR / specbleach / BNR / RADE / MQTT | off (`ENABLE_*=OFF`) |
| DAX virtual audio, CAT serial ports, TCI | not applicable on Android |
| MIDI, FlexControl, USB HID knobs, evdev | compiled out (no ALSA/hidapi/evdev) |

## Android-specific code

- `src/core/AndroidMulticastLock.{h,cpp}` — acquires two WifiManager
  locks at startup:
  - `MulticastLock`: Android Wi-Fi drivers filter UDP broadcast by
    default; without it the radio's discovery packets on `:4992` never
    arrive and the radio chooser stays empty.
  - `WifiLock` (`WIFI_MODE_FULL_LOW_LATENCY`): **critical.** Wi-Fi
    power save naps the chip between beacons; the AP buffers-then-drops
    the radio's ~350 pkt/s of unsolicited VITA-49 UDP (audio, FFT,
    waterfall, meters) while TCP survives on retransmissions. Symptom
    without it: controls work, S-meter/waterfall dead, audio is
    rhythmic popping. Verified on MediaTek MT6895 (Unihertz TANK 3,
    Android 15): ~1 pkt/s without the lock, full stream rate with it.
- `android/AndroidManifest.xml` — permissions (INTERNET, RECORD_AUDIO,
  CHANGE_WIFI_MULTICAST_STATE, WAKE_LOCK, …), landscape launch activity.
- `CMakeLists.txt` — `if(ANDROID)` gates: pkg-config blinded (host
  Homebrew libraries leaked into the cross-build otherwise), NEON
  try_run results preseeded for liquid-dsp, `qt_add_executable` for APK
  packaging, MIDI/ALSA + evdev + PipeWire DAX excluded, tests/tools/
  install rules skipped via early `return()`.

## Building

Prereqs (macOS host; see `../scripts/install-android-toolchain.sh` in the
workspace root for the exact bootstrap):

- Qt 6.8.x for macOS (host tools) **and** `android_arm64_v8a`, with
  `qtmultimedia`, `qtwebsockets`, `qtshadertools`
- Android SDK: platform-tools, `platforms;android-35`,
  `build-tools;35.0.0`, `ndk;26.1.10909125`
- OpenJDK 17 (Gradle/AGP are not happy on newer JDKs)

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home
export ANDROID_SDK_ROOT=/opt/homebrew/share/android-commandlinetools

~/Qt/6.8.2/android_arm64_v8a/bin/qt-cmake -S AetherSDR -B build-android -GNinja \
  -DQT_HOST_PATH=$HOME/Qt/6.8.2/macos \
  -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT \
  -DANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/26.1.10909125 \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_RADE=OFF -DENABLE_MQTT=OFF -DENABLE_SPECBLEACH=OFF -DENABLE_DFNR=OFF

cmake --build build-android
# APK lands in build-android/android-build/build/outputs/apk/
adb install -r build-android/android-build/build/outputs/apk/debug/android-build-debug.apk
```

## Known gaps / next steps

0. **QRhiWidget GPU spectrum on Android** — composites a black surface
   (Qt 6.8.2, OpenGL ES, Mali-G610) even though pipelines initialize and
   data decodes. Re-test on Qt 6.9/6.10; suspect QRhiWidget
   texture-composition or alpha handling on the GLES backend. The CPU
   QPainter path is the Android default meanwhile.

1. **Opus for Android** — cross-compile libopus (the vendored
   `third_party/opus-rade` snapshot or upstream) so compressed remote
   audio and later SmartLink work. Until then RX audio is uncompressed
   (LAN only, ~1.5 Mbit/s per stream).
2. **Qt6Keychain** — no Android build; SmartLink credential persistence
   needs Android Keystore (Phase 1.5, together with SmartLink).
3. **Touch ergonomics** — first-run UI scale default on small screens,
   bigger hit targets for sliders/knobs, kinetic scrolling in lists.
4. **Lifecycle** — handle `applicationStateChanged` (pause streams in
   background or hold a foreground service), audio focus, and a
   `WAKE_LOCK`/`keepScreenOn` toggle while connected.
5. **FFTW for Android** — optional; NR2 currently uses the fallback FFT.
6. **Phase 2** — QML shell: connection page, slice tuning, PTT, S-meter,
   panadapter as a `QQuickRhiItem`.
