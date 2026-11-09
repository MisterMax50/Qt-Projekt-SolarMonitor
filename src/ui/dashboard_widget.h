#pragma once

#include <QList>
#include <QWidget>

#include "ui/app_kontext.h"

class QAction;
class QGroupBox;
class QLabel;
class QTreeWidget;

/**
 * @brief Startseite "Übersicht": der Zustand der Flotte auf einen Blick.
 *
 * Aufgebaut wie die Übersichtsseiten klassischer Verwaltungsprogramme
 * (Gruppenfelder mit Beschriftung, Listen mit Spaltenköpfen, Verweise als
 * Links):
 * - "Flottenstatus": wichtigste Kennzahlen als Beschriftung/Wert-Paare,
 *   jeder Wert ist ein Link zur passenden Seite
 * - Karte der Flotte, gefärbt nach Handlungsbedarf (KarteView, Modus Kompakt)
 * - Listen: offene kritische Störungen, größte Ertragsverluste, Fehlerbilder.
 *   Doppelklick öffnet die Anlage.
 *
 * Es wird nichts neu berechnet: Die Seite fasst Ergebnisse der
 * Flottenanalyse und der Datenbasis zusammen und aktualisiert sich bei
 * jeder Änderung (gebündelt, höchstens einmal pro Ereignisschleife).
 */
class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    /// @param schnellzugriff wird nicht mehr angezeigt (Befehle stehen in der Befehlsleiste)
    DashboardWidget(const AppKontext& kontext, const QList<QAction*>& schnellzugriff = {}, QWidget* parent = nullptr);

    /// Seiten, zu denen die Übersicht springen kann.
    enum class Ziel { Anlagen, Stoerungen, Karte, Statistik, Serviceplanung, Datenpruefung };

signals:
    void anlageOeffnen(const QString& anlageId);
    void seiteOeffnen(DashboardWidget::Ziel ziel);

private:
    QLabel* neuerWert(class QGridLayout* raster, int zeile, int spalte, const QString& titel);
    QGroupBox* neueGruppe(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung);
    QTreeWidget* neueListe(const QStringList& spalten);
    void verlinke(QLabel* label, const QString& text, Ziel ziel);
    void spaeterAktualisieren();
    void aktualisiere();

    AppKontext m_kontext;
    bool       m_geplant = false;

    QLabel* m_betrieb = nullptr;
    QLabel* m_stoerungen = nullptr;
    QLabel* m_pr = nullptr;
    QLabel* m_verlust = nullptr;
    QLabel* m_daten = nullptr;
    QLabel* m_ausgewertet = nullptr;
    QTreeWidget* m_dringend = nullptr;
    QTreeWidget* m_verluste = nullptr;
    QTreeWidget* m_fehlerbilder = nullptr;
};
