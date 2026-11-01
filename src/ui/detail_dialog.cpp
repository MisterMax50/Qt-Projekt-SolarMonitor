#include "detail_dialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "eval/schwellwerte.h"
#include "model/datenbasis.h"
#include "ui/balken_diagramm.h"
#include "ui/tabellen_hilfe.h"
#include "ui/verlauf_plot.h"
#include "util/anzeige.h"
#include "util/geo.h"
#include "util/stil.h"

namespace {
enum Reiter { ReiterStamm = 0, ReiterVerlauf = 1, ReiterAuswertung = 2, ReiterStoerungen = 3 };

QLabel* wertLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}
} // namespace

DetailDialog::DetailDialog(const QString& anlageId, const AppKontext& kontext, QWidget* parent)
    : QDialog(parent)
    , m_anlageId(anlageId)
    , m_kontext(kontext)
{
    setWindowTitle(tr("Anlage %1").arg(anlageId));
    resize(1000, 720);

    m_kopf = new QLabel(this);
    m_kopf->setTextFormat(Qt::RichText);
    m_statusChip = new QLabel(this);
    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    m_banner->hide();

    m_tabs = new QTabWidget(this);
    m_stammSeite = new QWidget(m_tabs);
    m_tabs->addTab(m_stammSeite, tr("Stammdaten"));
    m_tabs->addTab(baueVerlaufSeite(), tr("Verlauf"));
    m_tabs->addTab(baueAuswertungSeite(), tr("Ertrag && PR"));
    m_tabs->addTab(baueStoerungSeite(), tr("Störungen"));

    m_btnBearbeiten = new QPushButton(Stil::icon(Stil::Symbol::Bearbeiten, Stil::text()),
                                      tr("Stammdaten bearbeiten …"), this);
    m_btnBearbeiten->setEnabled(m_kontext.darf(Recht::StammdatenBearbeiten));
    if (!m_btnBearbeiten->isEnabled())
        m_btnBearbeiten->setToolTip(tr("Nur für die Rolle Administrator."));
    connect(m_btnBearbeiten, &QPushButton::clicked, this, [this] { emit bearbeitenGewuenscht(m_anlageId); });

    m_btnStoerung = new QPushButton(Stil::icon(Stil::Symbol::Plus, Stil::text()),
                                    tr("Störung erfassen …"), this);
    m_btnStoerung->setEnabled(m_kontext.darf(Recht::StoerungBearbeiten));
    if (!m_btnStoerung->isEnabled())
        m_btnStoerung->setToolTip(tr("Nur für die Rollen Service und Administrator."));
    connect(m_btnStoerung, &QPushButton::clicked, this, [this] { emit stoerungErfassenGewuenscht(m_anlageId); });

    m_btnBericht = new QPushButton(Stil::icon(Stil::Symbol::Pdf, Stil::text()), tr("PDF-Bericht …"), this);
    m_btnBericht->setEnabled(m_kontext.darf(Recht::BerichtExportieren));
    m_btnBericht->setToolTip(tr("Stammdaten, Kennzahlen, Diagramme, Fehlerbilder und Störungen als PDF speichern"));
    m_btnBericht->setAutoDefault(false);
    connect(m_btnBericht, &QPushButton::clicked, this, [this] { emit berichtGewuenscht(m_anlageId); });

    // Enter soll nicht versehentlich die Bearbeitung öffnen
    m_btnBearbeiten->setAutoDefault(false);
    m_btnStoerung->setAutoDefault(false);
    auto* schliessen = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(schliessen, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* knoepfe = new QHBoxLayout;
    knoepfe->addWidget(m_btnBearbeiten);
    knoepfe->addWidget(m_btnStoerung);
    knoepfe->addWidget(m_btnBericht);
    knoepfe->addStretch();
    knoepfe->addWidget(schliessen);

    auto* v = new QVBoxLayout(this);
    auto* kopfZeile = new QHBoxLayout;
    kopfZeile->addWidget(m_kopf, 1);
    kopfZeile->addWidget(m_statusChip, 0, Qt::AlignTop);
    v->addLayout(kopfZeile);
    v->addWidget(m_banner);
    v->addWidget(m_tabs, 1);
    v->addLayout(knoepfe);

    zeigeStammdaten();
    zeigeStoerungen();

    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, [this](const QString& id) {
        if (id == m_anlageId)
            zeigeStammdaten();
    });
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, [this](const QString& id) {
        if (id == m_anlageId)
            zeigeStoerungen();
    });

    // ---------- Lazy Loading des Verlaufs ----------
    if (!m_kontext.verlaeufe->hatVerlauf(m_anlageId)) {
        m_tabs->setTabEnabled(ReiterVerlauf, false);
        m_tabs->setTabEnabled(ReiterAuswertung, false);
        setzeBanner(tr("Kein Verlauf verfügbar – es werden nur die Stammdaten angezeigt. %1")
                        .arg(m_kontext.verlaeufe->fehlgrund(m_anlageId)),
                    false);
    } else {
        connect(m_kontext.verlaeufe, &VerlaufRepository::verlaufGeladen, this, &DetailDialog::verlaufGeladen);
        m_kontext.verlaeufe->ladeAsync(m_anlageId);
    }
}

