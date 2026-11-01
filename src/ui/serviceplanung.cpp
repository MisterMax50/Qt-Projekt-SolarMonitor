#include "serviceplanung.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "eval/tourenplanung.h"
#include "model/datenbasis.h"
#include "repo/csv_parser.h"
#include "ui/badge_delegate.h"
#include "ui/tabellen_hilfe.h"
#include "util/anzeige.h"
#include "util/gewaesser.h"

namespace {
constexpr double kMinderertragSchwelle = 0.10; ///< Prio C ab 10 % Minderertrag
}

Serviceplanung::Serviceplanung(const AppKontext& kontext, QWidget* parent)
    : QWidget(parent)
    , m_kontext(kontext)
{
    m_hinweis = new QLabel(this);
    m_hinweis->setWordWrap(true);

    m_liste = new QTableWidget(this);
    TabellenHilfe::einrichten(m_liste, {tr("Prio"), tr("Anlage"), tr("Ort"), tr("Typ"), tr("Offene Störungen"),
                                        tr("Verlust (kWh)"), tr("Verlust (€)"), tr("Grund")});
    m_liste->setSortingEnabled(true);
    m_liste->setItemDelegateForColumn(0, new BadgeDelegate(m_liste));
    m_liste->setToolTip(tr("Doppelklick öffnet die Anlage."));
    connect(m_liste, &QTableWidget::cellDoubleClicked, this, [this](int zeile, int) {
        if (auto* item = m_liste->item(zeile, 1))
            emit anlageOeffnen(item->text());
    });

    // ---------- Tourenplanung ----------
    m_start = new QComboBox(this);
    for (const Wetterstation& w : m_kontext.daten->wetterstationen())
        m_start->addItem(QStringLiteral("%1 (%2)").arg(w.ort, w.stationId), QPointF(w.lat, w.lon));
    m_start->setToolTip(tr("Startpunkt des Technikers (Standort einer Wetterstation)"));
    m_anzahl = new QSpinBox(this);
    m_anzahl->setRange(1, 40);
    m_anzahl->setValue(10);
    m_anzahl->setSuffix(tr(" Einsätze"));
    m_anzahl->setToolTip(tr("Höchstzahl der Stopps; es werden die wichtigsten Einsätze im Umkreis gewählt."));
    m_umkreis = new QDoubleSpinBox(this);
    m_umkreis->setRange(0.0, 200.0);
    m_umkreis->setDecimals(0);
    m_umkreis->setSingleStep(5.0);
    m_umkreis->setValue(25.0);
    m_umkreis->setSuffix(QStringLiteral(" km"));
    m_umkreis->setSpecialValueText(tr("unbegrenzt"));
    m_umkreis->setToolTip(tr("Nur Einsätze in diesem Umkreis um den Startpunkt einplanen (0 = unbegrenzt).\n"
                             "Gemessen wird auf dem Landweg um den Bodensee herum, nicht quer über das Wasser."));
    m_strasse = new QCheckBox(tr("Autoroute (Internet)"), this);
    m_strasse->setChecked(true);
    m_strasse->setToolTip(tr("Reihenfolge und Strecke über das Straßennetz optimieren (OSRM, OpenStreetMap).\n"
                             "Bereits berechnete Touren werden gespeichert und sind offline verfügbar."));
    m_umkreisInfo = new QLabel(this);
    m_umkreisInfo->setObjectName(QStringLiteral("Gedimmt"));
    m_routen = new RoutenDienst(QDir(m_kontext.daten->datenOrdner()).filePath(QStringLiteral("routen_cache")), this);
    connect(m_routen, &RoutenDienst::routeBerechnet, this, &Serviceplanung::strassenRouteDa);
    auto* btnPlanen = new QPushButton(tr("Tour planen"), this);
    btnPlanen->setObjectName(QStringLiteral("Primaer"));
    m_btnKarte = new QPushButton(tr("Auf Karte zeigen"), this);
    m_btnKarte->setEnabled(false);
    m_btnExport = new QPushButton(tr("Einsatzliste exportieren …"), this);
    m_btnExport->setEnabled(m_kontext.darf(Recht::BerichtExportieren));

    m_tour = new QTableWidget(this);
    TabellenHilfe::einrichten(m_tour, {tr("Nr."), tr("Anlage"), tr("Ort"), tr("Prio"), tr("Etappe (km)"),
                                       tr("Fahrzeit (min)"), tr("Kumuliert (km)")});
    m_tour->setItemDelegateForColumn(3, new BadgeDelegate(m_tour));
    m_tourSumme = new QLabel(this);

    connect(btnPlanen, &QPushButton::clicked, this, &Serviceplanung::planeTour);
    connect(m_btnKarte, &QPushButton::clicked, this, [this] {
        emit tourAnzeigen(m_tourStart, m_tourIds, m_tourVerlauf, m_tourStrasse, m_umkreis->value());
    });
    connect(m_start, &QComboBox::currentIndexChanged, this, &Serviceplanung::aktualisiereUmkreisInfo);
    connect(m_umkreis, &QDoubleSpinBox::valueChanged, this, &Serviceplanung::aktualisiereUmkreisInfo);
    connect(m_btnExport, &QPushButton::clicked, this, &Serviceplanung::exportiere);

    auto* leiste = new QHBoxLayout;
    leiste->addWidget(new QLabel(tr("Start:"), this));
    leiste->addWidget(m_start);
    leiste->addWidget(new QLabel(tr("Umkreis:"), this));
    leiste->addWidget(m_umkreis);
    leiste->addWidget(new QLabel(tr("max."), this));
    leiste->addWidget(m_anzahl);
    leiste->addWidget(m_strasse);
    leiste->addWidget(btnPlanen);
    leiste->addWidget(m_btnKarte);
    leiste->addStretch();
    leiste->addWidget(m_btnExport);

    auto* gTour = new QGroupBox(tr("Tourenplanung"), this);
    auto* vTour = new QVBoxLayout(gTour);
    vTour->addLayout(leiste);
    vTour->addWidget(m_umkreisInfo);
    vTour->addWidget(m_tour, 1);
    vTour->addWidget(m_tourSumme);

    auto* gListe = new QGroupBox(tr("Priorisierte Einsatzliste"), this);
    auto* vListe = new QVBoxLayout(gListe);
    vListe->addWidget(m_hinweis);
    vListe->addWidget(m_liste, 1);

    auto* split = new QSplitter(Qt::Vertical, this);
    split->addWidget(gListe);
    split->addWidget(gTour);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    (new QVBoxLayout(this))->addWidget(split);

    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &Serviceplanung::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &Serviceplanung::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, &Serviceplanung::aktualisiere);
    aktualisiere();
}

