#pragma once

#include <QList>
#include <QWidget>

#include "ui/app_kontext.h"

class QAction;
class QGroupBox;
class QLabel;
class QTreeWidget;
class KennzahlKachel;

/**
 * @brief Startseite "Übersicht": der Zustand der Flotte auf einen Blick.
 *
 * Aufgebaut wie die Startseite eines Monitoring-Portals für Solarparks:
 * - oben fünf Kennzahlenkacheln (Betrieb, Störungen, PR, Verlust,
 *   Datenqualität); ein Klick öffnet die passende Seite
 * - Mitte: Karte der Flotte, gefärbt nach Handlungsbedarf (KarteView,
 *   Modus Kompakt), daneben die aktiven kritischen Alarme
 * - unten: größte Ertragsverluste und erkannte Fehlerbilder.
 *   Doppelklick öffnet die Anlage.
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
    enum class Ziel { Anlagen, Stoerungen, Karte, Statistik, Serviceplanung, Datenpruefung };

signals:
    void anlageOeffnen(const QString& anlageId);
    void seiteOeffnen(DashboardWidget::Ziel ziel);

private:
    QGroupBox* neueGruppe(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung);
    QTreeWidget* neueListe(const QStringList& spalten);
    void spaeterAktualisieren();
    void aktualisiere();

    AppKontext m_kontext;
    bool       m_geplant = false;

    KennzahlKachel* m_betrieb = nullptr;
    KennzahlKachel* m_stoerungen = nullptr;
    KennzahlKachel* m_pr = nullptr;
    KennzahlKachel* m_verlust = nullptr;
    KennzahlKachel* m_daten = nullptr;
    QTreeWidget* m_dringend = nullptr;
    QTreeWidget* m_verluste = nullptr;
    QTreeWidget* m_fehlerbilder = nullptr;
};
