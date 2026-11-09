#include <memory>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "repo/verlauf_repository.h"
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
 *  2. data/ im Projektordner (Ordner mit CMakeLists.txt). Der Pfad
 *     wird beim Bauen als SOLARMONITOR_PROJEKTORDNER eingetragen; die CSV-
 *     Dateien müssen daher nicht in den Build-Ordner kopiert werden.
 *  3. Relativ zur Exe: daneben und bis zu vier Ebenen darüber, danach im
 *     Arbeitsverzeichnis (falls das Projekt verschoben oder die Exe
 *     weitergegeben wurde).
 */
QString findeDatenordner(const QString& vorgabe)
{
    if (istDatenordner(vorgabe))
        return QDir(vorgabe).absolutePath();

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

QString fehlerText(const Datenbasis& daten)
{
    QStringList zeilen;
    for (const Pruefmeldung& m : daten.pruefmeldungen())
        if (m.stufe == Pruefmeldung::Stufe::Fehler && m.zeile == 0)
            zeilen.append(QStringLiteral("• ") + m.text);
    return zeilen.join(QLatin1Char('\n'));
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
    const QCommandLineOption spracheOpt(QStringLiteral("lang"), QStringLiteral("Sprache: de oder en"),
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

    // ---------- Datenordner ----------
    QString ordner = findeDatenordner(parser.value(datenOpt));
    if (ordner.isEmpty()) {
        QMessageBox::information(nullptr, QStringLiteral("SolarMonitor"),
                                 QCoreApplication::translate("main", "Der Datenordner (mit anlagen.csv) wurde nicht "
                                                                     "gefunden. Bitte im nächsten Schritt auswählen."));
        ordner = QFileDialog::getExistingDirectory(nullptr, QCoreApplication::translate("main", "Datenordner wählen"));
        if (ordner.isEmpty())
            return 1;
    }

    // ---------- Stammdaten ----------
    auto daten = std::make_unique<Datenbasis>();
    while (!daten->lade(ordner)) {
        const auto antwort = QMessageBox::critical(
            nullptr, QStringLiteral("SolarMonitor"),
            QCoreApplication::translate("main", "In %1 konnten keine Anlagen geladen werden:\n\n%2\n\n"
                                                "Einen anderen Datenordner wählen?")
                .arg(QDir::toNativeSeparators(ordner), fehlerText(*daten)),
            QMessageBox::Yes | QMessageBox::Cancel);
        if (antwort != QMessageBox::Yes)
            return 1;
        ordner = QFileDialog::getExistingDirectory(nullptr, QCoreApplication::translate("main", "Datenordner wählen"), ordner);
        if (ordner.isEmpty())
            return 1;
    }

    // ---------- Verlaufsindex (die JSON-Dateien selbst werden erst bei Bedarf geladen) ----------
    auto verlaeufe = std::make_unique<VerlaufRepository>();
    verlaeufe->ladeIndex(daten->datenOrdner());
    QVector<Pruefmeldung> meldungen = verlaeufe->indexMeldungen();
    for (const Anlage& a : daten->anlagen())
        if (!verlaeufe->istEingetragen(a.anlageId))
            meldungen.append({Pruefmeldung::Stufe::Info, QStringLiteral("master_verlauf_index.csv"), 0, a.anlageId,
                              QCoreApplication::translate("main", "Kein Verlauf im Index – nur Stammdaten verfügbar.")});
    for (const QString& id : verlaeufe->anlagenMitVerlauf())
        if (!daten->anlage(id))
            meldungen.append({Pruefmeldung::Stufe::Info, QStringLiteral("master_verlauf_index.csv"), 0, id,
                              QCoreApplication::translate("main", "Verlauf vorhanden, aber keine Stammdaten in anlagen.csv.")});
    daten->ergaenzeMeldungen(meldungen);

    // ---------- Hauptfenster: Intro → Anmeldung → Programm in einem Fenster ----------
    auto analyse = std::make_unique<FlottenAnalyse>(daten.get(), verlaeufe.get());
    AppKontext kontext;
    kontext.daten = daten.get();
    kontext.verlaeufe = verlaeufe.get();
    kontext.analyse = analyse.get();

    MainWindow fenster(kontext);
    // Intro abschaltbar im Benutzermenü oder mit --kein-intro
    fenster.starte(!parser.isSet(ohneIntroOpt) && einstellungen.value(QStringLiteral("intro"), true).toBool());

    return app.exec();
}
