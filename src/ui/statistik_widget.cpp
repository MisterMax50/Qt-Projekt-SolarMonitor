#include "statistik_widget.h"

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "ui/balken_diagramm.h"
#include "ui/tabellen_hilfe.h"
#include "util/anzeige.h"

StatistikWidget::StatistikWidget(const AppKontext& kontext, QWidget* parent)
    : QWidget(parent)
    , m_kontext(kontext)
{
    auto* inhalt = new QWidget;
    auto* v = new QVBoxLayout(inhalt);

    auto* kpis = new QHBoxLayout;
    m_kpiAnlagen    = kpiKachel(tr("Anlagen"), inhalt, kpis);
    m_kpiLeistung   = kpiKachel(tr("Installierte Leistung"), inhalt, kpis);
    m_kpiStoerungen = kpiKachel(tr("Offene Störungen (kritisch)"), inhalt, kpis);
    m_kpiPr         = kpiKachel(tr("Median-PR der Flotte"), inhalt, kpis);
    m_kpiErtrag     = kpiKachel(tr("Ertrag 01.–14.06."), inhalt, kpis);
    m_kpiAuffaellig = kpiKachel(tr("Anlagen mit Fehlerbild"), inhalt, kpis);
    v->addLayout(kpis);

    auto gruppe = [&](const QString& titel, QWidget* w) {
        auto* g = new QGroupBox(titel, inhalt);
        (new QVBoxLayout(g))->addWidget(w);
        return g;
    };

    m_typen = new QTableWidget(inhalt);
    TabellenHilfe::einrichten(m_typen, {tr("Typ"), tr("Anzahl"), tr("Leistung (kWp)"), tr("Anteil Leistung")});
    m_orte = new QTableWidget(inhalt);
    TabellenHilfe::einrichten(m_orte, {tr("Ort"), tr("Anzahl"), tr("Leistung (kWp)"), tr("davon Freifläche (kWp)")});
    m_stoerungen = new QTableWidget(inhalt);
    TabellenHilfe::einrichten(m_stoerungen, {tr("Code"), tr("Beschreibung"), tr("Info"), tr("Warnung"),
                                             tr("Fehler"), tr("Kritisch"), tr("Gesamt"), tr("Offen")});
    m_stationen = new QTableWidget(inhalt);
    TabellenHilfe::einrichten(m_stationen, {tr("Station"), tr("Name"), tr("Anlagen"), tr("Median-PR"),
                                            tr("Fehlerbilder"), tr("Verlust (€, 14 Tage)")});
    m_alter = new BalkenDiagramm(inhalt);
    m_alter->setMinimumHeight(220);

    for (QTableWidget* t : {m_typen, m_orte, m_stoerungen, m_stationen}) {
        t->setSortingEnabled(true);
        t->setMinimumHeight(220);
    }

    auto* grid = new QGridLayout;
    grid->addWidget(gruppe(tr("Installierte Leistung je Typ"), m_typen), 0, 0);
    grid->addWidget(gruppe(tr("Altersverteilung (Jahr der Inbetriebnahme)"), m_alter), 0, 1);
    grid->addWidget(gruppe(tr("Installierte Leistung je Ort"), m_orte), 1, 0);
    grid->addWidget(gruppe(tr("Performance je Wetterstation"), m_stationen), 1, 1);
    grid->addWidget(gruppe(tr("Störungen je Fehlercode und Schwere (seit 2024)"), m_stoerungen), 2, 0, 1, 2);
    v->addLayout(grid);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(inhalt);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    (new QVBoxLayout(this))->addWidget(scroll);

    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &StatistikWidget::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, &StatistikWidget::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &StatistikWidget::aktualisiere);
    aktualisiere();
}

QLabel* StatistikWidget::kpiKachel(const QString& titel, QWidget* parent, QLayout* layout)
{
    auto* rahmen = new QFrame(parent);
    rahmen->setObjectName(QStringLiteral("Karte"));
    auto* v = new QVBoxLayout(rahmen);
    v->setContentsMargins(16, 12, 16, 12);
    auto* t = new QLabel(titel, rahmen);
    t->setObjectName(QStringLiteral("KpiTitel"));
    auto* w = new QLabel(QStringLiteral("–"), rahmen);
    w->setObjectName(QStringLiteral("KpiWert"));
    v->addWidget(t);
    v->addWidget(w);
    layout->addWidget(rahmen);
    return w;
}

