#pragma once

#include <QString>

/**
 * @brief Wetterstation (eine Zeile aus wetterstationen.csv).
 */
struct Wetterstation {
    QString stationId;  ///< z. B. "WS03"
    QString name;
    QString ort;
    double  lat = 0.0;
    double  lon = 0.0;
    double  hoeheM = 0.0;
};