void DetailDialog::setzeBanner(const QString& text, bool fehler)
{
    m_banner->setText(text);
    m_banner->setStyleSheet(fehler ? QStringLiteral("background:#fee2e2; color:#991b1b; padding:10px 12px; border-radius:8px; border:1px solid #fecaca;")
                                   : QStringLiteral("background:#fef3c7; color:#92400e; padding:10px 12px; border-radius:8px; border:1px solid #fde68a;"));
    m_banner->setVisible(!text.isEmpty());
}

// -------------------------------------------------------------------
//  Reiter 1: Stammdaten
// -------------------------------------------------------------------

void DetailDialog::zeigeStammdaten()
{
    const Anlage* a = m_kontext.daten->anlage(m_anlageId);
    if (!a) {
        m_kopf->setText(tr("<h2>%1</h2>Anlage nicht gefunden.").arg(m_anlageId));
        return;
    }
    m_kopf->setText(QStringLiteral("<span style='font-size:18pt; font-weight:600'>%1</span><br>"
                                   "<span style='color:%5'>%2 · %3 · %4 kWp</span>")
                        .arg(a->anlageId, Anzeige::typName(a->typ), a->ort, Anzeige::zahl(a->leistungKwp, 2),
                             Stil::textGedimmt().name()));
    const QColor sf = Anzeige::statusFarbe(a->status);
    m_statusChip->setText(QStringLiteral("● ") + Anzeige::statusName(a->status));
    m_statusChip->setStyleSheet(QStringLiteral("background: rgba(%1,%2,%3,40); color: %4; border-radius: 11px; "
                                               "padding: 4px 12px; font-weight: 600;")
                                    .arg(sf.red()).arg(sf.green()).arg(sf.blue())
                                    .arg((Stil::istDunkel() ? sf.lighter(140) : sf.darker(130)).name()));

    auto* seite = new QWidget;
    auto* grid = new QGridLayout(seite);

    // Anlage
    auto* gA = new QGroupBox(tr("Anlage"), seite);
    auto* fA = new QFormLayout(gA);
    fA->addRow(tr("Anlagen-ID:"), wertLabel(a->anlageId, gA));
    fA->addRow(tr("Typ:"), wertLabel(Anzeige::typName(a->typ), gA));
    fA->addRow(tr("Ort:"), wertLabel(a->ort, gA));
    fA->addRow(tr("Koordinaten:"), wertLabel(QStringLiteral("%1° N, %2° O").arg(Anzeige::zahl(a->lat, 5), Anzeige::zahl(a->lon, 5)), gA));
    fA->addRow(tr("Leistung:"), wertLabel(Anzeige::zahl(a->leistungKwp, 2) + QStringLiteral(" kWp"), gA));
    fA->addRow(tr("Ausrichtung / Neigung:"), wertLabel(QStringLiteral("%1° / %2°").arg(a->ausrichtungGrad).arg(a->neigungGrad), gA));
    fA->addRow(tr("Inbetriebnahme:"), wertLabel(Anzeige::datum(a->inbetriebnahme), gA));
    fA->addRow(tr("Einspeisevergütung:"), wertLabel(Anzeige::zahl(a->verguetungCtKwh, 2) + QStringLiteral(" ct/kWh"), gA));
    fA->addRow(tr("Status:"), wertLabel(Anzeige::statusName(a->status), gA));
    fA->addRow(tr("Offene Störungen:"), wertLabel(QString::number(m_kontext.daten->offeneStoerungen(a->anlageId)), gA));
    grid->addWidget(gA, 0, 0, 2, 1);

    // Wechselrichter
    auto* gW = new QGroupBox(tr("Wechselrichter"), seite);
    auto* fW = new QFormLayout(gW);
    if (const Wechselrichter* wr = m_kontext.daten->wechselrichter(a->wechselrichterId)) {
        fW->addRow(tr("Modell:"), wertLabel(QStringLiteral("%1 (%2)").arg(wr->bezeichnung(), wr->wechselrichterId), gW));
        fW->addRow(tr("Anzahl:"), wertLabel(QString::number(a->anzahlWr), gW));
        fW->addRow(tr("Nennleistung:"), wertLabel(tr("%1 kW je Gerät, %2 kW gesamt")
                                                      .arg(Anzeige::zahl(wr->nennleistungKw, 1),
                                                           Anzeige::zahl(wr->nennleistungKw * a->anzahlWr, 1)), gW));
        fW->addRow(tr("Max. Wirkungsgrad:"),
                   wertLabel(Anzeige::zahl(wr->maxWirkungsgradProzent, 1) + QStringLiteral(" %")
                                 + (wr->maxWirkungsgradProzent > 100.0 ? tr(" (unplausibel)") : QString()), gW));
        fW->addRow(tr("Phasen / MPP-Tracker:"), wertLabel(QStringLiteral("%1 / %2").arg(wr->phasen).arg(wr->mppTracker), gW));
        if (wr->nennleistungKw > 0 && a->anzahlWr > 0)
            fW->addRow(tr("DC/AC-Verhältnis:"), wertLabel(Anzeige::zahl(a->leistungKwp / (wr->nennleistungKw * a->anzahlWr), 2), gW));
    } else {
        fW->addRow(wertLabel(tr("Wechselrichter \"%1\" ist unbekannt.").arg(a->wechselrichterId), gW));
    }
    grid->addWidget(gW, 0, 1);

    // Wetterstation
    auto* gS = new QGroupBox(tr("Wetterstation"), seite);
    auto* fS = new QFormLayout(gS);
    if (const Wetterstation* ws = m_kontext.daten->wetterstation(a->wetterstationId)) {
        fS->addRow(tr("Station:"), wertLabel(QStringLiteral("%1 (%2)").arg(ws->name, ws->stationId), gS));
        fS->addRow(tr("Ort / Höhe:"), wertLabel(tr("%1, %2 m ü. NN").arg(ws->ort, Anzeige::zahl(ws->hoeheM, 0)), gS));
        fS->addRow(tr("Koordinaten:"), wertLabel(QStringLiteral("%1° N, %2° O").arg(Anzeige::zahl(ws->lat, 5), Anzeige::zahl(ws->lon, 5)), gS));
        if (a->hatGueltigeKoordinaten())
            fS->addRow(tr("Entfernung zur Anlage:"),
                       wertLabel(Anzeige::zahl(Geo::entfernungKm(a->lat, a->lon, ws->lat, ws->lon), 1) + QStringLiteral(" km"), gS));
        const double median = m_kontext.analyse->medianPrStation(ws->stationId);
        if (median > 0.0)
            fS->addRow(tr("Median-PR der Station:"), wertLabel(Anzeige::pr(median), gS));
    } else {
        fS->addRow(wertLabel(tr("Wetterstation \"%1\" ist unbekannt.").arg(a->wetterstationId), gS));
    }
    grid->addWidget(gS, 1, 1);

    // Prüfmeldungen
    auto* gP = new QGroupBox(tr("Datenprüfung"), seite);
    auto* vP = new QVBoxLayout(gP);
    auto* liste = new QListWidget(gP);
    liste->setWordWrap(true);
    for (const Pruefmeldung& m : m_kontext.daten->pruefmeldungenFuer(a->anlageId)) {
        const QIcon icon = m.stufe == Pruefmeldung::Stufe::Info ? Stil::icon(Stil::Symbol::Info, QColor(37, 99, 235))
                         : m.stufe == Pruefmeldung::Stufe::Fehler ? Stil::icon(Stil::Symbol::Fehler, QColor(220, 38, 38))
                                                                  : Stil::icon(Stil::Symbol::Warnung, QColor(217, 119, 6));
        liste->addItem(new QListWidgetItem(icon,
                                           m.zeile > 0 ? QStringLiteral("%1 (%2, Zeile %3)").arg(m.text, m.datei).arg(m.zeile)
                                                       : QStringLiteral("%1 (%2)").arg(m.text, m.datei)));
    }
    if (liste->count() == 0)
        liste->addItem(tr("Keine Auffälligkeiten in den Stammdaten."));
    vP->addWidget(liste);
    grid->addWidget(gP, 2, 0, 1, 2);
    grid->setRowStretch(2, 1);

    const int aktuell = m_tabs->currentIndex();
    const bool verlaufAktiv = m_tabs->isTabEnabled(ReiterVerlauf);
    m_tabs->removeTab(ReiterStamm);
    m_stammSeite->deleteLater();
    m_stammSeite = seite;
    m_tabs->insertTab(ReiterStamm, seite, tr("Stammdaten"));
    m_tabs->setTabEnabled(ReiterVerlauf, verlaufAktiv);
    m_tabs->setCurrentIndex(qMax(0, aktuell));
}

