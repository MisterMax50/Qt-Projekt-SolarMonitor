#include "intro_animation.h"

#include <algorithm>
#include <cmath>

#include <QKeyEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QTimer>

#include "util/stil.h"

namespace {

constexpr double kDauer = 3.4;       ///< Gesamtdauer in Sekunden
constexpr double kAusblenden = 0.5;  ///< davon am Ende zum Ausblenden

/// Fortschritt 0 … 1 eines Abschnitts von @p start bis @p ende.
double phase(double t, double start, double ende)
{
    return std::clamp((t - start) / (ende - start), 0.0, 1.0);
}

double easeOut(double x) { return 1.0 - std::pow(1.0 - x, 3.0); }

/// Federndes Aufspringen (leicht über das Ziel hinaus und zurück).
double easeOutBack(double x)
{
    constexpr double c1 = 1.70158, c3 = c1 + 1.0;
    return 1.0 + c3 * std::pow(x - 1.0, 3.0) + c1 * std::pow(x - 1.0, 2.0);
}

} // namespace

IntroAnimation::IntroAnimation(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor);

    m_takt = new QTimer(this);
    m_takt->setInterval(16); // ~60 Bilder pro Sekunde
    connect(m_takt, &QTimer::timeout, this, [this] {
        m_zeit = m_uhr.elapsed() / 1000.0;
        update();
        if (m_zeit >= kDauer)
            beenden();
    });
}

void IntroAnimation::starte()
{
    m_beendet = false;
    m_zeit = 0.0;
    setFocus();
    m_uhr.start();
    m_takt->start();
}

void IntroAnimation::setzeZeit(double sekunden)
{
    m_zeit = sekunden;
    update();
}

void IntroAnimation::beenden()
{
    if (m_beendet)
        return;
    m_beendet = true;
    m_takt->stop();
    emit fertig();
}

void IntroAnimation::mousePressEvent(QMouseEvent*) { beenden(); }
void IntroAnimation::keyPressEvent(QKeyEvent*) { beenden(); }

void IntroAnimation::paintEvent(QPaintEvent*)
{
    const double t = m_zeit;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Hintergrund: Dunkelblau mit leichtem Verlauf zur Mitte
    const QRectF r = rect();
    QRadialGradient grund(r.center(), std::max(r.width(), r.height()) * 0.7);
    grund.setColorAt(0.0, QColor("#16294a"));
    grund.setColorAt(1.0, QColor("#08111f"));
    p.fillRect(r, grund);

    // Gesamte Deckkraft: am Anfang kurz einblenden, am Ende ausblenden
    const double deck = phase(t, 0.0, 0.25) * (1.0 - phase(t, kDauer - kAusblenden, kDauer));
    p.setOpacity(deck);

    const double g = std::min(r.width(), r.height()) * 0.22; // Größe des Logos
    const QPointF mitte(r.center().x(), r.center().y() - g * 0.35);

    // 1. Warmer Schein hinter dem Logo, wird stärker und pulsiert leicht
    const double schein = easeOut(phase(t, 0.2, 1.4)) * (0.85 + 0.15 * std::sin(t * 3.0));
    QRadialGradient licht(mitte, g * 1.6);
    licht.setColorAt(0.0, QColor(245, 158, 11, int(110 * schein)));
    licht.setColorAt(0.5, QColor(245, 158, 11, int(35 * schein)));
    licht.setColorAt(1.0, QColor(245, 158, 11, 0));
    p.fillRect(QRectF(mitte.x() - g * 1.6, mitte.y() - g * 1.6, g * 3.2, g * 3.2), licht);

    // 2. Kachel springt auf, danach Sonne, Strahlen und Messkurve
    const double auf = phase(t, 0.1, 0.75);
    if (auf > 0.0) {
        const double s = std::max(0.0, easeOutBack(auf));
        const double groesse = g * (0.55 + 0.45 * s);
        p.save();
        p.setOpacity(deck * std::min(1.0, auf * 2.0));
        Stil::zeichneLogo(p, QRectF(mitte.x() - groesse / 2, mitte.y() - groesse / 2, groesse, groesse),
                          easeOut(phase(t, 0.55, 1.15)), phase(t, 1.0, 1.6), easeOut(phase(t, 1.3, 2.0)));
        p.restore();
    }

    // 3. Schriftzug blendet ein und gleitet ein Stück nach oben
    const double text = easeOut(phase(t, 1.7, 2.3));
    if (text > 0.0) {
        QFont fett(font());
        fett.setPixelSize(int(g * 0.34));
        fett.setWeight(QFont::Bold);
        QFont normal(fett);
        normal.setWeight(QFont::Normal);
        const QString solar = QStringLiteral("Solar"), monitor = QStringLiteral("Monitor");
        const double b1 = QFontMetricsF(fett).horizontalAdvance(solar);
        const double b2 = QFontMetricsF(normal).horizontalAdvance(monitor);
        const double y = mitte.y() + g * 0.95 + (1.0 - text) * g * 0.15;
        const double x = r.center().x() - (b1 + b2) / 2;
        p.save();
        p.setOpacity(deck * text);
        p.setPen(Qt::white);
        p.setFont(fett);
        p.drawText(QPointF(x, y), solar);
        p.setFont(normal);
        p.setPen(QColor("#fbbf24"));
        p.drawText(QPointF(x + b1, y), monitor);

        QFont klein(font());
        klein.setPixelSize(std::max(12, int(g * 0.085)));
        klein.setLetterSpacing(QFont::AbsoluteSpacing, g * 0.012);
        p.setFont(klein);
        p.setPen(QColor(159, 176, 198));
        p.setOpacity(deck * easeOut(phase(t, 2.0, 2.5)));
        p.drawText(QRectF(0, y + g * 0.12, r.width(), g * 0.2), Qt::AlignHCenter | Qt::AlignTop,
                   tr("PV-FLOTTE BODENSEE · MONITORING & SERVICE"));
        p.restore();
    }

    // Hinweis zum Überspringen
    QFont hinweis(font());
    hinweis.setPixelSize(13);
    p.setFont(hinweis);
    p.setPen(QColor(125, 141, 163));
    p.setOpacity(deck * 0.8 * phase(t, 0.6, 1.2));
    p.drawText(r.adjusted(0, 0, -28, -24), Qt::AlignRight | Qt::AlignBottom,
               tr("Klicken oder beliebige Taste zum Überspringen"));
}
