#include "zahlung_dialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "util/stil.h"

int ZahlungDialog::verarbeitungMs = 3200;

namespace {

// Eigenes, festes Erscheinungsbild der erfundenen "Bezahlseite"
// (Farben der App: Dunkelblau + Sonnen-Orange), unabhängig vom Hell/Dunkel-Modus.
const char* const kStil = R"(
QDialog#Zahlung { background: #f4f6fb; }
QWidget#ZahlungKopf { background: #0f1b33; }
QLabel#ZahlungMarke { color: #ffffff; font-size: 15pt; font-weight: 800; }
QLabel#ZahlungMarkeZusatz { color: #fbbf24; font-size: 8.5pt; font-weight: 600; }
QLabel#ZahlungTest { color: #0f1b33; background: #fde68a; border-radius: 4px; padding: 2px 8px;
    font-size: 8pt; font-weight: 800; }
QLabel#ZahlungSchritt { color: #94a3b8; font-size: 8.5pt; }
QLabel#ZahlungUeberschrift { color: #0f172a; font-size: 15pt; font-weight: 800; }
QLabel { color: #334155; }
QLabel#ZahlungKlein { color: #64748b; font-size: 8.5pt; }
QLabel#ZahlungFehler { color: #dc2626; font-size: 8.5pt; }
QLabel#ZahlungBetrag { color: #0f172a; font-size: 22pt; font-weight: 900; }
QFrame#ZahlungBox { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 10px; }
QLineEdit { background: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 8px; padding: 8px 10px;
    font-size: 11pt; }
QLineEdit:focus { border: 2px solid #f59e0b; padding: 7px 9px; }
QLineEdit:read-only { background: #f1f5f9; color: #475569; }
QRadioButton { color: #0f172a; font-weight: 600; spacing: 10px; }
QCheckBox { color: #334155; spacing: 8px; }
QPushButton { background: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 18px; padding: 8px 18px;
    font-weight: 600; }
QPushButton:hover { background: #f1f5f9; }
QPushButton#ZahlungWeiter { background: #f59e0b; color: #0f1b33; border: none; font-weight: 800; padding: 9px 24px; }
QPushButton#ZahlungWeiter:hover { background: #fbbf24; }
QPushButton#ZahlungWeiter:disabled { background: #fde7b0; color: #8a7a55; }
QProgressBar { background: #e2e8f0; border: none; border-radius: 5px; height: 10px; }
QProgressBar::chunk { background: #f59e0b; border-radius: 5px; }
)";

QFrame* box(QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("ZahlungBox"));
    return f;
}

QLabel* ueberschrift(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("ZahlungUeberschrift"));
    l->setWordWrap(true);
    return l;
}

QLabel* klein(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("ZahlungKlein"));
    l->setWordWrap(true);
    return l;
}

/// Zeile "Bezeichnung ........ Wert"
QHBoxLayout* zeile(const QString& links, QLabel* rechts)
{
    auto* h = new QHBoxLayout;
    h->addWidget(new QLabel(links));
    h->addStretch(1);
    rechts->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(rechts);
    return h;
}

QPixmap haken(int g, qreal dpr)
{
    QPixmap pm(QSize(g, g) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(22, 163, 74));
    p.drawEllipse(QRectF(0, 0, g, g));
    QPen stift(Qt::white, g * 0.09, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(stift);
    p.drawPolyline(QPolygonF{QPointF(g * 0.28, g * 0.52), QPointF(g * 0.44, g * 0.67), QPointF(g * 0.73, g * 0.36)});
    return pm;
}

} // namespace

ZahlungDialog::ZahlungDialog(const QString& kontoName, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("Zahlung"));
    setWindowTitle(tr("SolarMonitor Pay – Premium abschließen"));
    setStyleSheet(QString::fromUtf8(kStil));
    setModal(true);
    setFixedWidth(520);

    // --- Kopf: Marke + Testmodus-Hinweis ------------------------------------
    auto* kopf = new QWidget(this);
    kopf->setObjectName(QStringLiteral("ZahlungKopf"));
    kopf->setAttribute(Qt::WA_StyledBackground, true);
    auto* logo = new QLabel(kopf);
    logo->setPixmap(Stil::appIcon().pixmap(36, 36));
    auto* marke = new QLabel(QStringLiteral("SolarMonitor Pay"), kopf);
    marke->setObjectName(QStringLiteral("ZahlungMarke"));
    auto* zusatz = new QLabel(tr("Die Spaßkasse – hier fließt kein echtes Geld"), kopf);
    zusatz->setObjectName(QStringLiteral("ZahlungMarkeZusatz"));
    auto* test = new QLabel(tr("TESTMODUS"), kopf);
    test->setObjectName(QStringLiteral("ZahlungTest"));
    auto* markeV = new QVBoxLayout;
    markeV->setSpacing(0);
    markeV->addWidget(marke);
    markeV->addWidget(zusatz);
    auto* kh = new QHBoxLayout(kopf);
    kh->setContentsMargins(20, 14, 20, 14);
    kh->setSpacing(12);
    kh->addWidget(logo);
    kh->addLayout(markeV, 1);
    kh->addWidget(test, 0, Qt::AlignTop);

    // --- Seiten -----------------------------------------------------------------
    m_schrittText = new QLabel(this);
    m_schrittText->setObjectName(QStringLiteral("ZahlungSchritt"));
    m_seiten = new QStackedWidget(this);
    m_seiten->addWidget(baueBestellung());
    m_seiten->addWidget(baueAnmelden(kontoName));
    m_seiten->addWidget(baueZahlart());
    m_seiten->addWidget(bauePruefen());
    m_seiten->addWidget(baueVerarbeitung());
    m_seiten->addWidget(baueFertig());

    // --- Fußzeile -------------------------------------------------------------
    m_zurueck = new QPushButton(tr("Zurück"), this);
    m_zurueck->setObjectName(QStringLiteral("ZahlungZurueck"));
    m_abbrechen = new QPushButton(tr("Abbrechen"), this);
    m_abbrechen->setObjectName(QStringLiteral("ZahlungAbbrechen"));
    m_weiter = new QPushButton(this);
    m_weiter->setObjectName(QStringLiteral("ZahlungWeiter"));
    m_weiter->setDefault(true);
    connect(m_zurueck, &QPushButton::clicked, this, &ZahlungDialog::zurueck);
    connect(m_abbrechen, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_weiter, &QPushButton::clicked, this, &ZahlungDialog::weiter);
    auto* fuss = new QHBoxLayout;
    fuss->setContentsMargins(24, 0, 24, 20);
    fuss->addWidget(m_zurueck);
    fuss->addStretch(1);
    fuss->addWidget(m_abbrechen);
    fuss->addWidget(m_weiter);

    auto* inhalt = new QVBoxLayout;
    inhalt->setContentsMargins(24, 16, 24, 16);
    inhalt->setSpacing(6);
    inhalt->addWidget(m_schrittText);
    inhalt->addWidget(m_seiten, 1);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(kopf);
    v->addLayout(inhalt, 1);
    v->addLayout(fuss);

    m_takt = new QTimer(this);
    m_takt->setInterval(40);
    connect(m_takt, &QTimer::timeout, this, [this] {
        m_verarbeitet += m_takt->interval();
        const int prozent = qMin(100, m_verarbeitet * 100 / qMax(1, verarbeitungMs));
        m_fortschritt->setValue(prozent);
        static const char* const schritte[] = {
            QT_TR_NOOP("Verbinde mit der Sonne …"),
            QT_TR_NOOP("Prüfe die Wolkenlage über dem Bodensee …"),
            QT_TR_NOOP("Bucht 9,99 € ab …"),
            QT_TR_NOOP("Entsorgt die Werbung fachgerecht …"),
        };
        m_status->setText(tr(schritte[qMin(3, prozent / 25)]));
        if (prozent >= 100) {
            m_takt->stop();
            geheZu(Fertig);
        }
    });

    geheZu(Bestellung);
}

int ZahlungDialog::schritt() const { return m_seiten->currentIndex(); }

QWidget* ZahlungDialog::baueBestellung()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);
    v->addWidget(ueberschrift(tr("Ihre Bestellung"), w));

    auto* b = box(w);
    auto* bv = new QVBoxLayout(b);
    bv->setContentsMargins(16, 14, 16, 14);
    bv->setSpacing(6);
    auto* produkt = new QLabel(tr("<b>SolarMonitor Premium</b> (werbefrei)"), b);
    bv->addWidget(produkt);
    for (const QString& vorteil : {tr("Keine Werbebanner mehr"), tr("Keine Popups im Minutentakt"),
                                   tr("Goldenes PREMIUM-Abzeichen im Kopf"), tr("Jederzeit im Kontomenü kündbar")})
        bv->addWidget(new QLabel(QStringLiteral("✔  ") + vorteil, b));
    v->addWidget(b);

    auto* summe = box(w);
    auto* sv = new QVBoxLayout(summe);
    sv->setContentsMargins(16, 12, 16, 12);
    sv->addLayout(zeile(tr("Monatlicher Preis"), new QLabel(tr("9,99 €"))));
    sv->addLayout(zeile(tr("Einrichtungsgebühr"), new QLabel(tr("0,00 €"))));
    auto* betrag = new QLabel(tr("9,99 €"));
    betrag->setObjectName(QStringLiteral("ZahlungBetrag"));
    sv->addLayout(zeile(tr("Heute fällig"), betrag));
    v->addWidget(summe);
    v->addStretch(1);
    return w;
}

QWidget* ZahlungDialog::baueAnmelden(const QString& kontoName)
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    v->addWidget(ueberschrift(tr("Bei SolarMonitor Pay anmelden"), w));
    v->addWidget(klein(tr("Testmodus: Jede vierstellige Sonnen-PIN funktioniert. Bitte keine echte PIN und kein "
                          "echtes Passwort eingeben – die Eingabe wird weder gespeichert noch gesendet."),
                       w));

    v->addWidget(new QLabel(tr("Konto"), w));
    auto* konto = new QLineEdit(kontoName, w);
    konto->setReadOnly(true);
    v->addWidget(konto);

    v->addWidget(new QLabel(tr("Sonnen-PIN (4 Ziffern)"), w));
    m_pin = new QLineEdit(w);
    m_pin->setObjectName(QStringLiteral("ZahlungPin"));
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setPlaceholderText(QStringLiteral("••••"));
    m_pin->setMaxLength(4);
    m_pin->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,4}")), m_pin));
    v->addWidget(m_pin);
    m_pinFehler = new QLabel(w);
    m_pinFehler->setObjectName(QStringLiteral("ZahlungFehler"));
    v->addWidget(m_pinFehler);
    connect(m_pin, &QLineEdit::textChanged, m_pinFehler, &QLabel::clear);
    v->addStretch(1);
    return w;
}