// -------------------------------------------------------------------
//  Reiter 2 und 3: Verlauf und Auswertung
// -------------------------------------------------------------------

QWidget* DetailDialog::baueVerlaufSeite()
{
    auto* seite = new QWidget;
    m_verlaufInfo = new QLabel(tr("Verlauf wird im Hintergrund geladen …"), seite);
    m_plotLeistung = new VerlaufPlot(seite);
    m_plotEinstrahlung = new VerlaufPlot(seite);
    m_plotTemperatur = new VerlaufPlot(seite);
    for (VerlaufPlot* p : {m_plotLeistung, m_plotEinstrahlung, m_plotTemperatur})
        p->leeren(tr("Wird geladen …"));

    auto* split = new QSplitter(Qt::Vertical, seite);
    split->addWidget(m_plotLeistung);
    split->addWidget(m_plotEinstrahlung);
    split->addWidget(m_plotTemperatur);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 3);
    split->setStretchFactor(2, 2);

    auto* v = new QVBoxLayout(seite);
    v->addWidget(m_verlaufInfo);
    v->addWidget(split, 1);
    return seite;
}

QWidget* DetailDialog::baueAuswertungSeite()
{
    auto* seite = new QWidget;
    m_auswertungHinweis = new QLabel(tr("Wird berechnet, sobald der Verlauf geladen ist …"), seite);
    m_auswertungHinweis->setObjectName(QStringLiteral("Gedimmt"));
    auto* kacheln = new QGridLayout;
    kacheln->setSpacing(10);
    const QStringList titel{tr("Energieertrag (14 Tage)"), tr("Spezifischer Ertrag"), tr("Performance Ratio"),
                            tr("Datenabdeckung"), tr("Erlös"), tr("Geschätzter Verlust")};
    for (int i = 0; i < titel.size(); ++i) {
        auto* karte = new QFrame(seite);
        karte->setObjectName(QStringLiteral("Karte"));
        auto* kv = new QVBoxLayout(karte);
        kv->setContentsMargins(14, 10, 14, 10);
        kv->setSpacing(2);
        auto* t = new QLabel(titel.at(i), karte);
        t->setObjectName(QStringLiteral("KpiTitel"));
        auto* w = new QLabel(QStringLiteral("–"), karte);
        w->setObjectName(QStringLiteral("KpiWert"));
        w->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* n = new QLabel(karte);
        n->setObjectName(QStringLiteral("KpiNotiz"));
        kv->addWidget(t);
        kv->addWidget(w);
        kv->addWidget(n);
        m_kpiWerte.append(w);
        m_kpiNotizen.append(n);
        kacheln->addWidget(karte, 0, i);
    }
    m_tagesDiagramm = new BalkenDiagramm(seite);
    m_tagesDiagramm->setToolTip(tr("Balkenfarbe = Tages-PR (rot niedrig, grün hoch)"));

    m_tagesTabelle = new QTableWidget(seite);
    TabellenHilfe::einrichten(m_tagesTabelle, {tr("Tag"), tr("Ertrag"), tr("Einstrahlung (kWh/m²)"), tr("PR"), tr("Lücken")});
    m_tagesTabelle->setSortingEnabled(true);

    m_anomalien = new QListWidget(seite);
    m_anomalien->setWordWrap(true);

    auto* gTabelle = new QGroupBox(tr("Tageserträge"), seite);
    (new QVBoxLayout(gTabelle))->addWidget(m_tagesTabelle);
    auto* gAnom = new QGroupBox(tr("Erkannte Fehlerbilder und Hinweise"), seite);
    (new QVBoxLayout(gAnom))->addWidget(m_anomalien);

    auto* unten = new QSplitter(Qt::Horizontal, seite);
    unten->addWidget(gTabelle);
    unten->addWidget(gAnom);
    unten->setStretchFactor(0, 3);
    unten->setStretchFactor(1, 2);

    auto* split = new QSplitter(Qt::Vertical, seite);
    split->addWidget(m_tagesDiagramm);
    split->addWidget(unten);

    auto* v = new QVBoxLayout(seite);
    v->addWidget(m_auswertungHinweis);
    v->addLayout(kacheln);
    v->addWidget(split, 1);
    return seite;
}

