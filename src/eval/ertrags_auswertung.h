#pragma once

#include <cmath>

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QVector>

#include "model/verlauf.h"

/// Ertrag eines einzelnen Tages.
struct TagesErtrag {
    QDate  tag;
    double energieKwh = 0.0;        ///< Σ gültiger Leistungswerte
    double einstrahlungKwhM2 = 0.0; ///< Σ gültiger Einstrahlungswerte
    double pr = std::nan("");       ///< Tages-PR (nur Intervalle mit P und G)
    int    messpunkte = 0;
    int    luecken = 0;             ///< Intervalle mit fehlendem P oder G
};

/**
 * @brief Erkanntes Fehlerbild (Erweiterung 6).
 *
 * Gespeichert werden nur Typ und Kennwerte; der Text wird erst bei der
 * Anzeige über @ref text() erzeugt, damit er der aktiven Sprache folgt.
 */
struct Anomalie {
    Q_DECLARE_TR_FUNCTIONS(Anomalie)
public:
    enum class Typ {
        KeineEinspeisung, ///< im ganzen Zeitraum praktisch kein Ertrag
        Ausfall,          ///< hohe Einstrahlung, Leistung ~0
        WrStoerung,       ///< Wechselrichter meldet "stoerung"
        Clipping,         ///< Leistungsbegrenzung durch den Wechselrichter
        MorgenSchatten,
        AbendSchatten,
        Minderleistung,   ///< PR deutlich unter Stationsmedian bzw. absolut niedrig
        Datenluecke
    };

    Typ       typ = Typ::Ausfall;
    QDateTime von;
    QDateTime bis;
    int       anzahl = 0;      ///< Intervalle bzw. Tage (je nach Typ)
    double    wert = 0.0;      ///< z. B. Stunden, PR, Verhältnis
    double    vergleich = 0.0; ///< z. B. Stationsmedian, Grenzleistung

    QString text() const;
    static QString typName(Typ typ);

    /// Nur ein Hinweis (kein Fehler, der einen Serviceeinsatz rechtfertigt)?
    bool istHinweis() const { return typ == Typ::Clipping || typ == Typ::Datenluecke; }
};

/// Ergebnis der Auswertung einer Anlage.
struct Auswertung {
    bool    gueltig = false;
    QString grund;                    ///< warum nicht gültig

    double energieKwh = 0.0;          ///< E: Σ gültiger P · Δt
    double einstrahlungKwhM2 = 0.0;   ///< H: Σ gültiger G · Δt
    double spezErtragKwhKwp = 0.0;    ///< Y = E / Ppeak
    double pr = std::nan("");         ///< PR über Intervalle mit P und G
    double abdeckung = 0.0;           ///< Anteil Intervalle mit P und G
    int    messpunkte = 0;
    int    luecken = 0;

    QVector<TagesErtrag> tage;
    QVector<Anomalie>    anomalien;

    bool hat(Anomalie::Typ typ) const;
    /// Mindestens ein Fehlerbild, das kein reiner Hinweis ist?
    bool istAuffaellig() const;
};

/**
 * @brief Fachliche Auswertung (Pflichtpunkt 6, Erweiterung 6).
 *
 *   E [kWh]   = Δt/1000 · Σ P_i        (P_i in W,   Δt = 0,25 h)
 *   H [kWh/m²]= Δt/1000 · Σ G_i        (G_i in W/m²)
 *   Y         = E / Ppeak               [kWh/kWp]
 *   PR        = E' / (H' · Ppeak)       (G_STC = 1 kW/m²)
 *
 * Datenlücken: E und H summieren jeweils alle vorhandenen Werte. Für die
 * PR werden dagegen nur Intervalle verwendet, in denen P *und* G
 * vorliegen (E', H'); sonst würde eine Lücke in P die PR künstlich
 * senken bzw. eine Lücke in G sie erhöhen.
 */
class ErtragsAuswertung {
    Q_DECLARE_TR_FUNCTIONS(ErtragsAuswertung)
public:
    /**
     * @param ppeakKwp    installierte Leistung
     * @param acGrenzeKw  Nennleistung WR · Anzahl WR (≤ 0: unbekannt, kein Clipping-Test)
     */
    static Auswertung berechne(const Verlauf& verlauf, double ppeakKwp, double acGrenzeKw);

    /**
     * @brief Vergleich mit dem PR-Median der Anlagen derselben Wetterstation.
     * @param medianPr ≤ 0: keine Vergleichsgruppe, dann absoluter Grenzwert
     */
    static void bewerteGegenStation(Auswertung& a, double medianPr);

    static double median(QVector<double> werte); ///< NaN bei leerer Liste
};