void Serviceplanung::aktualisiere()
{
    m_einsaetze.clear();
    const bool analyseFertig = m_kontext.analyse->istFertig();

    for (const Anlage& a : m_kontext.daten->anlagen()) {
        Einsatz e;
        e.anlageId = a.anlageId;
        e.offen = m_kontext.daten->offeneStoerungen(a.anlageId);
        e.kritisch = m_kontext.daten->offeneKritischeStoerungen(a.anlageId);

        QStringList gruende;
        bool prioA = e.kritisch > 0 || a.status == QLatin1String("gestoert");
        bool prioB = e.offen > 0;
        bool prioC = false;
        if (e.offen > 0)
            gruende.append(tr("%n offene Störung(en)", "", e.offen));
        if (a.status == QLatin1String("gestoert"))
            gruende.append(tr("Status gestört"));

        if (const Kennzahl* k = m_kontext.analyse->kennzahl(a.anlageId); k && k->ok) {
            e.verlustKwh = k->verlustKwh;
            e.verlustEuro = k->verlustEuro;
            for (const Anomalie& an : k->auswertung.anomalien) {
                if (an.istHinweis())
                    continue;
                if (!gruende.contains(Anomalie::typName(an.typ)))
                    gruende.append(Anomalie::typName(an.typ));
                prioB = true;
                if (an.typ == Anomalie::Typ::KeineEinspeisung)
                    prioA = true;
            }
            if (k->sollKwh > 0 && k->verlustKwh > kMinderertragSchwelle * k->sollKwh) {
                prioC = true;
                gruende.append(tr("Minderertrag %1").arg(Anzeige::prozent(k->verlustKwh / k->sollKwh, 0)));
            }
        }
        if (!(prioA || prioB || prioC))
            continue;
        e.prioritaet = prioA ? QLatin1Char('A') : (prioB ? QLatin1Char('B') : QLatin1Char('C'));
        e.grund = gruende.join(QStringLiteral(", "));
        m_einsaetze.append(e);
    }

    std::sort(m_einsaetze.begin(), m_einsaetze.end(), [](const Einsatz& a, const Einsatz& b) {
        if (a.prioritaet != b.prioritaet)
            return a.prioritaet < b.prioritaet;
        return a.verlustEuro > b.verlustEuro;
    });

    double summe = 0.0;
    m_liste->setSortingEnabled(false);
    m_liste->setRowCount(0);
    for (int i = 0; i < m_einsaetze.size(); ++i) {
        const Einsatz& e = m_einsaetze.at(i);
        const Anlage* a = m_kontext.daten->anlage(e.anlageId);
        m_liste->insertRow(i);
        auto* prio = new ZahlItem(e.prioritaet, i); // sortiert in Listenreihenfolge
        prio->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        prio->setData(Qt::DecorationRole, e.prioritaet == QLatin1Char('A') ? QColor(220, 38, 38)
                                          : e.prioritaet == QLatin1Char('B') ? QColor(217, 119, 6)
                                                                             : QColor(100, 116, 139));
        m_liste->setItem(i, 0, prio);
        m_liste->setItem(i, 1, TabellenHilfe::text(e.anlageId));
        m_liste->setItem(i, 2, TabellenHilfe::text(a ? a->ort : QString()));
        m_liste->setItem(i, 3, TabellenHilfe::text(a ? Anzeige::typName(a->typ) : QString()));
        m_liste->setItem(i, 4, new ZahlItem(e.kritisch > 0 ? tr("%1 (%2 krit.)").arg(e.offen).arg(e.kritisch)
                                                           : QString::number(e.offen),
                                            e.offen + 100 * e.kritisch));
        m_liste->setItem(i, 5, new ZahlItem(Anzeige::zahl(e.verlustKwh, 1), e.verlustKwh));
        m_liste->setItem(i, 6, new ZahlItem(Anzeige::euro(e.verlustEuro), e.verlustEuro));
        m_liste->setItem(i, 7, TabellenHilfe::text(e.grund));
        summe += e.verlustEuro;
    }
    m_liste->setSortingEnabled(true);
    m_liste->resizeColumnsToContents();

    if (!analyseFertig)
        m_hinweis->setText(tr("Die Flottenanalyse läuft noch – Verluste und Fehlerbilder werden ergänzt, "
                              "sobald sie abgeschlossen ist. Bis dahin sind nur Störungen berücksichtigt."));
    else
        m_hinweis->setText(tr("%1 Anlagen mit Handlungsbedarf, geschätzter Ertragsverlust zusammen %2 im "
                              "Zeitraum 01.–14.06.2026. Verlust = erwarteter Ertrag (Einstrahlung × kWp × "
                              "Median-PR der Station) − gemessener Ertrag, bewertet mit der Einspeisevergütung.")
                               .arg(m_einsaetze.size())
                               .arg(Anzeige::euro(summe)));
    aktualisiereUmkreisInfo();
}

