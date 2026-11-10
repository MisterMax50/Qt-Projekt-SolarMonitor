#include "login_seite.h"

#include <QSettings>

#include "model/benutzer.h"
#include "util/windows_hello.h"

#include <QComboBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "util/stil.h"

namespace {
constexpr int kMaxVersuche = 3;
constexpr int kSperreSekunden = 30;
} // namespace

LoginSeite::LoginSeite(Benutzerverwaltung* benutzer, QWidget* parent)
    : QWidget(parent)
    , m_benutzer(benutzer)
{
    setObjectName(QStringLiteral("Anmeldeseite"));
    setAttribute(Qt::WA_StyledBackground, true);

    // Aufbau wie die Anmeldeseite eines Monitoring-Portals: links eine
    // dunkelblaue Markenfläche, rechts das Formular
    auto* logo = new QLabel(this);
    logo->setPixmap(Stil::appIcon().pixmap(56, 56));
    auto* titel = new QLabel(QStringLiteral("SolarMonitor"), this);
    titel->setObjectName(QStringLiteral("DialogKopfTitel"));
    auto* untertitel = new QLabel(tr("Überwachung und Service für die PV-Flotte rund um den Bodensee"), this);
    untertitel->setObjectName(QStringLiteral("DialogKopfText"));
    untertitel->setWordWrap(true);
    auto* anmeldenTitel = new QLabel(tr("Anmelden"), this);
    anmeldenTitel->setObjectName(QStringLiteral("AnmeldenTitel"));
    auto* anmeldenText = new QLabel(tr("Melden Sie sich mit Ihrem Benutzer und Ihrer PIN an."), this);
    anmeldenText->setObjectName(QStringLiteral("Gedimmt"));
    auto* hinweis = new QLabel(tr("Demo-Zugänge: betrieb / 0000, service / 5678, admin / 1234"), this);
    hinweis->setObjectName(QStringLiteral("Gedimmt"));
    hinweis->setWordWrap(true);

    m_benutzerWahl = new QComboBox(this);
    m_benutzerWahl->setToolTip(tr("Betrieb: lesen und auswerten\n"
                                  "Service: zusätzlich Störungen erfassen und bearbeiten\n"
                                  "Administrator: zusätzlich Stammdaten und Benutzer verwalten"));
    connect(m_benutzerWahl, &QComboBox::activated, this, [this] { m_pin->setFocus(); });
    connect(m_benutzerWahl, &QComboBox::currentIndexChanged, this, &LoginSeite::aktualisiereHello);

    m_pin = new QLineEdit(this);
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(8);
    m_pin->setPlaceholderText(tr("PIN"));

    m_meldung = new QLabel(this);
    m_meldung->setStyleSheet(QStringLiteral("color: #c0392b;"));
    m_meldung->setWordWrap(true);

    auto* form = new QFormLayout;
    form->setVerticalSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Benutzer:"), m_benutzerWahl);
    form->addRow(tr("PIN:"), m_pin);
    // Windows Hello: nur sichtbar, wenn für den gewählten Benutzer eingerichtet
    m_hello = new QPushButton(Stil::icon(Stil::Symbol::Fingerabdruck, Stil::text()), tr("Mit Windows Hello anmelden"), this);
    m_hello->setIconSize(QSize(20, 20));
    m_hello->setToolTip(tr("Anmeldung per Fingerabdruck, Gesichtserkennung oder Windows-PIN"));
    connect(m_hello, &QPushButton::clicked, this, &LoginSeite::anmeldenMitHello);
    m_hello->hide();
    form->addRow(QString(), m_hello);

    m_anmelden = new QPushButton(tr("Anmelden"), this);
    m_anmelden->setObjectName(QStringLiteral("Primaer"));
    m_abbrechen = new QPushButton(tr("Beenden"), this);
    connect(m_anmelden, &QPushButton::clicked, this, &LoginSeite::anmelden);
    connect(m_pin, &QLineEdit::returnPressed, this, &LoginSeite::anmelden);
    connect(m_abbrechen, &QPushButton::clicked, this, &LoginSeite::abgebrochen);
    auto* knoepfe = new QHBoxLayout;
    knoepfe->addStretch(1);
    knoepfe->addWidget(m_abbrechen);
    knoepfe->addWidget(m_anmelden);

    // Karte in der Mitte: links Markenfläche, rechts das Formular
    auto* karte = new QFrame(this);
    karte->setObjectName(QStringLiteral("Anmeldekarte"));
    karte->setFixedWidth(760);
    karte->setMinimumHeight(370);
    auto* schatten = new QGraphicsDropShadowEffect(karte);
    schatten->setBlurRadius(60);
    schatten->setOffset(0, 12);
    schatten->setColor(QColor(0, 0, 0, 140));
    karte->setGraphicsEffect(schatten);

    auto* marke = new QWidget(karte);
    marke->setObjectName(QStringLiteral("DialogKopf"));
    marke->setAttribute(Qt::WA_StyledBackground, true);
    marke->setFixedWidth(280);
    auto* mv = new QVBoxLayout(marke);
    mv->setContentsMargins(32, 36, 32, 32);
    mv->setSpacing(10);
    mv->addWidget(logo);
    mv->addSpacing(6);
    mv->addWidget(titel);
    mv->addWidget(untertitel);
    mv->addStretch(1);

    auto* rechts = new QVBoxLayout;
    rechts->setContentsMargins(36, 36, 36, 28);
    rechts->setSpacing(10);
    rechts->addWidget(anmeldenTitel);
    rechts->addWidget(anmeldenText);
    rechts->addSpacing(10);
    rechts->addLayout(form);
    rechts->addWidget(m_meldung);
    rechts->addStretch(1);
    rechts->addWidget(hinweis);
    rechts->addSpacing(6);
    rechts->addLayout(knoepfe);

    auto* h = new QHBoxLayout(karte);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    h->addWidget(marke);
    h->addLayout(rechts, 1);

    auto* raster = new QVBoxLayout(this);
    raster->addStretch(1);
    raster->addWidget(karte, 0, Qt::AlignHCenter);
    raster->addStretch(1);

    m_sperrTimer = new QTimer(this);
    m_sperrTimer->setInterval(1000);
    connect(m_sperrTimer, &QTimer::timeout, this, &LoginSeite::sperreTick);
}

