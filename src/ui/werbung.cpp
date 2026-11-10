#include "werbung.h"

#include <QApplication>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>

#include "util/stil.h"

namespace {

struct Anzeige {
    Stil::Symbol symbol;
    QColor       farbe;
    const char*  domain;   ///< erfundene Adresse (.example ist reserviert)
    const char*  titel;
    const char*  text;
    const char*  aktion;
};

// Alle Produkte sind frei erfunden.
const Anzeige kAnzeigen[] = {
    {Stil::Symbol::Sonne, QColor(245, 158, 11), "sonnenputz3000.example", QT_TRANSLATE_NOOP("WerbeBanner", "SonnenPutz 3000"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Saubere Module, null Schatten: der Reinigungsroboter, der nebenbei auch Tauben verscheucht."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Jetzt vorbestellen")},
    {Stil::Symbol::Karte, QColor(37, 99, 235), "wolkenschieber.example", QT_TRANSLATE_NOOP("WerbeBanner", "Wolkenschieber Pro"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Nie wieder Ertragsverlust durch Wolken!* (*Funktioniert nicht bei Wetter.)"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Gratis testen")},
    {Stil::Symbol::Blitz, QColor(217, 119, 6), "kilowattkekse.example", QT_TRANSLATE_NOOP("WerbeBanner", "Kilowattkekse"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Die Energiekekse für lange Wartungstouren. Jetzt mit 30 % mehr Watt pro Biss."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Zum Keks-Shop")},
    {Stil::Symbol::Werkzeug, QColor(13, 148, 136), "seeweg-express.example", QT_TRANSLATE_NOOP("WerbeBanner", "Seeweg-Express"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Mit dem Tretboot quer über den Bodensee zur nächsten Störung – Werkzeugkoffer schwimmt leider nicht."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Ticket sichern")},
    {Stil::Symbol::Statistik, QColor(124, 58, 237), "pv-horoskop.example", QT_TRANSLATE_NOOP("WerbeBanner", "Das PV-Horoskop"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Was die Sterne über Ihre Performance Ratio sagen. Heute: Merkur rückläufig, Wechselrichter auch."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Horoskop lesen")},
    {Stil::Symbol::Euro, QColor(22, 163, 74), "einspeise-lotto.example", QT_TRANSLATE_NOOP("WerbeBanner", "Einspeise-Lotto"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Tippen Sie Ihren nächsten Einspeisetarif! Hauptgewinn: 0,47 Cent mehr pro Kilowattstunde."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Mitspielen")},
};
constexpr int kAnzahl = int(sizeof(kAnzeigen) / sizeof(kAnzeigen[0]));
constexpr int kWechselMs = 6000;
constexpr int kBlinkMs = 450;

QString tw(const char* s) { return QCoreApplication::translate("WerbeBanner", s); }

/// "Produktbild": Symbol auf einem Farbverlauf, optional leicht gedreht (Wackeln)
QPixmap produktbild(const Anzeige& a, int g, qreal dpr, qreal winkel = 0)
{
    QPixmap pm(QSize(g, g) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.translate(g / 2.0, g / 2.0);
    p.rotate(winkel);
    p.scale(0.86, 0.86);
    p.translate(-g / 2.0, -g / 2.0);
    QLinearGradient verlauf(0, 0, g, g);
    verlauf.setColorAt(0, a.farbe.lighter(130));
    verlauf.setColorAt(1, a.farbe.darker(115));
    p.setPen(Qt::NoPen);
    p.setBrush(verlauf);
    p.drawRoundedRect(QRectF(0, 0, g, g), g * 0.2, g * 0.2);
    const int r = g / 4;
    Stil::icon(a.symbol, Qt::white).paint(&p, QRect(r, r, g - 2 * r, g - 2 * r));
    p.end();
    return pm;
}

QString mmss(int sek) { return QStringLiteral("%1:%2").arg(sek / 60, 2, 10, QLatin1Char('0')).arg(sek % 60, 2, 10, QLatin1Char('0')); }

/// Setzt eine dynamische Eigenschaft und lässt das Stylesheet neu greifen.
void setzeBlink(QWidget* w, bool an)
{
    if (!w)
        return;
    w->setProperty("blink", an);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Lauftext ("+++ NUR HEUTE +++ ...") am unteren Rand des Banners.
class Lauftext : public QWidget {
public:
    explicit Lauftext(QWidget* parent)
        : QWidget(parent)
    {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 0.95);
        setFont(f);
        setFixedHeight(fontMetrics().height() + 8);
        m_text = QStringLiteral("   +++   ")
                 + QStringList{tw(QT_TRANSLATE_NOOP("WerbeBanner", "NUR HEUTE")),
                               tw(QT_TRANSLATE_NOOP("WerbeBanner", "Kilowattkekse: 2 Packungen zum Preis von 3")),
                               tw(QT_TRANSLATE_NOOP("WerbeBanner", "Wolkenschieber Pro jetzt mit Rückenwind")),
                               tw(QT_TRANSLATE_NOOP("WerbeBanner", "SonnenPutz 3000 – die Tauben hassen diesen Trick")),
                               tw(QT_TRANSLATE_NOOP("WerbeBanner", "Ärzte empfehlen: mehr Sonne")),
                               tw(QT_TRANSLATE_NOOP("WerbeBanner", "Werbefrei mit SolarMonitor Premium – nur 0,00 €"))}
                       .join(QStringLiteral("   +++   "));
        auto* t = new QTimer(this);
        t->setInterval(30);
        connect(t, &QTimer::timeout, this, [this] {
            m_versatz += 2;
            const int breite = fontMetrics().horizontalAdvance(m_text);
            if (breite > 0 && m_versatz >= breite)
                m_versatz -= breite;
            update();
        });
        t->start();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(185, 28, 28));
        p.setPen(QColor(255, 247, 214));
        const int breite = fontMetrics().horizontalAdvance(m_text);
        if (breite <= 0)
            return;
        for (int x = -m_versatz; x < width(); x += breite)
            p.drawText(QRect(x, 0, breite, height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
    }

private:
    QString m_text;
    int     m_versatz = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Banner
// ---------------------------------------------------------------------------

WerbeBanner::WerbeBanner(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Werbung"));
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Spaß-Werbung – mit SolarMonitor Premium verschwindet sie"));

    auto* hinweis = new QLabel(tr("ANZEIGE"), this);
    hinweis->setObjectName(QStringLiteral("WerbungHinweis"));
    m_sticker = new QLabel(tr("NUR HEUTE!"), this);
    m_sticker->setObjectName(QStringLiteral("WerbungSticker"));
    m_bild = new QLabel(this);
    m_bild->setFixedSize(64, 64);
    m_titel = new QLabel(this);
    m_titel->setObjectName(QStringLiteral("WerbungTitel"));
    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("WerbungText"));
    m_text->setWordWrap(true);
    m_countdown = new QLabel(this);
    m_countdown->setObjectName(QStringLiteral("WerbungCountdown"));
    m_aktion = new QPushButton(this);
    m_aktion->setObjectName(QStringLiteral("WerbungAktion"));
    m_aktion->setCursor(Qt::PointingHandCursor);
    connect(m_aktion, &QPushButton::clicked, this, [this] { emit webseiteGewuenscht(m_index); });

    auto* schliessen = new QToolButton(this);
    schliessen->setObjectName(QStringLiteral("WerbungSchliessen"));
    schliessen->setText(QStringLiteral("×"));
    schliessen->setToolTip(tr("Werbung entfernen"));
    connect(schliessen, &QToolButton::clicked, this, &WerbeBanner::schliessenGeklickt);

    auto* texte = new QVBoxLayout;
    texte->setSpacing(2);
    auto* kopf = new QHBoxLayout;
    kopf->setSpacing(8);
    kopf->addWidget(m_titel);
    kopf->addWidget(hinweis);
    kopf->addWidget(m_sticker);
    kopf->addStretch(1);
    texte->addLayout(kopf);
    texte->addWidget(m_text);
    texte->addWidget(m_countdown);

    auto* zeile = new QHBoxLayout;
    zeile->setContentsMargins(12, 8, 8, 8);
    zeile->setSpacing(14);
    zeile->addWidget(m_bild);
    zeile->addLayout(texte, 1);
    zeile->addWidget(m_aktion);
    zeile->addWidget(schliessen, 0, Qt::AlignTop);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(2, 2, 2, 2);
    v->setSpacing(0);
    v->addLayout(zeile);
    v->addWidget(new Lauftext(this));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // mit einer zufälligen Anzeige beginnen, dann regelmäßig wechseln
    m_index = int(QRandomGenerator::global()->bounded(kAnzahl));
    zeige(m_index);
    m_wechsel = new QTimer(this);
    m_wechsel->setInterval(kWechselMs);
    connect(m_wechsel, &QTimer::timeout, this, [this] { zeige((m_index + 1) % kAnzahl); });
    m_wechsel->start();

    // Blinken, Wackeln, Countdown
    m_blink = new QTimer(this);
    m_blink->setInterval(kBlinkMs);
    connect(m_blink, &QTimer::timeout, this, &WerbeBanner::takt);
    m_blink->start();
}

void WerbeBanner::zeige(int index)
{
    m_index = index;
    const Anzeige& a = kAnzeigen[index];
    m_bild->setPixmap(produktbild(a, 64, devicePixelRatioF()));
    m_titel->setText(tr(a.titel));
    m_text->setText(tr(a.text));
    m_aktion->setText(tr(a.aktion).toUpper() + QStringLiteral(" »"));
}

void WerbeBanner::takt()
{
    ++m_schritt;
    const bool an = m_schritt % 2;
    setzeBlink(m_sticker, an);
    setzeBlink(m_aktion, an);
    m_bild->setPixmap(produktbild(kAnzeigen[m_index], 64, devicePixelRatioF(), an ? 8 : -8));
    // ungefähr jede Sekunde weiterzählen; bei 0 fängt das "letzte Angebot" einfach neu an
    if (m_schritt % 2 == 0)
        m_restSekunden = m_restSekunden > 0 ? m_restSekunden - 1 : 299;
    m_countdown->setText(tr("Angebot endet in %1 – nicht verpassen!").arg(mmss(m_restSekunden)));
}

void WerbeBanner::mousePressEvent(QMouseEvent* e)
{
    // Klick irgendwo aufs Banner öffnet die "Webseite" – typisch Werbung
    if (e->button() == Qt::LeftButton) {
        emit webseiteGewuenscht(m_index);
        e->accept();
        return;
    }
    QFrame::mousePressEvent(e);
}

void WerbeBanner::schliessenGeklickt()
{
    m_wechsel->stop();
    QMessageBox frage(this);
    frage.setWindowTitle(tr("Spaß-Werbung"));
    frage.setIconPixmap(Stil::appIcon().pixmap(48, 48));
    frage.setText(tr("<b>Erwischt – das ist nur Spaß-Werbung.</b>"));
    frage.setInformativeText(tr("Keines dieser Produkte gibt es wirklich. Mit <b>SolarMonitor Premium</b> verschwindet "
                                "die Werbung – für sagenhafte 0,00 € im Monat."));
    QPushButton* premium = frage.addButton(tr("Premium aktivieren"), QMessageBox::AcceptRole);
    frage.addButton(tr("Weiter mit Werbung"), QMessageBox::RejectRole);
    frage.setDefaultButton(premium);
    frage.exec();
    if (frage.clickedButton() == premium)
        emit premiumGewuenscht();
    else
        m_wechsel->start();
}

// ---------------------------------------------------------------------------
// Popup
// ---------------------------------------------------------------------------

WerbePopup::WerbePopup(int anzeige, QWidget* wirt)
    : QWidget(wirt)
    , m_anzeige(anzeige)
{
    setObjectName(QStringLiteral("WerbePopup"));
    setFocusPolicy(Qt::StrongFocus);
    const Anzeige& a = kAnzeigen[anzeige];

    m_karte = new QFrame(this);
    m_karte->setObjectName(QStringLiteral("WerbePopupKarte"));
    auto* schatten = new QGraphicsDropShadowEffect(m_karte);
    schatten->setBlurRadius(48);
    schatten->setOffset(0, 12);
    schatten->setColor(QColor(0, 0, 0, 140));
    m_karte->setGraphicsEffect(schatten);

    m_kopf = new QLabel(tr("!!! EXKLUSIVES ANGEBOT NUR FÜR SIE !!!"), m_karte);
    m_kopf->setObjectName(QStringLiteral("WerbePopupKopf"));
    m_kopf->setAlignment(Qt::AlignCenter);

    m_bild = new QLabel(m_karte);
    m_bild->setFixedSize(104, 104);
    m_bild->setPixmap(produktbild(a, 104, devicePixelRatioF()));
    auto* titel = new QLabel(tw(a.titel), m_karte);
    titel->setObjectName(QStringLiteral("WerbePopupTitel"));
    auto* text = new QLabel(tw(a.text), m_karte);
    text->setObjectName(QStringLiteral("WerbePopupText"));
    text->setWordWrap(true);
    auto* preis = new QLabel(tr("Statt <s>999,99 €</s> nur <b>0,00 €*</b>"), m_karte);
    preis->setObjectName(QStringLiteral("WerbePopupPreis"));
    m_countdown = new QLabel(m_karte);
    m_countdown->setObjectName(QStringLiteral("WerbungCountdown"));
    m_countdown->setWordWrap(true);

    m_aktion = new QPushButton(tw(a.aktion).toUpper() + QStringLiteral(" »"), m_karte);
    m_aktion->setObjectName(QStringLiteral("WerbungAktion"));
    m_aktion->setCursor(Qt::PointingHandCursor);
    connect(m_aktion, &QPushButton::clicked, this, [this] { emit webseiteGewuenscht(m_anzeige); });

    auto* premium = new QPushButton(tr("Keine Lust mehr? Werbefrei mit SolarMonitor Premium"), m_karte);
    premium->setObjectName(QStringLiteral("WerbePopupPremium"));
    premium->setCursor(Qt::PointingHandCursor);
    connect(premium, &QPushButton::clicked, this, &WerbePopup::premiumGewuenscht);

    auto* klein = new QLabel(tr("* zzgl. Versand per Tretboot. Alle Produkte frei erfunden."), m_karte);
    klein->setObjectName(QStringLiteral("WerbePopupKlein"));

    // feste Textbreite (Karte 560 − Ränder − Bild − Abstand), damit die
    // umbrochenen Zeilen genau die nötige Höhe bekommen
    constexpr int kTextBreite = 560 - 2 * 3 - 2 * 24 - 104 - 20;
    for (QLabel* l : {titel, text, preis, m_countdown})
        l->setFixedWidth(kTextBreite);
    auto* rechts = new QVBoxLayout;
    rechts->setSpacing(6);
    rechts->addWidget(titel);
    rechts->addWidget(text);
    rechts->addWidget(preis);
    rechts->addWidget(m_countdown);

    auto* mitte = new QHBoxLayout;
    mitte->setContentsMargins(24, 18, 24, 6);
    mitte->setSpacing(20);
    mitte->addWidget(m_bild, 0, Qt::AlignTop);
    mitte->addLayout(rechts, 1);

    auto* v = new QVBoxLayout(m_karte);
    v->setContentsMargins(3, 3, 3, 12);
    v->setSpacing(8);
    v->addWidget(m_kopf);
    v->addLayout(mitte);
    v->addWidget(m_aktion, 0, Qt::AlignHCenter);
    v->addWidget(premium, 0, Qt::AlignHCenter);
    v->addWidget(klein, 0, Qt::AlignHCenter);

    // das winzige Kreuz, frei oben rechts über der Kopfzeile
    m_kreuz = new QToolButton(m_karte);
    m_kreuz->setObjectName(QStringLiteral("WerbePopupKreuz"));
    m_kreuz->setText(QStringLiteral("×"));
    m_kreuz->setFixedSize(11, 11);
    m_kreuz->setCursor(Qt::PointingHandCursor);
    connect(m_kreuz, &QToolButton::clicked, this, &WerbePopup::kreuzGeklickt);

    // Höhe passend zu den umbrochenen Texten bei fester Breite
    m_karte->setFixedWidth(560);
    m_karte->layout()->activate();
    const int h = m_karte->heightForWidth(560);
    m_karte->resize(560, (h > 0 ? h : m_karte->sizeHint().height()) + 20);

    wirt->installEventFilter(this);
    anpassen();

    m_blink = new QTimer(this);
    m_blink->setInterval(kBlinkMs);
    connect(m_blink, &QTimer::timeout, this, &WerbePopup::takt);
    m_blink->start();
    takt();

    // "Aufploppen"
    const QRect ziel = m_karte->geometry();
    auto* anim = new QPropertyAnimation(m_karte, "geometry", this);
    anim->setDuration(260);
    anim->setStartValue(QRect(ziel.center() - QPoint(ziel.width() / 4, ziel.height() / 4), ziel.size() / 2));
    anim->setEndValue(ziel);
    anim->setEasingCurve(QEasingCurve::OutBack);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void WerbePopup::anpassen()
{
    if (!parentWidget())
        return;
    setGeometry(parentWidget()->rect());
    const QSize k = m_karte->size();
    m_karte->move((width() - k.width()) / 2, (height() - k.height()) / 2);
    m_kreuz->move(k.width() - m_kreuz->width() - 6, 6);
    m_kreuz->raise();
}

bool WerbePopup::eventFilter(QObject* o, QEvent* e)
{
    if (o == parentWidget() && e->type() == QEvent::Resize)
        anpassen();
    return QWidget::eventFilter(o, e);
}

void WerbePopup::paintEvent(QPaintEvent*)
{
    // abgedunkelter Hintergrund; Klicks darauf landen hier und bewirken nichts
    QPainter p(this);
    p.fillRect(rect(), QColor(8, 12, 28, 150));
}

void WerbePopup::keyPressEvent(QKeyEvent* e)
{
    // Escape hilft hier nicht – das Popup will per Kreuz geschlossen werden
    e->accept();
}

void WerbePopup::takt()
{
    ++m_schritt;
    const bool an = m_schritt % 2;
    setzeBlink(m_kopf, an);
    setzeBlink(m_aktion, an);
    m_bild->setPixmap(produktbild(kAnzeigen[m_anzeige], 104, devicePixelRatioF(), an ? 6 : -6));
    if (m_schritt % 2 == 0)
        m_restSekunden = m_restSekunden > 0 ? m_restSekunden - 1 : 59;
    m_countdown->setText(tr("Nur noch %1 – dann ist das Angebot weg (bis zum nächsten Mal)").arg(mmss(m_restSekunden)));
}

void WerbePopup::kreuzGeklickt()
{
    ++m_kreuzKlicks;
    if (m_kreuzKlicks < kKlicksZumSchliessen) {
        // "verklickt" – stattdessen geht die Webseite der Anzeige auf
        emit webseiteGewuenscht(m_anzeige);
        return;
    }
    hide();
    deleteLater();
}

// ---------------------------------------------------------------------------
// "Webseite"
// ---------------------------------------------------------------------------

WerbeSeite::WerbeSeite(int anzeige, QWidget* fenster)
    : QWidget(fenster, Qt::Window)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("WerbeSeite"));
    setAttribute(Qt::WA_StyledBackground, true);
    const Anzeige& a = kAnzeigen[anzeige];
    const QString titel = tw(a.titel);
    setWindowTitle(tr("%1 – Offizieller Shop").arg(titel));
    resize(980, 700);

    // --- Browser-Rahmen -------------------------------------------------
    auto* reiterLeiste = new QWidget(this);
    reiterLeiste->setObjectName(QStringLiteral("WerbeSeiteReiter"));
    reiterLeiste->setAttribute(Qt::WA_StyledBackground, true);
    auto* reiter = new QLabel(tr("%1 – Offizieller Shop").arg(titel) + QStringLiteral("     ×"), reiterLeiste);
    reiter->setObjectName(QStringLiteral("WerbeSeiteTab"));
    auto* rh = new QHBoxLayout(reiterLeiste);
    rh->setContentsMargins(10, 8, 10, 0);
    rh->addWidget(reiter);
    rh->addStretch(1);

    auto* leiste = new QWidget(this);
    leiste->setObjectName(QStringLiteral("WerbeSeiteLeiste"));
    leiste->setAttribute(Qt::WA_StyledBackground, true);
    auto* lh = new QHBoxLayout(leiste);
    lh->setContentsMargins(10, 6, 10, 6);
    lh->setSpacing(4);
    for (const QString& z : {QStringLiteral("←"), QStringLiteral("→"), QStringLiteral("⟳")}) {
        auto* k = new QToolButton(leiste);
        k->setObjectName(QStringLiteral("WerbeSeiteNav"));
        k->setText(z);
        k->setEnabled(false);
        lh->addWidget(k);
    }
    auto* adresse = new QLineEdit(QStringLiteral("https://www.%1/angebot?ref=solarmonitor&gewinner=ja&popup=%2")
                                      .arg(QLatin1String(a.domain))
                                      .arg(QRandomGenerator::global()->bounded(100000, 999999)),
                                  leiste);
    adresse->setObjectName(QStringLiteral("WerbeSeiteAdresse"));
    adresse->setReadOnly(true);
    lh->addWidget(adresse, 1);

    // --- Seiteninhalt ------------------------------------------------------
    auto* inhalt = new QWidget;
    inhalt->setObjectName(QStringLiteral("WerbeSeiteInhalt"));
    inhalt->setAttribute(Qt::WA_StyledBackground, true);

    m_gewinn = new QLabel(tr("*** GLÜCKWUNSCH! Sie sind unser 1.000.000. Besucher! ***"), inhalt);
    m_gewinn->setObjectName(QStringLiteral("WerbeSeiteGewinn"));
    m_gewinn->setAlignment(Qt::AlignCenter);

    auto* bild = new QLabel(inhalt);
    bild->setFixedSize(180, 180);
    bild->setPixmap(produktbild(a, 180, devicePixelRatioF(), -4));
    auto* name = new QLabel(titel, inhalt);
    name->setObjectName(QStringLiteral("WerbeSeiteTitel"));
    auto* text = new QLabel(tw(a.text), inhalt);
    text->setObjectName(QStringLiteral("WerbeSeiteText"));
    text->setWordWrap(true);
    auto* preis = new QLabel(tr("Statt <s>999,99 €</s> nur <b>0,00 €*</b>"), inhalt);
    preis->setObjectName(QStringLiteral("WerbeSeitePreis"));
    auto* lager = new QLabel(tr("Nur noch 3 Stück auf Lager! 47 Personen sehen sich dieses Angebot gerade an."), inhalt);
    lager->setObjectName(QStringLiteral("WerbungCountdown"));
    lager->setWordWrap(true);
    m_countdown = new QLabel(inhalt);
    m_countdown->setObjectName(QStringLiteral("WerbungCountdown"));
    auto* kaufen = new QPushButton(tr("IN DEN WARENKORB »"), inhalt);
    kaufen->setObjectName(QStringLiteral("WerbungAktion"));
    kaufen->setCursor(Qt::PointingHandCursor);
    connect(kaufen, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Warenkorb"),
                                 tr("Diesen Shop gibt es nicht – das ist Spaß-Werbung in SolarMonitor.\n"
                                    "Ihr Warenkorb bleibt leer und Ihr Geld bei Ihnen."));
    });