void StatistikWidget::aktualisiere()
{
    const auto& anlagen = m_kontext.daten->anlagen();
    double kwp = 0.0;
    for (const Anlage& a : anlagen)
        kwp += qMax(0.0, a.leistungKwp);
    int offen = 0, kritisch = 0;
    for (const Stoerung& s : m_kontext.daten->stoerungen()) {
        if (s.istOffen()) {
            ++offen;
            if (s.istKritisch())
                ++kritisch;
        }
    }
    m_kpiAnlagen->setText(QString::number(anlagen.size()));
    m_kpiLeistung->setText(Anzeige::zahl(kwp / 1000.0, 2) + QStringLiteral(" MWp"));
    m_kpiStoerungen->setText(QStringLiteral("%1 (%2)").arg(offen).arg(kritisch));

    if (m_kontext.analyse->istFertig()) {
        double ertrag = 0.0;
        int auffaellig = 0;
        for (const Kennzahl& k : m_kontext.analyse->kennzahlen()) {
            if (k.ok)
                ertrag += k.auswertung.energieKwh;
            if (k.ok && k.auswertung.istAuffaellig())
                ++auffaellig;
        }
        m_kpiPr->setText(Anzeige::pr(m_kontext.analyse->medianPrFlotte()));
        m_kpiErtrag->setText(Anzeige::zahl(ertrag / 1000.0, 1) + QStringLiteral(" MWh"));
        m_kpiAuffaellig->setText(QString::number(auffaellig));
    } else {
        for (QLabel* l : {m_kpiPr, m_kpiErtrag, m_kpiAuffaellig})
            l->setText(tr("läuft …"));
    }

    fuelleTypen();
    fuelleOrte();
    fuelleStoerungen();
    fuelleAlter();
    fuelleStationen();
}

void StatistikWidget::fuelleTypen()
{
    QMap<QString, QPair<int, double>> m;
    double gesamt = 0.0;
    for (const Anlage& a : m_kontext.daten->anlagen()) {
        auto& e = m[a.typ];
        ++e.first;
        e.second += qMax(0.0, a.leistungKwp);
        gesamt += qMax(0.0, a.leistungKwp);
    }
    m_typen->setSortingEnabled(false);
    m_typen->setRowCount(0);
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        const int r = m_typen->rowCount();
        m_typen->insertRow(r);
        m_typen->setItem(r, 0, TabellenHilfe::text(Anzeige::typName(it.key())));
        m_typen->setItem(r, 1, new ZahlItem(QString::number(it.value().first), it.value().first));
        m_typen->setItem(r, 2, new ZahlItem(Anzeige::zahl(it.value().second, 1), it.value().second));
        const double anteil = gesamt > 0 ? it.value().second / gesamt : 0.0;
        m_typen->setItem(r, 3, new ZahlItem(Anzeige::prozent(anteil, 1), anteil));
    }
    m_typen->setSortingEnabled(true);
    m_typen->sortByColumn(2, Qt::DescendingOrder);
    m_typen->resizeColumnsToContents();
}

void StatistikWidget::fuelleOrte()
{
    struct Summe { int anzahl = 0; double kwp = 0.0; double freiflaeche = 0.0; };
    QMap<QString, Summe> m;
    for (const Anlage& a : m_kontext.daten->anlagen()) {
        Summe& s = m[a.ort];
        ++s.anzahl;
        s.kwp += qMax(0.0, a.leistungKwp);
        if (a.typ == QLatin1String("freiflaeche"))
            s.freiflaeche += qMax(0.0, a.leistungKwp);
    }
    m_orte->setSortingEnabled(false);
    m_orte->setRowCount(0);
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        const int r = m_orte->rowCount();
        m_orte->insertRow(r);
        m_orte->setItem(r, 0, TabellenHilfe::text(it.key()));
        m_orte->setItem(r, 1, new ZahlItem(QString::number(it.value().anzahl), it.value().anzahl));
        m_orte->setItem(r, 2, new ZahlItem(Anzeige::zahl(it.value().kwp, 1), it.value().kwp));
        m_orte->setItem(r, 3, new ZahlItem(Anzeige::zahl(it.value().freiflaeche, 1), it.value().freiflaeche));
    }
    m_orte->setSortingEnabled(true);
    m_orte->sortByColumn(2, Qt::DescendingOrder);
    m_orte->resizeColumnsToContents();
}

