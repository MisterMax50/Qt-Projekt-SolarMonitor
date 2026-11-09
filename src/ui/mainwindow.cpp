#include "mainwindow.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QListWidget>
#include <QPainter>
#include <QSettings>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QUndoView>
#include <QUrl>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "repo/csv_parser.h"
#include "ui/aenderungen.h"
#include "ui/anlage_info.h"
#include "ui/anlage_model.h"
#include "ui/badge_delegate.h"
#include "ui/dashboard_widget.h"
#include "ui/detail_dialog.h"
#include "ui/edit_dialog.h"
#include "ui/karte_view.h"
#include "ui/login_dialog.h"
#include "ui/menueband.h"
#include "ui/pdf_bericht.h"
#include "ui/pruefprotokoll_widget.h"
#include "ui/serviceplanung.h"
#include "ui/statistik_widget.h"
#include "ui/stoerung_dialog.h"
#include "ui/stoerung_model.h"
#include "util/anzeige.h"
#include "util/sprache.h"
#include "util/stil.h"
#include "util/zeit.h"

namespace {
enum Reiter { ReiterUebersicht, ReiterAnlagen, ReiterStoerungen, ReiterKarte, ReiterStatistik, ReiterService, ReiterPruefung };
const QString kQuelleJson = QStringLiteral("Verlauf (JSON)");

QIcon farbIcon(const QColor& farbe)
{
    QPixmap pm(12, 12);
    pm.fill(farbe);
    return QIcon(pm);
}
/// Rolle für den Grundnamen eines Navigationseintrags (ohne Zähler)
constexpr int kNameRolle = Qt::UserRole + 1;
} // namespace

MainWindow::MainWindow(const AppKontext& kontext, QWidget* parent)
    : QMainWindow(parent)
    , m_kontext(kontext)
{
    m_anlageModel = new AnlageModel(m_kontext, this);
    m_anlageFilter = new AnlageFilterModel(this);
    m_anlageFilter->setSourceModel(m_anlageModel);
    m_stoerungModel = new StoerungModel(m_kontext, this);
    m_stoerungFilter = new StoerungFilterModel(this);
    m_stoerungFilter->setSourceModel(m_stoerungModel);
    m_undo = new QUndoStack(this);
    m_undo->setUndoLimit(100);

    connect(m_anlageFilter, &QAbstractItemModel::rowsInserted, this, &MainWindow::aktualisiereZaehler);
    connect(m_anlageFilter, &QAbstractItemModel::rowsRemoved, this, &MainWindow::aktualisiereZaehler);
    connect(m_anlageFilter, &QAbstractItemModel::modelReset, this, &MainWindow::aktualisiereZaehler);
    connect(m_anlageFilter, &QAbstractItemModel::layoutChanged, this, &MainWindow::aktualisiereZaehler);
    connect(m_stoerungFilter, &QAbstractItemModel::rowsInserted, this, &MainWindow::aktualisiereZaehler);
    connect(m_stoerungFilter, &QAbstractItemModel::rowsRemoved, this, &MainWindow::aktualisiereZaehler);
    connect(m_stoerungFilter, &QAbstractItemModel::layoutChanged, this, &MainWindow::aktualisiereZaehler);

    connect(m_kontext.analyse, &FlottenAnalyse::fortschritt, this, &MainWindow::analyseFortschritt);
    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &MainWindow::analyseFertig);

    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &MainWindow::aktualisiereNavigation);
    connect(m_kontext.daten, &Datenbasis::meldungenGeaendert, this, &MainWindow::aktualisiereNavigation);
    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &MainWindow::aktualisiereNavigation);

    resize(1440, 900);
    restoreGeometry(QSettings().value(QStringLiteral("fenster")).toByteArray());
    baueUi();
}

void MainWindow::zeigeBeimStart()
{
    if (QSettings().value(QStringLiteral("vollbild"), true).toBool())
        showFullScreen();
    else
        showMaximized();
}

void MainWindow::setzeVollbild(bool an)
{
    QSettings().setValue(QStringLiteral("vollbild"), an);
    if (an)
        showFullScreen();
    else
        showMaximized();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings().setValue(QStringLiteral("fenster"), saveGeometry());
    QMainWindow::closeEvent(event);
}

void MainWindow::setzeDesign(bool dunkel)
{
    QSettings().setValue(QStringLiteral("dunkel"), dunkel);
    // verzögert, weil der auslösende Knopf beim Neuaufbau gelöscht wird
    QTimer::singleShot(0, this, [this, dunkel] {
        Stil::anwenden(dunkel);
        baueUi();
    });
}

void MainWindow::changeEvent(QEvent* event)
{
    // Beim Sprachwechsel kommen mehrere LanguageChange-Ereignisse
    // (Translator entfernt/installiert); die Oberfläche wird nur einmal
    // danach neu aufgebaut.
    if (event->type() == QEvent::LanguageChange && m_tabs && !m_neuaufbauGeplant) {
        m_neuaufbauGeplant = true;
        QTimer::singleShot(0, this, [this] {
            m_neuaufbauGeplant = false;
            baueUi();
        });
    }
    QMainWindow::changeEvent(event);
}

// -------------------------------------------------------------------
//  Aufbau der Oberfläche
// -------------------------------------------------------------------

