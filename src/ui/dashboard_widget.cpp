#include "dashboard_widget.h"

#include <algorithm>
#include <functional>
#include <map>

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "ui/karte_view.h"
#include "util/anzeige.h"
#include "util/stil.h"

namespace {

constexpr int kZeilenhoehe = 30;
constexpr int kMaxZeilen = 5;
constexpr int kIdRolle = Qt::UserRole + 10;

/// Karte, die auf Klick reagiert (ganze Fläche ist die Schaltfläche).
class KlickRahmen : public QFrame {
public:
    using QFrame::QFrame;
    std::function<void()> beiKlick;

protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && beiKlick)
            beiKlick();
        QFrame::mouseReleaseEvent(e);
    }
};

QTableWidgetItem* zelle(const QString& text, const QString& anlageId = {})
{
    auto* item = new QTableWidgetItem(text);
    item->setData(kIdRolle, anlageId);
    if (!anlageId.isEmpty())
        item->setToolTip(DashboardWidget::tr("Klick öffnet die Anlage %1").arg(anlageId));
    return item;
}

void leer(QTableWidget* t, const QString& text)
{
    t->setRowCount(1);
    auto* item = new QTableWidgetItem(text);
    item->setForeground(Stil::textGedimmt());
    item->setFlags(Qt::ItemIsEnabled);
    t->setItem(0, 0, item);
    t->setSpan(0, 0, 1, t->columnCount());
}

} // namespace

DashboardWidget::DashboardWidget(const AppKontext& kontext, const QList<QAction*>& schnellzugriff, QWidget* parent)
    : QWidget(parent)
    , m_kontext(kontext)
{
    auto* inhalt = new QWidget;
    inhalt->setObjectName(QStringLiteral("Inhalt"));
    auto* v = new QVBoxLayout(inhalt);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);

    m_stand = new QLabel(inhalt);
    m_stand->setObjectName(QStringLiteral("Gedimmt"));
    v->addWidget(m_stand);

    // ---------- Kennzahl-Kacheln ----------
    auto* kacheln = new QHBoxLayout;
    kacheln->setSpacing(12);
    m_betrieb = neueKachel(inhalt, kacheln, tr("Anlagen in Betrieb"), Ziel::Anlagen);
    m_pr = neueKachel(inhalt, kacheln, tr("Median-PR der Flotte"), Ziel::Statistik);
    m_stoerungen = neueKachel(inhalt, kacheln, tr("Offene Störungen"), Ziel::Stoerungen);
    m_verlust = neueKachel(inhalt, kacheln, tr("Geschätzter Ertragsverlust"), Ziel::Serviceplanung);
    m_daten = neueKachel(inhalt, kacheln, tr("Datenqualität"), Ziel::Datenpruefung);
    v->addLayout(kacheln);

    // ---------- Karte ----------
    auto* karte = new KarteView(m_kontext, nullptr, KarteView::Modus::Kompakt);
    karte->setMinimumHeight(240);
    karte->setToolTip(tr("Rot: kritische Störung, gestört oder Fehlerbild · Orange: offene Störung oder Wartung · Grün: in Ordnung\n"
                         "Klick auf einen Punkt öffnet die Anlage, Mausrad zoomt."));
    connect(karte, &KarteView::anlageGewaehlt, this, &DashboardWidget::anlageOeffnen);
    v->addWidget(neueKarte(tr("Karte – wo besteht Handlungsbedarf?"), karte, tr("Große Karte öffnen →"), Ziel::Karte), 1);

    // ---------- Listen ----------
    m_dringend = neueListe(3);
    m_verluste = neueListe(3);
    m_fehlerbilder = neueListe(2);
    for (QTableWidget* t : {m_dringend, m_verluste})
        connect(t, &QTableWidget::cellClicked, this, [this, t](int zeile, int spalte) {
            if (QTableWidgetItem* it = t->item(zeile, spalte); it && !it->data(kIdRolle).toString().isEmpty())
                emit anlageOeffnen(it->data(kIdRolle).toString());
        });

    auto* raster = new QGridLayout;
    raster->setSpacing(12);
    raster->addWidget(neueKarte(tr("Jetzt kümmern – offene kritische Störungen"), m_dringend,
                                tr("Alle Störungen →"), Ziel::Stoerungen),
                      0, 0);
    raster->addWidget(neueKarte(tr("Größte Ertragsverluste"), m_verluste, tr("Zur Serviceplanung →"),
                                Ziel::Serviceplanung),
                      0, 1);
    raster->addWidget(neueKarte(tr("Erkannte Fehlerbilder"), m_fehlerbilder, tr("Zur Anlagenliste →"), Ziel::Anlagen),
                      1, 0);

    // Schnellzugriff
    auto* schnell = new QWidget;
    auto* sv = new QVBoxLayout(schnell);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->setSpacing(6);
    auto* tour = new QAction(Stil::icon(Stil::Symbol::Werkzeug, Stil::text()), tr("Servicetour planen"), this);
    connect(tour, &QAction::triggered, this, [this] { emit seiteOeffnen(Ziel::Serviceplanung); });
    auto* pruefung = new QAction(Stil::icon(Stil::Symbol::Pruefung, Stil::text()), tr("Datenprüfung ansehen"), this);
    connect(pruefung, &QAction::triggered, this, [this] { emit seiteOeffnen(Ziel::Datenpruefung); });
    QList<QAction*> knoepfe = schnellzugriff;
    knoepfe << tour << pruefung;
    for (QAction* a : std::as_const(knoepfe)) {
        auto* knopf = new QToolButton(schnell);
        knopf->setObjectName(QStringLiteral("Schnell"));
        knopf->setDefaultAction(a);
        knopf->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        knopf->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        knopf->setCursor(Qt::PointingHandCursor);
        sv->addWidget(knopf);
    }
    sv->addStretch();
    raster->addWidget(neueKarte(tr("Schnellzugriff"), schnell, {}, Ziel::Anlagen), 1, 1);
    raster->setColumnStretch(0, 1);
    raster->setColumnStretch(1, 1);
    v->addLayout(raster);

    // Bei kleinen Fenstern scrollen statt quetschen
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inhalt);
    auto* aussen = new QVBoxLayout(this);
    aussen->setContentsMargins(0, 0, 0, 0);
    aussen->addWidget(scroll);

    connect(m_kontext.analyse, &FlottenAnalyse::fortschritt, this, &DashboardWidget::spaeterAktualisieren);
    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &DashboardWidget::spaeterAktualisieren);
    connect(m_kontext.analyse, &FlottenAnalyse::kennzahlGeaendert, this, &DashboardWidget::spaeterAktualisieren);
    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, &DashboardWidget::spaeterAktualisieren);
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &DashboardWidget::spaeterAktualisieren);
    connect(m_kontext.daten, &Datenbasis::meldungenGeaendert, this, &DashboardWidget::spaeterAktualisieren);
    aktualisiere();
}