void DetailDialog::verlaufGeladen(const QString& anlageId, const VerlaufLadeErgebnis& e)
{
    if (anlageId != m_anlageId)
        return;
    disconnect(m_kontext.verlaeufe, &VerlaufRepository::verlaufGeladen, this, &DetailDialog::verlaufGeladen);

    if (!e.ok()) {
        m_tabs->setTabEnabled(ReiterVerlauf, false);
        m_tabs->setTabEnabled(ReiterAuswertung, false);
        setzeBanner(tr("Der Verlauf konnte nicht geladen werden – Verlauf und Auswertung sind "
                       "nicht verfügbar.\nGrund: %1").arg(e.fehler),
                    true);
        return;
    }

    const Verlauf& v = *e.verlauf;
    m_verlaufInfo->setText(tr("%1 Messpunkte im 15-Minuten-Raster, %2 – %3. Rot hinterlegt: Datenlücken.")
                               .arg(v.anzahl())
                               .arg(Anzeige::zeitpunkt(v.zeit.first()), Anzeige::zeitpunkt(v.zeit.last())));
    m_plotLeistung->setDaten(v.zeit, v.leistungW, tr("Eingespeiste Leistung"), QStringLiteral("W"),
                             QColor(22, 119, 255), true);
    m_plotEinstrahlung->setDaten(v.zeit, v.einstrahlungWm2, tr("Einstrahlung in Modulebene"),
                                 QStringLiteral("W/m²"), QColor(230, 140, 20));
    m_plotTemperatur->setDaten(v.zeit, v.modultemperaturC, tr("Modultemperatur"), QStringLiteral("°C"),
                               QColor(200, 60, 60));
    zeigeAuswertung(v, e.hinweise);
}

