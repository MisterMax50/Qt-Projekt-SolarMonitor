#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

/**
 * @brief Einfaches Balkendiagramm (QPainter), z. B. für Tageserträge
 *        oder die Altersverteilung der Anlagen.
 *
 * Jeder Balken kann eine eigene Farbe und einen Tooltip haben; optional
 * wird eine waagerechte Referenzlinie (z. B. Mittelwert) gezeichnet.
 */
class BalkenDiagramm : public QWidget {
    Q_OBJECT
public:
    struct Balken {
        QString beschriftung;
        double  wert = 0.0;
        QColor  farbe;
        QString tooltip;
    };

    explicit BalkenDiagramm(QWidget* parent = nullptr);

    void setDaten(const QVector<Balken>& balken, const QString& titel, const QString& einheit,
                  int nachkommastellen = 1);
    void setReferenz(double wert, const QString& text);

    QSize sizeHint() const override { return {600, 220}; }
    QSize minimumSizeHint() const override { return {240, 140}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QRectF balkenRechteck(int index, const QRectF& bereich, double maxWert) const;
    QRectF plotBereich() const;
    double maxWert() const;

    QVector<Balken> m_balken;
    QString m_titel;
    QString m_einheit;
    int     m_nachkomma = 1;
    double  m_referenz = -1.0;
    QString m_referenzText;
};
