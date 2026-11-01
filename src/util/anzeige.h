#pragma once

#include <QColor>
#include <QCoreApplication>
#include <QString>

/**
 * @brief Zentrale Anzeige-Hilfen: übersetzt Rohwerte (Status, Typ,
 *        Schwere …) in lesbare Namen und formatiert Zahlen einheitlich.
 *
 * Die Namen werden bei jedem Aufruf über tr() erzeugt (keine statischen
 * Tabellen), damit ein Sprachwechsel zur Laufzeit sofort wirkt.
 * Zahlen werden in der Locale der aktiven Sprache formatiert
 * (Deutsch: Dezimalkomma, Englisch: Dezimalpunkt).
 */
class Anzeige {
    Q_DECLARE_TR_FUNCTIONS(Anzeige)
public:
    static QString typName(const QString& roh);
    static QString statusName(const QString& roh);
    static QString stoerungsStatusName(const QString& roh);
    static QString schwereName(const QString& roh);
    static QString wrStatusName(const QString& roh);

    static QColor statusFarbe(const QString& status);
    static QColor schwereFarbe(const QString& schwere);
    /// Farbe auf einer Skala rot (PR ≤ 0,5) – gelb – grün (PR ≥ 0,85).
    static QColor prFarbe(double pr);

    /// Sprache für Zahlenformate setzen ("de" | "en").
    static void setSprache(const QString& sprache);

    static QString zahl(double wert, int nachkommastellen = 1);
    static QString prozent(double anteil, int nachkommastellen = 1); ///< 0,824 -> "82,4 %"
    static QString pr(double pr);       ///< PR als Prozent; ungültig: "–"
    static QString kwh(double kwh);     ///< kWh / MWh automatisch
    static QString leistungW(double w); ///< W / kW / MW automatisch
    static QString euro(double euro);
    static QString datum(const QDate& d);
    static QString zeitpunkt(const QDateTime& dt);
};
