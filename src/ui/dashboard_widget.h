#pragma once

#include <QList>
#include <QWidget>

#include "ui/app_kontext.h"

class QAction;
class QGroupBox;
class QLabel;
class QTreeWidget;
class KennzahlKachel;
class AnordnungsRaster;

/**
 * @brief Startseite "Übersicht": der Zustand der Flotte auf einen Blick.
 *
 * Aufgebaut wie die Startseite eines Monitoring-Portals für Solarparks:
 * - oben sechs Kennzahlenkacheln (Betrieb, Störungen, PR, Verlust,
 *   Datenqualität, vermiedenes CO₂); ein Klick öffnet die passende Seite
 * - darunter die Favoriten (angepinnte Anlagen des Benutzers)
 * - Mitte: Karte der Flotte, gefärbt nach Handlungsbedarf (KarteView,
 *   Modus Kompakt), daneben die aktiven kritischen Alarme
 * - unten: größte Ertragsverluste und erkannte Fehlerbilder.
 *   Doppelklick öffnet die Anlage.
 *
 * Kacheln und Karten lassen sich per Ziehen auf feste Plätze verschieben
 * (AnordnungsRaster); die Anordnung wird gespeichert.
 *
 * Es wird nichts neu berechnet: Die Seite fasst Ergebnisse der
 * Flottenanalyse und der Datenbasis zusammen und aktualisiert sich bei
 * jeder Änderung (gebündelt, höchstens einmal pro Ereignisschleife).
 */
class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    /// @param schnellzugriff wird nicht angezeigt (Befehle stehen in der Kopfzeile der Seite)
    DashboardWidget(const AppKontext& kontext, const QList<QAction*>& schnellzugriff = {}, QWidget* parent = nullptr);

    /// Seiten, zu denen die Übersicht springen kann.
    enum class Ziel { Anlagen, Stoerungen, Karte, Statistik, Serviceplanung, Datenpruefung, Favoriten };

    /// Ohne Ultimate (Spaß-Abo) eine Ultimate-Anordnung auf "Standard" zurücksetzen.
    void pruefeAboAnordnung();
    /// Kacheln und Karten wieder auf ihre vorgegebenen Plätze setzen.
    void anordnungZuruecksetzen();
    /// Aktion mit Menü der vordefinierten Anordnungen (für die Aktionszeile der Seite).
    QAction* anordnungsAktion();

signals:
    void anlageOeffnen(const QString& anlageId);
    void seiteOeffnen(DashboardWidget::Ziel ziel);

private:
    QGroupBox* neueGruppe(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung);
    QTreeWidget* neueListe(const QStringList& spalten);
    void spaeterAktualisieren();
    void aktualisiere();
    void aktualisiereFavoriten();

    AppKontext m_kontext;
    bool       m_geplant = false;

    AnordnungsRaster* m_kachelRaster = nullptr;
    AnordnungsRaster* m_kartenRaster = nullptr;
    KennzahlKachel* m_betrieb = nullptr;
    KennzahlKachel* m_stoerungen = nullptr;
    KennzahlKachel* m_pr = nullptr;
    KennzahlKachel* m_verlust = nullptr;
    KennzahlKachel* m_daten = nullptr;
    KennzahlKachel* m_co2 = nullptr;
    QWidget*     m_favoritenInhalt = nullptr; ///< Leiste der angepinnten Anlagen
    QTreeWidget* m_dringend = nullptr;
    QTreeWidget* m_verluste = nullptr;
    QTreeWidget* m_fehlerbilder = nullptr;
};