void StatistikWidget::fuelleStoerungen()
{
    struct Zeile { QString beschreibung; QMap<QString, int> schwere; int gesamt = 0; int offen = 0; };
    QMap<QString, Zeile> m;
    for (const Stoerung& s : m_kontext.daten->stoerungen()) {
        Zeile& z = m[s.fehlercode];
        z.beschreibung = s.beschreibung;
        ++z.schwere[s.schwere];
        ++z.gesamt;
        if (s.istOffen())
            ++z.offen;
    }
    m_stoerungen->setSortingEnabled(false);
    m_stoerungen->setRowCount(0);
    const QStringList schweren = Datenbasis::gueltigeSchweren();
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        const int r = m_stoerungen->rowCount();
        m_stoerungen->insertRow(r);
        m_stoerungen->setItem(r, 0, TabellenHilfe::text(it.key()));
        m_stoerungen->setItem(r, 1, TabellenHilfe::text(it.value().beschreibung));
        for (int i = 0; i < schweren.size(); ++i) {
            const int n = it.value().schwere.value(schweren.at(i));
            m_stoerungen->setItem(r, 2 + i, new ZahlItem(QString::number(n), n));
        }
        m_stoerungen->setItem(r, 6, new ZahlItem(QString::number(it.value().gesamt), it.value().gesamt));
        auto* offen = new ZahlItem(QString::number(it.value().offen), it.value().offen);
        if (it.value().offen > 0)
            offen->setForeground(Anzeige::schwereFarbe(QStringLiteral("kritisch")));
        m_stoerungen->setItem(r, 7, offen);
    }
    m_stoerungen->setSortingEnabled(true);
    m_stoerungen->sortByColumn(6, Qt::DescendingOrder);
    m_stoerungen->resizeColumnsToContents();
}

void StatistikWidget::fuelleAlter()
{
    QMap<int, int> jahre;
    int unbekannt = 0;
    for (const Anlage& a : m_kontext.daten->anlagen()) {
        if (a.inbetriebnahme.isValid())
            ++jahre[a.inbetriebnahme.year()];
        else
            ++unbekannt;
    }
    QVector<BalkenDiagramm::Balken> balken;
    for (auto it = jahre.cbegin(); it != jahre.cend(); ++it) {
        BalkenDiagramm::Balken b;
        b.beschriftung = QString::number(it.key() % 100).rightJustified(2, QLatin1Char('0'));
        b.wert = it.value();
        b.farbe = QColor(52, 152, 219);
        b.tooltip = tr("%1: %2 Anlagen").arg(it.key()).arg(it.value());
        balken.append(b);
    }
    m_alter->setDaten(balken, unbekannt > 0 ? tr("Anlagen je Jahr (%1 ohne gültiges Datum)").arg(unbekannt)
                                            : tr("Anlagen je Jahr"),
                      tr("Anz."), 0);
}

void StatistikWidget::fuelleStationen()
{
    struct Summe { int anlagen = 0; int auffaellig = 0; double verlust = 0.0; };
    QMap<QString, Summe> m;
    for (const Anlage& a : m_kontext.daten->anlagen()) {
        Summe& s = m[a.wetterstationId];
        ++s.anlagen;
        if (const Kennzahl* k = m_kontext.analyse->kennzahl(a.anlageId)) {
            if (!k->ok || k->auswertung.istAuffaellig())
                ++s.auffaellig;
            s.verlust += k->verlustEuro;
        }
    }
    m_stationen->setSortingEnabled(false);
    m_stationen->setRowCount(0);
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        const int r = m_stationen->rowCount();
        m_stationen->insertRow(r);
        const Wetterstation* ws = m_kontext.daten->wetterstation(it.key());
        m_stationen->setItem(r, 0, TabellenHilfe::text(it.key()));
        m_stationen->setItem(r, 1, TabellenHilfe::text(ws ? ws->name : tr("(unbekannt)")));
        m_stationen->setItem(r, 2, new ZahlItem(QString::number(it.value().anlagen), it.value().anlagen));
        const double median = m_kontext.analyse->medianPrStation(it.key());
        m_stationen->setItem(r, 3, new ZahlItem(Anzeige::pr(median), median > 0 ? median : -1.0));
        m_stationen->setItem(r, 4, new ZahlItem(QString::number(it.value().auffaellig), it.value().auffaellig));
        m_stationen->setItem(r, 5, new ZahlItem(Anzeige::euro(it.value().verlust), it.value().verlust));
    }
    m_stationen->setSortingEnabled(true);
    m_stationen->sortByColumn(0, Qt::AscendingOrder);
    m_stationen->resizeColumnsToContents();
}
