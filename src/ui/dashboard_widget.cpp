#include "dashboard_widget.h"

#include <algorithm>
#include <map>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "ui/karte_view.h"
#include "util/anzeige.h"
#include "util/stil.h"

namespace {

constexpr int kMaxZeilen = 8;
constexpr int kZeilenhoehe = 20;
constexpr int kIdRolle = Qt::UserRole + 10;

QString zielName(DashboardWidget::Ziel z)
{
    return QString::number(int(z));
}

/// Einen Hinweistext als einzige, nicht anklickbare Zeile zeigen.
void leer(QTreeWidget* t, const QString& text)
{
    auto* it = new QTreeWidgetItem(t, {text});
    it->setForeground(0, Stil::textGedimmt());
    it->setFlags(Qt::ItemIsEnabled);
    it->setFirstColumnSpanned(true);
}

/// Einheitliche Zeilenhöhe, auch für Zeilen ohne Symbol.
class ZeilenDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        return QStyledItemDelegate::sizeHint(o, i).expandedTo(QSize(0, kZeilenhoehe));
    }
};

/// Spaltenbreiten nach dem Füllen an den Inhalt anpassen (außer der gestreckten Spalte).
void passeSpaltenAn(QTreeWidget* t)
{
    for (int c = 0; c < t->columnCount(); ++c)
        if (t->header()->sectionResizeMode(c) != QHeaderView::Stretch) {
            t->resizeColumnToContents(c);
            t->setColumnWidth(c, qMax(t->columnWidth(c), t->header()->sectionSizeHint(c)) + 8);
        }
}

} // namespace

DashboardWidget::DashboardWidget(const AppKontext& kontext, const QList<QAction*>& schnellzugriff, QWidget* parent)
    : QWidget(parent)
    , m_kontext(kontext)
{
    Q_UNUSED(schnellzugriff)
    auto* inhalt = new QWidget;
    auto* v = new QVBoxLayout(inhalt);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // ---------- Flottenstatus: Beschriftung/Wert-Paare ----------
    auto* status = new QWidget;
    auto* raster = new QGridLayout(status);
    raster->setContentsMargins(4, 2, 4, 2);
    raster->setHorizontalSpacing(12);
    raster->setVerticalSpacing(6);
    m_betrieb = neuerWert(raster, 0, 0, tr("Anlagen:"));
    m_stoerungen = neuerWert(raster, 1, 0, tr("Offene Störungen:"));
    m_pr = neuerWert(raster, 0, 1, tr("Median-PR der Flotte:"));
    m_verlust = neuerWert(raster, 1, 1, tr("Geschätzter Ertragsverlust:"));
    m_daten = neuerWert(raster, 0, 2, tr("Datenprüfung:"));
    m_ausgewertet = neuerWert(raster, 1, 2, tr("Ausgewertet:"));
    for (int s : {1, 3, 5})
        raster->setColumnStretch(s, 1);
    v->addWidget(neueGruppe(tr("Flottenstatus"), status, {}, Ziel::Anlagen));

    // ---------- Karte ----------
    auto* karte = new KarteView(m_kontext, nullptr, KarteView::Modus::Kompakt);
    karte->setMinimumHeight(240);
    karte->setToolTip(tr("Rot: kritische Störung, gestört oder Fehlerbild · Orange: offene Störung oder Wartung · Grün: in Ordnung\n"
                         "Klick auf einen Punkt öffnet die Anlage, Mausrad zoomt."));
    connect(karte, &KarteView::anlageGewaehlt, this, &DashboardWidget::anlageOeffnen);
    v->addWidget(neueGruppe(tr("Karte – Handlungsbedarf"), karte, tr("Kartenansicht öffnen"), Ziel::Karte), 1);

    // ---------- Listen ----------
    m_dringend = neueListe({tr("Anlage"), tr("Ort"), tr("Meldung"), tr("Seit")});
    m_verluste = neueListe({tr("Anlage"), tr("Ort"), tr("Ursache"), tr("Verlust")});
    m_fehlerbilder = neueListe({tr("Fehlerbild"), tr("Art"), tr("Anlagen")});
    m_verluste->headerItem()->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    m_fehlerbilder->headerItem()->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    for (QTreeWidget* t : {m_dringend, m_verluste})
        connect(t, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* it) {
            const QString id = it ? it->data(0, kIdRolle).toString() : QString();
            if (!id.isEmpty())
                emit anlageOeffnen(id);
        });

    auto* listen = new QHBoxLayout;
    listen->setSpacing(8);
    listen->addWidget(neueGruppe(tr("Offene kritische Störungen"), m_dringend, tr("Alle Störungen anzeigen"),
                                 Ziel::Stoerungen), 3);
    listen->addWidget(neueGruppe(tr("Größte Ertragsverluste"), m_verluste, tr("Serviceplanung öffnen"),
                                 Ziel::Serviceplanung), 3);
    listen->addWidget(neueGruppe(tr("Erkannte Fehlerbilder"), m_fehlerbilder, tr("Anlagenliste öffnen"), Ziel::Anlagen), 2);
    v->addLayout(listen);

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