QWidget* ZahlungDialog::baueZahlart()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    v->addWidget(ueberschrift(tr("Zahlart wählen"), w));

    m_zahlarten = new QButtonGroup(this);
    const QString arten[][2] = {
        {tr("Sonnenstunden-Guthaben"), tr("1.234 kWh verfügbar · wird nicht belastet")},
        {tr("Kilowattkekse"), tr("3 Stück in der Dose · bleiben alle drin")},
        {tr("Ein freundliches Lächeln"), tr("unbegrenzt verfügbar")},
    };
    int nr = 0;
    for (const auto& art : arten) {
        auto* b = box(w);
        auto* bh = new QVBoxLayout(b);
        bh->setContentsMargins(14, 10, 14, 10);
        bh->setSpacing(2);
        auto* r = new QRadioButton(art[0], b);
        r->setObjectName(QStringLiteral("ZahlungArt%1").arg(nr));
        auto* d = klein(art[1], b);
        d->setContentsMargins(28, 0, 0, 0);
        bh->addWidget(r);
        bh->addWidget(d);
        m_zahlarten->addButton(r, nr++);
        v->addWidget(b);
    }
    m_zahlarten->button(0)->setChecked(true);
    v->addStretch(1);
    return w;
}

QWidget* ZahlungDialog::bauePruefen()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);
    v->addWidget(ueberschrift(tr("Prüfen und abonnieren"), w));

    auto* b = box(w);
    auto* bv = new QVBoxLayout(b);
    bv->setContentsMargins(16, 12, 16, 12);
    bv->setSpacing(6);
    bv->addLayout(zeile(tr("Produkt"), new QLabel(tr("SolarMonitor Premium"))));
    m_pruefZahlart = new QLabel;
    bv->addLayout(zeile(tr("Zahlart"), m_pruefZahlart));
    bv->addLayout(zeile(tr("Laufzeit"), new QLabel(tr("monatlich, jederzeit kündbar"))));
    auto* betrag = new QLabel(tr("9,99 €"));
    betrag->setObjectName(QStringLiteral("ZahlungBetrag"));
    bv->addLayout(zeile(tr("Betrag"), betrag));
    v->addWidget(b);

    m_agb = new QCheckBox(tr("Ich akzeptiere die AGB (3.874 Seiten, sinngemäß: „Werbung weg, alles gut“)."), w);
    m_agb->setObjectName(QStringLiteral("ZahlungAgb"));
    connect(m_agb, &QCheckBox::toggled, this, [this](bool an) {
        if (schritt() == Pruefen)
            m_weiter->setEnabled(an);
    });
    v->addWidget(m_agb);
    v->addStretch(1);
    return w;
}

