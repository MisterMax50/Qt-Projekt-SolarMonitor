#include "keks_banner.h"

#include <QEasingCurve>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "util/stil.h"

namespace {

struct Kategorie {
    const char* titel;
    const char* text;
    bool        notwendig;   ///< immer an, nicht abwählbar
    QList<const char*> kekse;
};

// Alle Kekse sind frei erfunden – es wird nichts erhoben oder gespeichert.
QList<Kategorie> kategorien()
{
    return {
        {QT_TRANSLATE_NOOP("KeksEinstellungen", "Unbedingt notwendige Kekse"),
         QT_TRANSLATE_NOOP("KeksEinstellungen", "Ohne diese Kekse geht gar nichts. Sagen jedenfalls die Kekse."), true,
         {QT_TRANSLATE_NOOP("KeksEinstellungen", "Sitzungskeks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Anmeldekeks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Keks, der sich merkt, dass Sie Kekse abgelehnt haben"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Keks für diesen Keks-Banner")}},
        {QT_TRANSLATE_NOOP("KeksEinstellungen", "Funktionale Kekse"),
         QT_TRANSLATE_NOOP("KeksEinstellungen", "Machen SolarMonitor bequemer. Oder krümeliger."), false,
         {QT_TRANSLATE_NOOP("KeksEinstellungen", "Sprachkeks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Designkeks (hell oder dunkel gebacken)"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Lieblingsanlagen-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Zuletzt-gesehen-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Spaltenbreiten-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Kartenausschnitt-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Zoomstufen-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Mausrad-Geschwindigkeits-Keks")}},
        {QT_TRANSLATE_NOOP("KeksEinstellungen", "Sonnen-Analyse"),
         QT_TRANSLATE_NOOP("KeksEinstellungen", "Wir möchten Sie und die Sonne besser verstehen."), false,
         {QT_TRANSLATE_NOOP("KeksEinstellungen", "Zählt Ihre Blicke in die Sonne"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Misst, wie oft Sie „nur noch kurz“ sagen"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Sonnenbrillen-Erkennung"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Sonnencreme-Faktor-Schätzer"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Mittagspausen-Detektor"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Schattenwurf-Statistik"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Lieblingssonnenstrahl"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Sonnenuntergangs-Seufzer-Zähler"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Vitamin-D-Spiegel (geschätzt, sehr ungenau)")}},
        {QT_TRANSLATE_NOOP("KeksEinstellungen", "Wolken-Tracking"),
         QT_TRANSLATE_NOOP("KeksEinstellungen", "Damit wir wissen, welche Wolken Sie mögen."), false,
         {QT_TRANSLATE_NOOP("KeksEinstellungen", "Lieblingswolke ermitteln"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Wolkenform-Analyse (Schaf? Drache? Toast?)"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Regenschirm-Vorhersage"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Föhn-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Nebel-über-dem-See-Keks"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Wolkenschieber-Pro-Testabo"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Gewitter-Angst-Index"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Regenbogen-Sammelalbum")}},
        {QT_TRANSLATE_NOOP("KeksEinstellungen", "Marketing und Kilowattkekse"),
         QT_TRANSLATE_NOOP("KeksEinstellungen", "Damit Sie Werbung sehen, die so gut zu Ihnen passt wie Socken zu Sandalen."),
         false,
         {QT_TRANSLATE_NOOP("KeksEinstellungen", "Kilowattkekse-Empfehlungen"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "SonnenPutz-3000-Verfolgung über alle Seiten"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Tretboot-Werbung am Bodensee"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Personalisierte PV-Horoskope"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Einspeise-Lotto-Newsletter"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Krümel-Tracking"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Krümel-zu-Keks-Rekonstruktion"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Werbung für Werbung"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Werbung gegen Werbung"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Premium-Erinnerung alle fünf Minuten")}},
        {QT_TRANSLATE_NOOP("KeksEinstellungen", "Partner mit berechtigtem Interesse"),
         QT_TRANSLATE_NOOP("KeksEinstellungen", "Diese Partner finden ihr Interesse sehr berechtigt. Sie können "
                                                "einzeln widersprechen (Schalter aus)."),
         false,
         {QT_TRANSLATE_NOOP("KeksEinstellungen", "Die Tauben vom Dach"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Der Schornsteinfeger"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Ihr Wechselrichter (er will nur reden)"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Wetterstation WS07"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Der Bodensee"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Ein sehr neugieriger Kormoran"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "1.329 weitere Partner"),
          QT_TRANSLATE_NOOP("KeksEinstellungen", "Die Sonne selbst")}},
    };
}

} // namespace

// ---------------------------------------------------------------------------

QPixmap keksBild(int g, qreal dpr)
{
    QPixmap pm(QSize(g, g) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    // Keks mit Biss oben rechts
    QPainterPath keks;
    keks.addEllipse(QRectF(g * 0.04, g * 0.04, g * 0.92, g * 0.92));
    QPainterPath biss;
    biss.addEllipse(QRectF(g * 0.70, g * -0.08, g * 0.36, g * 0.36));
    biss.addEllipse(QRectF(g * 0.86, g * 0.14, g * 0.26, g * 0.26));
    keks = keks.subtracted(biss);
    QRadialGradient teig(QPointF(g * 0.42, g * 0.40), g * 0.6);
    teig.setColorAt(0, QColor(232, 185, 120));
    teig.setColorAt(1, QColor(196, 136, 70));
    p.setPen(QPen(QColor(150, 95, 40), qMax(1.0, g / 40.0)));
    p.setBrush(teig);
    p.drawPath(keks);
    // Schokostückchen
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(84, 48, 24));
    const QPointF stueck[] = {{0.30, 0.30}, {0.55, 0.42}, {0.28, 0.62}, {0.50, 0.70}, {0.70, 0.60}, {0.42, 0.48},
                              {0.62, 0.32}};
    for (const QPointF& s : stueck)
        p.drawEllipse(QPointF(s.x() * g, s.y() * g), g * 0.055, g * 0.045);
    p.end();
    return pm;
}

// ---------------------------------------------------------------------------

KeksSchalter::KeksSchalter(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
}

void KeksSchalter::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    QColor bahn = isChecked() ? QColor(22, 163, 74) : (Stil::istDunkel() ? QColor(71, 85, 105) : QColor(203, 213, 225));
    if (!isEnabled())
        bahn.setAlpha(110);
    p.setPen(Qt::NoPen);
    p.setBrush(bahn);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    const double d = r.height() - 4;
    const double x = isChecked() ? r.right() - d - 2 : r.left() + 2;
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(x, r.top() + 2, d, d));
}

// ---------------------------------------------------------------------------

KeksEinstellungen::KeksEinstellungen(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keks-Einstellungen"));
    resize(640, 720);

    auto* kopfBild = new QLabel(this);
    kopfBild->setPixmap(keksBild(44, devicePixelRatioF()));
    auto* kopf = new QLabel(tr("<b>Ihre Keks-Einstellungen</b><br>%1 Kekse, sorgfältig für Sie vorausgewählt.")
                                .arg(kAnzahlKekse),
                            this);
    kopf->setWordWrap(true);
    auto* kh = new QHBoxLayout;
    kh->addWidget(kopfBild);
    kh->addWidget(kopf, 1);

    auto* inhalt = new QWidget;
    auto* iv = new QVBoxLayout(inhalt);
    iv->setSpacing(14);
    for (const Kategorie& k : kategorien()) {
        auto* gruppe = new QFrame(inhalt);
        gruppe->setObjectName(QStringLiteral("KeksKategorie"));
        auto* gv = new QVBoxLayout(gruppe);
        gv->setContentsMargins(14, 10, 14, 10);
        gv->setSpacing(6);

        auto* titel = new QLabel(QStringLiteral("<b>%1</b> <span style='color:gray'>(%2)</span>")
                                     .arg(tr(k.titel).toHtmlEscaped())
                                     .arg(k.kekse.size()),
                                 gruppe);
        auto* alle = new KeksSchalter(gruppe);
        alle->setChecked(true);
        alle->setEnabled(!k.notwendig);
        alle->setToolTip(k.notwendig ? tr("Immer aktiv") : tr("Alle Kekse dieser Kategorie"));
        auto* th = new QHBoxLayout;
        th->addWidget(titel, 1);
        if (k.notwendig) {
            auto* immer = new QLabel(tr("immer aktiv"), gruppe);
            immer->setObjectName(QStringLiteral("Gedimmt"));
            th->addWidget(immer);
        }
        th->addWidget(alle);
        gv->addLayout(th);
        auto* text = new QLabel(tr(k.text), gruppe);
        text->setObjectName(QStringLiteral("Gedimmt"));
        text->setWordWrap(true);
        gv->addWidget(text);

        QList<KeksSchalter*> dieser;
        for (const char* name : k.kekse) {
            auto* zeile = new QHBoxLayout;
            zeile->setContentsMargins(12, 0, 0, 0);
            auto* l = new QLabel(tr(name), gruppe);
            l->setWordWrap(true);
            auto* s = new KeksSchalter(gruppe);
            s->setChecked(true); // natürlich vorausgewählt
            s->setEnabled(!k.notwendig);
            zeile->addWidget(l, 1);
            zeile->addWidget(s);
            gv->addLayout(zeile);
            dieser << s;
            m_schalter << s;
            if (!k.notwendig)
                m_waehlbar << s;
        }
        // Kategorie-Schalter schaltet alle; einzelne Schalter führen ihn nach
        connect(alle, &QAbstractButton::clicked, this, [alle, dieser] {
            for (KeksSchalter* s : dieser)
                s->setChecked(alle->isChecked());
        });
        for (KeksSchalter* s : dieser)
            connect(s, &QAbstractButton::toggled, alle, [alle, dieser] {
                bool einer = false;
                for (KeksSchalter* x : dieser)
                    einer = einer || x->isChecked();
                QSignalBlocker sperre(alle);
                alle->setChecked(einer);
                alle->update();
            });
        iv->addWidget(gruppe);
    }
    iv->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inhalt);

    // Knöpfe: "Alle ablehnen" klein und grau, "Alle akzeptieren" groß und orange
    auto* ablehnen = new QPushButton(tr("Alle ablehnen"), this);
    ablehnen->setObjectName(QStringLiteral("KeksAblehnen"));
    ablehnen->setCursor(Qt::PointingHandCursor);
    ablehnen->setAutoDefault(false);
    auto* speichern = new QPushButton(tr("Auswahl speichern"), this);
    speichern->setAutoDefault(false);
    auto* alleAn = new QPushButton(tr("Alle akzeptieren"), this);
    alleAn->setObjectName(QStringLiteral("KeksAlle"));
    alleAn->setCursor(Qt::PointingHandCursor);
    alleAn->setDefault(true);
    connect(alleAn, &QPushButton::clicked, this, [this] {
        setzeAlle(true);
        accept();
    });
    connect(speichern, &QPushButton::clicked, this, &QDialog::accept);
    connect(ablehnen, &QPushButton::clicked, this, [this] {
        QMessageBox frage(this);
        frage.setWindowTitle(tr("Alle ablehnen?"));
        frage.setIconPixmap(keksBild(48, devicePixelRatioF()));
        frage.setText(tr("Wirklich alle %1 abwählbaren Kekse ablehnen?").arg(m_waehlbar.size()));
        frage.setInformativeText(tr("Die Kekse wären dann sehr traurig."));
        QPushButton* ja = frage.addButton(tr("Ja, ablehnen"), QMessageBox::AcceptRole);
        QPushButton* nein = frage.addButton(tr("Nein, doch Kekse"), QMessageBox::RejectRole);
        frage.setDefaultButton(nein);
        frage.exec();
        if (frage.clickedButton() == ja) {
            setzeAlle(false);
            accept();
        }
    });
    auto* knoepfe = new QHBoxLayout;
    knoepfe->addWidget(ablehnen);
    knoepfe->addStretch(1);
    knoepfe->addWidget(speichern);
    knoepfe->addWidget(alleAn);

    auto* v = new QVBoxLayout(this);
    v->addLayout(kh);
    v->addWidget(scroll, 1);
    v->addLayout(knoepfe);
}

void KeksEinstellungen::setzeAlle(bool an)
{
    for (KeksSchalter* s : std::as_const(m_waehlbar))
        s->setChecked(an);
}

int KeksEinstellungen::anzahlAn() const
{
    int n = 0;
    for (KeksSchalter* s : m_schalter)
        n += s->isChecked() ? 1 : 0;
    return n;
}

// ---------------------------------------------------------------------------

KeksBanner::KeksBanner(QWidget* wirt)
    : QWidget(wirt)
{
    setObjectName(QStringLiteral("KeksBanner"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose);

    m_karte = new QFrame(this);
    m_karte->setObjectName(QStringLiteral("KeksKarte"));
    auto* schatten = new QGraphicsDropShadowEffect(m_karte);
    schatten->setBlurRadius(40);
    schatten->setOffset(0, -4);
    schatten->setColor(QColor(0, 0, 0, 120));
    m_karte->setGraphicsEffect(schatten);

    auto* bild = new QLabel(m_karte);
    bild->setPixmap(keksBild(84, devicePixelRatioF()));
    bild->setAlignment(Qt::AlignTop);
    auto* titel = new QLabel(tr("Wir verwenden Kekse!"), m_karte);
    titel->setObjectName(QStringLiteral("KeksTitel"));
    auto* text = new QLabel(tr("SolarMonitor und unsere 1.337 Partner verwenden Kekse, um Ihr Erlebnis zu verbessern, "
                               "Ihre Sonnenstunden zu zählen und Ihre Lieblingswolke zu ermitteln. Mit „Alle "
                               "akzeptieren“ stimmen Sie zu, dass wir Kekse backen, essen und darüber reden. "
                               "Ihre Einwilligung können Sie jederzeit in den Keks-Einstellungen im Kontomenü ändern."),
                            m_karte);
    text->setWordWrap(true);
    text->setObjectName(QStringLiteral("KeksText"));
    auto* klein = new QLabel(tr("Spaß-Funktion: Es werden keinerlei Daten erhoben. Mit Premium oder Ultimate entfällt "
                                "dieser Banner."),
                             m_karte);
    klein->setObjectName(QStringLiteral("Gedimmt"));
    klein->setWordWrap(true);

    m_alle = new QPushButton(tr("ALLE AKZEPTIEREN"), m_karte);
    m_alle->setObjectName(QStringLiteral("KeksAlleRiesig"));
    m_alle->setCursor(Qt::PointingHandCursor);
    m_einstellungen = new QPushButton(tr("Einstellungen (%1 Schalter)").arg(KeksEinstellungen::kAnzahlKekse), m_karte);
    m_einstellungen->setObjectName(QStringLiteral("KeksEinstellungenLink"));
    m_einstellungen->setCursor(Qt::PointingHandCursor);
    connect(m_alle, &QPushButton::clicked, this,
            [this] { fertig(KeksEinstellungen::kAnzahlKekse, KeksEinstellungen::kAnzahlKekse); });
    connect(m_einstellungen, &QPushButton::clicked, this, [this] {
        KeksEinstellungen dlg(window());
        if (dlg.exec() == QDialog::Accepted)
            fertig(dlg.anzahlAn(), dlg.anzahl());
    });

    auto* texte = new QVBoxLayout;
    texte->setSpacing(6);
    texte->addWidget(titel);
    texte->addWidget(text);
    texte->addWidget(klein);
    auto* rechts = new QVBoxLayout;
    rechts->setSpacing(6);
    rechts->addStretch(1);
    rechts->addWidget(m_alle);
    rechts->addWidget(m_einstellungen, 0, Qt::AlignHCenter);
    rechts->addStretch(1);
    auto* h = new QHBoxLayout(m_karte);
    h->setContentsMargins(24, 20, 24, 20);
    h->setSpacing(20);
    h->addWidget(bild, 0, Qt::AlignTop);
    h->addLayout(texte, 1);
    h->addLayout(rechts);

    wirt->installEventFilter(this);
    hide();
}

void KeksBanner::zeige()
{
    anpassen();
    show();
    raise();
    setFocus();
    // von unten hereinschieben
    const QRect ziel = m_karte->geometry();
    auto* anim = new QPropertyAnimation(m_karte, "pos", this);
    anim->setDuration(380);
    anim->setStartValue(QPoint(ziel.x(), height()));
    anim->setEndValue(ziel.topLeft());
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void KeksBanner::anpassen()
{
    if (!parentWidget())
        return;
    setGeometry(parentWidget()->rect());
    const int breite = qMin(1040, width() - 48);
    m_karte->setFixedWidth(breite);
    m_karte->adjustSize();
    m_karte->move((width() - breite) / 2, height() - m_karte->height() - 24);
}

void KeksBanner::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(8, 12, 28, 120));
}

void KeksBanner::keyPressEvent(QKeyEvent* e)
{
    e->accept(); // wie im Netz: Wegklicken geht nicht, eine Wahl muss her
}

void KeksBanner::mousePressEvent(QMouseEvent* e)
{
    e->accept(); // Klicks neben den Banner bewirken nichts
}

bool KeksBanner::eventFilter(QObject* o, QEvent* e)
{
    if (o == parentWidget() && e->type() == QEvent::Resize)
        anpassen();
    return QWidget::eventFilter(o, e);
}

void KeksBanner::fertig(int akzeptiert, int gesamt)
{
    hide();
    emit entschieden(akzeptiert, gesamt);
    close();
}
