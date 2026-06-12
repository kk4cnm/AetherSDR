#include "AndroidMulticastLock.h"

#include <QCoreApplication>
#include <QJniObject>
#include <QDebug>

namespace AetherSDR {

namespace {
// Process-wide lock objects. QJniObject holds a JNI global ref, so the
// Java lock objects stay alive as long as these do.
QJniObject g_multicastLock;
QJniObject g_wifiLock;

QJniObject wifiManager()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return {};
    return context.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString(QStringLiteral("wifi")).object<jstring>());
}
}

void AndroidMulticastLock::acquire()
{
    QJniObject wifi = wifiManager();
    if (!wifi.isValid()) {
        qWarning("AndroidMulticastLock: WifiManager unavailable");
        return;
    }

    // 1. MulticastLock — without it the Wi-Fi driver filters the UDP
    //    broadcast discovery packets FlexRadio sends on :4992.
    if (!g_multicastLock.isValid()) {
        g_multicastLock = wifi.callObjectMethod(
            "createMulticastLock",
            "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
            QJniObject::fromString(QStringLiteral("AetherSDR-discovery")).object<jstring>());
        if (g_multicastLock.isValid()) {
            g_multicastLock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
            g_multicastLock.callMethod<void>("acquire");
            qInfo("AndroidMulticastLock: multicast lock acquired");
        } else {
            qWarning("AndroidMulticastLock: createMulticastLock failed");
        }
    }

    // 2. WifiLock in low-latency mode — without it Wi-Fi power save naps
    //    the radio chip between beacons and the AP buffers-then-drops the
    //    radio's high-rate unsolicited UDP (VITA-49 audio, FFT, waterfall).
    //    TCP survives power save via retransmission, which is why the
    //    command channel works while streams starve. Verified on MediaTek
    //    (Unihertz TANK 3): ~0 stream packets without the lock, full rate
    //    with it. WIFI_MODE_FULL_LOW_LATENCY (4) needs API 29; fall back
    //    to WIFI_MODE_FULL_HIGH_PERF (3) on API 28.
    if (!g_wifiLock.isValid()) {
        const jint sdkInt = QNativeInterface::QAndroidApplication::sdkVersion();
        const jint mode = sdkInt >= 29 ? 4 /*LOW_LATENCY*/ : 3 /*HIGH_PERF*/;
        g_wifiLock = wifi.callObjectMethod(
            "createWifiLock",
            "(ILjava/lang/String;)Landroid/net/wifi/WifiManager$WifiLock;",
            mode,
            QJniObject::fromString(QStringLiteral("AetherSDR-streaming")).object<jstring>());
        if (g_wifiLock.isValid()) {
            g_wifiLock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
            g_wifiLock.callMethod<void>("acquire");
            qInfo("AndroidMulticastLock: wifi low-latency lock acquired (mode %d)", mode);
        } else {
            qWarning("AndroidMulticastLock: createWifiLock failed");
        }
    }
}

void AndroidMulticastLock::release()
{
    if (g_multicastLock.isValid()) {
        g_multicastLock.callMethod<void>("release");
        g_multicastLock = QJniObject();
    }
    if (g_wifiLock.isValid()) {
        g_wifiLock.callMethod<void>("release");
        g_wifiLock = QJniObject();
    }
}

} // namespace AetherSDR