void MainWindow::baueUi()
{
    const int aktuelleSeite = m_tabs ? m_tabs->currentIndex() : 0;
    setWindowTitle(tr("SolarMonitor – Monitoring der PV-Flotte"));
    erzeugeAktionen();

    m_tabs = new QStackedWidget;

    auto* uebersicht = new DashboardWidget(m_kontext, {m_aktPdfFlotte, m_aktExportBericht});
    connect(uebersicht, &DashboardWidget::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    connect(uebersicht, &DashboardWidget::seiteOeffnen, this, [this](DashboardWidget::Ziel ziel) {
        switch (ziel) {
        case DashboardWidget::Ziel::Anlagen: m_tabs->setCurrentIndex(ReiterAnlagen); break;
        case DashboardWidget::Ziel::Stoerungen: m_tabs->setCurrentIndex(ReiterStoerungen); break;
        case DashboardWidget::Ziel::Karte: m_tabs->setCurrentIndex(ReiterKarte); break;
        case DashboardWidget::Ziel::Statistik: m_tabs->setCurrentIndex(ReiterStatistik); break;
        case DashboardWidget::Ziel::Serviceplanung: m_tabs->setCurrentIndex(ReiterService); break;
        case DashboardWidget::Ziel::Datenpruefung: m_tabs->setCurrentIndex(ReiterPruefung); break;
        }
    });
    m_tabs->addWidget(seite(tr("Übersicht"), tr("Der Zustand der Flotte auf einen Blick – was ist zu tun?"), uebersicht));

    m_tabs->addWidget(seite(tr("Anlagen"),
                            tr("Alle PV-Anlagen der Flotte. Doppelklick öffnet die Details, Rechtsklick zeigt weitere Aktionen."),
                            baueAnlagenSeite(), {m_aktRueckgaengig, m_aktWiederholen, m_aktDetails, m_aktBearbeiten, m_aktStoerung, m_aktExportAnlagen}));
    m_tabs->addWidget(seite(tr("Störungen"),
                            tr("Störungsmeldungen seit 2024. Doppelklick öffnet eine Meldung, Rechtsklick ändert den Status."),
                            baueStoerungsSeite(), {m_aktRueckgaengig, m_aktWiederholen, m_aktExportStoerungen}));

    m_karte = new KarteView(m_kontext);
    connect(m_karte, &KarteView::anlageGewaehlt, this, [this](const QString& id) { oeffneAnlage(id); });
    m_tabs->addWidget(seite(tr("Karte"), tr("Lage der Anlagen und Wetterstationen. Mausrad zoomt, Ziehen verschiebt, Klick öffnet eine Anlage."),
                            m_karte));

    m_tabs->addWidget(seite(tr("Statistik"), tr("Kennzahlen und Auswertungen über die gesamte Flotte."),
                            new StatistikWidget(m_kontext), {m_aktPdfFlotte, m_aktExportBericht}));

    auto* service = new Serviceplanung(m_kontext);
    connect(service, &Serviceplanung::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    connect(service, &Serviceplanung::tourAnzeigen, this,
            [this](const QPointF& start, const QStringList& ids, const QVector<QPointF>& verlauf, bool strasse,
                   double umkreisKm) {
        m_karte->setTour(start, ids, verlauf, strasse, umkreisKm);
        m_tabs->setCurrentIndex(ReiterKarte);
    });
    m_tabs->addWidget(seite(tr("Serviceplanung"),
                            tr("Welche Anlagen sollten zuerst angefahren werden? Sortiert nach Priorität und geschätztem Ertragsverlust."),
                            service));

    auto* pruefung = new PruefprotokollWidget(m_kontext);
    connect(pruefung, &PruefprotokollWidget::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    m_tabs->addWidget(seite(tr("Datenprüfung"),
                            tr("Alle Auffälligkeiten, die beim Einlesen der Dateien gefunden und teilweise automatisch korrigiert wurden."),
                            pruefung));

    // Navigationsbereich links, Inhalt rechts, Breite per Trennlinie verstellbar
    auto* zentrum = new QSplitter(Qt::Horizontal);
    zentrum->setChildrenCollapsible(false);
    zentrum->setHandleWidth(1);
    zentrum->addWidget(baueNavigation());
    zentrum->addWidget(m_tabs);
    zentrum->setStretchFactor(1, 1);
    zentrum->setSizes({190, 1200});
    setCentralWidget(zentrum); // löscht die alte Oberfläche

    connect(m_tabs, &QStackedWidget::currentChanged, m_navigation, [this](int i) { m_navigation->setCurrentRow(i); });
    m_tabs->setCurrentIndex(aktuelleSeite);
    m_navigation->setCurrentRow(aktuelleSeite);

    baueMenues();
    baueStatusleiste();
    aktualisiereRechte();
    aktualisiereZaehler();
    aktualisiereNavigation();
    aktualisiereFilterKnopf();
}

QWidget* MainWindow::seite(const QString& titel, const QString& hinweis, QWidget* inhalt,
                           const QList<QAction*>& aktionen)
{
    // Kein Seitenkopf: Wo man ist, zeigt die Navigation; Befehle stehen in
    // der Befehlsleiste und im Menü, wie in Explorer oder Outlook.
    Q_UNUSED(aktionen)
    Q_UNUSED(hinweis)
    auto* w = new QWidget;
    w->setObjectName(QStringLiteral("Inhalt"));
    w->setWindowTitle(titel);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(6);
    if (QLayout* l = inhalt->layout())
        l->setContentsMargins(0, 0, 0, 0);
    v->addWidget(inhalt, 1);
    return w;
}

QWidget* MainWindow::baueNavigation()
{
    auto* bereich = new QWidget;
    bereich->setObjectName(QStringLiteral("Navigationsbereich"));
    bereich->setAttribute(Qt::WA_StyledBackground, true);

    m_navigation = new QListWidget(bereich);
    m_navigation->setObjectName(QStringLiteral("Navigation"));
    m_navigation->setIconSize(QSize(18, 18));
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setFrameShape(QFrame::NoFrame);
    const QColor iconFarbe = Stil::text();
    const QList<QPair<Stil::Symbol, QString>> eintraege{
        {Stil::Symbol::Uebersicht, tr("Übersicht")},
        {Stil::Symbol::Liste, tr("Anlagen")},       {Stil::Symbol::Warnung, tr("Störungen")},
        {Stil::Symbol::Karte, tr("Karte")},         {Stil::Symbol::Statistik, tr("Statistik")},
        {Stil::Symbol::Werkzeug, tr("Serviceplanung")}, {Stil::Symbol::Pruefung, tr("Datenprüfung")}};
    for (const auto& e : eintraege) {
        auto* it = new QListWidgetItem(Stil::icon(e.first, iconFarbe), e.second);
        it->setData(kNameRolle, e.second);
        m_navigation->addItem(it);
    }
    connect(m_navigation, &QListWidget::currentRowChanged, this, [this](int zeile) {
        if (zeile >= 0)
            m_tabs->setCurrentIndex(zeile);
    });

    auto* titel = new QLabel(tr("Ansichten"), bereich);
    titel->setObjectName(QStringLiteral("Bereichstitel"));
    auto* v = new QVBoxLayout(bereich);
    v->setContentsMargins(0, 0, 0, 6);
    v->setSpacing(0);
    v->addWidget(titel);
    v->addWidget(m_navigation, 1);
    return bereich;
}

void MainWindow::aktualisiereNavigation()
{
    if (!m_navigation || m_navigation->count() <= ReiterPruefung)
        return;
    // Anzahl in Klammern hinter dem Namen, wie "Posteingang (3)" in Outlook
    auto setze = [this](int zeile, int anzahl, const QString& tipp) {
        QListWidgetItem* it = m_navigation->item(zeile);
        const QString name = it->data(kNameRolle).toString();
        it->setText(anzahl > 0 ? QStringLiteral("%1 (%2)").arg(name).arg(anzahl) : name);
        QFont f = m_navigation->font();
        f.setBold(anzahl > 0);
        it->setFont(f);
        it->setToolTip(tipp);
    };
    int offen = 0;
    for (const Stoerung& st : m_kontext.daten->stoerungen())
        if (st.istOffen())
            ++offen;
    setze(ReiterStoerungen, offen, tr("%1 offene Störungen").arg(offen));
    int auffaellig = 0;
    for (const Kennzahl& k : m_kontext.analyse->kennzahlen())
        if (k.ok && k.auswertung.istAuffaellig())
            ++auffaellig;
    setze(ReiterAnlagen, auffaellig, tr("%1 Anlagen mit erkanntem Fehlerbild").arg(auffaellig));
    const int fehler = m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Fehler);
    setze(ReiterPruefung, fehler, tr("%1 Fehler beim Einlesen").arg(fehler));
}

void MainWindow::aktualisiereFilterKnopf()
{
    if (!m_filterReset)
        return;
    const int aktiv = int(!m_filter.suche.isEmpty()) + int(!m_filter.status.isEmpty()) + int(!m_filter.typ.isEmpty())
                    + int(!m_filter.ort.isEmpty()) + int(m_filter.minKwp > 0) + int(m_filter.maxKwp > 0)
                    + int(m_filter.nurAuffaellig) + int(m_filter.nurOffen);
    m_filterReset->setText(aktiv > 0 ? tr("Filter zurücksetzen (%1)").arg(aktiv) : tr("Filter zurücksetzen"));
    m_filterReset->setEnabled(aktiv > 0);
}

QWidget* MainWindow::baueAnlagenSeite()
{
    auto* seite = new QWidget;

    // Suchfeld sitzt rechts in der Befehlsleiste (wie im Explorer)
    m_suche = new QLineEdit(m_filter.suche);
    m_suche->setPlaceholderText(tr("Anlagen durchsuchen (Strg+F)"));
    m_suche->setToolTip(tr("Sucht in Anlagen-ID, Ort und Wetterstation"));
    m_suche->setClearButtonEnabled(true);
    m_suche->setFixedWidth(260);

    auto* status = new QComboBox(seite);
    status->addItem(tr("Alle Status"), QString());
    for (const QString& s : Datenbasis::gueltigeStatus())
        status->addItem(farbIcon(Anzeige::statusFarbe(s)), Anzeige::statusName(s), s);
    status->setCurrentIndex(qMax(0, status->findData(m_filter.status)));

    auto* typ = new QComboBox(seite);
    typ->addItem(tr("Alle Typen"), QString());
    for (const QString& t : Datenbasis::gueltigeTypen())
        typ->addItem(Anzeige::typName(t), t);
    typ->setCurrentIndex(qMax(0, typ->findData(m_filter.typ)));

    auto* ort = new QComboBox(seite);
    ort->addItem(tr("Alle Orte"), QString());
    for (const QString& o : m_kontext.daten->orte())
        ort->addItem(o, o);
    ort->setCurrentIndex(qMax(0, ort->findData(m_filter.ort)));

    auto* minKwp = new QDoubleSpinBox(seite);
    auto* maxKwp = new QDoubleSpinBox(seite);
    for (QDoubleSpinBox* sb : {minKwp, maxKwp}) {
        sb->setRange(0.0, 100000.0);
        sb->setDecimals(1);
        sb->setSuffix(QStringLiteral(" kWp"));
        sb->setSpecialValueText(tr("beliebig"));
        sb->setToolTip(tr("0 = keine Grenze"));
    }
    minKwp->setValue(m_filter.minKwp);
    maxKwp->setValue(m_filter.maxKwp);

    auto* nurAuffaellig = new QCheckBox(tr("nur mit Fehlerbild"), seite);
    nurAuffaellig->setChecked(m_filter.nurAuffaellig);
    nurAuffaellig->setToolTip(tr("Anlagen, bei denen die Flottenanalyse ein Fehlerbild erkannt hat"));
    auto* nurOffen = new QCheckBox(tr("nur mit offenen Störungen"), seite);
    nurOffen->setChecked(m_filter.nurOffen);

    auto* zuruecksetzen = new QPushButton(tr("Filter zurücksetzen"), seite);
    m_filterReset = zuruecksetzen;

    // Eine Filterzeile mit beschrifteten Feldern
    auto* zeile1 = new QHBoxLayout;
    zeile1->setSpacing(6);
    zeile1->addWidget(new QLabel(tr("Status:"), seite));
    zeile1->addWidget(status);
    zeile1->addSpacing(8);
    zeile1->addWidget(new QLabel(tr("Typ:"), seite));
    zeile1->addWidget(typ);
    zeile1->addSpacing(8);
    zeile1->addWidget(new QLabel(tr("Ort:"), seite));
    zeile1->addWidget(ort);
    zeile1->addSpacing(8);
    zeile1->addWidget(new QLabel(tr("Leistung:"), seite));
    zeile1->addWidget(minKwp);
    zeile1->addWidget(new QLabel(QStringLiteral("–"), seite));
    zeile1->addWidget(maxKwp);
    zeile1->addSpacing(8);
    zeile1->addWidget(nurAuffaellig);
    zeile1->addWidget(nurOffen);
    zeile1->addStretch();
    zeile1->addWidget(zuruecksetzen);

    // Sortierung merken: setSortingEnabled() sortiert sofort nach der
    // Standardspalte absteigend, danach wird der gewünschte Zustand gesetzt.
    const bool hatteSortierung = m_anlageFilter->sortColumn() >= 0;
    const int sortSpalte = hatteSortierung ? m_anlageFilter->sortColumn() : int(AnlageModel::SpId);
    const Qt::SortOrder sortRichtung = hatteSortierung ? m_anlageFilter->sortOrder() : Qt::AscendingOrder;

    m_anlagenTabelle = new QTableView(seite);
    m_anlagenTabelle->setModel(m_anlageFilter);
    m_anlagenTabelle->setSortingEnabled(true);
    m_anlagenTabelle->sortByColumn(sortSpalte, sortRichtung);
    m_anlagenTabelle->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_anlagenTabelle->setSelectionMode(QAbstractItemView::SingleSelection);
    m_anlagenTabelle->setAlternatingRowColors(false);
    m_anlagenTabelle->verticalHeader()->setVisible(false);
    m_anlagenTabelle->verticalHeader()->setDefaultSectionSize(24);
    m_anlagenTabelle->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_anlagenTabelle->setShowGrid(false);
    m_anlagenTabelle->setItemDelegateForColumn(AnlageModel::SpStatus, new BadgeDelegate(m_anlagenTabelle));
    m_anlagenTabelle->setItemDelegateForColumn(AnlageModel::SpPr, new BadgeDelegate(m_anlagenTabelle));
    m_anlagenTabelle->horizontalHeader()->setStretchLastSection(true);
    m_anlagenTabelle->setContextMenuPolicy(Qt::CustomContextMenu);
    m_anlagenTabelle->resizeColumnsToContents();
    m_anlagenTabelle->setColumnWidth(AnlageModel::SpPr, qMax(96, m_anlagenTabelle->columnWidth(AnlageModel::SpPr)));
    m_anlagenTabelle->setColumnWidth(AnlageModel::SpStatus, qMax(130, m_anlagenTabelle->columnWidth(AnlageModel::SpStatus)));

    connect(m_anlagenTabelle, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        oeffneAnlage(idx.data(AnlageModel::IdRolle).toString());
    });
    connect(m_anlagenTabelle, &QTableView::customContextMenuRequested, this, &MainWindow::anlagenKontextmenue);
    m_anlageInfo = new AnlageInfo(m_kontext);
    connect(m_anlageInfo, &AnlageInfo::detailsGewuenscht, this, [this](const QString& id) { oeffneAnlage(id); });
    connect(m_anlageInfo, &AnlageInfo::bearbeitenGewuenscht, this, [this](const QString& id) { bearbeiteAnlage(id); });
    connect(m_anlagenTabelle->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this] {
        m_anlageInfo->zeige(markierteAnlage());
        if (m_karte)
            m_karte->markiere(markierteAnlage());
    });

    // Filter verbinden (Zustand merken, damit er einen Neuaufbau übersteht)
    connect(m_suche, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_filter.suche = t;
        m_anlageFilter->setSuchtext(t);
        aktualisiereFilterKnopf();
    });
    connect(status, &QComboBox::currentIndexChanged, this, [this, status] {
        m_filter.status = status->currentData().toString();
        m_anlageFilter->setStatus(m_filter.status);
        aktualisiereFilterKnopf();
    });
    connect(typ, &QComboBox::currentIndexChanged, this, [this, typ] {
        m_filter.typ = typ->currentData().toString();
        m_anlageFilter->setTyp(m_filter.typ);
        aktualisiereFilterKnopf();
    });
    connect(ort, &QComboBox::currentIndexChanged, this, [this, ort] {
        m_filter.ort = ort->currentData().toString();
        m_anlageFilter->setOrt(m_filter.ort);
        aktualisiereFilterKnopf();
    });
    auto leistung = [this, minKwp, maxKwp] {
        m_filter.minKwp = minKwp->value();
        m_filter.maxKwp = maxKwp->value();
        m_anlageFilter->setLeistungsbereich(m_filter.minKwp, m_filter.maxKwp);
        aktualisiereFilterKnopf();
    };
    connect(minKwp, &QDoubleSpinBox::valueChanged, this, leistung);
    connect(maxKwp, &QDoubleSpinBox::valueChanged, this, leistung);
    connect(nurAuffaellig, &QCheckBox::toggled, this, [this](bool an) {
        m_filter.nurAuffaellig = an;
        m_anlageFilter->setNurAuffaellige(an);
        aktualisiereFilterKnopf();
    });
    connect(nurOffen, &QCheckBox::toggled, this, [this](bool an) {
        m_filter.nurOffen = an;
        m_anlageFilter->setNurOffeneStoerungen(an);
        aktualisiereFilterKnopf();
    });
    connect(zuruecksetzen, &QPushButton::clicked, this, [=] {
        m_suche->clear();
        status->setCurrentIndex(0);
        typ->setCurrentIndex(0);
        ort->setCurrentIndex(0);
        minKwp->setValue(0.0);
        maxKwp->setValue(0.0);
        nurAuffaellig->setChecked(false);
        nurOffen->setChecked(false);
    });

    m_anlagenLeer = new QLabel(tr("Keine Anlage entspricht den gewählten Filtern."), seite);
    m_anlagenLeer->setObjectName(QStringLiteral("Leerzustand"));
    m_anlagenLeer->setAlignment(Qt::AlignCenter);
    m_anlagenLeer->hide();

    // Tabelle links, Detailbereich rechts (verstellbar, wie der Lesebereich in Outlook)
    auto* links = new QWidget(seite);
    auto* lv = new QVBoxLayout(links);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);
    lv->addWidget(m_anlagenTabelle, 1);
    lv->addWidget(m_anlagenLeer, 1);
    auto* teiler = new QSplitter(Qt::Horizontal, seite);
    teiler->setChildrenCollapsible(false);
    teiler->addWidget(links);
    teiler->addWidget(m_anlageInfo);
    teiler->setStretchFactor(0, 1);
    teiler->setSizes({1000, 330});

    auto* v = new QVBoxLayout(seite);
    v->setSpacing(6);
    v->addLayout(zeile1);
    v->addWidget(teiler, 1);
    m_anlageInfo->zeige(markierteAnlage());
    return seite;
}

