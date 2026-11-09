#include "login_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
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

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Anmeldung – SolarMonitor"));

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
    auto* anmeldenText = new QLabel(tr("Melden Sie sich mit Ihrer Rolle und PIN an."), this);
    anmeldenText->setObjectName(QStringLiteral("Gedimmt"));
    auto* hinweis = new QLabel(tr("Demo-Zugänge: betrieb / 0000, service / 5678, admin / 1234"), this);
    hinweis->setObjectName(QStringLiteral("Gedimmt"));
    hinweis->setWordWrap(true);

    m_rollen = new QComboBox(this);
    for (Rolle r : {Rolle::Betrieb, Rolle::Service, Rolle::Admin})
        m_rollen->addItem(Rollen::anzeigename(r), int(r));
    m_rollen->setToolTip(tr("Betrieb: lesen und auswerten\n"
                            "Service: zusätzlich Störungen erfassen und bearbeiten\n"
                            "Administrator: zusätzlich Stammdaten bearbeiten und exportieren"));

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
    form->addRow(tr("Rolle:"), m_rollen);
    form->addRow(tr("PIN:"), m_pin);

    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_anmelden = knoepfe->addButton(tr("Anmelden"), QDialogButtonBox::AcceptRole);
    m_anmelden->setDefault(true);
    connect(m_anmelden, &QPushButton::clicked, this, &LoginDialog::anmelden);
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* marke = new QWidget(this);
    marke->setObjectName(QStringLiteral("DialogKopf"));
    marke->setAttribute(Qt::WA_StyledBackground, true);
    marke->setFixedWidth(250);
    auto* mv = new QVBoxLayout(marke);
    mv->setContentsMargins(28, 32, 28, 28);
    mv->setSpacing(10);
    mv->addWidget(logo);
    mv->addSpacing(6);
    mv->addWidget(titel);
    mv->addWidget(untertitel);
    mv->addStretch(1);

    auto* rechts = new QVBoxLayout;
    rechts->setContentsMargins(32, 32, 32, 24);
    rechts->setSpacing(10);
    rechts->addWidget(anmeldenTitel);
    rechts->addWidget(anmeldenText);
    rechts->addSpacing(8);
    rechts->addLayout(form);
    rechts->addWidget(m_meldung);
    rechts->addStretch(1);
    rechts->addWidget(hinweis);
    rechts->addSpacing(6);
    rechts->addWidget(knoepfe);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    h->addWidget(marke);
    h->addLayout(rechts, 1);
    setMinimumSize(700, 340);

    m_sperrTimer = new QTimer(this);
    m_sperrTimer->setInterval(1000);
    connect(m_sperrTimer, &QTimer::timeout, this, &LoginDialog::sperreTick);

    m_pin->setFocus();
}

void LoginDialog::anmelden()
{
    if (m_restSekunden > 0)
        return;

    const auto rolle = static_cast<Rolle>(m_rollen->currentData().toInt());
    if (Rollen::pinKorrekt(rolle, m_pin->text())) {
        m_rolle = rolle;
        accept();
        return;
    }

    ++m_fehlversuche;
    m_pin->clear();
    m_pin->setFocus();
    if (m_fehlversuche >= kMaxVersuche) {
        m_fehlversuche = 0;
        m_restSekunden = kSperreSekunden;
        m_anmelden->setEnabled(false);
        m_pin->setEnabled(false);
        sperreTick();
        m_sperrTimer->start();
    } else {
        m_meldung->setText(tr("PIN falsch (Versuch %1 von %2).").arg(m_fehlversuche).arg(kMaxVersuche));
    }
}

void LoginDialog::sperreTick()
{
    if (m_restSekunden <= 0) {
        m_sperrTimer->stop();
        m_anmelden->setEnabled(true);
        m_pin->setEnabled(true);
        m_pin->setFocus();
        m_meldung->clear();
        return;
    }
    m_meldung->setText(tr("Zu viele Fehlversuche – Anmeldung für %1 s gesperrt.").arg(m_restSekunden));
    --m_restSekunden;
}
