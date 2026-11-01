#pragma once

#include <QDateTime>
#include <QString>

/**
 * @brief Störungsmeldung (eine Zeile aus stoerungen.csv).
 *
 * schwere: info | warnung | fehler | kritisch
 * status:  offen | in_bearbeitung | behoben
 *
 * behoben_am steht in der CSV als Zeitstempel (JJJJ-MM-TT hh:mm:ss);
 * ein ungültiger QDateTime bedeutet "noch nicht behoben".
 */
struct Stoerung {
    QString   stoerungId;   ///< z. B. "ST01734"
    QString   anlageId;
    QDateTime zeitpunkt;
    QString   fehlercode;   ///< z. B. "E601"
    QString   beschreibung;
    QString   schwere;
    QString   status;
    QDateTime behobenAm;

    bool istOffen() const
    {
        return status == QLatin1String("offen") || status == QLatin1String("in_bearbeitung");
    }
    bool istKritisch() const { return schwere == QLatin1String("kritisch"); }
};
