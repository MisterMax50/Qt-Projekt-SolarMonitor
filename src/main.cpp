#include <memory>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include "model/datensatz.h"
#include "ui/mainwindow.h"
#include "util/sprache.h"
#include "util/stil.h"

namespace {

bool istDatenordner(const QString& ordner)
{
    return !ordner.isEmpty() && QFileInfo::exists(QDir(ordner).filePath(QStringLiteral("anlagen.csv")));
}

/**
 * Sucht den Datenordner in dieser Reihenfolge:
 *  1. Ordner aus --daten
 *  2. der zuletzt in der App gewählte Ordner (QSettings "datenordner")
 *  3. data/ im Projektordner (Ordner mit CMakeLists.txt). Der Pfad
 *     wird beim Bauen als SOLARMONITOR_PROJEKTORDNER eingetragen; die CSV-
 *     Dateien müssen daher nicht in den Build-Ordner kopiert werden.
 *  4. Relativ zur Exe: daneben und bis zu vier Ebenen darüber, danach im
 *     Arbeitsverzeichnis (falls das Projekt verschoben oder die Exe
 *     weitergegeben wurde).
 */
QString findeDatenordner(const QString& vorgabe)
{
    if (istDatenordner(vorgabe))
        return QDir(vorgabe).absolutePath();
    const QString gespeichert = QSettings().value(QStringLiteral("datenordner")).toString();
    if (istDatenordner(gespeichert))
        return QDir(gespeichert).absolutePath();

#ifdef SOLARMONITOR_PROJEKTORDNER
    const QString projektDaten =
        QDir(QString::fromUtf8(SOLARMONITOR_PROJEKTORDNER)).filePath(QStringLiteral("data"));
    if (istDatenordner(projektDaten))
        return QDir(projektDaten).absolutePath();
#endif

    QDir dir(QCoreApplication::applicationDirPath());
    for (int ebene = 0; ebene <= 4; ++ebene) {
        if (istDatenordner(dir.filePath(QStringLiteral("data"))))
            return QDir(dir.filePath(QStringLiteral("data"))).absolutePath();
        if (istDatenordner(dir.path()))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    if (istDatenordner(QStringLiteral("data")))
        return QDir(QStringLiteral("data")).absolutePath();
    return QString();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SolarMonitor"));
    QApplication::setApplicationVersion(QStringLiteral("2.0"));
    QApplication::setOrganizationName(QStringLiteral("SolarMonitor"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SolarMonitor – Monitoring einer PV-Flotte"));
    parser.addHelpOption();
    const QCommandLineOption datenOpt(QStringLiteral("daten"),
                                      QStringLiteral("Datenordner mit anlagen.csv (Standard: ./data)"),
                                      QStringLiteral("ordner"));
    const QCommandLineOption spracheOpt(QStringLiteral("lang"), QStringLiteral("Sprache: de, en, fr oder zh"),
                                        QStringLiteral("sprache"));
    const QCommandLineOption ohneIntroOpt(QStringLiteral("kein-intro"), QStringLiteral("Startanimation überspringen"));
    parser.addOption(datenOpt);
    parser.addOption(spracheOpt);
    parser.addOption(ohneIntroOpt);
    parser.process(app);

    // Gespeicherte Einstellungen (Sprache, Design); --lang hat Vorrang
    QSettings einstellungen;
    Stil::anwenden(einstellungen.value(QStringLiteral("dunkel"), false).toBool());
    Sprache::setze(parser.isSet(spracheOpt) ? parser.value(spracheOpt).toLower()
                                            : einstellungen.value(QStringLiteral("sprache"), QStringLiteral("de")).toString());

    // ---------- Daten laden; fehlen sie, fragt das Fenster nach der Anmeldung nach dem Ordner ----------
    auto datensatz = Datensatz::lade(findeDatenordner(parser.value(datenOpt)));

    // ---------- Hauptfenster: Intro → Anmeldung → (Datenordner) → Programm ----------
    MainWindow fenster(std::move(datensatz));
    // Intro abschaltbar im Benutzermenü oder mit --kein-intro
    fenster.starte(!parser.isSet(ohneIntroOpt) && einstellungen.value(QStringLiteral("intro"), true).toBool());
    return app.exec();
}
