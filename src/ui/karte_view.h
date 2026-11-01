#pragma once

#include <QPointF>
#include <QVector>
#include <QWidget>

#include "ui/app_kontext.h"

class QComboBox;
class QCheckBox;
class QLabel;
class KartenFlaeche;
class KachelLader;

/**
 * @brief Schematische Karte der Anlagen und Wetterstationen (Erweiterung 8).
 *
 * - Abbildung lat/lon -> Bildkoordinaten mit Web-Mercator (wie OSM), damit
 *   Punkte exakt auf den Kartenkacheln liegen; der Ausschnitt passt sich
 *   automatisch an die Daten an.
 * - Hintergrund: OpenStreetMap-Kacheln (KachelLader), dauerhaft im Ordner
 *   data/kartenkacheln gespeichert -> nach dem ersten Laden offline nutzbar.
 *   Ohne Hintergrund wird ein Gradnetz gezeichnet.
 * - Färbung wahlweise nach Status, Performance Ratio oder Auffälligkeit,
 *   Punktgröße nach installierter Leistung (logarithmisch).
 * - Mausrad zoomt, Ziehen verschiebt, Klick öffnet die Anlage.
 * - Eine geplante Servicetour wird als Linienzug eingezeichnet, dazu der
 *   Umkreis als auf dem Landweg (um den Bodensee herum) erreichbare Fläche.
 */
class KarteView : public QWidget {
    Q_OBJECT
public:
    /// Voll: mit Bedienleiste (Seite "Karte"). Kompakt: nur die Karte, gefärbt nach
    /// Handlungsbedarf, ohne Wetterstationen (für die Übersicht).
    enum class Modus { Voll, Kompakt };

    explicit KarteView(const AppKontext& kontext, QWidget* parent = nullptr, Modus modus = Modus::Voll);

    void aktualisiere();
    void markiere(const QString& anlageId);
    /**
     * @brief Tour einzeichnen: Startpunkt + Anlagen in Besuchsreihenfolge.
     * @param verlauf       Linienzug (lat, lon): Straßenverlauf oder Luftlinie um den See; leer = gerade Linien
     * @param strassenroute true: verlauf ist eine Straßenroute (durchgezogen), sonst gestrichelt
     * @param umkreisKm     Suchradius um den Start (≤ 0: keiner); gezeichnet wird die auf dem
     *                      Landweg um den See herum erreichbare Fläche
     */
    void setTour(const QPointF& startLatLon, const QStringList& anlagenIds, const QVector<QPointF>& verlauf = {},
                 bool strassenroute = false, double umkreisKm = 0.0);

signals:
    void anlageGewaehlt(const QString& anlageId);

private:
    AppKontext     m_kontext;
    KachelLader*   m_lader = nullptr;
    KartenFlaeche* m_flaeche = nullptr;
    QComboBox*     m_faerbung = nullptr;
    QCheckBox*     m_stationen = nullptr;
    QLabel*        m_info = nullptr;
};
