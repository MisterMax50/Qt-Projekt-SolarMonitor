#include "login_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
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

    auto* logo = new QLabel(this);
    logo->setPixmap(Stil::appIcon().pixmap(72, 72));
    logo->setAlignment(Qt::AlignCenter);
    auto* titel = new QLabel(Stil::wortmarke(), this);
    titel->setObjectName(QStringLiteral("Seitentitel"));
    titel->setAlignment(Qt::AlignCenter);
    auto* untertitel = new QLabel(tr("Monitoring der PV-Flotte – bitte anmelden"), this);
    untertitel->setObjectName(QStringLiteral("Gedimmt"));
    untertitel->setAlignment(Qt::AlignCenter);
    auto* hinweis = new QLabel(tr("Demo-Zugänge: betrieb / 0000, service / 5678, admin / 1234"), this);
    hinweis->setObjectName(QStringLiteral("KpiNotiz"));
    hinweis->setAlignment(Qt::AlignCenter);

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
    form->addRow(tr("Rolle:"), m_rollen);
    form->addRow(tr("PIN:"), m_pin);

    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_anmelden = knoepfe->addButton(tr("Anmelden"), QDialogButtonBox::AcceptRole);
    m_anmelden->setDefault(true);
    connect(m_anmelden, &QPushButton::clicked, this, &LoginDialog::anmelden);
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(32, 24, 32, 20);
    v->setSpacing(10);
    v->addWidget(logo);
    v->addWidget(titel);
    v->addWidget(untertitel);
    v->addSpacing(8);
    v->addLayout(form);
    v->addWidget(m_meldung);
    v->addWidget(knoepfe);
    v->addSpacing(6);
    v->addWidget(hinweis);
    setMinimumWidth(400);

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