QWidget* ZahlungDialog::baueVerarbeitung()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 24, 0, 0);
    v->setSpacing(14);
    v->addWidget(ueberschrift(tr("Zahlung wird verarbeitet …"), w));
    m_fortschritt = new QProgressBar(w);
    m_fortschritt->setRange(0, 100);
    m_fortschritt->setTextVisible(false);
    m_fortschritt->setFixedHeight(10);
    v->addWidget(m_fortschritt);
    m_status = new QLabel(w);
    v->addWidget(m_status);
    v->addWidget(klein(tr("Bitte das Fenster nicht schließen. (Es würde aber auch nichts passieren.)"), w));
    v->addStretch(1);
    return w;
}

QWidget* ZahlungDialog::baueFertig()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 8, 0, 0);
    v->setSpacing(10);
    auto* bild = new QLabel(w);
    bild->setPixmap(haken(64, devicePixelRatioF()));
    v->addWidget(bild, 0, Qt::AlignHCenter);
    auto* titel = ueberschrift(tr("Zahlung erfolgreich!"), w);
    titel->setAlignment(Qt::AlignHCenter);
    v->addWidget(titel);
    auto* b = box(w);
    auto* bv = new QVBoxLayout(b);
    bv->setContentsMargins(16, 12, 16, 12);
    m_quittung = new QLabel(b);
    m_quittung->setWordWrap(true);
    m_quittung->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bv->addWidget(m_quittung);
    v->addWidget(b);
    v->addWidget(klein(tr("Eine Quittung haben wir Ihnen nicht geschickt – es gibt ja auch nichts zu quittieren."), w));
    v->addStretch(1);
    return w;
}

