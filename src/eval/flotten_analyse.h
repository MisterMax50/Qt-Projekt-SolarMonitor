#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QVector>

#include "eval/ertrags_auswertung.h"
#include "model/anlage.h"
#include "model/stoerung.h"

class Datenbasis;
class VerlaufRepository;

/// Kennzahlen einer Anlage aus der Flottenanalyse.
struct Kennzahl {
    QString    anlageId;
    QString    stationId;
    bool       ok = false;
    QString    fehler;          ///< z. B. defekte JSON
    Auswertung auswertung;      ///< ohne Tageswerte (spart Speicher)
    double     medianPrStation = std::nan("");
    double     sollKwh = 0.0;   ///< H · Ppeak · Referenz-PR
    double     verlustKwh = 0.0;
    double     verlustEuro = 0.0;
    QVector<Stoerung> simStoerungen; ///< nur Simulationsmodus: aus dem erzeugten Verlauf, im GUI-Thread übernommen
};

/**
 * @brief Wertet die Verläufe aller Anlagen im Hintergrund aus
 *        (Erweiterungen 5, 6 und 7).
 *
 * Ablauf: Für jede Anlage mit Verlauf wird ein Auftrag erzeugt und mit
 * QtConcurrent::mapped parallel abgearbeitet (JSON lesen + berechnen,
 * ohne gemeinsamen Zustand). Danach werden im GUI-Thread die PR-Mediane
 * je Wetterstation gebildet, Minderleistung markiert und der
 * Ertragsverlust geschätzt:
 *
 *   Soll    = H · Ppeak · PR_ref     (PR_ref = Stationsmedian, sonst 0,80)
 *   Verlust = max(0, Soll − E),  Verlust[€] = Verlust · Vergütung
 *
 * Die Oberfläche bleibt währenddessen bedienbar; der Fortschritt wird
 * per Signal gemeldet.
 */
class FlottenAnalyse : public QObject {
    Q_OBJECT
public:
    FlottenAnalyse(Datenbasis* datenbasis, VerlaufRepository* verlaeufe, QObject* parent = nullptr);
    ~FlottenAnalyse() override;

    void starte();
    void abbrechen();
    bool laeuft() const { return m_watcher.isRunning(); }
    bool istFertig() const { return m_fertig; }

    /// Eine Anlage neu berechnen (z. B. nach Änderung der Stammdaten).
    void aktualisiere(const QString& anlageId);

    const Kennzahl* kennzahl(const QString& anlageId) const;
    const QHash<QString, Kennzahl>& kennzahlen() const { return m_kennzahlen; }
    double medianPrStation(const QString& stationId) const;
    double medianPrFlotte() const { return m_medianFlotte; }

signals:
    void fortschritt(int erledigt, int gesamt);
    void fertig();
    void kennzahlGeaendert(const QString& anlageId);

private:
    struct Auftrag {
        QString anlageId;
        QString stationId;
        QString pfad;          ///< Verlaufsdatei (nur Quelle CSV)
        bool    live = false;  ///< Verlauf vom Live-Generator erzeugen statt lesen
        Anlage  anlage;        ///< Stammdaten für den Live-Generator
        QDateTime ende;        ///< Simulationszeit, bei der der Live-Verlauf endet
        double  ppeakKwp = 0.0;
        double  acGrenzeKw = 0.0;
    };
    static Kennzahl analysiere(const Auftrag& auftrag);
    Auftrag auftragFuer(const QString& anlageId) const;
    void auswerten();
    void bewerte(Kennzahl& k) const;

    Datenbasis*        m_datenbasis;
    VerlaufRepository* m_verlaeufe;
    QFutureWatcher<Kennzahl> m_watcher;
    QHash<QString, Kennzahl> m_kennzahlen;
    QHash<QString, double>   m_medianStation;
    double m_medianFlotte = std::nan("");
    bool   m_fertig = false;
};