    auto* rechts = new QVBoxLayout;
    rechts->setSpacing(10);
    rechts->addWidget(name);
    rechts->addWidget(text);
    rechts->addWidget(preis);
    rechts->addWidget(lager);
    rechts->addWidget(m_countdown);
    rechts->addWidget(kaufen, 0, Qt::AlignLeft);

    auto* held = new QHBoxLayout;
    held->setSpacing(32);
    held->addWidget(bild, 0, Qt::AlignTop);
    held->addLayout(rechts, 1);

    // drei "Vorteile"
    auto* vorteile = new QHBoxLayout;
    vorteile->setSpacing(16);
    const char* const vorteilTexte[][2] = {
        {QT_TRANSLATE_NOOP("WerbeSeite", "100 % Sonnenstrom"), QT_TRANSLATE_NOOP("WerbeSeite", "Hergestellt ausschließlich bei Tageslicht.")},
        {QT_TRANSLATE_NOOP("WerbeSeite", "Bodensee-geprüft"), QT_TRANSLATE_NOOP("WerbeSeite", "Wasserdicht bis knapp über der Oberfläche.")},
        {QT_TRANSLATE_NOOP("WerbeSeite", "Versand per Tretboot"), QT_TRANSLATE_NOOP("WerbeSeite", "Kostenlos, klimaneutral, sehr langsam.")},
    };
    for (const auto& vt : vorteilTexte) {
        auto* box = new QFrame(inhalt);
        box->setObjectName(QStringLiteral("WerbeSeiteBox"));
        auto* bv = new QVBoxLayout(box);
        auto* t = new QLabel(QStringLiteral("✔ ") + tr(vt[0]), box);
        t->setObjectName(QStringLiteral("WerbeSeiteBoxTitel"));
        auto* d = new QLabel(tr(vt[1]), box);
        d->setWordWrap(true);
        d->setObjectName(QStringLiteral("WerbeSeiteText"));
        bv->addWidget(t);
        bv->addWidget(d);
        vorteile->addWidget(box, 1);
    }

