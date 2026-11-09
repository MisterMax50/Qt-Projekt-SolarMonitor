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
    QString neu = QStringLiteral("de");
    for (const auto& [k, name] : verfuegbar())
        if (k == kuerzel)
            neu = k;

    if (g_app) {
        QCoreApplication::removeTranslator(g_app.get());
        g_app.reset();
    }
    if (g_qt) {
        QCoreApplication::removeTranslator(g_qt.get());
        g_qt.reset();
    }

    // Deutsch ist die Quellsprache; SolarMonitor_de enthält nur Pluralformen
    // und darf daher fehlen. Für alle anderen Sprachen ist sie Pflicht.
    bool ok = true;
    auto t = std::make_unique<QTranslator>();
    if (t->load(QStringLiteral("SolarMonitor_") + neu, QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(t.get());
        g_app = std::move(t);
    } else if (neu != QLatin1String("de")) {
        ok = false;
    }

    // Qt-Standardtexte (OK/Abbrechen, Dateidialog …) in der Zielsprache
    auto qt = std::make_unique<QTranslator>();
    const QString qtName = neu == QLatin1String("zh") ? QStringLiteral("qtbase_zh_CN") : QStringLiteral("qtbase_") + neu;
    if (qt->load(qtName, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
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

QList<QPair<QString, QString>> Sprache::verfuegbar()
{
    return {{QStringLiteral("de"), QStringLiteral("Deutsch")},
            {QStringLiteral("en"), QStringLiteral("English")},
            {QStringLiteral("fr"), QStringLiteral("Français")},
            {QStringLiteral("zh"), QStringLiteral("中文（简体）")}};
}
