#include "ui/live_seite.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QVBoxLayout>

#include "model/anlage.h"
#include "model/datenbasis.h"
#include "ui/kennzahl_kachel.h"
#include "util/stil.h"

namespace {
const QColor kGruen(22, 163, 74);
const QColor kRot(220, 38, 38);
const QColor kBlau(37, 99, 235);
const QColor kOrange(234, 88, 12);
const QColor kGrau(100, 116, 139);

/// Tabellenzelle mit Zahlenwert: sortiert nach dem Wert, nicht nach dem Text.
class ZahlItem : public QTableWidgetItem {
public:
    ZahlItem(const QString& anzeige, double wert) : QTableWidgetItem(anzeige)
    {
        setData(Qt::UserRole, wert);
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    bool operator<(const QTableWidgetItem& andere) const override
    {
        return data(Qt::UserRole).toDouble() < andere.data(Qt::UserRole).toDouble();
    }
};

/// Tempo-Stufen: Simulationszeit je Wanduhrsekunde
struct Tempo {
    int     faktor;
    QString name;
};
} // namespace

LiveSeite::LiveSeite(const AppKontext& kontext, QWidget* parent)
    : QWidget(parent)
    , m_gen(new LiveGenerator(this))
{
    // ----- Bedienzeile -----
    m_knopf = new QPushButton(tr("Anhalten"));
    m_knopf->setCheckable(true);
    m_knopf->setChecked(true);
    m_knopf->setObjectName(QStringLiteral("LiveKnopf"));

    m_tempo = new QComboBox;
    const QList<Tempo> stufen = {
        {1, tr("1× Echtzeit")},
        {60, tr("60× (1 Minute je Sekunde)")},
        {900, tr("900× (15 Minuten je Sekunde)")},
        {3600, tr("3600× (1 Stunde je Sekunde)")},
    };
    for (const Tempo& t : stufen)
        m_tempo->addItem(t.name, t.faktor);
    m_tempo->setCurrentIndex(1);
    m_gen->setFaktor(m_tempo->currentData().toInt());

    m_zeit = new QLabel;
    m_zeit->setObjectName(QStringLiteral("Zaehler"));

    auto* bedienung = new QHBoxLayout;
    bedienung->setSpacing(10);
    bedienung->addWidget(m_knopf);
    bedienung->addWidget(new QLabel(tr("Tempo:")));
    bedienung->addWidget(m_tempo);
    bedienung->addStretch(1);
    bedienung->addWidget(m_zeit);

    // ----- Kennzahlen -----
    m_einspeisung = new KennzahlKachel(Stil::Symbol::Blitz, kGruen, tr("Einspeisung jetzt"), this);
    m_auslastung = new KennzahlKachel(Stil::Symbol::Statistik, kBlau, tr("Auslastung der Flotte"), this);
    m_einstrahlung = new KennzahlKachel(Stil::Symbol::Sonne, kOrange, tr("Einstrahlung (Mittel)"), this);
    m_stoerungen = new KennzahlKachel(Stil::Symbol::Warnung, kGruen, tr("Anlagen mit Störung"), this);

    auto* kacheln = new QHBoxLayout;
    kacheln->setSpacing(12);
    for (KennzahlKachel* k : {m_einspeisung, m_auslastung, m_einstrahlung, m_stoerungen}) {
        k->setMinimumHeight(96);
        kacheln->addWidget(k);
    }

    // ----- Tabelle: eine Zeile je Anlage -----
    m_tabelle = new QTableWidget(0, 8, this);
    m_tabelle->setObjectName(QStringLiteral("LiveTabelle"));
    m_tabelle->setHorizontalHeaderLabels({tr("Anlage"), tr("Ort"), tr("kWp"), tr("Leistung"),
                                          tr("Auslastung"), tr("Einstrahlung"), tr("Status"), tr("Stand")});
    m_tabelle->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabelle->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabelle->setAlternatingRowColors(true);
    m_tabelle->setSortingEnabled(true);
    m_tabelle->verticalHeader()->setVisible(false);
    m_tabelle->horizontalHeader()->setStretchLastSection(true);

    QVector<Anlage> anlagen;
    if (kontext.daten)
        anlagen = kontext.daten->anlagen();

    m_tabelle->setSortingEnabled(false);
    int zeile = 0;
    for (const Anlage& a : anlagen) {
        if (!a.hatGueltigeLeistung())
            continue;
        m_tabelle->insertRow(zeile);

        auto* id = new QTableWidgetItem(a.anlageId);
        id->setData(Qt::UserRole, a.anlageId);
        m_tabelle->setItem(zeile, 0, id);
        m_tabelle->setItem(zeile, 1, new QTableWidgetItem(a.ort));
        m_tabelle->setItem(zeile, 2, new ZahlItem(QLocale().toString(a.leistungKwp, 'f', 2), a.leistungKwp));

        Zeile z;
        z.kwp = a.leistungKwp;
        z.leistung = new ZahlItem(QStringLiteral("–"), 0.0);
        z.auslastung = new ZahlItem(QStringLiteral("–"), 0.0);
        z.einstrahlung = new ZahlItem(QStringLiteral("–"), 0.0);
        z.status = new QTableWidgetItem(QStringLiteral("–"));
        z.stand = new QTableWidgetItem(QStringLiteral("–"));
        m_tabelle->setItem(zeile, 3, z.leistung);
        m_tabelle->setItem(zeile, 4, z.auslastung);
        m_tabelle->setItem(zeile, 5, z.einstrahlung);
        m_tabelle->setItem(zeile, 6, z.status);
        m_tabelle->setItem(zeile, 7, z.stand);
        m_zeilen.insert(a.anlageId, z);
        ++zeile;
    }
    m_tabelle->setSortingEnabled(true);
    m_tabelle->sortItems(0, Qt::AscendingOrder);
    m_tabelle->resizeColumnsToContents();

    m_gen->setAnlagen(anlagen);

    // ----- Layout -----
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);
    v->addLayout(bedienung);
    v->addLayout(kacheln);
    v->addWidget(m_tabelle, 1);

