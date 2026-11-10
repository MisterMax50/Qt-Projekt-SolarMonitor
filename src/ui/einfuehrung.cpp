#include "einfuehrung.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kRand = 6;        ///< Abstand der Aussparung um das Ziel
constexpr int kBlasenBreite = 360;
constexpr int kAbstand = 16;    ///< Abstand der Blase zum Ziel
} // namespace

Einfuehrung::Einfuehrung(QWidget* wirt, QVector<Schritt> schritte)
    : QWidget(wirt)
    , m_schritte(std::move(schritte))
{
    setObjectName(QStringLiteral("Einfuehrung"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose);

    m_blase = new QFrame(this);
    m_blase->setObjectName(QStringLiteral("TourBlase"));
    m_blase->setFixedWidth(kBlasenBreite);
    auto* schatten = new QGraphicsDropShadowEffect(m_blase);
    schatten->setBlurRadius(32);
    schatten->setOffset(0, 8);
    schatten->setColor(QColor(0, 0, 0, 110));
    m_blase->setGraphicsEffect(schatten);

    m_zaehler = new QLabel(m_blase);
    m_zaehler->setObjectName(QStringLiteral("TourZaehler"));
    m_titel = new QLabel(m_blase);
    m_titel->setObjectName(QStringLiteral("TourTitel"));
    m_titel->setWordWrap(true);
    m_text = new QLabel(m_blase);
    m_text->setObjectName(QStringLiteral("TourText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);

    m_ueberspringen = new QPushButton(tr("Tour beenden"), m_blase);
    m_ueberspringen->setObjectName(QStringLiteral("TourUeberspringen"));
    m_ueberspringen->setCursor(Qt::PointingHandCursor);
    m_zurueck = new QPushButton(tr("Zurück"), m_blase);
    m_zurueck->setObjectName(QStringLiteral("TourZurueck"));
    m_weiter = new QPushButton(m_blase);
    m_weiter->setObjectName(QStringLiteral("TourWeiter"));
    m_weiter->setDefault(true);
    connect(m_ueberspringen, &QPushButton::clicked, this, [this] { beende(false); });
    connect(m_zurueck, &QPushButton::clicked, this, &Einfuehrung::zurueck);
    connect(m_weiter, &QPushButton::clicked, this, &Einfuehrung::weiter);

    auto* knoepfe = new QHBoxLayout;
    knoepfe->setSpacing(8);
    knoepfe->addWidget(m_ueberspringen);
    knoepfe->addStretch(1);
    knoepfe->addWidget(m_zurueck);
    knoepfe->addWidget(m_weiter);

    auto* v = new QVBoxLayout(m_blase);
    v->setContentsMargins(20, 16, 20, 16);
    v->setSpacing(8);
    v->addWidget(m_zaehler);
    v->addWidget(m_titel);
    v->addWidget(m_text);
    v->addSpacing(6);
    v->addLayout(knoepfe);

    wirt->installEventFilter(this);
    hide();
}

void Einfuehrung::starte()
{
    if (m_schritte.isEmpty()) {
        beende(true);
        return;
    }
    setGeometry(parentWidget()->rect());
    show();
    raise();
    zeige(0);
}

void Einfuehrung::zeige(int index)
{
    m_index = qBound(0, index, int(m_schritte.size()) - 1);
    const Schritt& s = m_schritte.at(m_index);
    if (s.vorher)
        s.vorher();
    m_ziel = s.ziel ? s.ziel() : nullptr;
    if (m_ziel && !m_ziel->isVisible())
        m_ziel = nullptr;

    const bool letzter = m_index == m_schritte.size() - 1;
    m_zaehler->setText(tr("Schritt %1 von %2").arg(m_index + 1).arg(m_schritte.size()));
    m_titel->setText(s.titel);
    m_text->setText(s.text);
    m_zurueck->setVisible(m_index > 0);
    m_ueberspringen->setVisible(!letzter);
    m_weiter->setText(letzter ? tr("Los geht's") : m_index == 0 ? tr("Tour starten") : tr("Weiter"));
    // Seitenwechsel im vorher() braucht ggf. einen Layout-Durchlauf
    platziere();
    QTimer::singleShot(0, this, [this] {
        platziere();
        update();
    });
    setFocus();
    update();
}

QRect Einfuehrung::zielRechteck() const
{
    if (!m_ziel)
        return {};
    const QPoint oben = m_ziel->mapTo(parentWidget(), QPoint(0, 0));
    return QRect(oben, m_ziel->size()).adjusted(-kRand, -kRand, kRand, kRand).intersected(rect().adjusted(4, 4, -4, -4));
}

void Einfuehrung::platziere()
{
    if (!parentWidget())
        return;
    setGeometry(parentWidget()->rect());
    m_blase->adjustSize();
    const QSize b = m_blase->size();
    const QRect z = zielRechteck();
    QPoint pos;
    if (z.isNull()) {
        pos = QPoint((width() - b.width()) / 2, (height() - b.height()) / 2);
    } else if (z.right() + kAbstand + b.width() < width() - 8) {
        pos = QPoint(z.right() + kAbstand, z.top());          // rechts daneben
    } else if (z.bottom() + kAbstand + b.height() < height() - 8) {
        pos = QPoint(z.left(), z.bottom() + kAbstand);        // darunter
    } else if (z.top() - kAbstand - b.height() > 8) {
        pos = QPoint(z.left(), z.top() - kAbstand - b.height()); // darüber
    } else if (z.left() - kAbstand - b.width() > 8) {
        pos = QPoint(z.left() - kAbstand - b.width(), z.top()); // links daneben
    } else {
        pos = QPoint(z.center().x() - b.width() / 2, z.center().y() - b.height() / 2); // im Ziel (großes Ziel)
    }
    pos.setX(qBound(8, pos.x(), qMax(8, width() - b.width() - 8)));
    pos.setY(qBound(8, pos.y(), qMax(8, height() - b.height() - 8)));
    m_blase->move(pos);
    m_blase->raise();
}

void Einfuehrung::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath dunkel;
    dunkel.setFillRule(Qt::OddEvenFill);
    dunkel.addRect(rect());
    const QRect z = zielRechteck();
    if (!z.isNull())
        dunkel.addRoundedRect(z, 10, 10);
    p.fillPath(dunkel, QColor(8, 14, 32, 165));
    if (!z.isNull()) {
        p.setPen(QPen(QColor(245, 158, 11), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(z).adjusted(1, 1, -1, -1), 10, 10);
    }
}

void Einfuehrung::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        beende(false);
        break;
    case Qt::Key_Right:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        weiter();
        break;
    case Qt::Key_Left:
    case Qt::Key_Backspace:
        zurueck();
        break;
    default:
        break;
    }
    e->accept();
}

void Einfuehrung::mousePressEvent(QMouseEvent* e)
{
    // Klicks neben die Blase bewirken nichts (die Oberfläche ist während der Tour gesperrt)
    e->accept();
}

bool Einfuehrung::eventFilter(QObject* o, QEvent* e)
{
    if (o == parentWidget() && (e->type() == QEvent::Resize || e->type() == QEvent::LayoutRequest)) {
        platziere();
        update();
    }
    return QWidget::eventFilter(o, e);
}

void Einfuehrung::weiter()
{
    if (m_index >= m_schritte.size() - 1)
        beende(true);
    else
        zeige(m_index + 1);
}

void Einfuehrung::zurueck()
{
    if (m_index > 0)
        zeige(m_index - 1);
}

void Einfuehrung::beende(bool vollstaendig)
{
    hide();
    emit beendet(vollstaendig);
    close();
}
