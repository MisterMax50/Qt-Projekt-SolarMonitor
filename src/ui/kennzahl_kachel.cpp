#include "kennzahl_kachel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kKreis = 42;
} // namespace

KennzahlKachel::KennzahlKachel(Stil::Symbol symbol, const QColor& farbe, const QString& titel, QWidget* parent)
    : QFrame(parent)
    , m_symbol(symbol)
    , m_farbe(farbe)
{
    setObjectName(QStringLiteral("Kachel"));
    setAttribute(Qt::WA_StyledBackground, true);
    // in der Höhe dehnbar: alle Kacheln einer Reihe werden so hoch wie die höchste
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_symbolLabel = new QLabel(this);
    m_symbolLabel->setFixedSize(kKreis, kKreis);
    auto* t = new QLabel(titel, this);
    t->setObjectName(QStringLiteral("KachelTitel"));
    t->setWordWrap(true); // lange Titel umbrechen statt abschneiden
    m_wert = new QLabel(QStringLiteral("–"), this);
    m_wert->setObjectName(QStringLiteral("KachelWert"));
    // Breite bestimmt die Kachel, nicht der Wert: bei wenig Platz wird die Schrift kleiner (passeWertAn)
    m_wert->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_wert->setMinimumWidth(1);
    m_notiz = new QLabel(this);
    m_notiz->setObjectName(QStringLiteral("KachelNotiz"));
    m_notiz->setWordWrap(true);
    m_notiz->setMinimumHeight(m_notiz->fontMetrics().height());

    auto* texte = new QVBoxLayout;
    texte->setSpacing(1);
    texte->addWidget(t);
    texte->addWidget(m_wert);
    texte->addWidget(m_notiz);
    texte->addStretch(1); // Inhalt oben halten, wenn die Kachel höher ist
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(16, 14, 16, 14);
    h->setSpacing(14);
    h->addWidget(m_symbolLabel, 0, Qt::AlignTop);
    h->addLayout(texte, 1);
    zeichneSymbol();
}

void KennzahlKachel::setzeWert(const QString& wert, const QString& notiz)
{
    m_wert->setText(wert);
    m_notiz->setText(notiz); // bleibt auch leer sichtbar, damit alle Kacheln gleich hoch sind
    passeWertAn();
}

void KennzahlKachel::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    passeWertAn();
}

void KennzahlKachel::passeWertAn()
{
    // Grundgröße einmal aus dem Stylesheet übernehmen
    if (m_basisPunkte <= 0.0) {
        ensurePolished();
        m_wert->ensurePolished();
        m_basisPunkte = m_wert->font().pointSizeF();
        if (m_basisPunkte <= 0.0)
            return;
    }
    QFont f = m_wert->font();
    f.setPointSizeF(m_basisPunkte);
    const int verfuegbar = width() - 16 - 16 - kKreis - 14 - 2;
    const int noetig = QFontMetrics(f).horizontalAdvance(m_wert->text());
    double punkte = m_basisPunkte;
    if (noetig > verfuegbar && verfuegbar > 0)
        punkte = std::max(m_basisPunkte * 0.6, m_basisPunkte * verfuegbar / noetig);
    if (std::abs(punkte - m_aktuellePunkte) < 0.05)
        return;
    m_aktuellePunkte = punkte;
    // lokales Stylesheet überstimmt die Schriftgröße aus dem globalen Stil
    m_wert->setStyleSheet(punkte < m_basisPunkte ? QStringLiteral("font-size: %1pt;").arg(punkte, 0, 'f', 1) : QString());
}

void KennzahlKachel::setzeFarbe(const QColor& farbe)
{
    if (farbe == m_farbe)
        return;
    m_farbe = farbe;
    zeichneSymbol();
}

void KennzahlKachel::setKlickbar(bool klickbar)
{
    m_klickbar = klickbar;
    setProperty("klickbar", klickbar);
    setCursor(klickbar ? Qt::PointingHandCursor : Qt::ArrowCursor);
    style()->unpolish(this);
    style()->polish(this);
}

void KennzahlKachel::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_klickbar && event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit geklickt();
    QFrame::mouseReleaseEvent(event);
}

void KennzahlKachel::zeichneSymbol()
{
    // Symbol in einem zart getönten Kreis der Kennzahlfarbe
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(QSize(kKreis, kKreis) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor kreis = m_farbe;
    kreis.setAlpha(Stil::istDunkel() ? 60 : 32);
    p.setPen(Qt::NoPen);
    p.setBrush(kreis);
    p.drawEllipse(QRectF(0, 0, kKreis, kKreis));
    const int g = 22;
    p.drawPixmap(QRect((kKreis - g) / 2, (kKreis - g) / 2, g, g),
                 Stil::icon(m_symbol, Stil::istDunkel() ? m_farbe.lighter(130) : m_farbe).pixmap(QSize(g, g) * dpr));
    p.end();
    m_symbolLabel->setPixmap(pm);
}