    // ----- Verbindungen -----
    connect(m_gen, &LiveGenerator::neueMesswerte, this, &LiveSeite::uebernehmen);
    connect(m_knopf, &QPushButton::toggled, this, &LiveSeite::aktualisiereLaufzustand);
    connect(m_tempo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_gen->setFaktor(m_tempo->itemData(index).toInt());
    });
    connect(m_tabelle, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* it) {
        if (QTableWidgetItem* id = m_tabelle->item(it->row(), 0))
            emit anlageOeffnen(id->data(Qt::UserRole).toString());
    });

    aktualisiereLaufzustand();
}

void LiveSeite::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    aktualisiereLaufzustand();
}

void LiveSeite::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // Unsichtbar: keine Berechnung im Hintergrund
    m_gen->stoppe();
}

void LiveSeite::aktualisiereLaufzustand()
{
    const bool laufen = m_knopf->isChecked();
    m_knopf->setText(laufen ? tr("Anhalten") : tr("Starten"));
    if (laufen && isVisible())
        m_gen->starte();
    else
        m_gen->stoppe();
}

void LiveSeite::uebernehmen(const QVector<LiveMesswert>& werte, const QDateTime& zeit)
{
    const QLocale loc;
    m_zeit->setText(tr("Simulationszeit: %1").arg(loc.toString(zeit, QStringLiteral("dd.MM.yyyy HH:mm:ss"))));

    double leistungSumme = 0.0;   // W
    double kwpSumme = 0.0;        // kWp aller Anlagen
    double einstrahlungSumme = 0.0;
    int einspeisend = 0;
    int stoerend = 0;
    int messbar = 0;

    m_tabelle->setSortingEnabled(false);
    for (const LiveMesswert& m : werte) {
        auto it = m_zeilen.find(m.anlageId);
        if (it == m_zeilen.end())
            continue;
        Zeile& z = it.value();

        const QString statusText = m.status == QLatin1String("ok") ? tr("Einspeisung")
                                 : m.status == QLatin1String("standby") ? tr("Standby")
                                 : m.status == QLatin1String("stoerung") ? tr("Störung")
                                 : tr("Abgeschaltet");
        const QColor statusFarbe = m.status == QLatin1String("ok") ? kGruen
                                 : m.status == QLatin1String("stoerung") ? kRot
                                 : kGrau;

        // Werte in die Zellen schreiben (die Zellen bleiben auch beim Sortieren gültig)
        const double kwp = z.kwp;
        const double auslastung = kwp > 0.0 ? m.leistungW / (kwp * 10.0) : 0.0; // W / (kWp·1000) · 100 %
        z.leistung->setText(loc.toString(m.leistungW, 'f', 0));
        z.leistung->setData(Qt::UserRole, m.leistungW);
        z.auslastung->setText(loc.toString(auslastung, 'f', 1) + QStringLiteral(" %"));
        z.auslastung->setData(Qt::UserRole, auslastung);
        z.einstrahlung->setText(loc.toString(m.einstrahlungWm2, 'f', 0) + QStringLiteral(" W/m²"));
        z.einstrahlung->setData(Qt::UserRole, m.einstrahlungWm2);
        z.status->setText(statusText);
        z.status->setForeground(statusFarbe);
        z.stand->setText(zeit.toString(QStringLiteral("HH:mm:ss")));

        kwpSumme += kwp;
        leistungSumme += m.leistungW;
        if (m.status == QLatin1String("ok"))
            ++einspeisend;
        if (m.status == QLatin1String("stoerung"))
            ++stoerend;
        if (m.status != QLatin1String("aus")) {
            einstrahlungSumme += m.einstrahlungWm2;
            ++messbar;
        }
    }
    m_tabelle->setSortingEnabled(true);

    const int gesamt = m_zeilen.size();
    const double auslastungFlotte = kwpSumme > 0.0 ? leistungSumme / (kwpSumme * 1000.0) * 100.0 : 0.0;
    const double einstrahlungMittel = messbar > 0 ? einstrahlungSumme / messbar : 0.0;

    m_einspeisung->setzeWert(loc.toString(leistungSumme / 1000.0, 'f', 1) + QStringLiteral(" kW"),
                             tr("%1 von %2 Anlagen speisen ein").arg(einspeisend).arg(gesamt));
    m_auslastung->setzeWert(loc.toString(auslastungFlotte, 'f', 1) + QStringLiteral(" %"),
                            tr("bezogen auf %1 kWp installiert").arg(loc.toString(kwpSumme, 'f', 1)));
    m_einstrahlung->setzeWert(loc.toString(einstrahlungMittel, 'f', 0) + QStringLiteral(" W/m²"),
                              tr("Mittelwert über alle Anlagen in Betrieb"));
    m_stoerungen->setzeWert(QString::number(stoerend), tr("von %1 Anlagen").arg(gesamt));
    m_stoerungen->setzeFarbe(stoerend > 0 ? kRot : kGruen);
}
