#include "anlage_info.h"

#include <algorithm>
#include <cmath>

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "util/anzeige.h"

namespace {

/// Farbiger Punkt vor einem Text (wie die Statusspalten der Tabellen).
QString mitPunkt(const QColor& farbe, const QString& text)
{
    return QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp;%2").arg(farbe.name(), text.toHtmlEscaped());
}

} // namespace

AnlageInfo::AnlageInfo(const AppKontext& kontext, QWidget* parent)
    : QWidget(parent)
    , m_kontext(kontext)
{
    setObjectName(QStringLiteral("Detailbereich"));
    setAttribute(Qt::WA_StyledBackground, true);

    // Seite 0: nichts markiert
    auto* leer = new QLabel(tr("Wählen Sie eine Anlage aus, um hier ihre Angaben zu sehen."));
    leer->setObjectName(QStringLiteral("Leerzustand"));
    leer->setAlignment(Qt::AlignCenter);
    leer->setWordWrap(true);

    // Seite 1: Angaben zur Anlage
    auto* inhalt = new QWidget;
    auto* v = new QVBoxLayout(inhalt);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(10);

    m_titel = new QLabel(inhalt);
    m_titel->setObjectName(QStringLiteral("DetailTitel"));
    m_untertitel = new QLabel(inhalt);
    m_untertitel->setObjectName(QStringLiteral("Gedimmt"));
    v->addWidget(m_titel);
    v->addWidget(m_untertitel);

    auto neueGruppe = [&](const QString& titel) {
        auto* g = new QGroupBox(titel, inhalt);
        auto* f = new QFormLayout(g);
        f->setLabelAlignment(Qt::AlignLeft);
        f->setHorizontalSpacing(12);
        f->setVerticalSpacing(4);
        v->addWidget(g);
        return f;
    };

    QFormLayout* stamm = neueGruppe(tr("Stammdaten"));
    m_status = zeile(stamm, tr("Status:"));
    m_typ = zeile(stamm, tr("Typ:"));
    m_leistung = zeile(stamm, tr("Leistung:"));
    m_ausrichtung = zeile(stamm, tr("Ausrichtung/Neigung:"));
    m_wr = zeile(stamm, tr("Wechselrichter:"));
    m_station = zeile(stamm, tr("Wetterstation:"));
    m_inbetrieb = zeile(stamm, tr("Inbetriebnahme:"));

    QFormLayout* aus = neueGruppe(tr("Auswertung"));
    m_energie = zeile(aus, tr("Energieertrag:"));
    m_spez = zeile(aus, tr("Spez. Ertrag:"));
    m_pr = zeile(aus, tr("Performance Ratio:"));
    m_verlust = zeile(aus, tr("Ertragsverlust:"));
    m_fehlerbilder = zeile(aus, tr("Fehlerbilder:"));

    QFormLayout* st = neueGruppe(tr("Störungen"));
    m_offen = zeile(st, tr("Offen:"));
    m_letzte = zeile(st, tr("Letzte Meldung:"));

    m_details = new QPushButton(tr("Details …"), inhalt);
    m_bearbeiten = new QPushButton(tr("Bearbeiten …"), inhalt);
    m_bearbeiten->setEnabled(m_kontext.darf(Recht::StammdatenBearbeiten));
    connect(m_details, &QPushButton::clicked, this, [this] { emit detailsGewuenscht(m_anlageId); });
    connect(m_bearbeiten, &QPushButton::clicked, this, [this] { emit bearbeitenGewuenscht(m_anlageId); });
    auto* knoepfe = new QHBoxLayout;
    knoepfe->addStretch();
    knoepfe->addWidget(m_details);
    knoepfe->addWidget(m_bearbeiten);
    v->addLayout(knoepfe);
    v->addStretch();

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inhalt);
    scroll->viewport()->setAutoFillBackground(false);
    inhalt->setAutoFillBackground(false);

    m_seiten = new QStackedWidget(this);
    m_seiten->addWidget(leer);
    m_seiten->addWidget(scroll);
    auto* aussen = new QVBoxLayout(this);
    aussen->setContentsMargins(0, 0, 0, 0);
    aussen->addWidget(m_seiten);

    // aktuell halten, wenn sich Daten oder Auswertung ändern
    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, [this](const QString& id) {
        if (id == m_anlageId)
            aktualisiere();
    });
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, [this](const QString& id) {
        if (id == m_anlageId || id.isEmpty())
            aktualisiere();
    });
    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &AnlageInfo::aktualisiere);
    connect(m_kontext.analyse, &FlottenAnalyse::kennzahlGeaendert, this, [this](const QString& id) {
        if (id == m_anlageId)
            aktualisiere();
    });
    aktualisiere();
}

