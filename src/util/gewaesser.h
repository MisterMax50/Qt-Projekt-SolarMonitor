#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

/**
 * @brief Große Gewässer (Bodensee) für Umkreis und Luftlinien-Tour.
 *
 * Eine reine Luftlinie würde z. B. von Meersburg nach Konstanz quer über
 * den See führen. Für die Serviceplanung zählt aber, wie weit ein
 * Techniker auf dem Landweg fahren muss. Deshalb wird hier der kürzeste
 * Weg um die Seen herum berechnet:
 *
 *  - Die Seeufer sind als Polygone hinterlegt (Natural Earth, gemeinfrei,
 *    fest einkompiliert -> immer offline verfügbar).
 *  - Ist die direkte Verbindung frei, gilt die Luftlinie.
 *  - Sonst führt der kürzeste Weg über Eckpunkte der Uferlinie
 *    (Sichtbarkeitsgraph + Dijkstra). Das ist die klassische Lösung für
 *    "kürzester Weg um polygonale Hindernisse".
 *  - Punkte, die wegen des groben Uferumrisses im See liegen (Anlagen
 *    direkt am Ufer), werden auf das nächste Ufer verschoben.
 *
 * Gerechnet wird in einer lokalen ebenen Projektion in km (Bezugsbreite
 * 47,65°); im Bodenseeraum ist der Fehler gegenüber der Großkreis-
 * entfernung kleiner als 0,5 %.
 *
 * Alle Punkte als QPointF(Breite, Länge) wie im restlichen Programm.
 */
namespace Gewaesser {

/// Ufer aller hinterlegten Seen als geschlossene Polygone (lat, lon).
const QVector<QVector<QPointF>>& ufer();

/// Liegt der Punkt in einem See?
bool imWasser(double lat, double lon);

/// Quellenangabe für die Uferlinien.
QString quelle();

/**
 * @brief Kürzeste Wege um die Seen herum von einem festen Startpunkt aus.
 *
 * Der Konstruktor rechnet einmal Dijkstra vom Start zu allen Ufer-
 * Eckpunkten; danach ist jede Abfrage billig. Damit lassen sich auch
 * Tausende Anlagen (Umkreisfilter) oder ein Raster (Kartenfläche) prüfen.
 */
class Umweg {
public:
    Umweg(double lat, double lon);

    /// Länge des kürzesten Wegs um die Seen in km.
    double km(double lat, double lon) const;
    /// Wegverlauf vom Start zum Ziel als (lat, lon) inklusive Endpunkte.
    QVector<QPointF> pfad(double lat, double lon) const;
    /// Wäre die Luftlinie über Wasser gegangen?
    bool umUfer(double lat, double lon) const;

private:
    double km(QPointF ziel, int* letzterKnoten) const;

    QPointF         m_startGrad; ///< (lat, lon) wie übergeben
    QPointF         m_start;   ///< in km (lokale Projektion), ggf. ans Ufer verschoben
    QVector<double> m_dist;    ///< Weglänge Start -> Eckpunkt i
    QVector<int>    m_vorg;    ///< Vorgänger im kürzesten Weg (-1 = Start)
    QVector<int>    m_sortiert;///< Eckpunkte aufsteigend nach m_dist
};

/// Bequemlichkeit für Einzelabfragen.
double wegKm(double lat1, double lon1, double lat2, double lon2);

} // namespace Gewaesser
