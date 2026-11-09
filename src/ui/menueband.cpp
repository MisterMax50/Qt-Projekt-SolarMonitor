#include "menueband.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kGrossSymbol = 32;
constexpr int kKleinSymbol = 16;
} // namespace

Menueband::Menueband(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Menueband"));
    setAttribute(Qt::WA_StyledBackground, true);

    m_dateiMenue = new QMenu(this);
    m_datei = new QToolButton(this);
    m_datei->setObjectName(QStringLiteral("DateiKnopf"));
    m_datei->setText(tr("Datei"));
    m_datei->setMenu(m_dateiMenue);
    m_datei->setPopupMode(QToolButton::InstantPopup);
    m_datei->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_reiter = new QTabBar(this);
    m_reiter->setObjectName(QStringLiteral("BandReiter"));
    m_reiter->setDrawBase(false);
    m_reiter->setExpanding(false);
    m_reiter->setUsesScrollButtons(false);
    m_reiter->setElideMode(Qt::ElideNone);
    m_reiter->setFocusPolicy(Qt::NoFocus);
    m_reiter->setToolTip(tr("Doppelklick blendet das Menüband ein oder aus"));

    m_kopf = new QHBoxLayout;
    m_kopf->setContentsMargins(0, 0, 8, 0);
    m_kopf->setSpacing(4);
    m_kopf->addWidget(m_datei);
    m_kopf->addWidget(m_reiter);
    m_kopf->addStretch(1);

    m_flaechen = new QStackedWidget(this);
    m_flaechen->setObjectName(QStringLiteral("BandFlaeche"));

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addLayout(m_kopf);
    v->addWidget(m_flaechen);

    connect(m_reiter, &QTabBar::currentChanged, this, [this](int i) {
        if (i >= 0)
            m_flaechen->setCurrentIndex(i);
        if (istEingeklappt() && i >= 0)
            m_flaechen->show(); // Klick auf einen Reiter zeigt das Band wieder
    });
    connect(m_reiter, &QTabBar::tabBarDoubleClicked, this, [this] { klappen(); });
}

void Menueband::neuerReiter(const QString& titel)
{
    auto* flaeche = new QWidget;
    flaeche->setObjectName(QStringLiteral("BandSeite"));
    m_aktuelleFlaeche = new QHBoxLayout(flaeche);
    m_aktuelleFlaeche->setContentsMargins(4, 2, 4, 0);
    m_aktuelleFlaeche->setSpacing(0);
    m_aktuelleFlaeche->addStretch(1);
    m_flaechen->addWidget(flaeche);
    m_reiter->addTab(titel);
    m_aktuelleGruppe = nullptr;
}

void Menueband::neueGruppe(const QString& titel)
{
    if (!m_aktuelleFlaeche)
        return;
    const int vorDemStretch = m_aktuelleFlaeche->count() - 1;
    if (vorDemStretch > 0) {
        auto* trenner = new QFrame;
        trenner->setObjectName(QStringLiteral("BandTrenner"));
        trenner->setFrameShape(QFrame::VLine);
        trenner->setFixedWidth(1);
        m_aktuelleFlaeche->insertWidget(m_aktuelleFlaeche->count() - 1, trenner);
    }

    auto* gruppe = new QWidget;
    gruppe->setObjectName(QStringLiteral("BandGruppe"));
    auto* v = new QVBoxLayout(gruppe);
    v->setContentsMargins(4, 2, 4, 2);
    v->setSpacing(0);
    m_aktuelleGruppe = new QHBoxLayout;
    m_aktuelleGruppe->setSpacing(2);
    v->addLayout(m_aktuelleGruppe, 1);
    auto* name = new QLabel(titel, gruppe);
    name->setObjectName(QStringLiteral("BandGruppenTitel"));
    name->setAlignment(Qt::AlignCenter);
    v->addWidget(name);
    m_aktuelleFlaeche->insertWidget(m_aktuelleFlaeche->count() - 1, gruppe);
}

QToolButton* Menueband::gross(QAction* aktion, const QString& kurzText)
{
    auto* k = new QToolButton;
    k->setObjectName(QStringLiteral("BandGross"));
    k->setDefaultAction(aktion);
    k->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    k->setIconSize(QSize(kGrossSymbol, kGrossSymbol));
    k->setAutoRaise(true);
    k->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    if (!kurzText.isEmpty()) {
        // Der Knopf übernimmt bei jeder Änderung der Aktion deren Text –
        // danach wieder den kurzen Bandtext setzen.
        k->setText(kurzText);
        connect(aktion, &QAction::changed, k, [k, kurzText] { k->setText(kurzText); });
    }
    if (m_aktuelleGruppe)
        m_aktuelleGruppe->addWidget(k);
    return k;
}

QToolButton* Menueband::grossMitMenue(const QIcon& symbol, const QString& text, QMenu* menue)
{
    auto* k = new QToolButton;
    k->setObjectName(QStringLiteral("BandGross"));
    k->setIcon(symbol);
    k->setText(text);
    k->setMenu(menue);
    k->setPopupMode(QToolButton::InstantPopup);
    k->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    k->setIconSize(QSize(kGrossSymbol, kGrossSymbol));
    k->setAutoRaise(true);
    k->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    if (m_aktuelleGruppe)
        m_aktuelleGruppe->addWidget(k);
    return k;
}

void Menueband::klein(const QList<QAction*>& aktionen)
{
    if (!m_aktuelleGruppe)
        return;
    auto* stapel = new QVBoxLayout;
    stapel->setSpacing(0);
    for (QAction* a : aktionen.mid(0, 3)) {
        auto* k = new QToolButton;
        k->setObjectName(QStringLiteral("BandKlein"));
        k->setDefaultAction(a);
        k->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        k->setIconSize(QSize(kKleinSymbol, kKleinSymbol));
        k->setAutoRaise(true);
        k->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        stapel->addWidget(k);
    }
    stapel->addStretch(1);
    m_aktuelleGruppe->addLayout(stapel);
}

void Menueband::widget(QWidget* w)
{
    if (m_aktuelleGruppe)
        m_aktuelleGruppe->addWidget(w);
}

void Menueband::setzeRechts(QWidget* w)
{
    m_kopf->addWidget(w, 0, Qt::AlignVCenter);
}

void Menueband::zeigeReiter(int index)
{
    m_reiter->setCurrentIndex(index);
}

bool Menueband::istEingeklappt() const
{
    return m_flaechen->isHidden();
}

void Menueband::klappen()
{
    m_flaechen->setVisible(istEingeklappt());
}