QLabel* DashboardWidget::neuerWert(QGridLayout* raster, int zeile, int spalte, const QString& titel)
{
    auto* t = new QLabel(titel);
    t->setObjectName(QStringLiteral("Gedimmt"));
    auto* w = new QLabel(QStringLiteral("–"));
    w->setTextFormat(Qt::RichText);
    connect(w, &QLabel::linkActivated, this, [this](const QString& ziel) {
        emit seiteOeffnen(static_cast<Ziel>(ziel.toInt()));
    });
    raster->addWidget(t, zeile, spalte * 2);
    raster->addWidget(w, zeile, spalte * 2 + 1);
    return w;
}

void DashboardWidget::verlinke(QLabel* label, const QString& text, Ziel ziel)
{
    label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(zielName(ziel), text.toHtmlEscaped()));
}

QGroupBox* DashboardWidget::neueGruppe(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung)
{
    auto* g = new QGroupBox(titel);
    auto* v = new QVBoxLayout(g);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(4);
    v->addWidget(inhalt, 1);
    if (!linkText.isEmpty()) {
        auto* link = new QLabel(QStringLiteral("<a href=\"%1\">%2</a>").arg(zielName(sprung), linkText.toHtmlEscaped()), g);
        link->setAlignment(Qt::AlignRight);
        connect(link, &QLabel::linkActivated, this, [this, sprung] { emit seiteOeffnen(sprung); });
        v->addWidget(link);
    }
    return g;
}

