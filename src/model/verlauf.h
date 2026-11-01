#pragma once

#include <cmath>
#include <limits>

#include <QDateTime>
#include <QString>
#include <QVector>

/**
 * @brief Messwertverlauf einer Anlage (Inhalt einer PXXXXX.json),
 *        14 Tage im 15-Minuten-Raster.
 *
 * Alle Arrays sind gleich lang und indexweise synchron. Fehlende
 * Messwerte (JSON null oder nicht numerisch) werden als NaN gespeichert,
 * damit die Indizes erhalten bleiben; @ref istLuecke prüft darauf.
 *
 * wr_status: ok | standby | stoerung | aus | keine_daten
 */
struct Verlauf {
    QString anlageId;
    int     intervallMin = 15;

    QVector<QDateTime> zeit;
    QVector<double>    leistungW;        ///< eingespeiste AC-Leistung in W
    QVector<double>    einstrahlungWm2;  ///< Bestrahlungsstärke in Modulebene in W/m²
    QVector<double>    modultemperaturC; ///< Modultemperatur in °C
    QVector<QString>   wrStatus;

    static constexpr double luecke() { return std::numeric_limits<double>::quiet_NaN(); }
    static bool istLuecke(double wert) { return std::isnan(wert); }

    int anzahl() const { return zeit.size(); }
    double intervallStunden() const { return intervallMin / 60.0; }

    /// Alle Arrays vorhanden und gleich lang?
    bool istKonsistent() const
    {
        const int n = zeit.size();
        return n > 0 && leistungW.size() == n && einstrahlungWm2.size() == n
            && modultemperaturC.size() == n && wrStatus.size() == n;
    }
};
