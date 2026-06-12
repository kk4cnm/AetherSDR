#include "AndroidMulticastLock.h"

#include <QCoreApplication>
#include <QJniObject>
#include <QDebug>

namespace AetherSDR {

namespace {
// Process-wide lock object. QJniObject holds a JNI global ref, so the
// Java MulticastLock stays alive as long as this does.
QJniObject g_lock;
}

void AndroidMulticastLock::acquire()
{
    if (g_lock.isValid())
        return;

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        qWarning("AndroidMulticastLock: no application context");
        return;
    }

    QJniObject wifiService = context.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString(QStringLiteral("wifi")).object<jstring>());
    if (!wifiService.isValid()) {
        qWarning("AndroidMulticastLock: WifiManager unavailable");
        return;
    }

    g_lock = wifiService.callObjectMethod(
        "createMulticastLock",
        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
        QJniObject::fromString(QStringLiteral("AetherSDR-discovery")).object<jstring>());
    if (!g_lock.isValid()) {
        qWarning("AndroidMulticastLock: createMulticastLock failed");
        return;
    }

    g_lock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
    g_lock.callMethod<void>("acquire");
    qInfo("AndroidMulticastLock: acquired (UDP broadcast discovery enabled)");
}

void AndroidMulticastLock::release()
{
    if (!g_lock.isValid())
        return;
    g_lock.callMethod<void>("release");
    g_lock = QJniObject();
}

} // namespace AetherSDR
