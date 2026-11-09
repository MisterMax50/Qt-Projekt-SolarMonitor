#pragma once

#include <QColor>
#include <QFrame>

#include "util/stil.h"

class QLabel;

/**
 * @brief Kennzahlenkachel, wie sie Monitoring-Portale für PV-Anlagen zeigen.
 *
 * Farbiger Symbolkreis links, daneben Beschriftung, großer Wert und eine
 * Notiz in kleiner Schrift. Ist die Kachel klickbar, hebt sie sich beim
 * Überfahren hervor und meldet einen Klick über geklickt().
 */
class KennzahlKachel : public QFrame {
    Q_OBJECT
public:
    KennzahlKachel(Stil::Symbol symbol, const QColor& farbe, const QString& titel, QWidget* parent = nullptr);

    void setzeWert(const QString& wert, const QString& notiz = QString());
    /// Farbe des Symbolkreises, z. B. rot, sobald es kritische Störungen gibt.
    void setzeFarbe(const QColor& farbe);
    void setKlickbar(bool klickbar);

signals:
    void geklickt();

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void zeichneSymbol();

    Stil::Symbol m_symbol;
    QColor  m_farbe;
    bool    m_klickbar = false;
    QLabel* m_symbolLabel = nullptr;
    QLabel* m_wert = nullptr;
    QLabel* m_notiz = nullptr;
};