QLabel* AnlageInfo::zeile(QFormLayout* form, const QString& titel)
{
    auto* wert = new QLabel;
    wert->setTextInteractionFlags(Qt::TextSelectableByMouse);
    wert->setWordWrap(true);
    form->addRow(titel, wert);
    return wert;
}

void AnlageInfo::zeige(const QString& anlageId)
{
    if (anlageId == m_anlageId)
        return;
    m_anlageId = anlageId;
    aktualisiere();
}

void AnlageInfo::aktualisiere()
{
    const Anlage* a = m_kontext.daten->anlage(m_anlageId);
    m_seiten->setCurrentIndex(a ? 1 : 0);
    if (!a)
        return;

    m_titel->setText(tr("Anlage %1").arg(a->anlageId));
    m_untertitel->setText(QStringLiteral("%1, %2").arg(a->ort, Anzeige::typName(a->typ)));

    m_status->setText(mitPunkt(Anzeige::statusFarbe(a->status), Anzeige::statusName(a->status)));
    m_typ->setText(Anzeige::typName(a->typ));
    m_leistung->setText(Anzeige::zahl(a->leistungKwp, 2) + QStringLiteral(" kWp"));
    m_ausrichtung->setText(QStringLiteral("%1° / %2°").arg(a->ausrichtungGrad).arg(a->neigungGrad));
    const Wechselrichter* wr = m_kontext.daten->wechselrichter(a->wechselrichterId);
    m_wr->setText(wr ? QStringLiteral("%1 × %2").arg(a->anzahlWr).arg(wr->bezeichnung()) : a->wechselrichterId);
    const Wetterstation* ws = m_kontext.daten->wetterstation(a->wetterstationId);
    m_station->setText(ws ? QStringLiteral("%1 (%2)").arg(ws->stationId, ws->ort) : a->wetterstationId);
    m_inbetrieb->setText(Anzeige::datum(a->inbetriebnahme));

    // Auswertung aus der Flottenanalyse
    const Kennzahl* k = m_kontext.analyse->kennzahl(a->anlageId);
    const QString strich = QStringLiteral("–");
    if (!k || !k->ok) {
        const QString grund = !m_kontext.analyse->istFertig() ? tr("Auswertung läuft …")
                            : k                                ? k->fehler
                                                               : tr("keine Verlaufsdaten");
        for (QLabel* l : {m_energie, m_spez, m_pr, m_verlust})
            l->setText(strich);
        m_fehlerbilder->setText(grund);
    } else {
        const Auswertung& aw = k->auswertung;
        m_energie->setText(Anzeige::kwh(aw.energieKwh));
        m_spez->setText(Anzeige::zahl(aw.spezErtragKwhKwp, 1) + QStringLiteral(" kWh/kWp"));
        const QString median = std::isnan(k->medianPrStation) ? QString()
                             : tr(" (Station: %1)").arg(Anzeige::pr(k->medianPrStation));
        m_pr->setText(mitPunkt(Anzeige::prFarbe(aw.pr), Anzeige::pr(aw.pr) + median));
        m_verlust->setText(k->verlustEuro > 0.005
                               ? QStringLiteral("%1 (%2)").arg(Anzeige::euro(k->verlustEuro), Anzeige::kwh(k->verlustKwh))
                               : tr("keiner"));
        QStringList namen;
        for (const Anomalie& an : aw.anomalien)
            namen.append(Anomalie::typName(an.typ));
        namen.removeDuplicates();
        m_fehlerbilder->setText(namen.isEmpty() ? tr("keine") : namen.join(QStringLiteral(", ")));
    }

    // Störungen
    const int offen = m_kontext.daten->offeneStoerungen(a->anlageId);
    const int kritisch = m_kontext.daten->offeneKritischeStoerungen(a->anlageId);
    m_offen->setText(offen == 0 ? tr("keine")
                     : kritisch > 0 ? mitPunkt(QColor(196, 43, 28), tr("%1, davon %2 kritisch").arg(offen).arg(kritisch))
                                    : QString::number(offen));
    QVector<Stoerung> alle = m_kontext.daten->stoerungenVon(a->anlageId);
    if (alle.isEmpty()) {
        m_letzte->setText(strich);
    } else {
        const auto neueste = std::max_element(alle.cbegin(), alle.cend(), [](const Stoerung& x, const Stoerung& y) {
            return x.zeitpunkt < y.zeitpunkt;
        });
        m_letzte->setText(QStringLiteral("%1<br>%2 (%3)")
                              .arg(Anzeige::zeitpunkt(neueste->zeitpunkt),
                                   neueste->beschreibung.toHtmlEscaped(),
                                   Anzeige::stoerungsStatusName(neueste->status)));
    }
    m_bearbeiten->setEnabled(m_kontext.darf(Recht::StammdatenBearbeiten));
}
