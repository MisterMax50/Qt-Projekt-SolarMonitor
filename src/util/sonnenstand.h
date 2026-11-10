#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>
#include <QTimeZone>

/**
 * @brief Sonnenstand ohne Internet berechnen.
 *
 * Vereinfachter Algorithmus nach dem Astronomical Almanac (wie ihn auch der
 * NOAA-Sonnenrechner verwendet): aus Julianischem Datum die ekliptikale
 * Länge der Sonne, daraus Deklination und Rektaszension, mit der
 * Sternzeit der Stundenwinkel und schließlich Höhe und Azimut am Ort.
 * Genauigkeit etwa 0,1°, für die Anzeige völlig ausreichend.
 */
namespace Sonnenstand {

struct Position {
    double hoeheGrad = 0.0;  ///< Höhe über dem Horizont (negativ = unter dem Horizont)
    double azimutGrad = 0.0; ///< 0 = Nord, 90 = Ost, 180 = Süd, 270 = West
};

struct Tag {
    QDateTime aufgang;       ///< ungültig, wenn die Sonne nicht aufgeht (Polarnacht)
    QDateTime untergang;
    QDateTime mittag;        ///< höchster Stand
    double    mittagsHoeheGrad = 0.0;
    int       tageslichtMinuten = 0;
};

/// Sonnenstand zum Zeitpunkt @p zeit (beliebige Zeitzone) am Ort lat/lon.
Position berechne(const QDateTime& zeit, double latGrad, double lonGrad);

/// Auf- und Untergang (Sonnenmittelpunkt −0,833°, mit Refraktion) und Mittag
/// am Kalendertag @p datum in der Zeitzone von @p zone.
Tag tag(const QDate& datum, double latGrad, double lonGrad, const QTimeZone& zone);

/// Himmelsrichtung als Kürzel ("S", "SSW" …).
QString richtung(double azimutGrad);

/// Zeitzone der Anlagen (Mitteleuropa); ersatzweise die des Rechners.
QTimeZone zone();

} // namespace Sonnenstand
