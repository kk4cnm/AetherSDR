// Proof-of-concept: AetherSDR's honest, API-policy-aware read of the public
// KiwiSDR directory (kiwisdr.com/public).
//
// Demonstrates exactly how AetherSDR honors each operator's external-API
// policy (ext_api) BEFORE attempting any connection:
//   • ext_api == 0  → "web only": AetherSDR will NOT open a native API
//                     connection; it routes the user to the web client.
//   • ext_api  > 0  → API permitted (up to that many channels).
//
// It identifies honestly as "AetherSDR/<ver>" (never a spoofed browser) and
// fetches only on this explicit invocation — the program IS the single human
// action; there is no polling, caching, or enumeration.
//
// Usage:
//   kiwi_directory_poc            # honest live fetch from kiwisdr.com/public
//   kiwi_directory_poc <file>     # parse a previously-saved directory HTML

#include "core/KiwiPublicDirectory.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>

using AetherSDR::KiwiPublicDirectory;
using AetherSDR::KiwiPublicReceiver;

static void report(const QVector<KiwiPublicReceiver>& rxs)
{
    QTextStream out(stdout);
    int disabled = 0, limited = 0, open = 0, unknown = 0;
    for (const auto& r : rxs) {
        switch (r.apiPolicy()) {
            case KiwiPublicReceiver::ApiPolicy::Disabled: ++disabled; break;
            case KiwiPublicReceiver::ApiPolicy::Limited:  ++limited;  break;
            case KiwiPublicReceiver::ApiPolicy::Open:     ++open;     break;
            case KiwiPublicReceiver::ApiPolicy::Unknown:  ++unknown;  break;
        }
    }

    out << "\n================ AetherSDR — KiwiSDR public directory ================\n";
    out << "User-Agent sent : " << KiwiPublicDirectory::userAgent() << "\n";
    out << "Receivers parsed: " << rxs.size() << "\n\n";
    out << "Per-operator external-API policy (read from the directory):\n";
    out << "  web-only (ext_api=0) : " << disabled
        << "   <- AetherSDR will NOT connect via API; uses web client\n";
    out << "  API limited          : " << limited  << "   (some channels reserved for web)\n";
    out << "  API open             : " << open     << "\n";
    out << "  policy not published : " << unknown  << "\n\n";

    out << "--- honoring 'web only' operators (first 8 of " << disabled << ") ---\n";
    int shown = 0;
    for (const auto& r : rxs) {
        if (r.apiPolicy() != KiwiPublicReceiver::ApiPolicy::Disabled) continue;
        out << "  [WEB ONLY] " << r.url << "\n"
            << "             " << r.location << "  |  " << r.apiBadge() << "\n"
            << "             mayConnectViaApi() = "
            << (r.mayConnectViaApi() ? "true  (!!)" : "false  -> route to web client") << "\n";
        if (++shown >= 8) break;
    }

    out << "\n--- a few API-permitted operators (AetherSDR streams natively) ---\n";
    shown = 0;
    for (const auto& r : rxs) {
        if (r.apiPolicy() == KiwiPublicReceiver::ApiPolicy::Disabled
            || r.apiPolicy() == KiwiPublicReceiver::ApiPolicy::Unknown) continue;
        out << "  [API OK]   " << r.url << "  |  users " << r.users << "/" << r.usersMax
            << "  |  " << r.apiBadge() << "\n";
        if (++shown >= 6) break;
    }
    out << "=====================================================================\n";
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    if (argc > 1) {
        // Offline parse of a saved directory page.
        QFile f(QString::fromLocal8Bit(argv[1]));
        if (!f.open(QIODevice::ReadOnly)) {
            QTextStream(stderr) << "cannot open " << argv[1] << "\n";
            return 1;
        }
        report(KiwiPublicDirectory::parse(f.readAll()));
        return 0;
    }

    // Honest live fetch.
    KiwiPublicDirectory dir;
    QObject::connect(&dir, &KiwiPublicDirectory::ready,
                     [](const QVector<KiwiPublicReceiver>& rxs) {
        report(rxs);
        QCoreApplication::quit();
    });
    QObject::connect(&dir, &KiwiPublicDirectory::failed, [](const QString& err) {
        QTextStream(stderr) << "fetch failed: " << err << "\n";
        QCoreApplication::exit(1);
    });
    QTimer::singleShot(0, &dir, &KiwiPublicDirectory::fetch);
    QTimer::singleShot(30000, []() {
        QTextStream(stderr) << "timeout\n"; QCoreApplication::exit(1);
    });
    return app.exec();
}
