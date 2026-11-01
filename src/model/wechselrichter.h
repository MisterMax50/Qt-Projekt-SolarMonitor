#pragma once

#include <QString>

/**
 * @brief Wechselrichter-Modell (eine Zeile aus wechselrichter.csv).
 *
 * anlagen.csv referenziert über wechselrichter_id ein Modell; mehrere
 * Anlagen teilen sich denselben Typ. Die tatsächliche AC-Grenzleistung
 * einer Anlage ist nennleistungKw · anzahl_wr.
 */
struct Wechselrichter {
    QString wechselrichterId;           ///< z. B. "W05"
    QString hersteller;
    QString modell;
    double  nennleistungKw = 0.0;
    double  maxWirkungsgradProzent = 0.0;
    int     phasen = 3;
    int     mppTracker = 1;

    QString bezeichnung() const { return hersteller + QLatin1Char(' ') + modell; }
};