DashboardWidget::Kachel DashboardWidget::neueKachel(QWidget* eltern, QLayout* ziel, const QString& titel, Ziel sprung)
{
    auto* rahmen = new KlickRahmen(eltern);
    rahmen->setObjectName(QStringLiteral("KarteKlick"));
    rahmen->setCursor(Qt::PointingHandCursor);
    rahmen->beiKlick = [this, sprung] { emit seiteOeffnen(sprung); };
    auto* v = new QVBoxLayout(rahmen);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(2);
    auto* t = new QLabel(titel, rahmen);
    t->setObjectName(QStringLiteral("KpiTitel"));
    Kachel k;
    k.wert = new QLabel(QStringLiteral("–"), rahmen);
    k.wert->setObjectName(QStringLiteral("KpiWert"));
    k.notiz = new QLabel(rahmen);
    k.notiz->setObjectName(QStringLiteral("KpiNotiz"));
    v->addWidget(t);
    v->addWidget(k.wert);
    v->addWidget(k.notiz);
    ziel->addWidget(rahmen);
    return k;
}

QWidget* DashboardWidget::neueKarte(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung)
{
    auto* karte = new QFrame;
    karte->setObjectName(QStringLiteral("Karte"));
    auto* v = new QVBoxLayout(karte);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(8);
    auto* kopf = new QHBoxLayout;
    auto* t = new QLabel(titel, karte);
    t->setObjectName(QStringLiteral("KartenTitel"));
    kopf->addWidget(t, 1);
    if (!linkText.isEmpty()) {
        auto* link = new QPushButton(linkText, karte);
        link->setObjectName(QStringLiteral("Link"));
        link->setFlat(true);
        link->setCursor(Qt::PointingHandCursor);
        connect(link, &QPushButton::clicked, this, [this, sprung] { emit seiteOeffnen(sprung); });
        kopf->addWidget(link);
    }
    v->addLayout(kopf);
    v->addWidget(inhalt);
    return karte;
}

QTableWidget* DashboardWidget::neueListe(int spalten)
{
    auto* t = new QTableWidget(0, spalten);
    t->setObjectName(QStringLiteral("Kompakt"));
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->setFrameShape(QFrame::NoFrame);
    t->horizontalHeader()->hide();
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(kZeilenhoehe);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setMouseTracking(true);
    t->setCursor(Qt::PointingHandCursor);
    t->setFixedHeight(kZeilenhoehe * kMaxZeilen + 2);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(spalten > 2 ? 1 : 0, QHeaderView::Stretch);
    return t;
}