QString ZahlungDialog::zahlartText() const
{
    const QAbstractButton* b = m_zahlarten ? m_zahlarten->checkedButton() : nullptr;
    return b ? b->text() : QString();
}

void ZahlungDialog::geheZu(int schritt)
{
    m_seiten->setCurrentIndex(schritt);
    const bool laeuft = schritt == Verarbeitung;
    const bool fertig = schritt == Fertig;
    m_zurueck->setVisible(schritt > Bestellung && schritt < Verarbeitung);
    m_abbrechen->setVisible(!fertig);
    m_abbrechen->setEnabled(!laeuft);
    m_weiter->setVisible(!laeuft);
    m_weiter->setEnabled(true);
    if (schritt < Verarbeitung)
        m_schrittText->setText(tr("Schritt %1 von 4").arg(schritt + 1));
    else
        m_schrittText->clear();

    switch (schritt) {
    case Bestellung:
        m_weiter->setText(tr("Weiter mit SolarMonitor Pay"));
        break;
    case Anmelden:
        m_weiter->setText(tr("Anmelden"));
        m_pin->setFocus();
        break;
    case Zahlart:
        m_weiter->setText(tr("Weiter"));
        break;
    case Pruefen:
        m_pruefZahlart->setText(zahlartText());
        m_weiter->setText(tr("Jetzt abonnieren · 9,99 €"));
        m_weiter->setEnabled(m_agb->isChecked());
        break;
    case Verarbeitung:
        starteVerarbeitung();
        break;
    case Fertig: {
        const QString nr = QStringLiteral("SMP-%1-%2")
                               .arg(QDate::currentDate().year())
                               .arg(QRandomGenerator::global()->bounded(100000, 999999));
        m_quittung->setText(tr("<b>SolarMonitor Premium</b> ist aktiv.<br>Transaktion: %1<br>Betrag: 9,99 € · "
                               "Zahlart: %2<br>Datum: %3")
                                .arg(nr, zahlartText().toHtmlEscaped(),
                                     QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat)));
        m_weiter->setText(tr("Fertig"));
        break;
    }
    }
}

void ZahlungDialog::weiter()
{
    switch (schritt()) {
    case Anmelden:
        if (m_pin->text().size() != 4) {
            m_pinFehler->setText(tr("Bitte eine vierstellige Sonnen-PIN eingeben (z. B. 1234)."));
            m_pin->setFocus();
            return;
        }
        m_pin->clear(); // nichts aufbewahren
        geheZu(Zahlart);
        return;
    case Pruefen:
        if (!m_agb->isChecked())
            return;
        geheZu(Verarbeitung);
        return;
    case Fertig:
        accept();
        return;
    default:
        geheZu(schritt() + 1);
    }
}

void ZahlungDialog::zurueck()
{
    if (schritt() > Bestellung && schritt() < Verarbeitung)
        geheZu(schritt() - 1);
}

void ZahlungDialog::starteVerarbeitung()
{
    m_verarbeitet = 0;
    m_fortschritt->setValue(0);
    m_status->setText(tr("Verbinde mit der Sonne …"));
    m_takt->start();
}
