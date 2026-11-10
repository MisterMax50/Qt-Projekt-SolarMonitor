#include "werbung.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "util/stil.h"

namespace {

struct Anzeige {
    Stil::Symbol symbol;
    QColor       farbe;
    const char*  titel;
    const char*  text;
    const char*  aktion;
};

// Alle Produkte sind frei erfunden.
const Anzeige kAnzeigen[] = {
    {Stil::Symbol::Sonne, QColor(245, 158, 11), QT_TRANSLATE_NOOP("WerbeBanner", "SonnenPutz 3000"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Saubere Module, null Schatten: der Reinigungsroboter, der nebenbei auch Tauben verscheucht."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Jetzt vorbestellen")},
    {Stil::Symbol::Karte, QColor(37, 99, 235), QT_TRANSLATE_NOOP("WerbeBanner", "Wolkenschieber Pro"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Nie wieder Ertragsverlust durch Wolken!* (*Funktioniert nicht bei Wetter.)"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Gratis testen")},
    {Stil::Symbol::Blitz, QColor(217, 119, 6), QT_TRANSLATE_NOOP("WerbeBanner", "Kilowattkekse"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Die Energiekekse für lange Wartungstouren. Jetzt mit 30 % mehr Watt pro Biss."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Zum Keks-Shop")},
    {Stil::Symbol::Werkzeug, QColor(13, 148, 136), QT_TRANSLATE_NOOP("WerbeBanner", "Seeweg-Express"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Mit dem Tretboot quer über den Bodensee zur nächsten Störung – Werkzeugkoffer schwimmt leider nicht."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Ticket sichern")},
    {Stil::Symbol::Statistik, QColor(124, 58, 237), QT_TRANSLATE_NOOP("WerbeBanner", "Das PV-Horoskop"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Was die Sterne über Ihre Performance Ratio sagen. Heute: Merkur rückläufig, Wechselrichter auch."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Horoskop lesen")},
    {Stil::Symbol::Euro, QColor(22, 163, 74), QT_TRANSLATE_NOOP("WerbeBanner", "Einspeise-Lotto"),
     QT_TRANSLATE_NOOP("WerbeBanner", "Tippen Sie Ihren nächsten Einspeisetarif! Hauptgewinn: 0,47 Cent mehr pro Kilowattstunde."),
     QT_TRANSLATE_NOOP("WerbeBanner", "Mitspielen")},
};
constexpr int kAnzahl = int(sizeof(kAnzeigen) / sizeof(kAnzeigen[0]));
constexpr int kWechselMs = 9000;

/// "Produktbild": Symbol auf einem Farbverlauf
QPixmap produktbild(const Anzeige& a, qreal dpr)
{
    const int g = 52;
    QPixmap pm(QSize(g, g) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient verlauf(0, 0, g, g);
    verlauf.setColorAt(0, a.farbe.lighter(130));
    verlauf.setColorAt(1, a.farbe.darker(115));
    p.setPen(Qt::NoPen);
    p.setBrush(verlauf);
    p.drawRoundedRect(QRectF(0, 0, g, g), 10, 10);
    Stil::icon(a.symbol, Qt::white).paint(&p, QRect(12, 12, g - 24, g - 24));
    p.end();
    return pm;
}

} // namespace

WerbeBanner::WerbeBanner(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Werbung"));
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Spaß-Werbung – mit SolarMonitor Premium verschwindet sie"));

    auto* hinweis = new QLabel(tr("ANZEIGE"), this);
    hinweis->setObjectName(QStringLiteral("WerbungHinweis"));
    m_bild = new QLabel(this);
    m_bild->setFixedSize(52, 52);
    m_titel = new QLabel(this);
    m_titel->setObjectName(QStringLiteral("WerbungTitel"));
    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("WerbungText"));
    m_text->setWordWrap(true);
    m_aktion = new QPushButton(this);
    m_aktion->setObjectName(QStringLiteral("WerbungAktion"));
    m_aktion->setCursor(Qt::PointingHandCursor);
    connect(m_aktion, &QPushButton::clicked, this, &WerbeBanner::angeklickt);

    auto* schliessen = new QToolButton(this);
    schliessen->setObjectName(QStringLiteral("WerbungSchliessen"));
    schliessen->setText(QStringLiteral("×"));
    schliessen->setToolTip(tr("Werbung entfernen"));
    connect(schliessen, &QToolButton::clicked, this, &WerbeBanner::angeklickt);

    auto* texte = new QVBoxLayout;
    texte->setSpacing(2);
    auto* kopf = new QHBoxLayout;
    kopf->setSpacing(8);
    kopf->addWidget(m_titel);
    kopf->addWidget(hinweis);
    kopf->addStretch(1);
    texte->addLayout(kopf);
    texte->addWidget(m_text);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(12, 10, 8, 10);
    h->setSpacing(14);
    h->addWidget(m_bild);
    h->addLayout(texte, 1);
    h->addWidget(m_aktion);
    h->addWidget(schliessen, 0, Qt::AlignTop);

    // mit einer zufälligen Anzeige beginnen, dann regelmäßig wechseln
    m_index = int(QRandomGenerator::global()->bounded(kAnzahl));
    zeige(m_index);
    m_wechsel = new QTimer(this);
    m_wechsel->setInterval(kWechselMs);
    connect(m_wechsel, &QTimer::timeout, this, [this] { zeige((m_index + 1) % kAnzahl); });
    m_wechsel->start();
}

void WerbeBanner::zeige(int index)
{
    m_index = index;
    const Anzeige& a = kAnzeigen[index];
    m_bild->setPixmap(produktbild(a, devicePixelRatioF()));
    m_titel->setText(tr(a.titel));
    m_text->setText(tr(a.text));
    m_aktion->setText(tr(a.aktion));
}

void WerbeBanner::angeklickt()
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