QTreeWidget* DashboardWidget::neueListe(const QStringList& spalten)
{
    auto* t = new QTreeWidget;
    t->setColumnCount(int(spalten.size()));
    t->setHeaderLabels(spalten);
    t->setRootIsDecorated(false);
    t->setItemDelegate(new ZeilenDelegate(t));
    t->setUniformRowHeights(true);
    t->setAllColumnsShowFocus(true);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setToolTip(tr("Doppelklick öffnet die Anlage"));
    t->header()->setStretchLastSection(false);
    t->header()->setSectionResizeMode(QHeaderView::Interactive);
    t->header()->setSectionResizeMode(spalten.size() > 3 ? 2 : 0, QHeaderView::Stretch);
    // Platz für kMaxZeilen Zeilen, damit die Listen nicht scrollen
    t->setFixedHeight(t->header()->sizeHint().height() + kZeilenhoehe * kMaxZeilen + 2 * t->frameWidth() + 4);
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
    verlinke(m_betrieb, tr("%1 von %2 in Betrieb (%3 gestört, %4 in Wartung)")
                            .arg(inBetrieb).arg(d.anlagen().size()).arg(gestoert).arg(wartung),
             Ziel::Anlagen);

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
    verlinke(m_stoerungen, tr("%1, davon %2 kritisch").arg(offen).arg(kritisch.size()), Ziel::Stoerungen);

    std::sort(kritisch.begin(), kritisch.end(),
              [](const Stoerung& x, const Stoerung& y) { return x.zeitpunkt > y.zeitpunkt; });
    m_dringend->clear();
    if (kritisch.isEmpty())
        leer(m_dringend, tr("Keine offenen kritischen Störungen."));
    for (int i = 0; i < qMin<qsizetype>(kMaxZeilen, kritisch.size()); ++i) {
        const Stoerung& s = kritisch.at(i);
        const Anlage* a = d.anlage(s.anlageId);
        auto* it = new QTreeWidgetItem(m_dringend, {s.anlageId, a ? a->ort : QString(), s.beschreibung,
                                                    Anzeige::zeitpunkt(s.zeitpunkt)});
        it->setIcon(0, Stil::icon(Stil::Symbol::Warnung, QColor(196, 43, 28)));
        it->setData(0, kIdRolle, s.anlageId);
    }

    // ---------- Datenqualität ----------
    const int fehler = d.anzahlMeldungen(Pruefmeldung::Stufe::Fehler);
    verlinke(m_daten, tr("%1 Fehler, %2 Warnungen beim Einlesen")
                          .arg(fehler).arg(d.anzahlMeldungen(Pruefmeldung::Stufe::Warnung)),
             Ziel::Datenpruefung);

    // ---------- Auswertung (erst nach der Flottenanalyse) ----------
    m_verluste->clear();
    m_fehlerbilder->clear();
    if (!fa.istFertig()) {
        const QString laeuft = tr("Flottenanalyse läuft …");
        for (QLabel* l : {m_pr, m_verlust, m_ausgewertet})
            l->setText(laeuft);
        leer(m_verluste, laeuft);
        leer(m_fehlerbilder, laeuft);
        passeSpaltenAn(m_dringend);
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

    verlinke(m_pr, Anzeige::pr(fa.medianPrFlotte()), Ziel::Statistik);
    verlinke(m_verlust, tr("%1 (%2)").arg(Anzeige::euro(verlustEuro), Anzeige::kwh(verlustKwh)), Ziel::Serviceplanung);
    m_ausgewertet->setText(tr("%1 von %2 Anlagen mit Messwerten").arg(ausgewertet).arg(d.anlagen().size()));

    std::sort(verluste.begin(), verluste.end(),
              [](const Kennzahl* x, const Kennzahl* y) { return x->verlustEuro > y->verlustEuro; });
    if (verluste.isEmpty())
        leer(m_verluste, tr("Keine nennenswerten Verluste."));
    for (int i = 0; i < qMin<qsizetype>(kMaxZeilen, verluste.size()); ++i) {
        const Kennzahl* k = verluste.at(i);
        const Anlage* a = d.anlage(k->anlageId);
        QStringList namen;
        for (const Anomalie& an : k->auswertung.anomalien)
            if (!an.istHinweis())
                namen.append(Anomalie::typName(an.typ));
        namen.removeDuplicates();
        auto* it = new QTreeWidgetItem(m_verluste, {k->anlageId, a ? a->ort : QString(),
                                                    namen.isEmpty() ? tr("Minderertrag") : namen.join(QStringLiteral(", ")),
                                                    Anzeige::euro(k->verlustEuro)});
        it->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        it->setData(0, kIdRolle, k->anlageId);
    }

    QVector<std::pair<Anomalie::Typ, int>> liste(bilder.begin(), bilder.end());
    std::sort(liste.begin(), liste.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
    if (liste.isEmpty())
        leer(m_fehlerbilder, tr("Keine Fehlerbilder erkannt."));
    for (const auto& [typ, anzahl] : liste) {
        Anomalie muster;
        muster.typ = typ;
        auto* it = new QTreeWidgetItem(m_fehlerbilder, {Anomalie::typName(typ),
                                                        muster.istHinweis() ? tr("Hinweis") : tr("Befund"),
                                                        QString::number(anzahl)});
        it->setIcon(0, muster.istHinweis() ? Stil::icon(Stil::Symbol::Info, QColor(0, 95, 184))
                                           : Stil::icon(Stil::Symbol::Warnung, QColor(202, 120, 0)));
        it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    }

    for (QTreeWidget* t : {m_dringend, m_verluste, m_fehlerbilder})
        passeSpaltenAn(t);
}