void LoginSeite::fuelleBenutzer()
{
    // aktive Konten, zuletzt angemeldeter Benutzer vorausgewählt
    const QString zuletzt = QSettings().value(QStringLiteral("letzterBenutzer")).toString();
    m_benutzerWahl->clear();
    for (const Benutzer& b : m_benutzer->alle())
        if (b.aktiv)
            m_benutzerWahl->addItem(QStringLiteral("%1 – %2").arg(b.name, Rollen::anzeigename(b.rolle)), b.kennung);
    const int i = m_benutzerWahl->findData(zuletzt);
    m_benutzerWahl->setCurrentIndex(i >= 0 ? i : 0);
}

void LoginSeite::aktualisiereHello()
{
    const Benutzer* b = m_benutzer->finde(m_benutzerWahl->currentData().toString());
    const bool eingerichtet = b && !b->helloKonto.isEmpty()
        && b->helloKonto.compare(WindowsHello::windowsKonto(), Qt::CaseInsensitive) == 0;
    m_hello->setVisible(eingerichtet && WindowsHello::verfuegbar());
}

void LoginSeite::anmeldenMitHello()
{
    if (m_restSekunden > 0)
        return;
    const QString kennung = m_benutzerWahl->currentData().toString();
    const Benutzer* b = m_benutzer->finde(kennung);
    if (!b)
        return;
    QString fehler;
    const auto ergebnis = WindowsHello::pruefe(this, tr("Anmeldung bei SolarMonitor als %1").arg(b->name), &fehler);
    if (ergebnis == WindowsHello::Ergebnis::Bestaetigt
        && m_benutzer->anmeldenMitHello(kennung, WindowsHello::windowsKonto())) {
        m_fehlversuche = 0;
        m_pin->clear();
        QSettings().setValue(QStringLiteral("letzterBenutzer"), kennung);
        emit angemeldet(kennung);
        return;
    }
    if (ergebnis != WindowsHello::Ergebnis::Abgebrochen)
        m_meldung->setText(fehler.isEmpty() ? tr("Anmeldung mit Windows Hello nicht möglich.") : fehler);
    m_pin->setFocus();
}

void LoginSeite::vorbereiten(bool zurueckMoeglich)
{
    fuelleBenutzer();
    aktualisiereHello();
    m_abbrechen->setText(zurueckMoeglich ? tr("Zurück") : tr("Beenden"));
    m_pin->clear();
    if (m_restSekunden <= 0)
        m_meldung->clear();
    m_pin->setFocus();
}

void LoginSeite::anmelden()
{
    if (m_restSekunden > 0)
        return;

    const QString kennung = m_benutzerWahl->currentData().toString();
    if (m_benutzer->anmelden(kennung, m_pin->text())) {
        m_fehlversuche = 0;
        m_pin->clear();
        QSettings().setValue(QStringLiteral("letzterBenutzer"), kennung);
        emit angemeldet(kennung);
        return;
    }

    ++m_fehlversuche;
    m_pin->clear();
    m_pin->setFocus();
    if (m_fehlversuche >= kMaxVersuche) {
        m_fehlversuche = 0;
        m_restSekunden = kSperreSekunden;
        m_anmelden->setEnabled(false);
        m_hello->setEnabled(false);
        m_pin->setEnabled(false);
        sperreTick();
        m_sperrTimer->start();
    } else {
        m_meldung->setText(tr("PIN falsch (Versuch %1 von %2).").arg(m_fehlversuche).arg(kMaxVersuche));
    }
}

void LoginSeite::sperreTick()
{
    if (m_restSekunden <= 0) {
        m_sperrTimer->stop();
        m_anmelden->setEnabled(true);
        m_hello->setEnabled(true);
        m_pin->setEnabled(true);
        m_pin->setFocus();
        m_meldung->clear();
        return;
    }
    m_meldung->setText(tr("Zu viele Fehlversuche – Anmeldung für %1 s gesperrt.").arg(m_restSekunden));
    --m_restSekunden;
}