QVector<int> Serviceplanung::zieleImUmkreis() const
{
    const QPointF start = m_start->currentData().toPointF();
    const double radius = m_umkreis->value();
    const Gewaesser::Umweg umweg(start.x(), start.y()); // Landweg um den See herum
    QVector<int> erg;
    for (int i = 0; i < m_einsaetze.size(); ++i) {
        const Anlage* a = m_kontext.daten->anlage(m_einsaetze.at(i).anlageId);
        if (!a || !a->hatGueltigeKoordinaten())
            continue;
        if (radius > 0.0 && umweg.km(a->lat, a->lon) > radius)
            continue;
        erg.append(i);
    }
    return erg;
}

void Serviceplanung::aktualisiereUmkreisInfo()
{
    const QVector<int> ziele = zieleImUmkreis();
    int prioA = 0;
    for (int i : ziele)
        if (m_einsaetze.at(i).prioritaet == QLatin1Char('A'))
            ++prioA;
    m_umkreisInfo->setText(m_umkreis->value() > 0.0
                               ? tr("%1 Einsätze im Umkreis von %2 km, gemessen um den Bodensee herum "
                                    "(davon %3 mit Priorität A); geplant werden die wichtigsten.")
                                     .arg(ziele.size())
                                     .arg(Anzeige::zahl(m_umkreis->value(), 0))
                                     .arg(prioA)
                               : tr("%1 Einsätze ohne Umkreisbegrenzung (davon %2 mit Priorität A).")
                                     .arg(ziele.size())
                                     .arg(prioA));
}