void DashboardWidget::spaeterAktualisieren()
{
    // Viele Signale kurz hintereinander (z. B. Fortschritt) -> nur einmal neu aufbauen
    if (m_geplant)
        return;
    m_geplant = true;
    QTimer::singleShot(0, this, [this] {
        m_geplant = false;
        aktualisiere();
    });
}

void DashboardWidget::aktualisiere()
{
    const Datenbasis& d = *m_kontext.daten;
    const FlottenAnalyse& fa = *m_kontext.analyse;
    const bool fertig = fa.istFertig();

    // ---------- Anlagen ----------
    int inBetrieb = 0, gestoert = 0, wartung = 0;
    for (const Anlage& a : d.anlagen()) {
        if (a.status == QLatin1String("in_betrieb"))
            ++inBetrieb;
        else if (a.status == QLatin1String("gestoert"))
            ++gestoert;
        else if (a.status == QLatin1String("wartung"))
            ++wartung;
    }
    m_betrieb.wert->setText(QStringLiteral("%1 / %2").arg(inBetrieb).arg(d.anlagen().size()));
    m_betrieb.notiz->setText(tr("%1 gestört · %2 in Wartung").arg(gestoert).arg(wartung));

    // ---------- Störungen ----------
    int offen = 0;
    QVector<Stoerung> kritisch;
    for (const Stoerung& s : d.stoerungen()) {
        if (!s.istOffen())
            continue;
        ++offen;
        if (s.istKritisch())
            kritisch.append(s);
    }
    m_stoerungen.wert->setText(QString::number(offen));
    m_stoerungen.wert->setStyleSheet(kritisch.isEmpty() ? QString() : QStringLiteral("color: #dc2626;"));
    m_stoerungen.notiz->setText(tr("davon %1 kritisch").arg(kritisch.size()));

    std::sort(kritisch.begin(), kritisch.end(),
              [](const Stoerung& x, const Stoerung& y) { return x.zeitpunkt > y.zeitpunkt; });
    m_dringend->clearSpans();
    m_dringend->setRowCount(0);
    if (kritisch.isEmpty()) {
        leer(m_dringend, tr("Keine offenen kritischen Störungen."));
    } else {
        for (int i = 0; i < qMin<qsizetype>(kMaxZeilen, kritisch.size()); ++i) {
            const Stoerung& s = kritisch.at(i);
            const Anlage* a = d.anlage(s.anlageId);
            m_dringend->insertRow(i);
            auto* anlage = zelle(QStringLiteral("%1 %2").arg(s.anlageId, a ? a->ort : QString()), s.anlageId);
            anlage->setIcon(Stil::icon(Stil::Symbol::Warnung, QColor(220, 38, 38)));
            m_dringend->setItem(i, 0, anlage);
            m_dringend->setItem(i, 1, zelle(s.beschreibung, s.anlageId));
            auto* zeit = zelle(Anzeige::zeitpunkt(s.zeitpunkt), s.anlageId);
            zeit->setForeground(Stil::textGedimmt());
            m_dringend->setItem(i, 2, zeit);
        }
    }

    // ---------- Datenqualität ----------
    const int fehler = d.anzahlMeldungen(Pruefmeldung::Stufe::Fehler);
    m_daten.wert->setText(fehler == 0 ? tr("OK") : tr("%n Fehler", nullptr, fehler));
    m_daten.wert->setStyleSheet(fehler > 0 ? QStringLiteral("color: #dc2626;") : QStringLiteral("color: #16a34a;"));
    m_daten.notiz->setText(tr("%1 Warnungen beim Einlesen").arg(d.anzahlMeldungen(Pruefmeldung::Stufe::Warnung)));

    // ---------- Auswertung (erst nach der Flottenanalyse) ----------
    m_verluste->clearSpans();
    m_verluste->setRowCount(0);
    m_fehlerbilder->clearSpans();
    m_fehlerbilder->setRowCount(0);
    if (!fertig) {
        const QString laeuft = tr("Flottenanalyse läuft …");
        for (Kachel* k : {&m_pr, &m_verlust}) {
            k->wert->setText(QStringLiteral("…"));
            k->wert->setStyleSheet(QString());
            k->notiz->setText(laeuft);
        }
        leer(m_verluste, laeuft);
        leer(m_fehlerbilder, laeuft);
        m_stand->setText(tr("Angemeldet als %1 · Messwerte werden ausgewertet …").arg(Rollen::anzeigename(m_kontext.rolle)));
        return;
    }

    int ausgewertet = 0;
    double verlustEuro = 0.0, verlustKwh = 0.0;
    QVector<const Kennzahl*> verluste;
    std::map<Anomalie::Typ, int> bilder;
    for (const Kennzahl& k : fa.kennzahlen()) {
        if (!k.ok)
            continue;
        ++ausgewertet;
        verlustEuro += k.verlustEuro;
        verlustKwh += k.verlustKwh;
        if (k.verlustEuro > 0.005)
            verluste.append(&k);
        QSet<int> gezaehlt;
        for (const Anomalie& an : k.auswertung.anomalien)
            if (!gezaehlt.contains(int(an.typ))) {
                gezaehlt.insert(int(an.typ));
                ++bilder[an.typ];
            }
    }

    const double median = fa.medianPrFlotte();
    m_pr.wert->setText(Anzeige::pr(median));
    m_pr.wert->setStyleSheet(QStringLiteral("color: %1;").arg(Anzeige::prFarbe(median).darker(Stil::istDunkel() ? 90 : 125).name()));
    m_pr.notiz->setText(tr("%1 Anlagen ausgewertet").arg(ausgewertet));
    m_verlust.wert->setText(Anzeige::euro(verlustEuro));
    m_verlust.wert->setStyleSheet(verlustEuro > 0.0 ? QStringLiteral("color: #d97706;") : QString());
    m_verlust.notiz->setText(tr("%1 entgangener Ertrag").arg(Anzeige::kwh(verlustKwh)));

    std::sort(verluste.begin(), verluste.end(),
              [](const Kennzahl* x, const Kennzahl* y) { return x->verlustEuro > y->verlustEuro; });
    if (verluste.isEmpty()) {
        leer(m_verluste, tr("Keine nennenswerten Verluste."));
    } else {
        for (int i = 0; i < qMin<qsizetype>(kMaxZeilen, verluste.size()); ++i) {
            const Kennzahl* k = verluste.at(i);
            const Anlage* a = d.anlage(k->anlageId);
            QStringList namen;
            for (const Anomalie& an : k->auswertung.anomalien)
                if (!an.istHinweis())
                    namen.append(Anomalie::typName(an.typ));
            namen.removeDuplicates();
            m_verluste->insertRow(i);
            m_verluste->setItem(i, 0, zelle(QStringLiteral("%1 %2").arg(k->anlageId, a ? a->ort : QString()), k->anlageId));
            auto* grund = zelle(namen.isEmpty() ? tr("Minderertrag") : namen.join(QStringLiteral(", ")), k->anlageId);
            grund->setForeground(Stil::textGedimmt());
            m_verluste->setItem(i, 1, grund);
            auto* euro = zelle(Anzeige::euro(k->verlustEuro), k->anlageId);
            euro->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_verluste->setItem(i, 2, euro);
        }
    }

    QVector<std::pair<Anomalie::Typ, int>> liste(bilder.begin(), bilder.end());
    std::sort(liste.begin(), liste.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
    if (liste.isEmpty()) {
        leer(m_fehlerbilder, tr("Keine Fehlerbilder erkannt."));
    } else {
        m_fehlerbilder->setCursor(Qt::ArrowCursor);
        for (int i = 0; i < qMin<qsizetype>(kMaxZeilen, liste.size()); ++i) {
            Anomalie muster;
            muster.typ = liste.at(i).first;
            m_fehlerbilder->insertRow(i);
            auto* name = zelle(Anomalie::typName(muster.typ));
            name->setIcon(muster.istHinweis() ? Stil::icon(Stil::Symbol::Info, QColor(37, 99, 235))
                                              : Stil::icon(Stil::Symbol::Warnung, QColor(217, 119, 6)));
            name->setToolTip(muster.istHinweis() ? tr("Hinweis – kein Serviceeinsatz nötig")
                                                 : tr("Befund – sollte geprüft werden"));
            m_fehlerbilder->setItem(i, 0, name);
            auto* anzahl = zelle(tr("%n Anlage(n)", nullptr, liste.at(i).second));
            anzahl->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_fehlerbilder->setItem(i, 1, anzahl);
        }
    }

    m_stand->setText(tr("Angemeldet als %1 · %2 Anlagen, davon %3 mit Messwerten ausgewertet · "
                        "Ein Klick auf eine Kachel oder Zeile führt zu den Details.")
                         .arg(Rollen::anzeigename(m_kontext.rolle))
                         .arg(d.anlagen().size())
                         .arg(ausgewertet));
}
