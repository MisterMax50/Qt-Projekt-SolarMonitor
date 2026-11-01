#pragma once

#include <QList>
#include <QWidget>

#include "ui/app_kontext.h"

class QAction;
class QLabel;
class QTableWidget;
class QVBoxLayout;

/**
 * @brief Startseite "Übersicht": der Zustand der Flotte auf einen Blick.
 *
 * - Kennzahl-Kacheln (Anlagen in Betrieb, Median-PR, offene Störungen,
 *   Ertragsverlust, Datenqualität). Ein Klick springt zur passenden Seite.
 * - Karte der Flotte, gefärbt nach Handlungsbedarf (KarteView im Modus Kompakt)
 * - "Jetzt kümmern": neueste offene kritische Störungen
 * - "Größte Verluste": Anlagen mit dem höchsten geschätzten Verlust in €
 * - "Erkannte Fehlerbilder": wie viele Anlagen je Fehlerbild betroffen sind
 * - Schnellzugriff auf häufige Aktionen (z. B. Flottenbericht als PDF)
 *
 * Es wird nichts neu berechnet: Die Seite fasst Ergebnisse der
 * Flottenanalyse und der Datenbasis zusammen und aktualisiert sich bei
 * jeder Änderung (gebündelt, höchstens einmal pro Ereignisschleife).
 */
class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    /// @param schnellzugriff Aktionen, die als Knöpfe angeboten werden
    DashboardWidget(const AppKontext& kontext, const QList<QAction*>& schnellzugriff, QWidget* parent = nullptr);

    /// Seiten, zu denen die Übersicht springen kann (Reihenfolge wie in der Navigation).
    enum class Ziel { Anlagen, Stoerungen, Karte, Statistik, Serviceplanung, Datenpruefung };

signals:
    void anlageOeffnen(const QString& anlageId);
    void seiteOeffnen(DashboardWidget::Ziel ziel);

private:
    struct Kachel {
        QLabel* wert = nullptr;
        QLabel* notiz = nullptr;
    };

    Kachel neueKachel(QWidget* eltern, QLayout* ziel, const QString& titel, Ziel sprung);
    QWidget* neueKarte(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung);
    QTableWidget* neueListe(int spalten);
    void spaeterAktualisieren();
    void aktualisiere();

    AppKontext m_kontext;
    bool       m_geplant = false;

    QLabel* m_stand = nullptr;
    Kachel  m_betrieb, m_pr, m_stoerungen, m_verlust, m_daten;
    QTableWidget* m_dringend = nullptr;
    QTableWidget* m_verluste = nullptr;
    QTableWidget* m_fehlerbilder = nullptr;
};
