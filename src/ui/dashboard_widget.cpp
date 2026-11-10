#include "dashboard_widget.h"

#include <algorithm>
#include <map>

#include <QActionGroup>
#include <QGridLayout>
#include <QMenu>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "model/favoriten.h"
#include "ui/karte_view.h"
#include "ui/anordnungs_raster.h"
#include "ui/kennzahl_kachel.h"
#include "util/anzeige.h"
#include "util/abo.h"
#include "util/stil.h"

namespace {

constexpr int kMaxZeilen = 8;
constexpr int kZeilenhoehe = 26;
constexpr int kGriffHoehe = 36; ///< Überschrift der Karten, an der gezogen wird
const QColor kGruen(22, 163, 74);
const QColor kRot(220, 38, 38);
const QColor kOrange(217, 119, 6);
const QColor kBlau(37, 99, 235);
const QColor kViolett(124, 58, 237);
constexpr int kIdRolle = Qt::UserRole + 10;
constexpr int kMaxFavoriten = 6; ///< so viele Favoriten passen nebeneinander

// CO₂-Kachel: Netto-Vermeidungsfaktor der Photovoltaik laut Umweltbundesamt
// (690 g CO₂-Äquivalente je kWh) und die übliche Faustzahl, dass eine Buche
// etwa 12,5 kg CO₂ pro Jahr bindet (Wald-Zentrum der Universität Münster).
constexpr double kCo2KgProKwh = 0.690;
constexpr double kCo2KgProBucheUndJahr = 12.5;

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

/// Menü mit größeren Symbolen (Vorschaubilder der Anordnungen).
class GrosseMenueSymbole : public QProxyStyle {
public:
    explicit GrosseMenueSymbole(QObject* eltern)
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
        setParent(eltern);
    }
    int pixelMetric(PixelMetric m, const QStyleOption* o = nullptr, const QWidget* w = nullptr) const override
    {
        if (m == PM_SmallIconSize)
            return 30;
        return QProxyStyle::pixelMetric(m, o, w);
    }
};

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
    v->setSpacing(16);

    // ---------- Kennzahlenkacheln (klickbar) ----------
    auto kachel = [this](Stil::Symbol symbol, const QColor& farbe, const QString& titel, Ziel ziel) {
        auto* k = new KennzahlKachel(symbol, farbe, titel);
        k->setKlickbar(true);
        connect(k, &KennzahlKachel::geklickt, this, [this, ziel] { emit seiteOeffnen(ziel); });
        return k;
    };
    m_betrieb = kachel(Stil::Symbol::Sonne, kGruen, tr("Anlagen in Betrieb"), Ziel::Anlagen);
    m_stoerungen = kachel(Stil::Symbol::Warnung, kRot, tr("Offene Störungen"), Ziel::Stoerungen);
    m_pr = kachel(Stil::Symbol::Statistik, kBlau, tr("Performance Ratio (Median)"), Ziel::Statistik);
    m_verlust = kachel(Stil::Symbol::Euro, kOrange, tr("Geschätzter Ertragsverlust"), Ziel::Serviceplanung);
    m_daten = kachel(Stil::Symbol::Pruefung, kViolett, tr("Datenqualität"), Ziel::Datenpruefung);
    m_co2 = kachel(Stil::Symbol::Blatt, kGruen, tr("CO₂ vermieden"), Ziel::Statistik);
    // Kacheln und Karten lassen sich per Ziehen auf feste Plätze verschieben
    m_kachelRaster = new AnordnungsRaster(QStringLiteral("uebersicht/kacheln"));
    m_kachelRaster->setObjectName(QStringLiteral("UebersichtKacheln"));
    const QList<QPair<QString, KennzahlKachel*>> kachelListe{{QStringLiteral("betrieb"), m_betrieb},
                                                             {QStringLiteral("stoerungen"), m_stoerungen},
                                                             {QStringLiteral("pr"), m_pr},
                                                             {QStringLiteral("verlust"), m_verlust},
                                                             {QStringLiteral("daten"), m_daten},
                                                             {QStringLiteral("co2"), m_co2}};
    for (int i = 0; i < kachelListe.size(); ++i) {
        m_kachelRaster->neuerPlatz(0, i);
        m_kachelRaster->raster()->setColumnStretch(i, 1);
        m_kachelRaster->fuegeHinzu(kachelListe.at(i).first, kachelListe.at(i).second);
        kachelListe.at(i).second->setToolTip(tr("Klick öffnet die Seite · Ziehen verschiebt die Kachel"));
    }
    m_kachelRaster->ladeAnordnung();
    v->addWidget(m_kachelRaster);
    m_co2->setToolTip(tr("Vermiedene Treibhausgase durch den Solarstrom der Flotte im Auswertungszeitraum.\n"
                         "Grundlage: Netto-Vermeidungsfaktor Photovoltaik 690 g CO₂-Äquivalente je kWh "
                         "(Umweltbundesamt);\neine Buche bindet etwa 12,5 kg CO₂ im Jahr.\n"
                         "Klick öffnet die Statistik · Ziehen verschiebt die Kachel"));

    // ---------- Favoriten des Benutzers ----------
    m_favoritenInhalt = new QWidget;
    auto* favoriten = neueGruppe(tr("Favoriten"), m_favoritenInhalt, tr("Alle Favoriten in der Anlagenliste"),
                                 Ziel::Favoriten);
    favoriten->setObjectName(QStringLiteral("FavoritenGruppe"));
    favoriten->setToolTip(QString());
    v->addWidget(favoriten);
    connect(Favoriten::instanz(), &Favoriten::geaendert, this, &DashboardWidget::aktualisiereFavoriten);

    // ---------- Karte und Alarmliste nebeneinander ----------
    auto* karte = new KarteView(m_kontext, nullptr, KarteView::Modus::Kompakt);
    karte->setMinimumHeight(330);
    karte->setToolTip(tr("Rot: kritische Störung, gestört oder Fehlerbild · Orange: offene Störung oder Wartung · Grün: in Ordnung\n"
                         "Klick auf einen Punkt öffnet die Anlage, Mausrad zoomt."));
    connect(karte, &KarteView::anlageGewaehlt, this, &DashboardWidget::anlageOeffnen);

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

    // Karten auf festen Plätzen; mehrere vordefinierte Anordnungen zur Auswahl
    m_kartenRaster = new AnordnungsRaster(QStringLiteral("uebersicht/karten"));
    using V = AnordnungsRaster::Vorlage;
    const QString k = QStringLiteral("karte"), a = QStringLiteral("alarme"), l = QStringLiteral("verluste"),
                  f = QStringLiteral("fehlerbilder");
    m_kartenRaster->neueVorlage(V{QStringLiteral("standard"), tr("Standard"),
                                  {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {3, 2}, {1, 0}, {k, a, l, f}});
    m_kartenRaster->neueVorlage(V{QStringLiteral("karte_gross"), tr("Karte groß"),
                                  {{0, 0, 3, 1}, {0, 1}, {1, 1}, {2, 1}}, {3, 2}, {1, 1, 1}, {k, a, l, f}});
    m_kartenRaster->neueVorlage(V{QStringLiteral("karte_oben"), tr("Karte oben, Listen darunter"),
                                  {{0, 0, 1, 3}, {1, 0}, {1, 1}, {1, 2}}, {1, 1, 1}, {1, 0}, {k, a, l, f}});
    m_kartenRaster->neueVorlage(V{QStringLiteral("listen_oben"), tr("Listen oben, Karte darunter"),
                                  {{0, 0}, {0, 1}, {0, 2}, {1, 0, 1, 3}}, {1, 1, 1}, {0, 1}, {a, l, f, k}});
    m_kartenRaster->neueVorlage(V{QStringLiteral("gleich"), tr("Vier gleich große Felder"),
                                  {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {1, 1}, {1, 1}, {k, a, l, f}});
    m_kartenRaster->neueVorlage(V{QStringLiteral("alarme_zuerst"), tr("Alarme zuerst"),
                                  {{0, 0, 1, 2}, {1, 0}, {1, 1}, {2, 0, 1, 2}}, {3, 2}, {0, 1, 0}, {a, k, f, l}});
    m_kartenRaster->fuegeHinzu(QStringLiteral("karte"),
                               neueGruppe(tr("Anlagenkarte – Handlungsbedarf"), karte, tr("Karte öffnen"), Ziel::Karte),
                               kGriffHoehe);
    m_kartenRaster->fuegeHinzu(QStringLiteral("alarme"),
                               neueGruppe(tr("Aktive Alarme (kritisch)"), m_dringend, tr("Alle Störungen"), Ziel::Stoerungen),
                               kGriffHoehe);
    m_kartenRaster->fuegeHinzu(QStringLiteral("verluste"),
                               neueGruppe(tr("Größte Ertragsverluste"), m_verluste, tr("Zur Serviceplanung"),
                                          Ziel::Serviceplanung),
                               kGriffHoehe);
    m_kartenRaster->fuegeHinzu(QStringLiteral("fehlerbilder"),
                               neueGruppe(tr("Erkannte Fehlerbilder"), m_fehlerbilder, tr("Zur Anlagenliste"), Ziel::Anlagen),
                               kGriffHoehe);
    m_kartenRaster->ladeAnordnung();
    v->addWidget(m_kartenRaster, 1);

    // Ultimate-Anordnungen (Spaß-Abo): ohne Ultimate zurück auf "Standard" –
    // gleich beim Aufbau (gespeicherte Anordnung) und beim Wechsel der Stufe
    pruefeAboAnordnung();
    connect(Abo::instanz(), &Abo::stufeGeaendert, this, &DashboardWidget::pruefeAboAnordnung);

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

void DashboardWidget::pruefeAboAnordnung()
{
    const QString aktuell = m_kartenRaster->aktuelleVorlage();
    if (Abo::erlaubt(Abo::Funktion::Anordnungen) || aktuell.isEmpty() || aktuell == QLatin1String("standard"))
        return;
    m_kartenRaster->waehleVorlage(QStringLiteral("standard"));
}

void DashboardWidget::anordnungZuruecksetzen()
{
    m_kachelRaster->zuruecksetzen();
    m_kartenRaster->waehleVorlage(m_kartenRaster->vorlagen().constFirst().id);
}

QAction* DashboardWidget::anordnungsAktion()
{
    // Knopf "Anordnung" mit Menü: vordefinierte Anordnungen (mit Vorschau)
    // und Zurücksetzen auf die Standardanordnung
    auto* aktion = new QAction(Stil::icon(Stil::Symbol::Uebersicht, Stil::text()), tr("Anordnung"), this);
    aktion->setToolTip(tr("Vordefinierte Anordnung der Karten wählen"));
    auto* menue = new QMenu(this);
    auto* gruppe = new QActionGroup(menue);
    menue->setStyle(new GrosseMenueSymbole(menue)); // größere Vorschaubilder
    // gewählte Anordnung fett und mit Haken markieren; ohne Ultimate sind alle
    // Vorlagen außer "Standard" gesperrt (Spaß-Abo, siehe util/abo.h)
    auto markiere = [this, gruppe] {
        const auto& vorlagen = m_kartenRaster->vorlagen();
        const bool frei = Abo::erlaubt(Abo::Funktion::Anordnungen);
        for (int i = 0; i < vorlagen.size() && i < gruppe->actions().size(); ++i) {
            QAction* a = gruppe->actions().at(i);
            const auto& v = vorlagen.at(i);
            const bool aktiv = v.id == m_kartenRaster->aktuelleVorlage();
            a->setChecked(aktiv);
            QFont f = a->font();
            f.setBold(aktiv);
            a->setFont(f);
            QString text = v.name;
            if (!frei && v.id != QLatin1String("standard"))
                text += QStringLiteral("  ·  ") + tr("Ultimate");
            if (aktiv)
                text += QStringLiteral("  ✓");
            a->setText(text);
        }
    };
    for (const AnordnungsRaster::Vorlage& v : m_kartenRaster->vorlagen()) {
        QAction* a = menue->addAction(m_kartenRaster->vorschau(v.id), v.name);
        a->setCheckable(true);
        gruppe->addAction(a);
        connect(a, &QAction::triggered, this, [this, id = v.id, markiere] {
            if (id != QLatin1String("standard") && !Abo::pruefe(Abo::Funktion::Anordnungen)
                && !Abo::erlaubt(Abo::Funktion::Anordnungen)) {
                markiere();
                return;
            }
            m_kartenRaster->waehleVorlage(id);
        });
    }
    menue->addSeparator();
    menue->addAction(tr("Anordnung zurücksetzen"), this, &DashboardWidget::anordnungZuruecksetzen);
    markiere();
    connect(m_kartenRaster, &AnordnungsRaster::anordnungGeaendert, menue, markiere);
    connect(Abo::instanz(), &Abo::stufeGeaendert, menue, markiere);
    aktion->setMenu(menue);
    return aktion;
}

QGroupBox* DashboardWidget::neueGruppe(const QString& titel, QWidget* inhalt, const QString& linkText, Ziel sprung)
{
    auto* g = new QGroupBox(titel);
    g->setToolTip(tr("An der Überschrift ziehen, um die Karte auf einen anderen Platz zu verschieben"));
    auto* v = new QVBoxLayout(g);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(inhalt, 1);
    if (!linkText.isEmpty()) {
        auto* link = new QLabel(QStringLiteral("<a href=\"%1\">%2 →</a>").arg(zielName(sprung), linkText.toHtmlEscaped()), g);
        link->setContentsMargins(10, 6, 10, 6);
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
    t->setMinimumHeight(t->header()->sizeHint().height() + kZeilenhoehe * kMaxZeilen + 2 * t->frameWidth() + 4);
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
    aktualisiereFavoriten();

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
    m_betrieb->setzeWert(QStringLiteral("%1 / %2").arg(inBetrieb).arg(d.anlagen().size()),
                         tr("%1 gestört · %2 in Wartung").arg(gestoert).arg(wartung));
    m_betrieb->setzeFarbe(gestoert > 0 ? kOrange : kGruen);

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
    m_stoerungen->setzeWert(QString::number(offen), tr("davon %1 kritisch").arg(kritisch.size()));
    m_stoerungen->setzeFarbe(!kritisch.isEmpty() ? kRot : offen > 0 ? kOrange : kGruen);

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
        it->setIcon(0, Stil::icon(Stil::Symbol::Warnung, kRot));
        it->setData(0, kIdRolle, s.anlageId);
    }

    // ---------- Datenqualität ----------
    const int fehler = d.anzahlMeldungen(Pruefmeldung::Stufe::Fehler);
    m_daten->setzeWert(tr("%1 Fehler").arg(fehler),
                       tr("%1 Warnungen beim Einlesen").arg(d.anzahlMeldungen(Pruefmeldung::Stufe::Warnung)));

    // ---------- Auswertung (erst nach der Flottenanalyse) ----------
    m_verluste->clear();
    m_fehlerbilder->clear();
    if (!fa.istFertig()) {
        const QString laeuft = tr("Flottenanalyse läuft …");
        for (KennzahlKachel* k : {m_pr, m_verlust, m_co2})
            k->setzeWert(QStringLiteral("…"), laeuft);
        leer(m_verluste, laeuft);
        leer(m_fehlerbilder, laeuft);
        passeSpaltenAn(m_dringend);
        return;
    }

    int ausgewertet = 0;
    double verlustEuro = 0.0, verlustKwh = 0.0, energieKwh = 0.0;
    QVector<const Kennzahl*> verluste;
    std::map<Anomalie::Typ, int> bilder;
    for (const Kennzahl& k : fa.kennzahlen()) {
        if (!k.ok)
            continue;
        ++ausgewertet;
        verlustEuro += k.verlustEuro;
        verlustKwh += k.verlustKwh;
        energieKwh += k.auswertung.energieKwh;
        if (k.verlustEuro > 0.005)
            verluste.append(&k);
        QSet<int> gezaehlt;
        for (const Anomalie& an : k.auswertung.anomalien)
            if (!gezaehlt.contains(int(an.typ))) {
                gezaehlt.insert(int(an.typ));
                ++bilder[an.typ];
            }
    }

    m_pr->setzeWert(Anzeige::pr(fa.medianPrFlotte()),
                    tr("%1 von %2 Anlagen ausgewertet").arg(ausgewertet).arg(d.anlagen().size()));
    m_verlust->setzeWert(Anzeige::euro(verlustEuro), tr("%1 in 14 Tagen").arg(Anzeige::kwh(verlustKwh)));
    const double co2Kg = energieKwh * kCo2KgProKwh;
    const QString co2Text = co2Kg >= 1000.0 ? tr("%1 t").arg(Anzeige::zahl(co2Kg / 1000.0, 1))
                                            : tr("%1 kg").arg(Anzeige::zahl(co2Kg, 0));
    m_co2->setzeWert(co2Text, tr("in 14 Tagen · ≈ %1 Buchen in einem Jahr")
                                  .arg(Anzeige::zahl(co2Kg / kCo2KgProBucheUndJahr, 0)));

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

void DashboardWidget::aktualisiereFavoriten()
{
    if (!m_favoritenInhalt)
        return;
    // Inhalt neu aufbauen (höchstens kMaxFavoriten Knöpfe, Rest als "+N")
    delete m_favoritenInhalt->layout();
    qDeleteAll(m_favoritenInhalt->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    auto* h = new QHBoxLayout(m_favoritenInhalt);
    h->setContentsMargins(12, 10, 12, 2);
    h->setSpacing(10);

    const Datenbasis& d = *m_kontext.daten;
    QStringList ids;
    for (const QString& id : Favoriten::instanz()->liste())
        if (d.anlage(id))
            ids.append(id);
    if (ids.isEmpty()) {
        auto* hinweis = new QLabel(tr("Noch keine Favoriten. Wichtige Anlagen in der Anlagenliste mit Strg+D, per "
                                      "Rechtsklick oder über den Stern im Detailbereich anpinnen."),
                                   m_favoritenInhalt);
        hinweis->setObjectName(QStringLiteral("Gedimmt"));
        hinweis->setWordWrap(true);
        h->addWidget(hinweis, 1);
        return;
    }
    for (int i = 0; i < qMin<qsizetype>(kMaxFavoriten, ids.size()); ++i) {
        const Anlage* a = d.anlage(ids.at(i));
        const int offen = d.offeneStoerungen(a->anlageId);
        const int kritisch = d.offeneKritischeStoerungen(a->anlageId);
        auto* k = new QToolButton(m_favoritenInhalt);
        k->setObjectName(QStringLiteral("FavoritChip"));
        k->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        k->setCursor(Qt::PointingHandCursor);
        // Farbe: kritisch rot, offen orange, sonst Status
        const QColor farbe = kritisch > 0 ? kRot : offen > 0 ? kOrange : Anzeige::statusFarbe(a->status);
        QPixmap punkt(14, 14);
        punkt.fill(Qt::transparent);
        {
            QPainter p(&punkt);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(farbe);
            p.drawEllipse(QRectF(2, 2, 10, 10));
        }
        k->setIcon(QIcon(punkt));
        const QString zweite = offen > 0 ? tr("%n offene Störung(en)", nullptr, offen) : Anzeige::statusName(a->status);
        k->setText(QStringLiteral("%1 · %2\n%3").arg(a->anlageId, a->ort, zweite));
        k->setToolTip(tr("%1 in %2 · %3 kWp · Klick öffnet die Anlage")
                          .arg(a->anlageId, a->ort, Anzeige::zahl(a->leistungKwp, 1)));
        k->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        connect(k, &QToolButton::clicked, this, [this, id = a->anlageId] { emit anlageOeffnen(id); });
        h->addWidget(k);
    }
    if (ids.size() > kMaxFavoriten) {
        auto* mehr = new QLabel(tr("+%1 weitere").arg(ids.size() - kMaxFavoriten), m_favoritenInhalt);
        mehr->setObjectName(QStringLiteral("Gedimmt"));
        h->addWidget(mehr);
    }
    h->addStretch(1);
}
