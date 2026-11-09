#include "kennzahl_kachel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>

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
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_symbolLabel = new QLabel(this);
    m_symbolLabel->setFixedSize(kKreis, kKreis);
    auto* t = new QLabel(titel, this);
    t->setObjectName(QStringLiteral("KachelTitel"));
    m_wert = new QLabel(QStringLiteral("–"), this);
    m_wert->setObjectName(QStringLiteral("KachelWert"));
    m_notiz = new QLabel(this);
    m_notiz->setObjectName(QStringLiteral("KachelNotiz"));
    m_notiz->setWordWrap(true);
    m_notiz->setMinimumHeight(m_notiz->fontMetrics().height());

    auto* texte = new QVBoxLayout;
    texte->setSpacing(1);
    texte->addWidget(t);
    texte->addWidget(m_wert);
    texte->addWidget(m_notiz);

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