QWidget* MainWindow::baueStoerungsSeite()
{
    auto* seite = new QWidget;

    auto* suche = new QLineEdit(m_filter.stoerSuche, seite);
    suche->setPlaceholderText(tr("Suche nach Störung, Anlage, Code oder Beschreibung"));
    suche->setClearButtonEnabled(true);

    m_stoerStatus = new QComboBox(seite);
    m_stoerStatus->addItem(tr("Alle"));
    m_stoerStatus->addItem(tr("Offen + in Bearbeitung"));
    m_stoerStatus->addItem(Anzeige::stoerungsStatusName(QStringLiteral("offen")));
    m_stoerStatus->addItem(Anzeige::stoerungsStatusName(QStringLiteral("in_bearbeitung")));
    m_stoerStatus->addItem(Anzeige::stoerungsStatusName(QStringLiteral("behoben")));
    m_stoerStatus->setCurrentIndex(m_filter.stoerStatus);

    auto* schwere = new QComboBox(seite);
    schwere->addItem(tr("Alle Schweregrade"), QString());
    for (const QString& s : Datenbasis::gueltigeSchweren())
        schwere->addItem(Anzeige::schwereName(s), s);
    schwere->setCurrentIndex(qMax(0, schwere->findData(m_filter.stoerSchwere)));

    m_stoerAnlage = new QLineEdit(m_filter.stoerAnlage, seite);
    m_stoerAnlage->setPlaceholderText(tr("Anlage, z. B. P00051"));
    m_stoerAnlage->setClearButtonEnabled(true);
    m_stoerAnlage->setMaximumWidth(180);


    auto* leiste = new QHBoxLayout;
    leiste->setSpacing(6);
    leiste->addWidget(new QLabel(tr("Suchen:"), seite));
    leiste->addWidget(suche, 2);
    leiste->addSpacing(8);
    leiste->addWidget(new QLabel(tr("Status:"), seite));
    leiste->addWidget(m_stoerStatus);
    leiste->addSpacing(8);
    leiste->addWidget(new QLabel(tr("Schwere:"), seite));
    leiste->addWidget(schwere);
    leiste->addSpacing(8);
    leiste->addWidget(new QLabel(tr("Anlage:"), seite));
    leiste->addWidget(m_stoerAnlage);
    leiste->addStretch(1);

    const bool hatteSortierung = m_stoerungFilter->sortColumn() >= 0;
    const int sortSpalte = hatteSortierung ? m_stoerungFilter->sortColumn() : int(StoerungModel::SpZeitpunkt);
    const Qt::SortOrder sortRichtung = hatteSortierung ? m_stoerungFilter->sortOrder() : Qt::DescendingOrder;

    m_stoerungsTabelle = new QTableView(seite);
    m_stoerungsTabelle->setModel(m_stoerungFilter);
    m_stoerungsTabelle->setSortingEnabled(true);
    m_stoerungsTabelle->sortByColumn(sortSpalte, sortRichtung);
    m_stoerungsTabelle->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stoerungsTabelle->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stoerungsTabelle->setAlternatingRowColors(false);
    m_stoerungsTabelle->verticalHeader()->setVisible(false);
    m_stoerungsTabelle->verticalHeader()->setDefaultSectionSize(24);
    m_stoerungsTabelle->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_stoerungsTabelle->setShowGrid(false);
    m_stoerungsTabelle->setItemDelegateForColumn(StoerungModel::SpSchwere, new BadgeDelegate(m_stoerungsTabelle));
    m_stoerungsTabelle->setItemDelegateForColumn(StoerungModel::SpStatus, new BadgeDelegate(m_stoerungsTabelle));
    m_stoerungsTabelle->horizontalHeader()->setStretchLastSection(true);
    m_stoerungsTabelle->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stoerungsTabelle->resizeColumnsToContents();

    connect(m_stoerungsTabelle, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        oeffneStoerung(idx.data(StoerungModel::IdRolle).toString());
    });
    connect(m_stoerungsTabelle, &QTableView::customContextMenuRequested, this, &MainWindow::stoerungenKontextmenue);

    connect(suche, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_filter.stoerSuche = t;
        m_stoerungFilter->setSuchtext(t);
    });
    connect(m_stoerStatus, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_filter.stoerStatus = i;
        m_stoerungFilter->setStatusFilter(static_cast<StoerungFilterModel::StatusFilter>(i));
    });
    connect(schwere, &QComboBox::currentIndexChanged, this, [this, schwere] {
        m_filter.stoerSchwere = schwere->currentData().toString();
        m_stoerungFilter->setSchwere(m_filter.stoerSchwere);
    });
    connect(m_stoerAnlage, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_filter.stoerAnlage = t;
        m_stoerungFilter->setAnlage(t);
    });


    m_stoerungenLeer = new QLabel(tr("Keine Störung entspricht den gewählten Filtern."), seite);
    m_stoerungenLeer->setObjectName(QStringLiteral("Leerzustand"));
    m_stoerungenLeer->setAlignment(Qt::AlignCenter);
    m_stoerungenLeer->hide();

    auto* v = new QVBoxLayout(seite);
    v->setSpacing(6);
    v->addLayout(leiste);
    v->addWidget(m_stoerungsTabelle, 1);
    v->addWidget(m_stoerungenLeer, 1);
    m_stoerungsTabelle->resizeColumnsToContents();
    return seite;
}

