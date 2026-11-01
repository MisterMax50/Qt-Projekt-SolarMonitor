#include "sprache.h"

#include <memory>

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QTranslator>

#include "util/anzeige.h"

namespace {
std::unique_ptr<QTranslator> g_app;
std::unique_ptr<QTranslator> g_qt;
QString g_aktuell = QStringLiteral("de");
} // namespace

bool Sprache::setze(const QString& kuerzel)
{
    const QString neu = (kuerzel == QLatin1String("en")) ? QStringLiteral("en") : QStringLiteral("de");

    if (g_app) {
        QCoreApplication::removeTranslator(g_app.get());
        g_app.reset();
    }
    if (g_qt) {
        QCoreApplication::removeTranslator(g_qt.get());
        g_qt.reset();
    }

    // Deutsch ist die Quellsprache; SolarMonitor_de enthält nur Pluralformen
    // und darf daher fehlen. Für Englisch ist die Übersetzung Pflicht.
    bool ok = true;
    auto t = std::make_unique<QTranslator>();
    if (t->load(QStringLiteral("SolarMonitor_") + neu, QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(t.get());
        g_app = std::move(t);
    } else if (neu == QLatin1String("en")) {
        ok = false;
    }

    // Qt-Standardtexte (OK/Abbrechen, Dateidialog …) in der Zielsprache
    auto qt = std::make_unique<QTranslator>();
    if (qt->load(QStringLiteral("qtbase_") + neu, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(qt.get());
        g_qt = std::move(qt);
    }

    g_aktuell = ok ? neu : QStringLiteral("de");
    Anzeige::setSprache(g_aktuell);
    return ok;
}

QString Sprache::aktuell()
{
    return g_aktuell;
}
