#include "verlauf_plot.h"

#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include "util/anzeige.h"

namespace {
constexpr double kRandLinks = 64.0;
constexpr double kRandRechts = 12.0;
constexpr double kRandOben = 26.0;
constexpr double kRandUnten = 26.0;

/// "Schöne" Schrittweite für Achsenbeschriftungen (1, 2, 5 · 10^n).
double schoenerSchritt(double spanne, int zielAnzahl)
{
    if (spanne <= 0.0)
        return 1.0;
    const double roh = spanne / zielAnzahl;
    const double basis = std::pow(10.0, std::floor(std::log10(roh)));
    for (double f : {1.0, 2.0, 5.0, 10.0})
        if (f * basis >= roh)
            return f * basis;
    return 10.0 * basis;
}
} // namespace

VerlaufPlot::VerlaufPlot(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void VerlaufPlot::leeren(const QString& hinweis)
{
    m_zeit.clear();
    m_werte.clear();
    m_hinweis = hinweis;
    m_hoverIndex = -1;
    update();
}

void VerlaufPlot::setDaten(const QVector<QDateTime>& zeit, const QVector<double>& werte,
                           const QString& titel, const QString& einheit, const QColor& farbe,
                           bool skalieren)
{
    m_zeit = zeit;
    m_titel = titel;
    m_farbe = farbe;
    m_hinweis.clear();
    m_hoverIndex = -1;

    double maxAbs = 0.0;
    double yMin = 0.0, yMax = 0.0;
    bool erster = true;
    for (double w : werte) {
        if (std::isnan(w))
            continue;
        maxAbs = qMax(maxAbs, std::abs(w));
        yMin = erster ? w : qMin(yMin, w);
        yMax = erster ? w : qMax(yMax, w);
        erster = false;
    }

    double faktor = 1.0;
    m_einheit = einheit;
    if (skalieren && einheit == QLatin1String("W")) {
        if (maxAbs >= 1.0e6) {
            faktor = 1.0e-6;
            m_einheit = QStringLiteral("MW");
        } else if (maxAbs >= 1.0e3) {
            faktor = 1.0e-3;
            m_einheit = QStringLiteral("kW");
        }
    }
    m_werte.resize(werte.size());
    for (int i = 0; i < werte.size(); ++i)
        m_werte[i] = werte.at(i) * faktor;

    if (erster) {
        m_hinweis = tr("Alle Messwerte fehlen.");
        m_yMin = 0.0;
        m_yMax = 1.0;
    } else {
        m_yMin = qMin(0.0, yMin * faktor);
        m_yMax = yMax * faktor;
        if (m_yMax - m_yMin < 1e-9)
            m_yMax = m_yMin + 1.0;
        const double schritt = schoenerSchritt(m_yMax - m_yMin, 5);
        m_yMax = std::ceil(m_yMax / schritt) * schritt;
        m_yMin = std::floor(m_yMin / schritt) * schritt;
    }
    update();
}

QRectF VerlaufPlot::plotBereich() const
{
    return QRectF(kRandLinks, kRandOben, width() - kRandLinks - kRandRechts,
                  height() - kRandOben - kRandUnten);
}

double VerlaufPlot::xFuer(int index, const QRectF& r) const
{
    const qint64 t0 = m_zeit.first().toMSecsSinceEpoch();
    const qint64 t1 = m_zeit.last().toMSecsSinceEpoch();
    const double spanne = t1 > t0 ? double(t1 - t0) : 1.0;
    return r.left() + double(m_zeit.at(index).toMSecsSinceEpoch() - t0) / spanne * r.width();
}

void VerlaufPlot::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().base());

    const QColor textFarbe = palette().color(QPalette::Text);
    const QColor gitter = palette().color(QPalette::Mid).lighter(130);

    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(textFarbe);
    p.drawText(QRectF(8, 2, width() - 16, kRandOben - 4), Qt::AlignLeft | Qt::AlignVCenter,
               m_einheit.isEmpty() ? m_titel : QStringLiteral("%1 [%2]").arg(m_titel, m_einheit));
    p.setFont(font());

    const QRectF r = plotBereich();
    if (r.width() < 40 || r.height() < 30)
        return;

    const int n = qMin(m_zeit.size(), m_werte.size());
    if (n < 2 || !m_hinweis.isEmpty()) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap,
                   m_hinweis.isEmpty() ? tr("Keine Messdaten vorhanden.") : m_hinweis);
        return;
    }

    auto yFuer = [&](double w) { return r.bottom() - (w - m_yMin) / (m_yMax - m_yMin) * r.height(); };

    // Datenlücken als Streifen
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 60, 60, 40));
    for (int i = 0; i < n; ++i) {
        if (!std::isnan(m_werte.at(i)))
            continue;
        int j = i;
        while (j + 1 < n && std::isnan(m_werte.at(j + 1)))
            ++j;
        const double x0 = xFuer(i, r);
        const double x1 = (j + 1 < n) ? xFuer(j + 1, r) : r.right();
        p.drawRect(QRectF(x0, r.top(), qMax(1.5, x1 - x0), r.height()));
        i = j;
    }

    // y-Gitter
    const double schritt = schoenerSchritt(m_yMax - m_yMin, 5);
    for (double w = m_yMin; w <= m_yMax + schritt * 0.01; w += schritt) {
        const double y = yFuer(w);
        p.setPen(gitter);
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.setPen(textFarbe);
        const int nk = schritt < 1.0 ? (schritt < 0.1 ? 2 : 1) : 0;
        p.drawText(QRectF(0, y - 9, kRandLinks - 6, 18), Qt::AlignRight | Qt::AlignVCenter,
                   Anzeige::zahl(w, nk));
    }

    // Tagesgrenzen
    const double proTag = r.width() / qMax(1.0, m_zeit.first().daysTo(m_zeit.last()) + 1.0);
    const int jedenNtenTag = proTag < 40 ? 2 : 1;
    int tagNr = 0;
    for (int i = 0; i < n; ++i) {
        if (i > 0 && m_zeit.at(i).date() == m_zeit.at(i - 1).date())
            continue;
        const double x = xFuer(i, r);
        p.setPen(gitter);
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        if (tagNr++ % jedenNtenTag == 0) {
            p.setPen(textFarbe);
            p.drawText(QRectF(x + 2, r.bottom() + 4, 60, 16), Qt::AlignLeft | Qt::AlignTop,
                       m_zeit.at(i).toString(QStringLiteral("dd.MM.")));
        }
    }

    // Messreihe
    QPainterPath pfad;
    bool offen = false;
    for (int i = 0; i < n; ++i) {
        const double w = m_werte.at(i);
        if (std::isnan(w)) {
            offen = false;
            continue;
        }
        const QPointF pt(xFuer(i, r), yFuer(w));
        if (offen) {
            pfad.lineTo(pt);
        } else {
            pfad.moveTo(pt);
            offen = true;
        }
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(m_farbe, 1.4));
    p.drawPath(pfad);

    p.setPen(palette().color(QPalette::Mid));
    p.drawRect(r);

    // Fadenkreuz
    if (m_hoverIndex >= 0 && m_hoverIndex < n) {
        const double x = xFuer(m_hoverIndex, r);
        p.setPen(QPen(textFarbe, 1, Qt::DashLine));
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        const double w = m_werte.at(m_hoverIndex);
        if (!std::isnan(w)) {
            p.setPen(Qt::NoPen);
            p.setBrush(m_farbe);
            p.drawEllipse(QPointF(x, yFuer(w)), 3.5, 3.5);
        }
    }
}

void VerlaufPlot::mouseMoveEvent(QMouseEvent* event)
{
    const QRectF r = plotBereich();
    const int n = qMin(m_zeit.size(), m_werte.size());
    if (n < 2 || !m_hinweis.isEmpty() || !r.contains(event->position())) {
        if (m_hoverIndex != -1) {
            m_hoverIndex = -1;
            QToolTip::hideText();
            update();
        }
        return;
    }
    const double anteil = (event->position().x() - r.left()) / r.width();
    const int idx = qBound(0, int(std::lround(anteil * (n - 1))), n - 1);
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        const double w = m_werte.at(idx);
        const QString wert = std::isnan(w) ? tr("Lücke (kein Messwert)")
                                           : Anzeige::zahl(w, 2) + QLatin1Char(' ') + m_einheit;
        QToolTip::showText(event->globalPosition().toPoint(),
                           m_zeit.at(idx).toString(QStringLiteral("dd.MM.yyyy hh:mm")) + QStringLiteral(": ") + wert,
                           this);
        update();
    }
}

void VerlaufPlot::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    update();
}
