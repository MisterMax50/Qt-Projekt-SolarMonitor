#pragma once

#include <QColor>
#include <QDateTime>
#include <QVector>
#include <QWidget>

/**
 * @brief Zeitreihen-Diagramm auf Basis von QPainter (Pflichtpunkt 5).
 *
 * - Datenlücken (NaN) unterbrechen die Linie und werden als hellroter
 *   Hintergrundstreifen markiert, statt sie mit 0 zu verbinden.
 * - Die Einheit "W" wird automatisch auf kW bzw. MW skaliert.
 * - Tagesgrenzen werden als Gitterlinien mit Datum beschriftet.
 * - Mit der Maus lässt sich jeder Messpunkt ablesen (Fadenkreuz + Tooltip).
 *
 * Bewusst ohne Qt Charts, damit das Projekt mit jeder Qt-Installation baut.
 */
class VerlaufPlot : public QWidget {
    Q_OBJECT
public:
    explicit VerlaufPlot(QWidget* parent = nullptr);

    /**
     * @param einheit   Basiseinheit, z. B. "W", "W/m²", "°C"
     * @param skalieren bei "W": automatisch kW/MW verwenden
     */
    void setDaten(const QVector<QDateTime>& zeit, const QVector<double>& werte,
                  const QString& titel, const QString& einheit, const QColor& farbe,
                  bool skalieren = false);
    void leeren(const QString& hinweis);

    QSize sizeHint() const override { return {600, 200}; }
    QSize minimumSizeHint() const override { return {240, 140}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plotBereich() const;
    double xFuer(int index, const QRectF& r) const;

    QVector<QDateTime> m_zeit;
    QVector<double>    m_werte;   ///< bereits skaliert
    QString m_titel;
    QString m_einheit;            ///< angezeigte Einheit (nach Skalierung)
    QString m_hinweis;
    QColor  m_farbe;
    double  m_yMin = 0.0;
    double  m_yMax = 1.0;
    int     m_hoverIndex = -1;
};