    auto* fuss = new QFrame(inhalt);
    fuss->setObjectName(QStringLiteral("WerbeSeiteFuss"));
    auto* fh = new QHBoxLayout(fuss);
    fh->setContentsMargins(16, 12, 16, 12);
    auto* fussText = new QLabel(tr("Diese Webseite ist frei erfunden und Teil der Spaß-Werbung in SolarMonitor. "
                                   "Mit SolarMonitor Premium ist Ruhe."),
                                fuss);
    fussText->setWordWrap(true);
    auto* premium = new QPushButton(tr("Premium aktivieren"), fuss);
    premium->setObjectName(QStringLiteral("WerbeSeitePremium"));
    premium->setCursor(Qt::PointingHandCursor);
    connect(premium, &QPushButton::clicked, this, [this] {
        emit premiumGewuenscht();
        close();
    });
    fh->addWidget(fussText, 1);
    fh->addWidget(premium);

    auto* iv = new QVBoxLayout(inhalt);
    iv->setContentsMargins(40, 24, 40, 24);
    iv->setSpacing(24);
    iv->addWidget(m_gewinn);
    iv->addLayout(held);
    iv->addLayout(vorteile);
    iv->addStretch(1);
    iv->addWidget(fuss);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("WerbeSeiteScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inhalt);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(reiterLeiste);
    v->addWidget(leiste);
    v->addWidget(scroll, 1);

    m_blink = new QTimer(this);
    m_blink->setInterval(kBlinkMs);
    connect(m_blink, &QTimer::timeout, this, [this, kaufen] {
        ++m_schritt;
        const bool an = m_schritt % 2;
        setzeBlink(m_gewinn, an);
        setzeBlink(kaufen, an);
        if (m_schritt % 2 == 0)
            m_restSekunden = m_restSekunden > 0 ? m_restSekunden - 1 : 599;
        m_countdown->setText(tr("Angebot endet in %1").arg(mmss(m_restSekunden)));
    });
    m_blink->start();
    m_countdown->setText(tr("Angebot endet in %1").arg(mmss(m_restSekunden)));
}

// ---------------------------------------------------------------------------
// Steuerung
// ---------------------------------------------------------------------------

WerbeSteuerung::WerbeSteuerung(QWidget* fenster, std::function<QWidget*()> wirt)
    : QObject(fenster)
    , m_fenster(fenster)
    , m_wirt(std::move(wirt))
{
    setObjectName(QStringLiteral("WerbeSteuerung"));
    m_takt = new QTimer(this);
    m_takt->setInterval(kTaktMs);
    connect(m_takt, &QTimer::timeout, this, [this] {
        if (!m_popup)
            zeigePopup();
    });
}

void WerbeSteuerung::setAktiv(bool an)
{
    if (an) {
        if (!m_takt->isActive())
            m_takt->start();
        return;
    }
    m_takt->stop();
    if (m_popup)
        m_popup->deleteLater();
    m_popup = nullptr;
    for (const QPointer<WerbeSeite>& s : std::as_const(m_seiten))
        if (s)
            s->close();
    m_seiten.clear();
}

bool WerbeSteuerung::aktiv() const { return m_takt->isActive(); }

void WerbeSteuerung::zeigePopup()
{
    QWidget* wirt = m_wirt ? m_wirt() : nullptr;
    if (!wirt || m_popup)
        return;
    const int nr = int(QRandomGenerator::global()->bounded(kAnzahl));
    auto* popup = new WerbePopup(nr, wirt);
    connect(popup, &WerbePopup::webseiteGewuenscht, this, &WerbeSteuerung::oeffneWebseite);
    connect(popup, &WerbePopup::premiumGewuenscht, this, &WerbeSteuerung::premiumGewuenscht);
    m_popup = popup;
    popup->show();
    popup->raise();
    popup->setFocus();
}

void WerbeSteuerung::oeffneWebseite(int anzeige)
{
    if (anzeige < 0 || anzeige >= kAnzahl)
        anzeige = 0;
    // geschlossene Fenster aus der Liste werfen
    m_seiten.removeIf([](const QPointer<WerbeSeite>& s) { return s.isNull(); });
    auto* seite = new WerbeSeite(anzeige, m_fenster);
    connect(seite, &WerbeSeite::premiumGewuenscht, this, &WerbeSteuerung::premiumGewuenscht);
    // wie echte Popups: jedes neue Fenster etwas versetzt
    const QRect basis = m_fenster->window()->geometry();
    const int versatz = 32 * int(m_seiten.size());
    QPoint pos = basis.center() - QPoint(seite->width() / 2, seite->height() / 2) + QPoint(versatz, versatz);
    if (QScreen* bildschirm = m_fenster->screen()) {
        const QRect frei = bildschirm->availableGeometry();
        pos.setX(qBound(frei.left(), pos.x(), qMax(frei.left(), frei.right() - seite->width())));
        pos.setY(qBound(frei.top(), pos.y(), qMax(frei.top(), frei.bottom() - seite->height())));
    }
    seite->move(pos);
    m_seiten.append(seite);
    seite->show();
    seite->raise();
    seite->activateWindow();
}

int WerbeSteuerung::offeneWebseiten() const
{
    int n = 0;
    for (const QPointer<WerbeSeite>& s : m_seiten)
        if (s && s->isVisible())
            ++n;
    return n;
}