void MainWindow::erzeugeAktionen()
{
    // Aktionen werden bei jedem Neuaufbau neu erzeugt (Texte in aktueller Sprache)
    for (QAction* alt : {m_aktDetails, m_aktBearbeiten, m_aktStoerung, m_aktExportAnlagen, m_aktExportStoerungen,
                         m_aktExportBericht, m_aktPdfAnlage, m_aktPdfFlotte, m_aktRueckgaengig, m_aktWiederholen})
        if (alt)
            alt->deleteLater();

    const QColor f = Stil::text();
    m_aktDetails = new QAction(Stil::icon(Stil::Symbol::Details, f), tr("&Details …"), this);
    m_aktDetails->setShortcut(Qt::Key_Return);
    m_aktDetails->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_aktDetails, &QAction::triggered, this, [this] {
        if (!markierteAnlage().isEmpty())
            oeffneAnlage(markierteAnlage());
    });

    m_aktBearbeiten = new QAction(Stil::icon(Stil::Symbol::Bearbeiten, f), tr("Stammdaten &bearbeiten …"), this);
    m_aktBearbeiten->setShortcut(Qt::Key_F2);
    connect(m_aktBearbeiten, &QAction::triggered, this, [this] {
        if (!markierteAnlage().isEmpty())
            bearbeiteAnlage(markierteAnlage());
    });
    m_aktStoerung = new QAction(Stil::icon(Stil::Symbol::Plus, f), tr("Störung &erfassen …"), this);
    connect(m_aktStoerung, &QAction::triggered, this, [this] {
        if (!markierteAnlage().isEmpty())
            erfasseStoerung(markierteAnlage());
    });

    m_aktExportAnlagen = new QAction(Stil::icon(Stil::Symbol::Export, f), tr("&Anlagen exportieren …"), this);
    connect(m_aktExportAnlagen, &QAction::triggered, this, &MainWindow::exportiereAnlagen);
    m_aktExportStoerungen = new QAction(Stil::icon(Stil::Symbol::Export, f), tr("&Störungen exportieren …"), this);
    connect(m_aktExportStoerungen, &QAction::triggered, this, &MainWindow::exportiereStoerungen);
    m_aktExportBericht = new QAction(Stil::icon(Stil::Symbol::Bericht, f), tr("&Ertragsbericht exportieren …"), this);
    connect(m_aktExportBericht, &QAction::triggered, this, &MainWindow::exportiereBericht);

    m_aktPdfAnlage = new QAction(Stil::icon(Stil::Symbol::Pdf, f), tr("Anlagenbericht als &PDF …"), this);
    connect(m_aktPdfAnlage, &QAction::triggered, this, [this] {
        if (!markierteAnlage().isEmpty())
            exportiereAnlagenPdf(markierteAnlage());
    });
    m_aktPdfFlotte = new QAction(Stil::icon(Stil::Symbol::Pdf, f), tr("&Flottenbericht als PDF …"), this);
    connect(m_aktPdfFlotte, &QAction::triggered, this, &MainWindow::exportiereFlottenPdf);

    // Rückgängig/Wiederholen: Text zeigt, welche Änderung betroffen ist
    m_aktRueckgaengig = new QAction(Stil::icon(Stil::Symbol::Rueckgaengig, f), tr("&Rückgängig"), this);
    m_aktRueckgaengig->setShortcut(QKeySequence::Undo);
    m_aktWiederholen = new QAction(Stil::icon(Stil::Symbol::Wiederholen, f), tr("&Wiederholen"), this);
    m_aktWiederholen->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(m_aktRueckgaengig, &QAction::triggered, m_undo, &QUndoStack::undo);
    connect(m_aktWiederholen, &QAction::triggered, m_undo, &QUndoStack::redo);
    auto beschrifte = [this] {
        const QString u = m_undo->undoText(), w = m_undo->redoText();
        m_aktRueckgaengig->setEnabled(m_undo->canUndo());
        m_aktRueckgaengig->setText(u.isEmpty() ? tr("&Rückgängig") : tr("&Rückgängig: %1").arg(u));
        m_aktRueckgaengig->setToolTip(u.isEmpty() ? tr("Nichts rückgängig zu machen") : tr("Rückgängig: %1 (Strg+Z)").arg(u));
        m_aktWiederholen->setEnabled(m_undo->canRedo());
        m_aktWiederholen->setText(w.isEmpty() ? tr("&Wiederholen") : tr("&Wiederholen: %1").arg(w));
        m_aktWiederholen->setToolTip(w.isEmpty() ? tr("Nichts zu wiederholen") : tr("Wiederholen: %1 (Strg+Y)").arg(w));
    };
    connect(m_undo, &QUndoStack::indexChanged, m_aktRueckgaengig, beschrifte);
    connect(m_undo, &QUndoStack::cleanChanged, m_aktRueckgaengig, beschrifte);
    beschrifte();
}