void Serviceplanung::planeTour()
{
    m_tourIds.clear();
    m_tourZiele.clear();
    m_tourPrio.clear();
    m_tourVerlauf.clear();
    m_tourStrasse = false;
    m_tour->setRowCount(0);

    for (int i : zieleImUmkreis()) {
        const Einsatz& e = m_einsaetze.at(i);
        const Anlage* a = m_kontext.daten->anlage(e.anlageId);
        m_tourZiele.append({a->anlageId, a->lat, a->lon});
        m_tourPrio.append(e.prioritaet);
        if (m_tourZiele.size() >= m_anzahl->value())
            break;
    }
    if (m_tourZiele.isEmpty()) {
        m_tourSumme->setText(tr("Keine Einsätze im gewählten Umkreis – Umkreis vergrößern oder anderen Start wählen."));
        m_btnKarte->setEnabled(false);
        return;
    }

    m_tourStart = m_start->currentData().toPointF();
    const TourStopp start{QStringLiteral("Start"), m_tourStart.x(), m_tourStart.y()};

    // Sofort: Luftlinien-Tour (funktioniert immer, auch offline)
    const Tour tour = Tourenplanung::plane(start, m_tourZiele);
    m_tourVerlauf = tour.verlauf;
    zeigeTour(tour.reihenfolge, tour.etappenKm, {});
    m_tourSumme->setText(tr("Luftlinie (um den See herum) inkl. Rückfahrt: <b>%1 km</b> (%2 Stopps, "
                            "Nearest Neighbour + 2-opt)")
                             .arg(Anzeige::zahl(tour.gesamtKm, 1))
                             .arg(tour.reihenfolge.size()));
    m_btnKarte->setEnabled(true);

    // Danach: optimierte Autoroute über das Straßennetz
    if (m_strasse->isChecked()) {
        m_tourSumme->setText(m_tourSumme->text() + tr(" · Autoroute wird berechnet …"));
        m_routen->berechne(start, m_tourZiele);
    }
}

void Serviceplanung::strassenRouteDa(const StrassenRoute& route)
{
    if (m_tourZiele.isEmpty())
        return;
    if (!route.ok) {
        m_tourSumme->setText(m_tourSumme->text().replace(tr(" · Autoroute wird berechnet …"), QString())
                             + tr(" · <span style='color:#d97706'>Autoroute nicht verfügbar: %1 Es wird die "
                                  "Luftlinien-Tour verwendet.</span>").arg(route.fehler));
        return;
    }
    m_tourVerlauf = route.verlauf;
    m_tourStrasse = true;
    zeigeTour(route.reihenfolge, route.etappenKm, route.etappenMin);
    const int stunden = int(route.gesamtMin) / 60;
    const int minuten = int(std::lround(route.gesamtMin)) % 60;
    m_tourSumme->setText(tr("Autoroute inkl. Rückfahrt: <b>%1 km</b>, Fahrzeit <b>%2 h %3 min</b> "
                            "(%4 Stopps, schnellste Reihenfolge über das Straßennetz%5)")
                             .arg(Anzeige::zahl(route.gesamtKm, 1))
                             .arg(stunden)
                             .arg(minuten, 2, 10, QLatin1Char('0'))
                             .arg(route.reihenfolge.size())
                             .arg(route.ausCache ? tr(", gespeicherte Route") : QString()));
}

