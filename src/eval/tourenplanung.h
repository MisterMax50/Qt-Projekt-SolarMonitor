#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

/// Ein anzufahrender Ort (Anlage oder Startpunkt).
struct TourStopp {
    QString id;
    double  lat = 0.0;
    double  lon = 0.0;
};

/// Ergebnis der Tourenplanung.
struct Tour {
    QVector<int>    reihenfolge; ///< Indizes in die übergebene Zielliste
    QVector<double> etappenKm;   ///< Etappe i endet bei reihenfolge[i]; letzte = Rückfahrt
    double          gesamtKm = 0.0;
    QVector<QPointF> verlauf;    ///< Linienzug (lat, lon) Start -> ... -> Start, um Seen herum
};

/**
 * @brief Einfache Tourenplanung für einen Servicetechniker (Erweiterung 7).
 *
 * 1. Konstruktion mit Nearest Neighbour (immer zum nächstgelegenen,
 *    noch nicht besuchten Ziel).
 * 2. Verbesserung mit 2-opt: zwei Kanten werden vertauscht, solange
 *    dadurch die Gesamtstrecke kürzer wird.
 *
 * Entfernungen sind Luftlinien, die um große Gewässer (Bodensee) herum
 * geführt werden (siehe Gewaesser::Umweg); Straßenentfernungen sind
 * offline nicht verfügbar.
 */
class Tourenplanung {
public:
    static Tour plane(const TourStopp& start, const QVector<TourStopp>& ziele, bool mitRueckfahrt = true);
};