void MainWindow::baueMenues()
{
    // Menüband wie in Office statt Menü- und Symbolleiste
    if (QWidget* alt = menuWidget())
        alt->deleteLater();
    for (QToolBar* tb : findChildren<QToolBar*>()) {
        removeToolBar(tb);
        tb->deleteLater();
    }
    m_anlagenTabelle->addAction(m_aktDetails);
    baueBefehlsleiste();

    // Tastenkürzel gelten im ganzen Fenster, auch wenn der Knopf gerade
    // auf einem anderen Reiter des Bands liegt
    addActions({m_aktRueckgaengig, m_aktWiederholen, m_aktBearbeiten});

    auto* sucheKurz = new QShortcut(QKeySequence::Find, m_tabs);
    connect(sucheKurz, &QShortcut::activated, this, [this] {
        m_tabs->setCurrentIndex(ReiterAnlagen);
        m_suche->setFocus();
        m_suche->selectAll();
    });
}

void MainWindow::baueBefehlsleiste()
{
    auto* band = new Menueband;
    const QColor f = Stil::text();
    auto aktion = [band](const QIcon& icon, const QString& text, auto&& slot) {
        auto* a = new QAction(icon, text, band);
        QObject::connect(a, &QAction::triggered, band, slot);
        return a;
    };

    // ---------- Datei (Menü hinter dem farbigen Knopf) ----------
    QMenu* datei = band->dateiMenue();
    datei->addAction(Stil::icon(Stil::Symbol::Benutzer, f), tr("Benutzer &wechseln …"), this, &MainWindow::benutzerWechseln);
    datei->addSeparator();
    datei->addAction(m_aktExportAnlagen);
    datei->addAction(m_aktExportStoerungen);
    datei->addAction(m_aktExportBericht);
    datei->addSeparator();
    datei->addAction(m_aktPdfAnlage);
    datei->addAction(m_aktPdfFlotte);
    datei->addSeparator();
    datei->addAction(Stil::icon(Stil::Symbol::Info, f), tr("&Über SolarMonitor"), this, &MainWindow::zeigeUeber);
    datei->addSeparator();
    QAction* beenden = datei->addAction(tr("&Beenden"), this, &QWidget::close);
    beenden->setShortcut(QKeySequence::Quit);
    addAction(beenden);

    // Zusätzliche Befehle für das Band
    QAction* verlauf = aktion(Stil::icon(Stil::Symbol::Liste, f), tr("Änderungsverlauf …"),
                              [this] { zeigeAenderungsverlauf(); });
    QAction* aufKarte = aktion(Stil::icon(Stil::Symbol::Karte, f), tr("Auf der Karte zeigen"), [this] {
        if (markierteAnlage().isEmpty())
            return;
        m_karte->markiere(markierteAnlage());
        m_tabs->setCurrentIndex(ReiterKarte);
    });
    QAction* stoerungenDerAnlage = aktion(Stil::icon(Stil::Symbol::Warnung, f), tr("Störungen der Anlage anzeigen"), [this] {
        if (markierteAnlage().isEmpty())
            return;
        m_stoerAnlage->setText(markierteAnlage());
        m_stoerStatus->setCurrentIndex(0);
        m_tabs->setCurrentIndex(ReiterStoerungen);
    });
    QAction* analyse = aktion(Stil::icon(Stil::Symbol::Neuladen, f), tr("Flottenanalyse neu starten"),
                              [this] { m_kontext.analyse->starte(); });
    auto seitenAktion = [&](Stil::Symbol symbol, const QString& text, int seite) {
        QAction* a = aktion(Stil::icon(symbol, f), text, [this, seite] { m_tabs->setCurrentIndex(seite); });
        a->setCheckable(true);
        a->setChecked(m_tabs->currentIndex() == seite);
        connect(m_tabs, &QStackedWidget::currentChanged, a, [a, seite](int i) { a->setChecked(i == seite); });
        return a;
    };

    // ---------- Start ----------
    band->neuerReiter(tr("Start"));
    band->neueGruppe(tr("Rückgängig"));
    band->klein({m_aktRueckgaengig, m_aktWiederholen, verlauf});
    band->neueGruppe(tr("Anlage"));
    band->gross(m_aktDetails, tr("Details"));
    band->gross(m_aktBearbeiten, tr("Stammdaten\nbearbeiten"));
    band->gross(m_aktStoerung, tr("Störung\nerfassen"));
    band->neueGruppe(tr("Berichte"));
    band->gross(m_aktPdfAnlage, tr("Anlagen-\nbericht"));
    band->gross(m_aktPdfFlotte, tr("Flotten-\nbericht"));
    band->neueGruppe(tr("Exportieren"));
    band->klein({m_aktExportAnlagen, m_aktExportStoerungen, m_aktExportBericht});

    // ---------- Anlage ----------
    band->neuerReiter(tr("Anlage"));
    band->neueGruppe(tr("Bearbeiten"));
    band->gross(m_aktDetails, tr("Details"));
    band->gross(m_aktBearbeiten, tr("Stammdaten\nbearbeiten"));
    band->gross(m_aktStoerung, tr("Störung\nerfassen"));
    band->neueGruppe(tr("Anzeigen"));
    band->gross(aufKarte, tr("Auf Karte\nzeigen"));
    band->gross(stoerungenDerAnlage, tr("Störungen\nanzeigen"));
    band->neueGruppe(tr("Bericht"));
    band->gross(m_aktPdfAnlage, tr("Anlagen-\nbericht"));

    // ---------- Auswertung ----------
    band->neuerReiter(tr("Auswertung"));
    band->neueGruppe(tr("Flotte"));
    band->gross(analyse, tr("Analyse neu\nstarten"));
    band->gross(seitenAktion(Stil::Symbol::Statistik, tr("Statistik"), ReiterStatistik), tr("Statistik"));
    band->neueGruppe(tr("Service"));
    band->gross(seitenAktion(Stil::Symbol::Werkzeug, tr("Serviceplanung"), ReiterService), tr("Service-\nplanung"));
    band->gross(seitenAktion(Stil::Symbol::Pruefung, tr("Datenprüfung"), ReiterPruefung), tr("Daten-\nprüfung"));
    band->neueGruppe(tr("Berichte"));
    band->gross(m_aktPdfFlotte, tr("Flotten-\nbericht"));
    band->klein({m_aktExportAnlagen, m_aktExportStoerungen, m_aktExportBericht});

    // ---------- Ansicht ----------
    band->neuerReiter(tr("Ansicht"));
    band->neueGruppe(tr("Seiten"));
    band->gross(seitenAktion(Stil::Symbol::Uebersicht, tr("Übersicht"), ReiterUebersicht), tr("Übersicht"));
    band->gross(seitenAktion(Stil::Symbol::Liste, tr("Anlagen"), ReiterAnlagen), tr("Anlagen"));
    band->gross(seitenAktion(Stil::Symbol::Warnung, tr("Störungen"), ReiterStoerungen), tr("Störungen"));
    band->gross(seitenAktion(Stil::Symbol::Karte, tr("Karte"), ReiterKarte), tr("Karte"));

    band->neueGruppe(tr("Bereiche"));
    QAction* navi = aktion(Stil::icon(Stil::Symbol::Liste, f), tr("Navigationsbereich"), [this](bool an) {
        if (m_navigation)
            m_navigation->parentWidget()->setVisible(an);
    });
    navi->setCheckable(true);
    navi->setChecked(true);
    QAction* detail = aktion(Stil::icon(Stil::Symbol::Details, f), tr("Detailbereich"), [this](bool an) {
        if (m_anlageInfo)
            m_anlageInfo->setVisible(an);
    });
    detail->setCheckable(true);
    detail->setChecked(true);
    QAction* vollbild = aktion(Stil::icon(Stil::Symbol::Vollbild, f), tr("Vollbild"), [this](bool an) { setzeVollbild(an); });
    vollbild->setCheckable(true);
    vollbild->setChecked(isFullScreen() || QSettings().value(QStringLiteral("vollbild"), true).toBool());
    vollbild->setShortcut(Qt::Key_F11);
    addAction(vollbild);
    band->klein({navi, detail, vollbild});

    band->neueGruppe(tr("Design"));
    QAction* dunkel = aktion(Stil::icon(Stil::Symbol::Mond, f), tr("Dunkles Design"), [this](bool an) { setzeDesign(an); });
    dunkel->setCheckable(true);
    dunkel->setChecked(Stil::istDunkel());
    band->gross(dunkel, tr("Dunkles\nDesign"));

    band->neueGruppe(tr("Sprache"));
    auto* sprachen = new QActionGroup(band);
    QList<QAction*> sprachAktionen;
    for (const auto& [kuerzel, name] : {std::pair{QStringLiteral("de"), QStringLiteral("Deutsch")},
                                        std::pair{QStringLiteral("en"), QStringLiteral("English")}}) {
        QAction* a = aktion(QIcon(), name, [this, k = kuerzel] {
            QSettings().setValue(QStringLiteral("sprache"), k);
            if (k != Sprache::aktuell() && !Sprache::setze(k))
                QMessageBox::warning(this, QStringLiteral("SolarMonitor"), tr("Die Übersetzung konnte nicht geladen werden."));
        });
        a->setCheckable(true);
        a->setChecked(Sprache::aktuell() == kuerzel);
        sprachen->addAction(a);
        sprachAktionen.append(a);
    }
    band->klein(sprachAktionen);

    // Suchfeld rechts neben den Reitern (wie "Suchen" in Office)
    if (m_suche) {
        m_suche->setObjectName(QStringLiteral("Suchfeld"));
        band->setzeRechts(m_suche);
        connect(m_suche, &QLineEdit::textEdited, this, [this] { m_tabs->setCurrentIndex(ReiterAnlagen); });
    }
    setMenuWidget(band);
}