void DetailDialog::zeigeAuswertung(const Verlauf& verlauf, const QStringList& dateiHinweise)
{
    const Anlage* a = m_kontext.daten->anlage(m_anlageId);
    if (!a)
        return;
    const Wechselrichter* wr = m_kontext.daten->wechselrichter(a->wechselrichterId);
    const double acGrenze = wr ? wr->nennleistungKw * qMax(1, a->anzahlWr) : 0.0;
    const double median = m_kontext.analyse->medianPrStation(a->wetterstationId);

    Auswertung aus = ErtragsAuswertung::berechne(verlauf, a->leistungKwp, acGrenze);
    m_anomalien->clear();
    for (const QString& h : dateiHinweise)
        m_anomalien->addItem(new QListWidgetItem(Stil::icon(Stil::Symbol::Info, QColor(37, 99, 235)),
                                                 tr("Datei: %1").arg(h)));

    if (!aus.gueltig) {
        m_auswertungHinweis->setText(aus.grund);
        m_tagesDiagramm->setDaten({}, tr("Tageserträge"), QStringLiteral("kWh"));
        return;
    }
    ErtragsAuswertung::bewerteGegenStation(aus, median);

    const double prRef = median > 0.0 ? median : Schwellwerte::kStandardPr;
    const double soll = aus.einstrahlungKwhM2 * a->leistungKwp * prRef;
    const double verlust = qMax(0.0, soll - aus.energieKwh);
    const double erloes = aus.energieKwh * a->verguetungCtKwh / 100.0;

    m_auswertungHinweis->setText(tr("Zeitraum %1 – %2 · Einstrahlung H = %3 kWh/m² · Bewertung mit %4 ct/kWh")
                                     .arg(Anzeige::datum(aus.tage.isEmpty() ? QDate() : aus.tage.first().tag),
                                          Anzeige::datum(aus.tage.isEmpty() ? QDate() : aus.tage.last().tag),
                                          Anzeige::zahl(aus.einstrahlungKwhM2, 1),
                                          Anzeige::zahl(a->verguetungCtKwh, 2)));
    m_kpiWerte[0]->setText(Anzeige::kwh(aus.energieKwh));
    m_kpiNotizen[0]->setText(tr("erwartet %1").arg(Anzeige::kwh(soll)));
    m_kpiWerte[1]->setText(Anzeige::zahl(aus.spezErtragKwhKwp, 1) + QStringLiteral(" kWh/kWp"));
    m_kpiNotizen[1]->setText(tr("Ertrag je installiertem kWp"));
    m_kpiWerte[2]->setText(Anzeige::pr(aus.pr));
    m_kpiWerte[2]->setStyleSheet(QStringLiteral("color: %1;").arg(Anzeige::prFarbe(aus.pr).darker(Stil::istDunkel() ? 90 : 120).name()));
    m_kpiNotizen[2]->setText(median > 0.0 ? tr("Median der Station: %1").arg(Anzeige::pr(median))
                                          : tr("kein Stationsvergleich"));
    m_kpiWerte[3]->setText(Anzeige::prozent(aus.abdeckung, 1));
    m_kpiNotizen[3]->setText(tr("%1 Lücken von %2 Messpunkten").arg(aus.luecken).arg(aus.messpunkte));
    m_kpiWerte[4]->setText(Anzeige::euro(erloes));
    m_kpiNotizen[4]->setText(tr("Ertrag × Einspeisevergütung"));
    m_kpiWerte[5]->setText(Anzeige::kwh(verlust));
    m_kpiNotizen[5]->setText(Anzeige::euro(verlust * a->verguetungCtKwh / 100.0));

    // Tageserträge
    QVector<BalkenDiagramm::Balken> balken;
    double summe = 0.0;
    m_tagesTabelle->setSortingEnabled(false);
    m_tagesTabelle->setRowCount(0);
    for (const TagesErtrag& t : aus.tage) {
        BalkenDiagramm::Balken b;
        b.beschriftung = t.tag.toString(QStringLiteral("dd.MM."));
        b.wert = t.energieKwh;
        b.farbe = Anzeige::prFarbe(t.pr);
        b.tooltip = tr("%1: %2, Einstrahlung %3 kWh/m², PR %4, %5 Lücken")
                        .arg(Anzeige::datum(t.tag), Anzeige::kwh(t.energieKwh),
                             Anzeige::zahl(t.einstrahlungKwhM2, 2), Anzeige::pr(t.pr))
                        .arg(t.luecken);
        balken.append(b);
        summe += t.energieKwh;

        const int r = m_tagesTabelle->rowCount();
        m_tagesTabelle->insertRow(r);
        auto* tag = new ZahlItem(Anzeige::datum(t.tag), t.tag.toJulianDay());
        tag->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_tagesTabelle->setItem(r, 0, tag);
        m_tagesTabelle->setItem(r, 1, new ZahlItem(Anzeige::kwh(t.energieKwh), t.energieKwh));
        m_tagesTabelle->setItem(r, 2, new ZahlItem(Anzeige::zahl(t.einstrahlungKwhM2, 2), t.einstrahlungKwhM2));
        m_tagesTabelle->setItem(r, 3, new ZahlItem(Anzeige::pr(t.pr), t.pr > 0 ? t.pr : -1.0));
        auto* l = new ZahlItem(QString::number(t.luecken), t.luecken);
        if (t.luecken > 0)
            l->setForeground(QColor(192, 57, 43));
        m_tagesTabelle->setItem(r, 4, l);
    }
    m_tagesTabelle->setSortingEnabled(true);
    m_tagesTabelle->resizeColumnsToContents();
    m_tagesDiagramm->setDaten(balken, tr("Tageserträge"), QStringLiteral("kWh"));
    if (!aus.tage.isEmpty())
        m_tagesDiagramm->setReferenz(summe / aus.tage.size(),
                                     tr("Mittel %1").arg(Anzeige::kwh(summe / aus.tage.size())));

    for (const Anomalie& an : aus.anomalien)
        m_anomalien->addItem(new QListWidgetItem(
            an.istHinweis() ? Stil::icon(Stil::Symbol::Info, QColor(37, 99, 235)) : Stil::icon(Stil::Symbol::Warnung, QColor(217, 119, 6)),
            an.text()));
    if (aus.anomalien.isEmpty())
        m_anomalien->addItem(new QListWidgetItem(Stil::icon(Stil::Symbol::Haken, QColor(22, 163, 74)),
                                                 tr("Keine Auffälligkeiten erkannt.")));
}

