#pragma once

#include <QHash>
#include <QWidget>

#include "model/live_generator.h"
#include "ui/app_kontext.h"

class KennzahlKachel;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

/**
 * @brief Seite "Live": simulierte Messwerte aller Anlagen im Takt.
 *
 * Oben vier Kennzahlenkacheln (Einspeisung, Auslastung, Einstrahlung,
 * Störungen), darunter eine Tabelle mit je einer Zeile pro Anlage. Der
 * LiveGenerator liefert die Werte; die Seite läuft nur, solange sie
 * sichtbar ist und "Starten" aktiv ist.
 */
class LiveSeite : public QWidget {
    Q_OBJECT
public:
    explicit LiveSeite(const AppKontext& kontext, QWidget* parent = nullptr);

signals:
    void anlageOeffnen(const QString& anlageId);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    /// Zellen einer Anlage, deren Inhalte sich im Takt ändern
    struct Zeile {
        double            kwp = 0.0;
        QTableWidgetItem* leistung = nullptr;
        QTableWidgetItem* auslastung = nullptr;
        QTableWidgetItem* einstrahlung = nullptr;
        QTableWidgetItem* status = nullptr;
        QTableWidgetItem* stand = nullptr;
    };

    void aktualisiereLaufzustand();
    void uebernehmen(const QVector<LiveMesswert>& werte, const QDateTime& zeit);

    LiveGenerator*     m_gen = nullptr;
    QPushButton*       m_knopf = nullptr;
    QComboBox*         m_tempo = nullptr;
    QLabel*            m_zeit = nullptr;
    KennzahlKachel*    m_einspeisung = nullptr;
    KennzahlKachel*    m_auslastung = nullptr;
    KennzahlKachel*    m_einstrahlung = nullptr;
    KennzahlKachel*    m_stoerungen = nullptr;
    QTableWidget*      m_tabelle = nullptr;
    QHash<QString, Zeile> m_zeilen;
};