void MainWindow::baueStatusleiste()
{
    auto* sb = new QStatusBar(this);
    setStatusBar(sb);

    // Anzahl der Einträge der aktuellen Liste, wie "1.499 Elemente" im Explorer
    m_anlagenZaehler = new QLabel(sb);
    m_stoerungsZaehler = new QLabel(sb);
    auto zaehlerZeigen = [this] {
        m_anlagenZaehler->setVisible(m_tabs->currentIndex() == ReiterAnlagen);
        m_stoerungsZaehler->setVisible(m_tabs->currentIndex() == ReiterStoerungen);
    };
    connect(m_tabs, &QStackedWidget::currentChanged, sb, zaehlerZeigen);
    zaehlerZeigen();

    m_meldungenKnopf = new QPushButton(sb);
    m_meldungenKnopf->setFlat(true);
    m_meldungenKnopf->setCursor(Qt::PointingHandCursor);
    m_meldungenKnopf->setToolTip(tr("Zur Datenprüfung"));
    connect(m_meldungenKnopf, &QPushButton::clicked, this, [this] { m_tabs->setCurrentIndex(ReiterPruefung); });
    auto meldungenText = [this] {
        const int fehler = m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Fehler);
        m_meldungenKnopf->setIcon(fehler > 0 ? Stil::icon(Stil::Symbol::Warnung, Qt::white) : QIcon());
        m_meldungenKnopf->setText(tr("%1 Fehler, %2 Warnungen beim Einlesen")
                                      .arg(fehler)
                                      .arg(m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Warnung)));
    };
    meldungenText();
    connect(m_kontext.daten, &Datenbasis::meldungenGeaendert, m_meldungenKnopf, meldungenText);

    m_fortschritt = new QProgressBar(sb);
    m_fortschritt->setMaximumWidth(180);
    m_fortschritt->setMaximumHeight(16);
    m_fortschritt->setFormat(tr("Analyse %p %"));
    analyseFortschritt(m_fortschrittWert, m_fortschrittMax);

    auto* benutzer = new QLabel(tr("Angemeldet: %1").arg(Rollen::anzeigename(m_kontext.rolle)), sb);
    auto* ordner = new QLabel(QDir::toNativeSeparators(m_kontext.daten->datenOrdner()), sb);
    ordner->setToolTip(tr("Datenordner"));

    sb->addWidget(ordner, 1);
    sb->addPermanentWidget(m_anlagenZaehler);
    sb->addPermanentWidget(m_stoerungsZaehler);
    sb->addPermanentWidget(m_meldungenKnopf);
    sb->addPermanentWidget(m_fortschritt);
    sb->addPermanentWidget(benutzer);
}

void MainWindow::aktualisiereRechte()
{
    m_aktBearbeiten->setEnabled(m_kontext.darf(Recht::StammdatenBearbeiten));
    m_aktStoerung->setEnabled(m_kontext.darf(Recht::StoerungBearbeiten));
    m_aktExportAnlagen->setEnabled(m_kontext.darf(Recht::StammdatenExportieren));
    m_aktExportStoerungen->setEnabled(m_kontext.darf(Recht::StoerungBearbeiten));
    m_aktExportBericht->setEnabled(m_kontext.darf(Recht::BerichtExportieren));
    m_aktPdfAnlage->setEnabled(m_kontext.darf(Recht::BerichtExportieren));
    m_aktPdfFlotte->setEnabled(m_kontext.darf(Recht::BerichtExportieren));

    const QString nurAdmin = tr("Nur für die Rolle Administrator.");
    const QString nurService = tr("Nur für die Rollen Service und Administrator.");
    m_aktBearbeiten->setToolTip(m_aktBearbeiten->isEnabled() ? tr("Stammdaten der markierten Anlage bearbeiten (F2)") : nurAdmin);
    m_aktStoerung->setToolTip(m_aktStoerung->isEnabled() ? tr("Neue Störung für die markierte Anlage erfassen") : nurService);
    m_aktExportAnlagen->setToolTip(m_aktExportAnlagen->isEnabled() ? tr("Anlagen als CSV speichern (anlagen_out.csv)") : nurAdmin);
}

void MainWindow::aktualisiereZaehler()
{
    if (m_anlagenZaehler)
        m_anlagenZaehler->setText(tr("%1 von %2 Anlagen")
                                      .arg(m_anlageFilter->rowCount())
                                      .arg(m_anlageModel->rowCount()));
    if (m_stoerungsZaehler)
        m_stoerungsZaehler->setText(tr("%1 von %2 Störungen")
                                        .arg(m_stoerungFilter->rowCount())
                                        .arg(m_stoerungModel->rowCount()));
    if (m_anlagenLeer) {
        const bool leer = m_anlageFilter->rowCount() == 0;
        m_anlagenLeer->setVisible(leer);
        m_anlagenTabelle->setVisible(!leer);
    }
    if (m_stoerungenLeer) {
        const bool leer = m_stoerungFilter->rowCount() == 0;
        m_stoerungenLeer->setVisible(leer);
        m_stoerungsTabelle->setVisible(!leer);
    }
}

QString MainWindow::markierteAnlage() const
{
    const QModelIndex idx = m_anlagenTabelle ? m_anlagenTabelle->currentIndex() : QModelIndex();
    return idx.isValid() ? idx.data(AnlageModel::IdRolle).toString() : QString();
}

QString MainWindow::markierteStoerung() const
{
    const QModelIndex idx = m_stoerungsTabelle ? m_stoerungsTabelle->currentIndex() : QModelIndex();
    return idx.isValid() ? idx.data(StoerungModel::IdRolle).toString() : QString();
}

// -------------------------------------------------------------------
//  Kontextmenüs
// -------------------------------------------------------------------

void MainWindow::anlagenKontextmenue(const QPoint& pos)
{
    const QModelIndex idx = m_anlagenTabelle->indexAt(pos);
    if (!idx.isValid())
        return;
    const QString id = idx.data(AnlageModel::IdRolle).toString();

    QMenu menue(this);
    menue.addAction(m_aktDetails);
    menue.addAction(m_aktBearbeiten);
    menue.addAction(m_aktStoerung);
    menue.addAction(m_aktPdfAnlage);
    menue.addSeparator();
    menue.addAction(tr("Störungen dieser Anlage anzeigen"), this, [this, id] {
        m_stoerAnlage->setText(id);
        m_stoerStatus->setCurrentIndex(0);
        m_tabs->setCurrentIndex(ReiterStoerungen);
    });
    menue.addAction(tr("Auf der Karte zeigen"), this, [this, id] {
        m_karte->markiere(id);
        m_tabs->setCurrentIndex(ReiterKarte);
    });
    menue.exec(m_anlagenTabelle->viewport()->mapToGlobal(pos));
}

