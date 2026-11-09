#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QAction;
class QBoxLayout;
class QHBoxLayout;
class QMenu;
class QStackedWidget;
class QTabBar;
class QToolButton;

/**
 * @brief Menüband ("Ribbon") nach dem Vorbild von Microsoft Office.
 *
 * Aufbau:
 *  - links ein farbiger Knopf "Datei" mit Menü (Benutzer, Export, Beenden …)
 *  - daneben die Reiter (Start, Anlage, Auswertung, Ansicht)
 *  - rechts ein frei belegbarer Bereich, z. B. das Suchfeld
 *  - darunter je Reiter eine Fläche mit Gruppen. Jede Gruppe enthält große
 *    Knöpfe (Symbol über Text) oder Stapel aus bis zu drei kleinen Knöpfen
 *    und trägt ihren Namen unten.
 *
 * Die Knöpfe zeigen QActions an; Aktivierung, Häkchen und Tooltips kommen
 * aus der Aktion. Auf dem Band kann ein kürzerer Text stehen als im Menü
 * (z. B. "Stammdaten\nbearbeiten" statt "Stammdaten bearbeiten …").
 *
 * Doppelklick auf einen Reiter klappt das Band ein bzw. wieder aus.
 */
class Menueband : public QWidget {
    Q_OBJECT
public:
    explicit Menueband(QWidget* parent = nullptr);

    /// Menü hinter dem Knopf "Datei".
    QMenu* dateiMenue() const { return m_dateiMenue; }

    /// Neuen Reiter anlegen; folgende Gruppen landen darin.
    void neuerReiter(const QString& titel);
    /// Neue Gruppe im aktuellen Reiter beginnen.
    void neueGruppe(const QString& titel);
    /// Großer Knopf (Symbol 32 px, Text darunter).
    QToolButton* gross(QAction* aktion, const QString& kurzText = {});
    /// Großer Knopf mit Klappmenü.
    QToolButton* grossMitMenue(const QIcon& symbol, const QString& text, QMenu* menue);
    /// Stapel kleiner Knöpfe (Symbol 16 px, Text daneben), höchstens drei.
    void klein(const QList<QAction*>& aktionen);
    /// Beliebiges Widget in die aktuelle Gruppe setzen (z. B. Auswahlliste).
    void widget(QWidget* w);
    /// Widget rechts neben den Reitern (z. B. Suchfeld).
    void setzeRechts(QWidget* w);

    void zeigeReiter(int index);
    bool istEingeklappt() const;

private:
    void klappen();

    QMenu*          m_dateiMenue = nullptr;
    QToolButton*    m_datei = nullptr;
    QTabBar*        m_reiter = nullptr;
    QStackedWidget* m_flaechen = nullptr;
    QHBoxLayout*    m_kopf = nullptr;
    QHBoxLayout*    m_aktuelleFlaeche = nullptr;
    QHBoxLayout*    m_aktuelleGruppe = nullptr;
};
