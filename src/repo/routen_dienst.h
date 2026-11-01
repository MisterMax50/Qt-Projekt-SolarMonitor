#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "eval/tourenplanung.h"

class QNetworkAccessManager;

/// Ergebnis einer Straßenroute.
struct StrassenRoute {
    bool    ok = false;
    QString fehler;
    bool    ausCache = false;
    QVector<int>     reihenfolge;   ///< Indizes in die Zielliste (ohne Start)
    QVector<double>  etappenKm;     ///< Etappe i endet bei reihenfolge[i]; letzte = Rückfahrt
    QVector<double>  etappenMin;
    double           gesamtKm = 0.0;
    double           gesamtMin = 0.0;
    QVector<QPointF> verlauf;       ///< Straßenverlauf als (lat, lon)
};

/**
 * @brief Optimierte Autoroute über den OSRM-Routingdienst (OpenStreetMap).
 *
 * Nutzt den "trip"-Dienst von OSRM: Er löst das Rundreiseproblem auf dem
 * echten Straßennetz (Fahrzeit als Kriterium), Start und Ziel ist der
 * Startpunkt. Autofähren werden ausgeschlossen (exclude=ferry), damit die
 * Route nicht über den Bodensee führt. Zurück kommen Besuchsreihenfolge, Etappenlängen, Fahrzeiten
 * und der Straßenverlauf zum Einzeichnen in die Karte.
 *
 * Jede Antwort wird im Ordner data/routen_cache gespeichert (Dateiname =
 * Hash der Anfrage). Dieselbe Tour ist danach auch offline verfügbar.
 * Ohne Internet und ohne Cache meldet das Ergebnis einen Fehler; die
 * Serviceplanung zeigt dann die Luftlinien-Tour.
 */
class RoutenDienst : public QObject {
    Q_OBJECT
public:
    RoutenDienst(const QString& cacheOrdner, QObject* parent = nullptr);

    /// Asynchron berechnen; Ergebnis über @ref routeBerechnet.
    void berechne(const TourStopp& start, const QVector<TourStopp>& ziele);

    /// Antwort von OSRM auswerten (öffentlich für Tests).
    static StrassenRoute auswerten(const QByteArray& json, int anzahlZiele);

signals:
    void routeBerechnet(const StrassenRoute& route);

private:
    QString cachePfad(const QString& url) const;
    /// GET an OSRM; bei HTTP 400 (Option unbekannt) einmal mit @p ersatzUrl wiederholen.
    void frage(const QString& url, const QString& ersatzUrl, int nr, int anzahl);

    QString                m_cacheOrdner;
    QNetworkAccessManager* m_netz = nullptr;
    int                    m_anfrageNr = 0; ///< veraltete Antworten verwerfen
};