void MainWindow::stoerungenKontextmenue(const QPoint& pos)
{
    const QModelIndex idx = m_stoerungsTabelle->indexAt(pos);
    if (!idx.isValid())
        return;
    const QString id = idx.data(StoerungModel::IdRolle).toString();
    const Stoerung* s = m_kontext.daten->stoerung(id);
    if (!s)
        return;
    const bool darf = m_kontext.darf(Recht::StoerungBearbeiten);

    QMenu menue(this);
    menue.addAction(tr("Öffnen …"), this, [this, id] { oeffneStoerung(id); });
    QAction* inArbeit = menue.addAction(tr("Status: in Bearbeitung"), this,
                                        [this, id] { setzeStoerungsStatus(id, QStringLiteral("in_bearbeitung")); });
    inArbeit->setEnabled(darf && s->status == QLatin1String("offen"));
    QAction* behoben = menue.addAction(tr("Status: behoben"), this,
                                       [this, id] { setzeStoerungsStatus(id, QStringLiteral("behoben")); });
    behoben->setEnabled(darf && s->istOffen());
    menue.addSeparator();
    QAction* anlage = menue.addAction(tr("Anlage %1 öffnen").arg(s->anlageId), this,
                                      [this, a = s->anlageId] { oeffneAnlage(a); });
    anlage->setEnabled(m_kontext.daten->anlage(s->anlageId) != nullptr);
    menue.exec(m_stoerungsTabelle->viewport()->mapToGlobal(pos));
}

// -------------------------------------------------------------------
//  Aktionen
// -------------------------------------------------------------------

void MainWindow::oeffneAnlage(const QString& anlageId)
{
    if (!m_kontext.daten->anlage(anlageId)) {
        QMessageBox::information(this, tr("Anlage"), tr("Die Anlage %1 ist nicht vorhanden.").arg(anlageId));
        return;
    }
    if (m_karte)
        m_karte->markiere(anlageId);

    DetailDialog dlg(anlageId, m_kontext, this);
    connect(&dlg, &DetailDialog::bearbeitenGewuenscht, this, [this, &dlg](const QString& id) { bearbeiteAnlage(id, &dlg); });
    connect(&dlg, &DetailDialog::stoerungErfassenGewuenscht, this, [this, &dlg](const QString& id) { erfasseStoerung(id, &dlg); });
    connect(&dlg, &DetailDialog::stoerungOeffnenGewuenscht, this, [this, &dlg](const QString& id) { oeffneStoerung(id, &dlg); });
    connect(&dlg, &DetailDialog::berichtGewuenscht, this, [this, &dlg](const QString& id) { exportiereAnlagenPdf(id, &dlg); });
    dlg.exec();
}

