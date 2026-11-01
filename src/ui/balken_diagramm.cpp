#include "balken_diagramm.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include "util/anzeige.h"

namespace {
constexpr double kRandLinks = 56.0;
constexpr double kRandRechts = 12.0;
constexpr double kRandOben = 26.0;
constexpr double kRandUnten = 30.0;
} // namespace

BalkenDiagramm::BalkenDiagramm(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

void BalkenDiagramm::setDaten(const QVector<Balken>& balken, const QString& titel,
                              const QString& einheit, int nachkommastellen)
{
    m_balken = balken;
    m_titel = titel;
    m_einheit = einheit;
    m_nachkomma = nachkommastellen;
    m_referenz = -1.0;
    update();
}

void BalkenDiagramm::setReferenz(double wert, const QString& text)
{
    m_referenz = wert;
    m_referenzText = text;
    update();
}

QRectF BalkenDiagramm::plotBereich() const
{
    return QRectF(kRandLinks, kRandOben, width() - kRandLinks - kRandRechts,
                  height() - kRandOben - kRandUnten);
}

double BalkenDiagramm::maxWert() const
{
    double m = qMax(0.0, m_referenz);
    for (const Balken& b : m_balken)
        m = qMax(m, b.wert);
    return m > 0.0 ? m * 1.1 : 1.0;
}

QRectF BalkenDiagramm::balkenRechteck(int index, const QRectF& r, double maxW) const
{
    const double breite = r.width() / qMax(1, int(m_balken.size()));
    const double h = qMax(0.0, m_balken.at(index).wert) / maxW * r.height();
    return QRectF(r.left() + index * breite + breite * 0.15, r.bottom() - h, breite * 0.7, h);
}

void BalkenDiagramm::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().base());
    const QColor textFarbe = palette().color(QPalette::Text);

    QFont fett = font();
    fett.setBold(true);
    p.setFont(fett);
    p.setPen(textFarbe);
    p.drawText(QRectF(8, 2, width() - 16, kRandOben - 4), Qt::AlignLeft | Qt::AlignVCenter,
               m_einheit.isEmpty() ? m_titel : QStringLiteral("%1 [%2]").arg(m_titel, m_einheit));
    p.setFont(font());

    const QRectF r = plotBereich();
    if (r.width() < 40 || r.height() < 30)
        return;
    if (m_balken.isEmpty()) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(r, Qt::AlignCenter, tr("Keine Daten."));
        return;
    }

    const double maxW = maxWert();
    for (int i = 0; i <= 4; ++i) {
        const double y = r.bottom() - r.height() * i / 4.0;
        p.setPen(palette().color(QPalette::Mid).lighter(130));
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.setPen(textFarbe);
        p.drawText(QRectF(0, y - 9, kRandLinks - 6, 18), Qt::AlignRight | Qt::AlignVCenter,
                   Anzeige::zahl(maxW * i / 4.0, maxW < 10 ? 1 : 0));
    }

    const double breite = r.width() / m_balken.size();
    const int jedeNte = breite < 30 ? 2 : 1;
    for (int i = 0; i < m_balken.size(); ++i) {
        const Balken& b = m_balken.at(i);
        p.setPen(Qt::NoPen);
        p.setBrush(b.farbe.isValid() ? b.farbe : palette().color(QPalette::Highlight));
        p.drawRect(balkenRechteck(i, r, maxW));
        if (i % jedeNte == 0) {
            p.setPen(textFarbe);
            p.drawText(QRectF(r.left() + i * breite - 10, r.bottom() + 4, breite + 20, 16),
                       Qt::AlignHCenter | Qt::AlignTop, b.beschriftung);
        }
    }

    if (m_referenz > 0.0) {
        const double y = r.bottom() - m_referenz / maxW * r.height();
        p.setPen(QPen(textFarbe, 1, Qt::DashLine));
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.drawText(QRectF(r.right() - 260, y - 18, 256, 16), Qt::AlignRight | Qt::AlignVCenter, m_referenzText);
    }

    p.setPen(palette().color(QPalette::Mid));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);
}

void BalkenDiagramm::mouseMoveEvent(QMouseEvent* event)
{
    const QRectF r = plotBereich();
    if (m_balken.isEmpty() || !r.contains(event->position())) {
        QToolTip::hideText();
        return;
    }
    const int i = qBound(0, int((event->position().x() - r.left()) / (r.width() / m_balken.size())),
                         int(m_balken.size()) - 1);
    const Balken& b = m_balken.at(i);
    const QString text = b.tooltip.isEmpty()
        ? b.beschriftung + QStringLiteral(": ") + Anzeige::zahl(b.wert, m_nachkomma) + QLatin1Char(' ') + m_einheit
        : b.tooltip;
    QToolTip::showText(event->globalPosition().toPoint(), text, this);
}