void Serviceplanung::zeigeTour(const QVector<int>& reihenfolge, const QVector<double>& etappenKm,
                               const QVector<double>& etappenMin)
{
    m_tourIds.clear();
    m_tour->setRowCount(0);
    double kumuliert = 0.0;
    for (int i = 0; i < reihenfolge.size(); ++i) {
        const TourStopp& z = m_tourZiele.at(reihenfolge.at(i));
        const Anlage* a = m_kontext.daten->anlage(z.id);
        const QChar prio = m_tourPrio.at(reihenfolge.at(i));
        kumuliert += etappenKm.at(i);
        m_tourIds.append(z.id);
        m_tour->insertRow(i);
        m_tour->setItem(i, 0, new ZahlItem(QString::number(i + 1), i + 1));
        m_tour->setItem(i, 1, TabellenHilfe::text(z.id));
        m_tour->setItem(i, 2, TabellenHilfe::text(a ? a->ort : QString()));
        auto* p = TabellenHilfe::text(prio);
        p->setData(Qt::DecorationRole, prio == QLatin1Char('A') ? QColor(220, 38, 38)
                                       : prio == QLatin1Char('B') ? QColor(217, 119, 6) : QColor(100, 116, 139));
        m_tour->setItem(i, 3, p);
        m_tour->setItem(i, 4, new ZahlItem(Anzeige::zahl(etappenKm.at(i), 1), etappenKm.at(i)));
        m_tour->setItem(i, 5, etappenMin.isEmpty() ? TabellenHilfe::text(QStringLiteral("–"))
                                                   : new ZahlItem(Anzeige::zahl(etappenMin.at(i), 0), etappenMin.at(i)));
        m_tour->setItem(i, 6, new ZahlItem(Anzeige::zahl(kumuliert, 1), kumuliert));
    }
    m_tour->resizeColumnsToContents();
}

void Serviceplanung::exportiere()
{
    const QString pfad = QFileDialog::getSaveFileName(this, tr("Einsatzliste exportieren"),
                                                      m_kontext.daten->ausgabePfad(QStringLiteral("einsatzliste.csv")),
                                                      tr("CSV-Dateien (*.csv)"));
    if (pfad.isEmpty())
        return;

    QSaveFile datei(pfad);
    if (!datei.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export fehlgeschlagen"), datei.errorString());
        return;
    }
    QTextStream out(&datei);
    out.setEncoding(QStringConverter::Utf8);
    out << "prioritaet,anlage_id,ort,offene_stoerungen,kritische_stoerungen,verlust_kwh,verlust_eur,grund,tour_position\n";
    for (const Einsatz& e : std::as_const(m_einsaetze)) {
        const Anlage* a = m_kontext.daten->anlage(e.anlageId);
        const int pos = int(m_tourIds.indexOf(e.anlageId));
        out << e.prioritaet << ',' << e.anlageId << ',' << CsvReader::maskiere(a ? a->ort : QString()) << ','
            << e.offen << ',' << e.kritisch << ',' << QString::number(e.verlustKwh, 'f', 1) << ','
            << QString::number(e.verlustEuro, 'f', 2) << ',' << CsvReader::maskiere(e.grund) << ','
            << (pos >= 0 ? QString::number(pos + 1) : QString()) << '\n';
    }
    out.flush();
    if (!datei.commit())
        QMessageBox::critical(this, tr("Export fehlgeschlagen"), datei.errorString());
}