void MainWindow::bearbeiteAnlage(const QString& anlageId, QWidget* parent)
{
    if (!m_kontext.darf(Recht::StammdatenBearbeiten)) {
        QMessageBox::information(parent ? parent : this, tr("Keine Berechtigung"),
                                 tr("Stammdaten dürfen nur von der Rolle Administrator bearbeitet werden."));
        return;
    }
    const Anlage* a = m_kontext.daten->anlage(anlageId);
    if (!a)
        return;

    EditDialog dlg(*a, m_kontext.daten, parent ? parent : this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const Anlage* vorher = m_kontext.daten->anlage(anlageId);
    if (!vorher) {
        QMessageBox::warning(this, tr("Speichern fehlgeschlagen"), tr("Anlage %1 ist unbekannt.").arg(anlageId));
        return;
    }
    // Ausführen über den Undo-Stack (ruft redo() sofort auf)
    m_undo->push(new AnlageAendern(m_kontext.daten, *vorher, dlg.anlage(),
                                   [this](const QString& id) { nachAnlagenAenderung(id); }));
}

void MainWindow::nachAnlagenAenderung(const QString& anlageId)
{
    m_kontext.analyse->aktualisiere(anlageId);
    speichereAnlagen();
}

void MainWindow::nachStoerungsAenderung(const QString&)
{
    speichereStoerungen();
}

void MainWindow::erfasseStoerung(const QString& anlageId, QWidget* parent)
{
    if (!m_kontext.darf(Recht::StoerungBearbeiten)) {
        QMessageBox::information(parent ? parent : this, tr("Keine Berechtigung"),
                                 tr("Störungen dürfen nur von den Rollen Service und Administrator erfasst werden."));
        return;
    }
    StoerungDialog dlg(anlageId, m_kontext.daten, parent ? parent : this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    auto* befehl = new StoerungErfassen(m_kontext.daten, dlg.stoerung(),
                                        [this](const QString& id) { nachStoerungsAenderung(id); });
    m_undo->push(befehl);
    statusBar()->showMessage(tr("Störung %1 für Anlage %2 erfasst (Strg+Z macht es rückgängig).")
                                 .arg(befehl->stoerungId(), anlageId),
                             6000);
}

void MainWindow::oeffneStoerung(const QString& stoerungId, QWidget* parent)
{
    const Stoerung* s = m_kontext.daten->stoerung(stoerungId);
    if (!s)
        return;
    const bool darf = m_kontext.darf(Recht::StoerungBearbeiten);
    StoerungDialog dlg(*s, m_kontext.daten, darf, parent ? parent : this);
    if (dlg.exec() != QDialog::Accepted || !darf)
        return;
    const Stoerung* vorher = m_kontext.daten->stoerung(stoerungId);
    if (!vorher) {
        QMessageBox::warning(this, tr("Speichern fehlgeschlagen"), tr("Störung %1 ist unbekannt.").arg(stoerungId));
        return;
    }
    m_undo->push(new StoerungAendern(m_kontext.daten, *vorher, dlg.stoerung(),
                                     [this](const QString& id) { nachStoerungsAenderung(id); }));
}

void MainWindow::setzeStoerungsStatus(const QString& stoerungId, const QString& status)
{
    const Stoerung* alt = m_kontext.daten->stoerung(stoerungId);
    if (!alt || !m_kontext.darf(Recht::StoerungBearbeiten))
        return;
    Stoerung neu = *alt;
    neu.status = status;
    neu.behobenAm = (status == QLatin1String("behoben")) ? Zeit::jetzt() : QDateTime();
    m_undo->push(new StoerungAendern(m_kontext.daten, *alt, neu,
                                     [this](const QString& id) { nachStoerungsAenderung(id); }));
}

void MainWindow::speichereAnlagen()
{
    const QString pfad = m_kontext.daten->ausgabePfad(QStringLiteral("anlagen_out.csv"));
    QString fehler;
    if (m_kontext.daten->exportiereAnlagen(pfad, &fehler))
        statusBar()->showMessage(tr("Änderung gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 6000);
    else
        QMessageBox::warning(this, tr("Automatisches Speichern fehlgeschlagen"),
                             tr("Die Änderung ist im Speicher, wurde aber nicht gesichert:\n%1").arg(fehler));
}

void MainWindow::speichereStoerungen()
{
    const QString pfad = m_kontext.daten->ausgabePfad(QStringLiteral("stoerungen_out.csv"));
    QString fehler;
    if (m_kontext.daten->exportiereStoerungen(pfad, &fehler))
        statusBar()->showMessage(tr("Änderung gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 6000);
    else
        QMessageBox::warning(this, tr("Automatisches Speichern fehlgeschlagen"),
                             tr("Die Änderung ist im Speicher, wurde aber nicht gesichert:\n%1").arg(fehler));
}

void MainWindow::exportiereAnlagen()
{
    const QString pfad = QFileDialog::getSaveFileName(this, tr("Anlagen exportieren"),
                                                      m_kontext.daten->ausgabePfad(QStringLiteral("anlagen_out.csv")),
                                                      tr("CSV-Dateien (*.csv)"));
    if (pfad.isEmpty())
        return;
    QString fehler;
    if (m_kontext.daten->exportiereAnlagen(pfad, &fehler))
        statusBar()->showMessage(tr("%1 Anlagen exportiert nach %2").arg(m_kontext.daten->anlagen().size())
                                     .arg(QDir::toNativeSeparators(pfad)), 6000);
    else
        QMessageBox::critical(this, tr("Export fehlgeschlagen"), fehler);
}

void MainWindow::exportiereStoerungen()
{
    const QString pfad = QFileDialog::getSaveFileName(this, tr("Störungen exportieren"),
                                                      m_kontext.daten->ausgabePfad(QStringLiteral("stoerungen_out.csv")),
                                                      tr("CSV-Dateien (*.csv)"));
    if (pfad.isEmpty())
        return;
    QString fehler;
    if (m_kontext.daten->exportiereStoerungen(pfad, &fehler))
        statusBar()->showMessage(tr("Störungen exportiert nach %1").arg(QDir::toNativeSeparators(pfad)), 6000);
    else
        QMessageBox::critical(this, tr("Export fehlgeschlagen"), fehler);
}

void MainWindow::exportiereBericht()
{
    if (!m_kontext.analyse->istFertig()) {
        QMessageBox::information(this, tr("Ertragsbericht"),
                                 tr("Die Flottenanalyse ist noch nicht abgeschlossen. Bitte kurz warten."));
        return;
    }
    const QString pfad = QFileDialog::getSaveFileName(this, tr("Ertragsbericht exportieren"),
                                                      m_kontext.daten->ausgabePfad(QStringLiteral("ertragsbericht.csv")),
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
    out << "anlage_id,typ,ort,leistung_kwp,wetterstation_id,energie_kwh,einstrahlung_kwh_m2,"
           "spez_ertrag_kwh_kwp,pr,median_pr_station,erloes_eur,verlust_kwh,verlust_eur,fehlerbilder,hinweis\n";
    for (const Anlage& a : m_kontext.daten->anlagen()) {
        const Kennzahl* k = m_kontext.analyse->kennzahl(a.anlageId);
        out << a.anlageId << ',' << a.typ << ',' << CsvReader::maskiere(a.ort) << ','
            << QString::number(a.leistungKwp, 'f', 2) << ',' << a.wetterstationId << ',';
        if (k && k->ok) {
            const Auswertung& w = k->auswertung;
            QStringList bilder;
            for (const Anomalie& an : w.anomalien)
                bilder.append(Anomalie::typName(an.typ));
            out << QString::number(w.energieKwh, 'f', 2) << ',' << QString::number(w.einstrahlungKwhM2, 'f', 2) << ','
                << QString::number(w.spezErtragKwhKwp, 'f', 2) << ','
                << (w.pr > 0 ? QString::number(w.pr, 'f', 4) : QString()) << ','
                << (k->medianPrStation > 0 ? QString::number(k->medianPrStation, 'f', 4) : QString()) << ','
                << QString::number(w.energieKwh * a.verguetungCtKwh / 100.0, 'f', 2) << ','
                << QString::number(k->verlustKwh, 'f', 2) << ',' << QString::number(k->verlustEuro, 'f', 2) << ','
                << CsvReader::maskiere(bilder.join(QStringLiteral("; "))) << ",\n";
        } else {
            const QString grund = k ? k->fehler : m_kontext.verlaeufe->fehlgrund(a.anlageId);
            out << ",,,,,,,,," << CsvReader::maskiere(QString(grund).replace(QLatin1Char('\n'), QLatin1Char(' '))) << '\n';
        }
    }
    out.flush();
    if (datei.commit())
        statusBar()->showMessage(tr("Ertragsbericht gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 6000);
    else
        QMessageBox::critical(this, tr("Export fehlgeschlagen"), datei.errorString());
}

// -------------------------------------------------------------------
//  PDF-Berichte und Änderungsverlauf
// -------------------------------------------------------------------

bool MainWindow::pdfZiel(const QString& titel, const QString& vorschlag, QString* pfad)
{
    *pfad = QFileDialog::getSaveFileName(this, titel, m_kontext.daten->ausgabePfad(vorschlag),
                                         tr("PDF-Dateien (*.pdf)"));
    if (pfad->isEmpty())
        return false;
    if (!pfad->endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        *pfad += QStringLiteral(".pdf");
    return true;
}

void MainWindow::pdfFertig(bool ok, const QString& pfad, const QString& fehler)
{
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::critical(this, tr("PDF-Bericht"), fehler);
        return;
    }
    statusBar()->showMessage(tr("Bericht gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 8000);
    if (QMessageBox::question(this, tr("PDF-Bericht"),
                              tr("Der Bericht wurde gespeichert:\n%1\n\nJetzt öffnen?").arg(QDir::toNativeSeparators(pfad)))
        == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(pfad));
}

void MainWindow::exportiereAnlagenPdf(const QString& anlageId, QWidget* parent)
{
    if (!m_kontext.darf(Recht::BerichtExportieren))
        return;
    QString pfad;
    if (!pdfZiel(tr("Anlagenbericht speichern"), QStringLiteral("bericht_%1.pdf").arg(anlageId), &pfad))
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString fehler;
    const bool ok = PdfBericht::anlage(m_kontext, anlageId, pfad, &fehler);
    pdfFertig(ok, pfad, fehler);
    Q_UNUSED(parent)
}

void MainWindow::exportiereFlottenPdf()
{
    if (!m_kontext.analyse->istFertig()) {
        QMessageBox::information(this, tr("Flottenbericht"),
                                 tr("Die Flottenanalyse ist noch nicht abgeschlossen. Bitte kurz warten."));
        return;
    }
    QString pfad;
    if (!pdfZiel(tr("Flottenbericht speichern"), QStringLiteral("flottenbericht.pdf"), &pfad))
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString fehler;
    const bool ok = PdfBericht::flotte(m_kontext, pfad, &fehler);
    pdfFertig(ok, pfad, fehler);
}

void MainWindow::zeigeAenderungsverlauf()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Änderungsverlauf"));
    auto* info = new QLabel(tr("Alle Änderungen dieser Sitzung. Ein Klick auf einen Eintrag setzt die Daten auf "
                               "genau diesen Stand zurück (oder wieder vor)."),
                            &dlg);
    info->setWordWrap(true);
    auto* ansicht = new QUndoView(m_undo, &dlg);
    ansicht->setEmptyLabel(tr("<Ausgangszustand nach dem Laden>"));
    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(knoepfe, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto* v = new QVBoxLayout(&dlg);
    v->addWidget(info);
    v->addWidget(ansicht, 1);
    v->addWidget(knoepfe);
    dlg.resize(520, 420);
    dlg.exec();
}

void MainWindow::benutzerWechseln()
{
    LoginDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_kontext.rolle = dlg.rolle();
    // Die neue Rolle darf die Änderungen der vorherigen nicht rückgängig machen
    m_undo->clear();
    baueUi();
    statusBar()->showMessage(tr("Angemeldet als %1.").arg(Rollen::anzeigename(m_kontext.rolle)), 5000);
}

void MainWindow::zeigeUeber()
{
    QMessageBox::about(this, tr("Über SolarMonitor"),
                       tr("<h3>SolarMonitor</h3>"
                          "<p>Offline-Monitoring einer PV-Flotte: Stammdaten, Störungen, "
                          "Verläufe, Ertrag und Performance Ratio, Fehlererkennung, "
                          "Serviceplanung und Karte.</p>"
                          "<p>C++17 · Qt %1 · Diagramme und Karte mit QPainter</p>"
                          "<p>Datenordner:<br>%2</p>")
                           .arg(QString::fromLatin1(qVersion()),
                                QDir::toNativeSeparators(m_kontext.daten->datenOrdner())));
}

// -------------------------------------------------------------------
//  Flottenanalyse
// -------------------------------------------------------------------

void MainWindow::analyseFortschritt(int erledigt, int gesamt)
{
    m_fortschrittWert = erledigt;
    m_fortschrittMax = gesamt;
    if (!m_fortschritt)
        return;
    m_fortschritt->setRange(0, qMax(1, gesamt));
    m_fortschritt->setValue(erledigt);
    m_fortschritt->setVisible(gesamt > 0 && erledigt < gesamt);
    m_fortschritt->setToolTip(tr("Verläufe aller Anlagen werden im Hintergrund ausgewertet (%1 von %2).")
                                  .arg(erledigt)
                                  .arg(gesamt));
}

void MainWindow::analyseFertig()
{
    analyseFortschritt(m_fortschrittMax, m_fortschrittMax);
    if (m_anlagenTabelle)
        for (int sp : {AnlageModel::SpPr, AnlageModel::SpSpezErtrag})
            m_anlagenTabelle->resizeColumnToContents(sp);

    // Defekte Verlaufsdateien in die Datenprüfung übernehmen
    QVector<Pruefmeldung> meldungen;
    int auffaellig = 0;
    for (const Kennzahl& k : m_kontext.analyse->kennzahlen()) {
        if (!k.ok)
            meldungen.append({Pruefmeldung::Stufe::Fehler, kQuelleJson, 0, k.anlageId, k.fehler});
        else if (k.auswertung.istAuffaellig())
            ++auffaellig;
    }
    m_kontext.daten->entferneMeldungen(kQuelleJson);
    m_kontext.daten->ergaenzeMeldungen(meldungen);

    statusBar()->showMessage(tr("Flottenanalyse abgeschlossen: %1 Anlagen ausgewertet, %2 mit Fehlerbild, "
                                "%3 Verlaufsdateien defekt.")
                                 .arg(m_kontext.analyse->kennzahlen().size())
                                 .arg(auffaellig)
                                 .arg(meldungen.size()),
                             10000);
}