// -------------------------------------------------------------------
//  Reiter 4: Störungen
// -------------------------------------------------------------------

QWidget* DetailDialog::baueStoerungSeite()
{
    auto* seite = new QWidget;
    m_stoerungen = new QTableWidget(seite);
    TabellenHilfe::einrichten(m_stoerungen, {tr("Zeitpunkt"), tr("Störung"), tr("Code"), tr("Beschreibung"),
                                             tr("Schwere"), tr("Status"), tr("Behoben am")});
    m_stoerungen->setToolTip(tr("Doppelklick öffnet die Störung."));
    connect(m_stoerungen, &QTableWidget::cellDoubleClicked, this, [this](int zeile, int) {
        if (auto* item = m_stoerungen->item(zeile, 1))
            emit stoerungOeffnenGewuenscht(item->text());
    });
    auto* v = new QVBoxLayout(seite);
    v->addWidget(m_stoerungen);
    return seite;
}

void DetailDialog::zeigeStoerungen()
{
    const QVector<Stoerung> liste = m_kontext.daten->stoerungenVon(m_anlageId);
    m_stoerungen->setRowCount(0);
    int offen = 0;
    for (const Stoerung& s : liste) {
        const int r = m_stoerungen->rowCount();
        m_stoerungen->insertRow(r);
        m_stoerungen->setItem(r, 0, TabellenHilfe::text(Anzeige::zeitpunkt(s.zeitpunkt)));
        m_stoerungen->setItem(r, 1, TabellenHilfe::text(s.stoerungId));
        m_stoerungen->setItem(r, 2, TabellenHilfe::text(s.fehlercode));
        m_stoerungen->setItem(r, 3, TabellenHilfe::text(s.beschreibung));
        auto* schwere = TabellenHilfe::text(Anzeige::schwereName(s.schwere));
        schwere->setForeground(Anzeige::schwereFarbe(s.schwere));
        m_stoerungen->setItem(r, 4, schwere);
        m_stoerungen->setItem(r, 5, TabellenHilfe::text(Anzeige::stoerungsStatusName(s.status)));
        m_stoerungen->setItem(r, 6, TabellenHilfe::text(s.behobenAm.isValid() ? Anzeige::zeitpunkt(s.behobenAm) : QString()));
        if (s.istOffen())
            ++offen;
    }
    m_stoerungen->resizeColumnsToContents();
    m_tabs->setTabText(ReiterStoerungen, tr("Störungen (%1 offen / %2)").arg(offen).arg(liste.size()));
}
