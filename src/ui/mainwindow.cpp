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
#include <QSignalBlocker>
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
#include "model/datensatz.h"
#include "repo/csv_parser.h"
#include "ui/aenderungen.h"
#include "ui/anlage_info.h"
#include "ui/anlage_model.h"
#include "ui/badge_delegate.h"
#include "ui/dashboard_widget.h"
#include "ui/detail_dialog.h"
#include "ui/edit_dialog.h"
#include "ui/karte_view.h"
#include "ui/intro_animation.h"
#include "ui/login_seite.h"
#include "ui/benutzer_seite.h"
#include "ui/werbung.h"
#include "ui/zahlung_dialog.h"
#include "model/favoriten.h"
#include "ui/einfuehrung.h"
#include "ui/keks_banner.h"
#include "ui/vergleich_dialog.h"
#include "util/abo.h"
#include "model/benutzer.h"
#include "util/windows_hello.h"
#include "ui/datenordner_seite.h"
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
enum Reiter { ReiterUebersicht, ReiterAnlagen, ReiterStoerungen, ReiterKarte, ReiterStatistik, ReiterService, ReiterPruefung,
              ReiterBenutzer };
const QString kQuelleJson = QStringLiteral("Verlauf (JSON)");

QIcon farbIcon(const QColor& farbe)
{
    QPixmap pm(12, 12);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(farbe);
    p.drawEllipse(QRectF(1, 1, 10, 10));
    p.end();
    return QIcon(pm);
}

// Rollen der Navigationseinträge
constexpr int kSeiteRolle = Qt::UserRole + 1;   ///< Seitennummer, -1 = Abschnittsüberschrift
constexpr int kSymbolRolle = Qt::UserRole + 2;  ///< Stil::Symbol
constexpr int kZahlRolle = Qt::UserRole + 3;    ///< Zahl im Abzeichen (0 = keins)
constexpr int kZahlFarbeRolle = Qt::UserRole + 4;

// Farben der Seitenleiste (in hellem und dunklem Design gleich)
const QColor kNavText(199, 209, 222);
const QColor kNavGedimmt(125, 141, 163);
const QColor kNavHover(255, 255, 255, 14);
const QColor kNavAuswahl(255, 255, 255, 26);
const QColor kSonne(245, 158, 11);
const QColor kRot(220, 38, 38);

/**
 * Zeichnet die Einträge der Seitenleiste: Symbol, Name, rechts ein farbiges
 * Abzeichen mit der Anzahl (z. B. offene Störungen) und für die gewählte
 * Seite eine hellere Fläche mit orangem Balken links.
 */
class NavDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        const bool abschnitt = i.data(kSeiteRolle).toInt() < 0;
        return {o.rect.width(), abschnitt ? 34 : 40};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        const QRect r = o.rect;
        if (i.data(kSeiteRolle).toInt() < 0) {
            // Abschnittsüberschrift in Kapitälchen-Optik
            QFont f = o.font;
            f.setPointSizeF(f.pointSizeF() * 0.8);
            f.setBold(true);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p->setFont(f);
            p->setPen(kNavGedimmt);
            p->drawText(r.adjusted(22, 0, -10, -4), Qt::AlignLeft | Qt::AlignBottom, i.data().toString().toUpper());
            p->restore();
            return;
        }

        const bool gewaehlt = o.state & QStyle::State_Selected;
        const bool hover = o.state & QStyle::State_MouseOver;
        const QRectF flaeche = QRectF(r).adjusted(10, 2, -10, -2);
        if (gewaehlt || hover) {
            p->setPen(Qt::NoPen);
            p->setBrush(gewaehlt ? kNavAuswahl : kNavHover);
            p->drawRoundedRect(flaeche, 6, 6);
        }
        if (gewaehlt) {
            p->setBrush(kSonne);
            p->drawRoundedRect(QRectF(flaeche.left(), flaeche.top() + 8, 3, flaeche.height() - 16), 1.5, 1.5);
        }

        const QColor farbe = gewaehlt ? QColor(Qt::white) : kNavText;
        const auto symbol = static_cast<Stil::Symbol>(i.data(kSymbolRolle).toInt());
        const int g = 18;
        const QRect symRect(int(flaeche.left()) + 14, r.center().y() - g / 2 + 1, g, g);
        Stil::icon(symbol, gewaehlt ? kSonne : kNavText).paint(p, symRect);

        QFont f = o.font;
        f.setBold(gewaehlt);
        p->setFont(f);
        p->setPen(farbe);

        int rechts = int(flaeche.right()) - 10;
        const int zahl = i.data(kZahlRolle).toInt();
        if (zahl > 0) {
            QFont fz = o.font;
            fz.setPointSizeF(fz.pointSizeF() * 0.8);
            fz.setBold(true);
            const QString t = zahl > 999 ? QStringLiteral("999+") : QString::number(zahl);
            const int b = qMax(20, QFontMetrics(fz).horizontalAdvance(t) + 12);
            const QRectF abz(rechts - b, r.center().y() - 9, b, 18);
            p->setPen(Qt::NoPen);
            p->setBrush(i.data(kZahlFarbeRolle).value<QColor>());
            p->drawRoundedRect(abz, 9, 9);
            p->setFont(fz);
            p->setPen(Qt::white);
            p->drawText(abz, Qt::AlignCenter, t);
            rechts = int(abz.left()) - 6;
            p->setFont(f);
            p->setPen(farbe);
        }
        const QRect textRect(symRect.right() + 12, r.top(), rechts - symRect.right() - 12, r.height());
        p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(f).elidedText(i.data().toString(), Qt::ElideRight, textRect.width()));
        p->restore();
    }
};

/// Glocken-Symbol mit rotem Zähler oben rechts (Anzahl kritischer Alarme).
QIcon glocke(int anzahl)
{
    QIcon icon;
    for (int g : {24, 48}) {
        QPixmap pm(g, g);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        Stil::icon(Stil::Symbol::Glocke, Stil::text()).paint(&p, QRect(0, 0, g, g));
        if (anzahl > 0) {
            const double s = g / 24.0;
            const QRectF kreis(10 * s, 0, 14 * s, 14 * s);
            p.setPen(QPen(Stil::flaeche(), 1.5 * s));
            p.setBrush(kRot);
            p.drawEllipse(kreis);
            QFont f;
            f.setPixelSize(int(9 * s));
            f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(kreis, Qt::AlignCenter, anzahl > 9 ? QStringLiteral("9+") : QString::number(anzahl));
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

/// Runder Avatar mit den Anfangsbuchstaben der Rolle.
QIcon avatar(const QString& name)
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(Stil::seitenleiste());
    p.drawEllipse(QRectF(0, 0, 64, 64));
    QFont f;
    f.setPixelSize(26);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kSonne);
    // Anfangsbuchstaben von Vor- und Nachname
    const QStringList teile = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString kuerzel = teile.isEmpty() ? QStringLiteral("?") : teile.first().left(1);
    if (teile.size() > 1 && teile.last().at(0).isLetter())
        kuerzel += teile.last().left(1);
    f.setPixelSize(kuerzel.size() > 1 ? 23 : 26);
    p.setFont(f);
    p.drawText(QRectF(0, 0, 64, 64), Qt::AlignCenter, kuerzel.toUpper());
    p.end();
    return QIcon(pm);
}
} // namespace

MainWindow::MainWindow(std::unique_ptr<Datensatz> datensatz, QWidget* parent)
    : QMainWindow(parent)
{
    m_undo = new QUndoStack(this);
    m_undo->setUndoLimit(100);
    connect(m_undo, &QUndoStack::indexChanged, this, &MainWindow::undoStandGeaendert);
    m_benutzer = new Benutzerverwaltung(this);
    uebernimmDaten(std::move(datensatz));

    resize(1440, 900);
    restoreGeometry(QSettings().value(QStringLiteral("fenster")).toByteArray());
    setWindowTitle(QStringLiteral("SolarMonitor"));

    // Ein einziges Fenster mit drei Ebenen: Intro → Anmeldung → Programm
    m_ebenen = new QStackedWidget(this);
    m_intro = new IntroAnimation(m_ebenen);
    connect(m_intro, &IntroAnimation::fertig, this, [this] { zeigeAnmeldung(false); });
    m_ebenen->addWidget(m_intro);
    neueLoginSeite();
    setCentralWidget(m_ebenen);

    // Spaß-Abo: Stufenwechsel und Paywall der Ultimate-Funktionen
    connect(Abo::instanz(), &Abo::stufeGeaendert, this, &MainWindow::aboAnwenden);
    connect(Abo::instanz(), &Abo::paywall, this, &MainWindow::zeigePaywall);
}

MainWindow::~MainWindow()
{
    // Erst die Oberfläche und die Modelle, dann die Daten, auf die sie zeigen
    m_undo->clear();
    delete m_ebenen; // enthält auch die Programmoberfläche
    m_ebenen = nullptr;
    m_programm = nullptr;
    m_intro = nullptr;
    m_login = nullptr;
    m_ordnerSeite = nullptr;
    trenneDaten();
}

void MainWindow::uebernimmDaten(std::unique_ptr<Datensatz> datensatz)
{
    m_datensatz = std::move(datensatz); // ein alter Datensatz wird hier freigegeben
    m_kontext.daten = m_datensatz->daten();
    m_kontext.verlaeufe = m_datensatz->verlaeufe();
    m_kontext.analyse = m_datensatz->analyse();
    m_datenFehler = m_datensatz->fehler();
    m_datenOrdner = m_datensatz->ordner();
    m_fortschrittWert = m_fortschrittMax = 0;
    m_filter = FilterZustand{};

    m_anlageModel = new AnlageModel(m_kontext, this);
    m_anlageFilter = new AnlageFilterModel(this);
    m_anlageFilter->setSourceModel(m_anlageModel);
    m_stoerungModel = new StoerungModel(m_kontext, this);
    m_stoerungFilter = new StoerungFilterModel(this);
    m_stoerungFilter->setSourceModel(m_stoerungModel);

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
}

void MainWindow::trenneDaten()
{
    // Programmoberfläche und Modelle entfernen, solange die alten Daten noch leben
    if (m_programm) {
        if (m_ebenen)
            m_ebenen->removeWidget(m_programm);
        delete m_programm;
    }
    m_programm = nullptr;
    m_tabs = nullptr;
    m_seitenTitel = m_seitenHinweis = m_analyseText = nullptr;
    m_anlagenLeer = m_stoerungenLeer = m_anlagenZaehler = m_stoerungsZaehler = nullptr;
    m_glocke = nullptr;
    m_benutzerKnopf = nullptr;
    m_werbungsBereich = nullptr;
    m_premiumChip = nullptr;
    m_aboMenue = nullptr;
    m_navigation = nullptr;
    m_anlageInfo = nullptr;
    m_filterReset = nullptr;
    m_nurFavoriten = nullptr;
    m_anlagenTabelle = m_stoerungsTabelle = nullptr;
    m_karte = nullptr;
    m_suche = m_stoerAnlage = nullptr;
    m_stoerStatus = nullptr;
    m_fortschritt = nullptr;
    delete m_anlageFilter;
    delete m_anlageModel;
    delete m_stoerungFilter;
    delete m_stoerungModel;
    m_anlageFilter = nullptr;
    m_anlageModel = nullptr;
    m_stoerungFilter = nullptr;
    m_stoerungModel = nullptr;
}

void MainWindow::wechsleDaten(const QString& ordner)
{
    // Neue Daten laden, während das Fenster offen bleibt; erst bei Erfolg
    // wird der alte Datensatz ersetzt.
    if (m_ordnerSeite)
        m_ordnerSeite->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    meldung(tr("Daten werden geladen …"), 60000);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    auto neu = Datensatz::lade(ordner);
    QApplication::restoreOverrideCursor();
    if (m_ordnerSeite)
        m_ordnerSeite->setEnabled(true);
    if (m_toast)
        m_toast->hide();

    if (!neu->ok()) {
        // Ordner bleibt angezeigt, mit dem Grund
        m_ordnerSeite->vorbereiten(ordner, neu->fehler(), m_programm != nullptr);
        return;
    }
    QSettings().setValue(QStringLiteral("datenordner"), neu->ordner());

    // Änderungen der alten Daten lassen sich nicht mehr zurücknehmen
    m_undo->clear();
    trenneDaten();
    uebernimmDaten(std::move(neu));
    baueUi();
    zeigeProgramm();
    QTimer::singleShot(0, m_kontext.analyse, &FlottenAnalyse::starte); // Auswertung im Hintergrund
    meldung(tr("Datenordner geladen: %1 Anlagen aus %2")
                .arg(m_kontext.daten->anlagen().size())
                .arg(QDir::toNativeSeparators(m_datenOrdner)),
            6000);
}

void MainWindow::neueLoginSeite()
{
    // Neu erzeugen statt umzuschalten, damit die Texte der aktuellen Sprache folgen
    int stelle = 1;
    if (m_login) {
        stelle = m_ebenen->indexOf(m_login);
        m_ebenen->removeWidget(m_login);
        m_login->deleteLater();
    }
    m_login = new LoginSeite(m_benutzer);
    m_ebenen->insertWidget(stelle, m_login);
    connect(m_login, &LoginSeite::angemeldet, this, &MainWindow::angemeldet);
    connect(m_login, &LoginSeite::abgebrochen, this, [this] {
        if (m_programm)
            zeigeProgramm(); // Benutzerwechsel abgebrochen: zurück zum Programm
        else
            close();
    });
}

void MainWindow::starte(bool mitIntro)
{
    if (QSettings().value(QStringLiteral("vollbild"), true).toBool())
        showFullScreen();
    else
        showMaximized();
    if (mitIntro) {
        m_ebenen->setCurrentWidget(m_intro);
        m_intro->starte();
    } else {
        zeigeAnmeldung(false);
    }
}

void MainWindow::zeigeAnmeldung(bool wechsel)
{
    setWindowTitle(tr("Anmeldung – SolarMonitor"));
    if (m_toast)
        m_toast->hide();
    m_login->vorbereiten(wechsel);
    m_ebenen->setCurrentWidget(m_login);
    m_login->vorbereiten(wechsel); // Fokus erst, wenn die Seite sichtbar ist
}

void MainWindow::zeigeProgramm()
{
    setWindowTitle(tr("SolarMonitor – Monitoring der PV-Flotte"));
    m_ebenen->setCurrentWidget(m_programm);
}

void MainWindow::zeigeDatenordner(const QString& fehler, bool zurueckMoeglich)
{
    // Seite jedes Mal neu erzeugen, damit sie der aktuellen Sprache folgt
    if (m_ordnerSeite) {
        m_ebenen->removeWidget(m_ordnerSeite);
        m_ordnerSeite->deleteLater();
    }
    m_ordnerSeite = new DatenordnerSeite;
    m_ebenen->addWidget(m_ordnerSeite);
    connect(m_ordnerSeite, &DatenordnerSeite::abgebrochen, this, [this] {
        if (m_programm)
            zeigeProgramm();
        else
            close();
    });
    connect(m_ordnerSeite, &DatenordnerSeite::ordnerGewaehlt, this, [this](const QString& ordner) {
        if (m_programm && QDir(ordner) == QDir(m_datenOrdner)) {
            QSettings().setValue(QStringLiteral("datenordner"), ordner);
            zeigeProgramm(); // unverändert
            return;
        }
        wechsleDaten(ordner);
    });
    setWindowTitle(tr("Datenordner – SolarMonitor"));
    if (m_toast)
        m_toast->hide();
    m_ordnerSeite->vorbereiten(m_datenOrdner, fehler, zurueckMoeglich);
    m_ebenen->setCurrentWidget(m_ordnerSeite);
    m_ordnerSeite->vorbereiten(m_datenOrdner, fehler, zurueckMoeglich);
}

void MainWindow::angemeldet(const QString& kennung)
{
    const Benutzer* b = m_benutzer->finde(kennung);
    if (!b)
        return;
    const Rolle rolle = b->rolle;
    const bool andererBenutzer = kennung != m_angemeldet;
    m_angemeldet = kennung;
    if (!m_datenFehler.isEmpty()) {
        // Keine Daten: erst den Datenordner festlegen lassen
        m_kontext.rolle = rolle;
        zeigeDatenordner(m_datenFehler, false);
        return;
    }
    const bool wechsel = m_programm != nullptr;
    // Die neue Rolle darf die Änderungen der vorherigen nicht rückgängig machen
    if (wechsel && (rolle != m_kontext.rolle || andererBenutzer))
        m_undo->clear();
    m_kontext.rolle = rolle;
    Favoriten::instanz()->setzeBenutzer(kennung);
    baueUi();
    zeigeProgramm();
    if (!wechsel)
        QTimer::singleShot(0, m_kontext.analyse, &FlottenAnalyse::starte); // Auswertung im Hintergrund
    else
        meldung(tr("Angemeldet als %1.").arg(b->name), 5000);
    // Beim ersten Anmelden dieses Benutzers die Einführungstour zeigen
    // Keks-Banner (ohne Abo) kommt danach bzw. nach der Tour
    if (!QSettings().value(QStringLiteral("einfuehrung/") + kennung, false).toBool())
        QTimer::singleShot(600, this, &MainWindow::starteEinfuehrung);
    else
        QTimer::singleShot(900, this, [this] { zeigeKeksBanner(); });
}

void MainWindow::setzeDatenquelle(bool live)
{
    if (!m_kontext.verlaeufe || !m_kontext.analyse)
        return;
    const auto neu = live ? VerlaufRepository::Quelle::Live : VerlaufRepository::Quelle::Csv;
    if (m_kontext.verlaeufe->quelle() == neu)
        return;
    m_kontext.verlaeufe->setQuelle(neu);
    m_kontext.analyse->starte(); // alle Seiten aktualisieren sich über "fertig"
    meldung(live ? tr("Datenquelle: Live-Generator. Alle Auswertungen zeigen simulierte Werte.")
                 : tr("Datenquelle: CSV-Dateien. Auswertungen zeigen die Verlaufsdateien."),
            5000, false);
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
        if (m_programm)
            baueUi();
    });
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Schließen-Knopf: Symbol beim Überfahren weiß auf rotem Grund
    if (obj->objectName() == QLatin1String("Schliessen")
        && (event->type() == QEvent::Enter || event->type() == QEvent::Leave)) {
        if (auto* k = qobject_cast<QToolButton*>(obj))
            k->setIcon(k->property(event->type() == QEvent::Enter ? "iconHover" : "iconNormal").value<QIcon>());
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::changeEvent(QEvent* event)
{
    // Beim Sprachwechsel kommen mehrere LanguageChange-Ereignisse
    // (Translator entfernt/installiert); die Oberfläche wird nur einmal
    // danach neu aufgebaut.
    if (event->type() == QEvent::LanguageChange && m_ebenen && !m_neuaufbauGeplant) {
        m_neuaufbauGeplant = true;
        QTimer::singleShot(0, this, [this] {
            m_neuaufbauGeplant = false;
            const bool loginSichtbar = m_ebenen->currentWidget() == m_login;
            neueLoginSeite();
            if (m_programm)
                baueUi();
            if (loginSichtbar)
                zeigeAnmeldung(m_programm != nullptr);
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
    erzeugeAktionen();

    m_tabs = new QStackedWidget;

    auto* uebersicht = new DashboardWidget(m_kontext);
    connect(uebersicht, &DashboardWidget::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    connect(uebersicht, &DashboardWidget::seiteOeffnen, this, [this](DashboardWidget::Ziel ziel) {
        switch (ziel) {
        case DashboardWidget::Ziel::Anlagen: m_tabs->setCurrentIndex(ReiterAnlagen); break;
        case DashboardWidget::Ziel::Stoerungen: m_tabs->setCurrentIndex(ReiterStoerungen); break;
        case DashboardWidget::Ziel::Karte: m_tabs->setCurrentIndex(ReiterKarte); break;
        case DashboardWidget::Ziel::Statistik: m_tabs->setCurrentIndex(ReiterStatistik); break;
        case DashboardWidget::Ziel::Serviceplanung: m_tabs->setCurrentIndex(ReiterService); break;
        case DashboardWidget::Ziel::Datenpruefung: m_tabs->setCurrentIndex(ReiterPruefung); break;
        case DashboardWidget::Ziel::Favoriten:
            if (m_nurFavoriten)
                m_nurFavoriten->setChecked(true);
            m_tabs->setCurrentIndex(ReiterAnlagen);
            break;
        }
    });
    QAction* anordnung = uebersicht->anordnungsAktion();
    m_tabs->addWidget(seite(tr("Übersicht"), tr("Zustand der Flotte auf einen Blick · Kacheln und Karten lassen sich per Ziehen verschieben"),
                            uebersicht, {anordnung, m_aktAnalyse}, {m_aktPdfFlotte, m_aktExportBericht}));

    // Zähler links in der Aktionszeile der Listen
    m_anlagenZaehler = new QLabel;
    m_anlagenZaehler->setObjectName(QStringLiteral("Zaehler"));
    m_stoerungsZaehler = new QLabel;
    m_stoerungsZaehler->setObjectName(QStringLiteral("Zaehler"));

    m_tabs->addWidget(seite(tr("Anlagen"), tr("Alle PV-Anlagen der Flotte · Doppelklick öffnet die Details"),
                            baueAnlagenSeite(),
                            {m_aktRueckgaengig, m_aktWiederholen, nullptr, m_aktFavorit, m_aktVergleich, nullptr,
                             m_aktDetails, m_aktBearbeiten, m_aktStoerung},
                            {m_aktExportAnlagen, m_aktPdfAnlage, nullptr, m_aktVerlauf}, m_anlagenZaehler));
    m_tabs->addWidget(seite(tr("Störungen"), tr("Störungsmeldungen seit 2024 · Rechtsklick ändert den Status"),
                            baueStoerungsSeite(), {m_aktRueckgaengig, m_aktWiederholen}, {m_aktExportStoerungen},
                            m_stoerungsZaehler));

    m_karte = new KarteView(m_kontext);
    connect(m_karte, &KarteView::anlageGewaehlt, this, [this](const QString& id) { oeffneAnlage(id); });
    m_tabs->addWidget(seite(tr("Karte"), tr("Lage der Anlagen und Wetterstationen · Mausrad zoomt, Klick öffnet eine Anlage"),
                            m_karte));

    m_tabs->addWidget(seite(tr("Statistik"), tr("Kennzahlen und Auswertungen über die gesamte Flotte"),
                            new StatistikWidget(m_kontext), {m_aktAnalyse}, {m_aktPdfFlotte, m_aktExportBericht}));

    auto* service = new Serviceplanung(m_kontext);
    connect(service, &Serviceplanung::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    connect(service, &Serviceplanung::tourAnzeigen, this,
            [this](const QPointF& start, const QStringList& ids, const QVector<QPointF>& verlauf, bool strasse,
                   double umkreisKm) {
        m_karte->setTour(start, ids, verlauf, strasse, umkreisKm);
        m_tabs->setCurrentIndex(ReiterKarte);
    });
    m_tabs->addWidget(seite(tr("Serviceplanung"),
                            tr("Welche Anlagen zuerst? Sortiert nach Priorität und geschätztem Ertragsverlust"),
                            service));

    auto* pruefung = new PruefprotokollWidget(m_kontext);
    connect(pruefung, &PruefprotokollWidget::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    m_tabs->addWidget(seite(tr("Datenprüfung"),
                            tr("Auffälligkeiten beim Einlesen der Dateien und automatische Korrekturen"), pruefung));

    // Benutzerverwaltung nur für Administratoren
    if (m_kontext.darf(Recht::BenutzerVerwalten)) {
        auto* benutzerSeite = new BenutzerSeite(m_benutzer, m_angemeldet);
        connect(benutzerSeite, &BenutzerSeite::meldung, this,
                [this](const QString& text, bool hinweis) { meldung(text, hinweis ? 3500 : 5000, hinweis); });
        // eigenen Namen in der Kopfzeile aktuell halten
        connect(m_benutzer, &Benutzerverwaltung::geaendert, benutzerSeite, [this] {
            if (const Benutzer* b = m_benutzer->finde(m_angemeldet))
                if (m_benutzerKnopf) {
                    m_benutzerKnopf->setText(QStringLiteral("%1   ").arg(b->name));
                    m_benutzerKnopf->setIcon(avatar(b->name));
                }
        });
        benutzerSeite->aktionNeu()->setProperty("primaer", true);
        m_tabs->addWidget(seite(tr("Benutzerverwaltung"),
                                tr("Konten anlegen, Rollen vergeben, PINs zurücksetzen und Zugänge sperren"),
                                benutzerSeite,
                                {benutzerSeite->aktionBearbeiten(), benutzerSeite->aktionPin(),
                                 benutzerSeite->aktionLoeschen(), nullptr, benutzerSeite->aktionNeu()}));
    }

    // Seitenleiste links, rechts Kopfzeile und Inhalt
    auto* rechts = new QWidget;
    auto* rv = new QVBoxLayout(rechts);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    rv->addWidget(baueKopfzeile());
    rv->addWidget(m_tabs, 1);

    // Spaß-Funktion: Werbebanner, solange SolarMonitor Premium aus ist
    m_werbungsBereich = new QWidget;
    m_werbungsBereich->setObjectName(QStringLiteral("Inhalt"));
    m_werbungsBereich->setAttribute(Qt::WA_StyledBackground, true);
    auto* wv = new QVBoxLayout(m_werbungsBereich);
    wv->setContentsMargins(24, 0, 24, 16);
    auto* banner = new WerbeBanner(m_werbungsBereich);
    connect(banner, &WerbeBanner::premiumGewuenscht, this, &MainWindow::premiumKaufen);
    if (!m_werbeSteuerung) {
        // Popup-Werbung jede Minute – nur über dem Programm, nie über Anmeldung oder offenen Dialogen
        m_werbeSteuerung = new WerbeSteuerung(this, [this]() -> QWidget* {
            if (!m_programm || !m_ebenen || m_ebenen->currentWidget() != m_programm)
                return nullptr;
            if (!isVisible() || isMinimized() || QApplication::activeModalWidget() || m_einfuehrung || m_keksBanner)
                return nullptr;
            return m_programm;
        });
        connect(m_werbeSteuerung, &WerbeSteuerung::premiumGewuenscht, this, &MainWindow::premiumKaufen);
    }
    connect(banner, &WerbeBanner::webseiteGewuenscht, m_werbeSteuerung, &WerbeSteuerung::oeffneWebseite);
    m_werbeSteuerung->setAktiv(!Abo::werbefrei());
    wv->addWidget(banner);
    rv->addWidget(m_werbungsBereich);
    m_werbungsBereich->setVisible(!Abo::werbefrei());

    auto* zentrum = new QWidget;
    auto* h = new QHBoxLayout(zentrum);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    h->addWidget(baueNavigation());
    h->addWidget(rechts, 1);
    // alte Oberfläche ersetzen (Ebene "Programm" im Fenster)
    const bool warSichtbar = m_programm && m_ebenen->currentWidget() == m_programm;
    if (m_programm) {
        m_ebenen->removeWidget(m_programm);
        m_programm->deleteLater();
    }
    m_programm = zentrum;
    m_ebenen->addWidget(m_programm);
    if (warSichtbar)
        m_ebenen->setCurrentWidget(m_programm);

    connect(m_tabs, &QStackedWidget::currentChanged, this, &MainWindow::seiteGewechselt);
    m_tabs->setCurrentIndex(aktuelleSeite);
    seiteGewechselt(aktuelleSeite);

    baueMenues();
    aktualisiereRechte();
    aktualisiereZaehler();
    aktualisiereNavigation();
    aktualisiereFilterKnopf();
    analyseFortschritt(m_fortschrittWert, m_fortschrittMax);
}

QWidget* MainWindow::seite(const QString& titel, const QString& hinweis, QWidget* inhalt,
                           const QList<QAction*>& aktionen, const QList<QAction*>& exportieren, QWidget* links)
{
    // Titel und Hinweis stehen in der Kopfzeile; auf der Seite selbst eine
    // Aktionszeile (links Zähler, rechts Befehle) und darunter der Inhalt.
    auto* w = new QWidget;
    w->setObjectName(QStringLiteral("Inhalt"));
    w->setAttribute(Qt::WA_StyledBackground, true);
    w->setWindowTitle(titel);
    w->setProperty("hinweis", hinweis);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(24, 18, 24, 20);
    v->setSpacing(14);

    if (!aktionen.isEmpty() || !exportieren.isEmpty() || links) {
        auto* zeile = new QHBoxLayout;
        zeile->setSpacing(8);
        if (links)
            zeile->addWidget(links);
        zeile->addStretch(1);
        for (QAction* a : aktionen) {
            if (!a) {
                zeile->addSpacing(8);
                continue;
            }
            auto* k = new QToolButton(w);
            k->setDefaultAction(a);
            if (a->menu()) {
                // Aktion mit Menü: Klick öffnet direkt das Menü
                k->setPopupMode(QToolButton::InstantPopup);
                k->setProperty("menue", true);
            }
            const bool primaer = a->property("primaer").toBool();
            const bool nurSymbol = a == m_aktRueckgaengig || a == m_aktWiederholen;
            k->setObjectName(primaer ? QStringLiteral("SeitenAktionPrimaer") : QStringLiteral("SeitenAktion"));
            k->setProperty("nurSymbol", nurSymbol);
            k->setToolButtonStyle(nurSymbol ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
            k->setIconSize(QSize(16, 16));
            if (primaer)
                k->setIcon(Stil::icon(Stil::Symbol::Plus, Qt::white));
            connect(a, &QAction::changed, k, [k, a, primaer] {
                // Text ohne "&" und "…" wirkt auf Schaltflächen ruhiger
                k->setText(QString(a->text()).remove(QLatin1Char('&')).remove(QStringLiteral(" …")));
                if (primaer)
                    k->setIcon(Stil::icon(Stil::Symbol::Plus, a->isEnabled() ? QColor(Qt::white) : Stil::textGedimmt()));
            });
            emit a->changed();
            zeile->addWidget(k);
        }
        if (!exportieren.isEmpty()) {
            auto* k = new QToolButton(w);
            k->setObjectName(QStringLiteral("SeitenAktion"));
            k->setProperty("menue", true);
            k->setText(tr("Exportieren"));
            k->setIcon(Stil::icon(Stil::Symbol::Export, Stil::text()));
            k->setIconSize(QSize(16, 16));
            k->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            k->setPopupMode(QToolButton::InstantPopup);
            auto* menue = new QMenu(k);
            for (QAction* a : exportieren) {
                if (a)
                    menue->addAction(a);
                else
                    menue->addSeparator();
            }
            k->setMenu(menue);
            zeile->addWidget(k);
        }
        v->addLayout(zeile);
    }
    if (QLayout* l = inhalt->layout())
        l->setContentsMargins(0, 0, 0, 0);
    v->addWidget(inhalt, 1);
    return w;
}

QWidget* MainWindow::baueNavigation()
{
    auto* leiste = new QWidget;
    leiste->setObjectName(QStringLiteral("Seitenleiste"));
    leiste->setAttribute(Qt::WA_StyledBackground, true);
    leiste->setFixedWidth(236);

    // Marke oben: Logo, Name und Flotte
    auto* logo = new QLabel(leiste);
    logo->setPixmap(Stil::appIcon().pixmap(34, 34));
    auto* name = new QLabel(Stil::wortmarke(), leiste);
    name->setObjectName(QStringLiteral("Marke"));
    auto* flotte = new QLabel(tr("PV-Flotte Bodensee"), leiste);
    flotte->setObjectName(QStringLiteral("MarkeText"));
    auto* namen = new QVBoxLayout;
    namen->setSpacing(0);
    namen->addWidget(name);
    namen->addWidget(flotte);
    auto* kopf = new QHBoxLayout;
    kopf->setContentsMargins(20, 18, 16, 14);
    kopf->setSpacing(10);
    kopf->addWidget(logo);
    kopf->addLayout(namen, 1);

    m_navigation = new QListWidget(leiste);
    m_navigation->setObjectName(QStringLiteral("Navigation"));
    m_navigation->setItemDelegate(new NavDelegate(m_navigation));
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setFrameShape(QFrame::NoFrame);
    m_navigation->setMouseTracking(true);
    m_navigation->viewport()->setAttribute(Qt::WA_Hover, true);
    m_navigation->setCursor(Qt::PointingHandCursor);

    auto abschnitt = [this](const QString& text) {
        auto* it = new QListWidgetItem(text);
        it->setData(kSeiteRolle, -1);
        it->setFlags(Qt::NoItemFlags);
        m_navigation->addItem(it);
    };
    auto eintrag = [this](Stil::Symbol symbol, const QString& text, int seite) {
        auto* it = new QListWidgetItem(text);
        it->setData(kSeiteRolle, seite);
        it->setData(kSymbolRolle, int(symbol));
        m_navigation->addItem(it);
    };
    abschnitt(tr("Überwachung"));
    eintrag(Stil::Symbol::Uebersicht, tr("Übersicht"), ReiterUebersicht);
    eintrag(Stil::Symbol::Liste, tr("Anlagen"), ReiterAnlagen);
    eintrag(Stil::Symbol::Warnung, tr("Störungen"), ReiterStoerungen);
    eintrag(Stil::Symbol::Karte, tr("Karte"), ReiterKarte);
    abschnitt(tr("Auswertung"));
    eintrag(Stil::Symbol::Statistik, tr("Statistik"), ReiterStatistik);
    eintrag(Stil::Symbol::Werkzeug, tr("Serviceplanung"), ReiterService);
    eintrag(Stil::Symbol::Pruefung, tr("Datenprüfung"), ReiterPruefung);
    connect(m_navigation, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* it) {
        if (it && it->data(kSeiteRolle).toInt() >= 0)
            m_tabs->setCurrentIndex(it->data(kSeiteRolle).toInt());
    });

    // Unten: Zustand der Flottenanalyse
    m_analyseText = new QLabel(leiste);
    m_analyseText->setObjectName(QStringLiteral("AnalyseText"));
    m_analyseText->setWordWrap(true);
    m_fortschritt = new QProgressBar(leiste);
    m_fortschritt->setTextVisible(false);
    auto* unten = new QVBoxLayout;
    unten->setContentsMargins(22, 10, 22, 18);
    unten->setSpacing(6);
    unten->addWidget(m_fortschritt);
    unten->addWidget(m_analyseText);

    auto* v = new QVBoxLayout(leiste);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addLayout(kopf);
    v->addWidget(m_navigation, 1);
    v->addLayout(unten);
    return leiste;
}

QWidget* MainWindow::baueKopfzeile()
{
    auto* kopf = new QWidget;
    kopf->setObjectName(QStringLiteral("Kopfzeile"));
    kopf->setAttribute(Qt::WA_StyledBackground, true);
    kopf->setFixedHeight(68);

    m_seitenTitel = new QLabel(kopf);
    m_seitenTitel->setObjectName(QStringLiteral("SeitenTitel"));
    m_seitenHinweis = new QLabel(kopf);
    m_seitenHinweis->setObjectName(QStringLiteral("SeitenHinweis"));
    auto* titel = new QVBoxLayout;
    titel->setSpacing(0);
    titel->addStretch(1);
    titel->addWidget(m_seitenTitel);
    titel->addWidget(m_seitenHinweis);
    titel->addStretch(1);

    // Suche nach Anlagen, wie das Suchfeld oben in Monitoring-Portalen
    m_suche->setObjectName(QStringLiteral("Suchfeld"));
    m_suche->addAction(Stil::icon(Stil::Symbol::Suche, Stil::textGedimmt()), QLineEdit::LeadingPosition);
    connect(m_suche, &QLineEdit::textEdited, this, [this] { m_tabs->setCurrentIndex(ReiterAnlagen); });

    // Glocke: kritische offene Störungen
    m_glocke = new QToolButton(kopf);
    m_glocke->setObjectName(QStringLiteral("Glocke"));
    m_glocke->setIconSize(QSize(22, 22));
    m_glocke->setCursor(Qt::PointingHandCursor);
    connect(m_glocke, &QToolButton::clicked, this, [this] {
        m_stoerStatus->setCurrentIndex(1); // offen + in Bearbeitung
        m_stoerAnlage->clear();
        m_tabs->setCurrentIndex(ReiterStoerungen);
    });

    auto* trenner = new QFrame(kopf);
    trenner->setObjectName(QStringLiteral("KopfTrenner"));
    trenner->setFixedWidth(1);

    // Benutzer mit Menü (Konto, Darstellung, Sprache, Programm)
    const QString rolle = Rollen::anzeigename(m_kontext.rolle);
    const Benutzer* ich = m_benutzer->finde(m_angemeldet);
    const QString name = ich ? ich->name : rolle;
    auto* benutzer = new QToolButton(kopf);
    benutzer->setObjectName(QStringLiteral("BenutzerKnopf"));
    benutzer->setIcon(avatar(name));
    benutzer->setIconSize(QSize(32, 32));
    benutzer->setText(QStringLiteral("%1   ").arg(name));
    benutzer->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    benutzer->setPopupMode(QToolButton::InstantPopup);
    benutzer->setCursor(Qt::PointingHandCursor);
    benutzer->setToolTip(tr("Angemeldet als %1 (%2)").arg(name, rolle));
    auto* menue = new QMenu(benutzer);
    menue->addAction(Stil::icon(Stil::Symbol::Benutzer, Stil::text()), tr("Benutzer &wechseln …"), this,
                     &MainWindow::benutzerWechseln);
    if (m_kontext.darf(Recht::BenutzerVerwalten)) {
        // nur für Administratoren: Seite "Benutzerverwaltung"
        menue->addAction(Stil::icon(Stil::Symbol::Werkzeug, Stil::text()), tr("Benutzer&verwaltung"), this,
                         [this] { m_tabs->setCurrentIndex(ReiterBenutzer); });
    }
    // Windows Hello für das eigene Konto ein-/ausschalten
    QAction* hello = menue->addAction(tr("Anmeldung mit Windows &Hello")); // ohne Symbol, damit der Haken sichtbar ist
    hello->setCheckable(true);
    auto helloStand = [this, hello] {
        const Benutzer* b = m_benutzer->finde(m_angemeldet);
        QSignalBlocker sperre(hello);
        hello->setChecked(b && !b->helloKonto.isEmpty());
    };
    helloStand();
    menue->setToolTipsVisible(true);
    hello->setToolTip(tr("Statt der PIN per Fingerabdruck, Gesicht oder Windows-PIN anmelden"));
    connect(hello, &QAction::toggled, this, [this, helloStand](bool an) {
        if (!an) {
            const QString f = m_benutzer->setzeHello(m_angemeldet, QString());
            meldung(f.isEmpty() ? tr("Anmeldung mit Windows Hello ausgeschaltet.") : f, 5000, !f.isEmpty());
            helloStand();
            return;
        }
        // Einschalten nur nach erfolgreicher Bestätigung; ist Hello nicht
        // nutzbar, den Grund deutlich anzeigen (Menüpunkt bleibt anklickbar)
        QString grund;
        if (!WindowsHello::verfuegbar(&grund)) {
            meldung(grund, 9000, true);
            helloStand();
            return;
        }
        QString fehler;
        const auto ergebnis = WindowsHello::pruefe(this, tr("Windows Hello für SolarMonitor einrichten"), &fehler);
        if (ergebnis == WindowsHello::Ergebnis::Bestaetigt) {
            const QString f = m_benutzer->setzeHello(m_angemeldet, WindowsHello::windowsKonto());
            meldung(f.isEmpty() ? tr("Windows Hello eingerichtet. Ab jetzt können Sie sich per Fingerabdruck anmelden.") : f,
                    6000, !f.isEmpty());
        } else if (ergebnis != WindowsHello::Ergebnis::Abgebrochen) {
            meldung(fehler, 5000, true);
        }
        helloStand();
    });
    menue->addAction(m_aktVerlauf);
    menue->addAction(Stil::icon(Stil::Symbol::Liste, Stil::text()), tr("Daten&ordner ändern …"), this,
                     [this] { zeigeDatenordner(QString(), true); });
    // Datenquelle: Verlaufsdateien (CSV) oder Live-Generator für alle Auswertungen
    QMenu* quelle = menue->addMenu(Stil::icon(Stil::Symbol::Blitz, Stil::text()), tr("Datenquelle"));
    quelle->setToolTipsVisible(true);
    auto* quellenGruppe = new QActionGroup(quelle);
    QAction* quelleCsv = quelle->addAction(tr("CSV-Dateien (Verlaufsdateien)"));
    QAction* quelleLive = quelle->addAction(tr("Live-Generator (simuliert)"));
    quelleLive->setToolTip(tr("Alle Auswertungen mit simulierten Messwerten aus dem Sonnenstand und einer Wetterlage füttern"));
    for (QAction* a : {quelleCsv, quelleLive}) {
        a->setCheckable(true);
        quellenGruppe->addAction(a);
    }
    const bool live = m_kontext.verlaeufe && m_kontext.verlaeufe->quelle() == VerlaufRepository::Quelle::Live;
    (live ? quelleLive : quelleCsv)->setChecked(true);
    connect(quellenGruppe, &QActionGroup::triggered, this,
            [this, quelleLive](QAction* a) { setzeDatenquelle(a == quelleLive); });
    menue->addSeparator();
    // Abo (nur zum Spaß): Kostenlos mit Werbung, Premium werbefrei, Ultimate mit Extras
    m_aboMenue = menue->addMenu(Stil::icon(Stil::Symbol::Euro, Stil::text()), QString());
    m_aboMenue->setToolTipsVisible(true);
    auto* aboGruppe = new QActionGroup(m_aboMenue);
    for (Abo::Stufe st : {Abo::Stufe::Kostenlos, Abo::Stufe::Premium, Abo::Stufe::Ultimate}) {
        QString text;
        switch (st) {
        case Abo::Stufe::Kostenlos:
            text = tr("Kostenlos (mit Werbung)");
            break;
        case Abo::Stufe::Premium:
            text = tr("Premium – werbefrei · %1/Monat").arg(Abo::preis(st));
            break;
        case Abo::Stufe::Ultimate:
            text = tr("Ultimate – werbefrei + alle Funktionen · %1/Monat").arg(Abo::preis(st));
            break;
        }
        QAction* a = m_aboMenue->addAction(text);
        a->setCheckable(true);
        a->setData(int(st));
        a->setToolTip(Abo::vorteile(st).join(QStringLiteral(" · ")));
        aboGruppe->addAction(a);
        connect(a, &QAction::triggered, this, [this, st] { waehleAbo(int(st)); });
    }
    aktualisiereAboMenue();

    // Ultimate-Funktionen: Schloss-Symbol, solange gesperrt
    QAction* dunkel = menue->addAction(tr("&Dunkles Design"));
    dunkel->setCheckable(true);
    dunkel->setChecked(Stil::istDunkel());
    connect(dunkel, &QAction::toggled, this, [this, dunkel](bool an) {
        if (an && !Abo::pruefe(Abo::Funktion::DunklesDesign) && !Abo::erlaubt(Abo::Funktion::DunklesDesign)) {
            QSignalBlocker sperre(dunkel);
            dunkel->setChecked(false);
            return;
        }
        setzeDesign(an);
    });
    QAction* vollbild = menue->addAction(Stil::icon(Stil::Symbol::Vollbild, Stil::text()), tr("&Vollbild"));
    vollbild->setCheckable(true);
    vollbild->setChecked(isFullScreen() || QSettings().value(QStringLiteral("vollbild"), true).toBool());
    vollbild->setShortcut(Qt::Key_F11);
    connect(vollbild, &QAction::toggled, this, &MainWindow::setzeVollbild);
    addAction(vollbild);
    QAction* intro = menue->addAction(tr("&Intro beim Start"));
    intro->setCheckable(true);
    intro->setChecked(QSettings().value(QStringLiteral("intro"), true).toBool());
    intro->setToolTip(tr("Logo-Animation beim Programmstart zeigen"));
    connect(intro, &QAction::toggled, this, [intro](bool an) {
        if (!an && !Abo::pruefe(Abo::Funktion::IntroAus) && !Abo::erlaubt(Abo::Funktion::IntroAus)) {
            QSignalBlocker sperre(intro);
            intro->setChecked(true);
            return;
        }
        QSettings().setValue(QStringLiteral("intro"), an);
    });
    QMenu* sprache = menue->addMenu(tr("&Sprache"));
    auto* sprachen = new QActionGroup(sprache);
    QList<QAction*> paketSprachen;
    for (const auto& [kuerzel, sprachName] : Sprache::verfuegbar()) {
        QAction* a = sprache->addAction(sprachName);
        a->setCheckable(true);
        a->setChecked(Sprache::aktuell() == kuerzel);
        a->setData(kuerzel);
        sprachen->addAction(a);
        if (Abo::istPaketSprache(kuerzel))
            paketSprachen << a;
        connect(a, &QAction::triggered, this, [this, k = kuerzel, sprachen] {
            if (Abo::istPaketSprache(k) && !Abo::pruefe(Abo::Funktion::Sprachpaket)
                && !Abo::erlaubt(Abo::Funktion::Sprachpaket)) {
                // gesperrt: Haken zurück auf die aktuelle Sprache
                for (QAction* x : sprachen->actions())
                    x->setChecked(x->data().toString() == Sprache::aktuell());
                return;
            }
            QSettings().setValue(QStringLiteral("sprache"), k);
            if (k != Sprache::aktuell() && !Sprache::setze(k))
                QMessageBox::warning(this, QStringLiteral("SolarMonitor"), tr("Die Übersetzung konnte nicht geladen werden."));
        });
    }
    auto schloesser = [dunkel, intro, paketSprachen] {
        const bool frei = Abo::stufe() == Abo::Stufe::Ultimate;
        const QIcon schloss = Stil::icon(Stil::Symbol::Schloss, Stil::textGedimmt());
        dunkel->setIcon(frei ? Stil::icon(Stil::Symbol::Mond, Stil::text()) : schloss);
        intro->setIcon(frei ? QIcon() : schloss);
        for (QAction* a : paketSprachen)
            a->setIcon(frei ? QIcon() : schloss);
        const QString hinweis = frei ? QString() : tr("Nur mit SolarMonitor Ultimate");
        dunkel->setToolTip(hinweis);
        for (QAction* a : paketSprachen)
            a->setToolTip(hinweis);
    };
    schloesser();
    sprache->setToolTipsVisible(true);
    menue->setToolTipsVisible(true);
    connect(Abo::instanz(), &Abo::stufeGeaendert, menue, schloesser);
    menue->addSeparator();
    menue->addAction(Stil::icon(Stil::Symbol::Kompass, Stil::text()), tr("&Einführungstour starten"), this,
                     &MainWindow::starteEinfuehrung);
    menue->addAction(QIcon(keksBild(16, devicePixelRatioF())), tr("&Keks-Einstellungen …"), this,
                     &MainWindow::zeigeKeksEinstellungen);
    menue->addAction(Stil::icon(Stil::Symbol::Info, Stil::text()), tr("&Über SolarMonitor"), this, &MainWindow::zeigeUeber);
    QAction* beenden = menue->addAction(Stil::icon(Stil::Symbol::Abmelden, Stil::text()), tr("&Beenden"), this, &QWidget::close);
    beenden->setShortcut(QKeySequence::Quit);
    addAction(beenden);
    benutzer->setMenu(menue);
    m_benutzerKnopf = benutzer;

    auto* h = new QHBoxLayout(kopf);
    h->setContentsMargins(24, 0, 18, 0);
    h->setSpacing(10);
    h->addLayout(titel, 1);
    h->addWidget(m_suche);
    h->addWidget(m_glocke);
    h->addWidget(trenner);
    m_premiumChip = new QLabel(tr("PREMIUM"), kopf);
    m_premiumChip->setObjectName(QStringLiteral("PremiumChip"));
    aktualisiereAboChip();
    m_premiumChip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    h->addWidget(m_premiumChip, 0, Qt::AlignVCenter);
    h->addWidget(benutzer);

    // Programm schließen (wichtig im Vollbild, wo die Titelleiste fehlt)
    auto* schliessen = new QToolButton(kopf);
    schliessen->setObjectName(QStringLiteral("Schliessen"));
    schliessen->setIconSize(QSize(20, 20));
    schliessen->setCursor(Qt::PointingHandCursor);
    schliessen->setToolTip(tr("SolarMonitor beenden (Strg+Q)"));
    const QIcon normal = Stil::icon(Stil::Symbol::Schliessen, Stil::textGedimmt());
    const QIcon weiss = Stil::icon(Stil::Symbol::Schliessen, Qt::white);
    schliessen->setIcon(normal);
    schliessen->installEventFilter(this);
    schliessen->setProperty("iconNormal", normal);
    schliessen->setProperty("iconHover", weiss);
    connect(schliessen, &QToolButton::clicked, this, &QWidget::close);
    h->addWidget(schliessen);
    return kopf;
}

void MainWindow::seiteGewechselt(int index)
{
    QWidget* w = m_tabs->widget(index);
    if (!w)
        return;
    m_seitenTitel->setText(w->windowTitle());
    m_seitenHinweis->setText(w->property("hinweis").toString());
    // passenden Eintrag der Seitenleiste markieren; Seiten aus dem
    // Benutzermenü (Benutzerverwaltung) haben keinen, dann nichts markieren
    QSignalBlocker sperre(m_navigation);
    m_navigation->setCurrentRow(-1);
    for (int i = 0; i < m_navigation->count(); ++i)
        if (m_navigation->item(i)->data(kSeiteRolle).toInt() == index) {
            m_navigation->setCurrentRow(i);
            break;
        }
    m_navigation->viewport()->update();
}

void MainWindow::aktualisiereNavigation()
{
    if (!m_navigation)
        return;
    // Abzeichen mit der Anzahl, wie ungelesene Meldungen in einem Portal
    auto setze = [this](int seite, int anzahl, const QColor& farbe, const QString& tipp) {
        for (int i = 0; i < m_navigation->count(); ++i) {
            QListWidgetItem* it = m_navigation->item(i);
            if (it->data(kSeiteRolle).toInt() != seite)
                continue;
            it->setData(kZahlRolle, anzahl);
            it->setData(kZahlFarbeRolle, farbe);
            it->setToolTip(tipp);
        }
    };
    int offen = 0, kritisch = 0;
    for (const Stoerung& st : m_kontext.daten->stoerungen())
        if (st.istOffen()) {
            ++offen;
            if (st.istKritisch())
                ++kritisch;
        }
    setze(ReiterStoerungen, offen, kritisch > 0 ? kRot : kSonne,
          tr("%1 offene Störungen, davon %2 kritisch").arg(offen).arg(kritisch));
    int auffaellig = 0;
    for (const Kennzahl& k : m_kontext.analyse->kennzahlen())
        if (k.ok && k.auswertung.istAuffaellig())
            ++auffaellig;
    setze(ReiterAnlagen, auffaellig, kSonne, tr("%1 Anlagen mit erkanntem Fehlerbild").arg(auffaellig));
    const int fehler = m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Fehler);
    setze(ReiterPruefung, fehler, kRot, tr("%1 Fehler beim Einlesen").arg(fehler));

    if (m_glocke) {
        m_glocke->setIcon(glocke(kritisch));
        m_glocke->setToolTip(kritisch > 0 ? tr("%1 kritische Störungen offen – anzeigen").arg(kritisch)
                                          : tr("Keine kritischen Störungen offen"));
    }
}

void MainWindow::starteEinfuehrung()
{
    if (m_einfuehrung || !m_programm || !m_tabs || m_ebenen->currentWidget() != m_programm)
        return;
    const int vorherSeite = m_tabs->currentIndex();
    // Knopf einer Seitenaktion finden (für Hervorhebungen in der Aktionszeile)
    auto knopf = [this](int reiter, QAction* aktion) -> QWidget* {
        if (QWidget* seite = m_tabs->widget(reiter))
            for (QToolButton* k : seite->findChildren<QToolButton*>())
                if (k->defaultAction() == aktion)
                    return k;
        return nullptr;
    };
    auto seite = [this](int reiter) { return [this, reiter] { m_tabs->setCurrentIndex(reiter); }; };
    auto inSeite = [this](int reiter, const char* name) -> QWidget* {
        QWidget* s = m_tabs->widget(reiter);
        return s ? s->findChild<QWidget*>(QLatin1String(name)) : nullptr;
    };

    QVector<Einfuehrung::Schritt> schritte{
        {nullptr, tr("Willkommen bei SolarMonitor"),
         tr("In einer knappen Minute zeigen wir Ihnen die wichtigsten Stellen der Oberfläche.<br><br>"
            "Weiter mit <b>→</b> oder Enter, zurück mit <b>←</b>, abbrechen mit <b>Esc</b>. Die Tour lässt sich "
            "jederzeit im Kontomenü oben rechts erneut starten."),
         seite(ReiterUebersicht)},
        {[this] { return static_cast<QWidget*>(m_navigation); }, tr("Navigation"),
         tr("Hier wechseln Sie zwischen Übersicht, Anlagen, Störungen, Karte, Statistik, Serviceplanung und "
            "Datenprüfung. Zahlen an den Einträgen zeigen offene Punkte.")},
        {[inSeite] { return inSeite(ReiterUebersicht, "UebersichtKacheln"); }, tr("Kennzahlen"),
         tr("Der Zustand der Flotte auf einen Blick – inklusive vermiedenem CO₂. Ein Klick auf eine Kachel öffnet "
            "die passende Seite; per Ziehen lassen sich Kacheln und Karten umordnen."),
         seite(ReiterUebersicht)},
        {[inSeite] { return inSeite(ReiterUebersicht, "FavoritenGruppe"); }, tr("Favoriten"),
         tr("Ihre wichtigsten Anlagen immer im Blick. Anpinnen geht in der Anlagenliste mit <b>Strg+D</b>, per "
            "Rechtsklick oder über den Stern im Detailbereich.")},
        {[this] { return static_cast<QWidget*>(m_anlagenTabelle); }, tr("Anlagenliste"),
         tr("Alle Anlagen mit Kennzahlen. Doppelklick öffnet die Details, rechts daneben stehen die wichtigsten "
            "Angaben zur markierten Anlage. Die Filter darüber grenzen die Liste ein – auch auf Favoriten."),
         seite(ReiterAnlagen)},
        {[knopf, this] { return knopf(ReiterAnlagen, m_aktVergleich); }, tr("Anlagen vergleichen"),
         tr("Zwei Anlagen nebeneinander: Stammdaten und Kennzahlen, der bessere Wert grün markiert, dazu die "
            "Tageserträge im Diagramm.")},
        {[knopf, this] { return knopf(ReiterAnlagen, m_aktRueckgaengig); }, tr("Rückgängig"),
         tr("Jede Änderung an Anlagen und Störungen lässt sich mit <b>Strg+Z</b> zurücknehmen und mit "
            "<b>Strg+Y</b> wiederholen. Den ganzen Verlauf zeigt <i>Exportieren → Änderungsverlauf</i>.")},
        {[this] { return static_cast<QWidget*>(m_karte); }, tr("Karte und Sonnenstand"),
         tr("Lage aller Anlagen und Wetterstationen. Die Leiste darüber zeigt den Sonnenstand: Nachts wird die "
            "Karte dunkel, mit dem Regler spielen Sie den Tagesverlauf durch – so wird klar, warum abends kein "
            "Ertrag kommt."),
         seite(ReiterKarte)},
        {[this] { return static_cast<QWidget*>(m_suche); }, tr("Suche"),
         tr("<b>Strg+F</b> springt hierher. Gesucht wird in Anlagen-ID, Ort und Wetterstation."),
         seite(ReiterUebersicht)},
        {[this] { return static_cast<QWidget*>(m_glocke); }, tr("Kritische Störungen"),
         tr("Die Glocke zeigt, wie viele kritische Störungen offen sind; ein Klick öffnet die Liste.")},
        {[this] { return static_cast<QWidget*>(m_benutzerKnopf); }, tr("Kontomenü"),
         tr("Sprache, Design, Abo, Benutzer wechseln und Abmelden – und hier starten Sie diese Tour erneut.")},
        {nullptr, tr("Fertig!"), tr("Das war's schon. Viel Erfolg mit SolarMonitor!")},
    };

    m_einfuehrung = new Einfuehrung(m_programm, std::move(schritte));
    connect(m_einfuehrung, &Einfuehrung::beendet, this, [this, vorherSeite](bool) {
        QSettings().setValue(QStringLiteral("einfuehrung/") + m_angemeldet, true);
        if (m_tabs)
            m_tabs->setCurrentIndex(vorherSeite == ReiterUebersicht ? ReiterUebersicht : vorherSeite);
        QTimer::singleShot(400, this, [this] { zeigeKeksBanner(); });
    });
    m_einfuehrung->starte();
}

void MainWindow::zeigeKeksBanner(bool nurEinmal)
{
    // Spaß: nur ohne Abo, nur über dem Programm und nicht während Tour oder Dialogen
    if (Abo::werbefrei() || m_keksBanner || m_einfuehrung || (nurEinmal && m_keksGezeigt))
        return;
    if (!m_programm || m_ebenen->currentWidget() != m_programm || QApplication::activeModalWidget())
        return;
    m_keksGezeigt = true;
    m_keksBanner = new KeksBanner(m_programm);
    connect(m_keksBanner, &KeksBanner::entschieden, this, [this](int an, int gesamt) {
        const QString hinweis = tr("(Keine Sorge: Es werden keinerlei Daten erhoben.)");
        if (an == gesamt)
            meldung(tr("Mmh! Alle %1 Kekse akzeptiert. %2").arg(gesamt).arg(hinweis), 6000);
        else if (an <= 4)
            meldung(tr("Alle Kekse abgelehnt. Die 4 notwendigen backen wir trotzdem. %1").arg(hinweis), 6000);
        else
            meldung(tr("%1 von %2 Keksen akzeptiert – gute Wahl. %3").arg(an).arg(gesamt).arg(hinweis), 6000);
    });
    m_keksBanner->zeige();
}

void MainWindow::zeigeKeksEinstellungen()
{
    KeksEinstellungen dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        meldung(tr("Keks-Einstellungen gespeichert: %1 von %2 Keksen aktiv. (Es werden keinerlei Daten erhoben.)")
                    .arg(dlg.anzahlAn())
                    .arg(dlg.anzahl()),
                5000);
}

void MainWindow::favoritUmschalten(const QString& anlageId)
{
    Favoriten* fav = Favoriten::instanz();
    fav->umschalten(anlageId);
    meldung(fav->ist(anlageId) ? tr("★ %1 ist jetzt ein Favorit und erscheint in der Übersicht.").arg(anlageId)
                               : tr("%1 aus den Favoriten entfernt.").arg(anlageId),
            3000);
}

void MainWindow::oeffneVergleich(const QString& erste)
{
    VergleichDialog dlg(m_kontext, erste, this);
    connect(&dlg, &VergleichDialog::anlageOeffnen, this, [this](const QString& id) { oeffneAnlage(id); });
    dlg.exec();
}

void MainWindow::premiumKaufen()
{
    aboKaufen(int(Abo::Stufe::Premium));
}

bool MainWindow::aboKaufen(int stufe)
{
    const auto ziel = Abo::Stufe(stufe);
    if (Abo::stufe() >= ziel)
        return true;
    // Spaß-Bezahlablauf "SolarMonitor Pay" – kein echtes Geld, nichts wird gesendet
    QString konto = m_angemeldet;
    if (m_benutzer)
        if (const Benutzer* b = m_benutzer->finde(m_angemeldet))
            konto = b->name;
    ZahlungDialog zahlung(konto, ziel, this);
    const bool bezahlt = zahlung.exec() == QDialog::Accepted;
    if (bezahlt) {
        Abo::setzeStufe(ziel);
        meldung(ziel == Abo::Stufe::Ultimate
                    ? tr("Danke für Ihr Abo! SolarMonitor Ultimate ist aktiv – werbefrei und mit allen Funktionen "
                         "für %1 im Monat. (Keine Sorge: Spielgeld.)").arg(Abo::preis(ziel))
                    : tr("Danke für Ihr Abo! SolarMonitor Premium ist aktiv – werbefrei für %1 im Monat. "
                         "(Keine Sorge: Spielgeld.)").arg(Abo::preis(ziel)),
                6000);
    } else {
        meldung(Abo::werbefrei() ? tr("Zahlung abgebrochen – es bleibt beim bisherigen Abo.")
                                 : tr("Zahlung abgebrochen – die Werbung bleibt."),
                4000, true);
    }
    aktualisiereAboMenue();
    return bezahlt;
}

void MainWindow::waehleAbo(int stufe)
{
    const auto ziel = Abo::Stufe(stufe);
    const auto alt = Abo::stufe();
    if (ziel == alt) {
        aktualisiereAboMenue();
        return;
    }
    if (ziel > alt) {
        aboKaufen(stufe);
        return;
    }
    // Herabstufen geht sofort (Geld zurück gibt es nicht – es war ja auch keins)
    Abo::setzeStufe(ziel);
    meldung(ziel == Abo::Stufe::Kostenlos
                ? tr("Abo gekündigt. Die Werbung ist zurück.")
                : tr("Abo auf Premium umgestellt. Die Ultimate-Funktionen sind wieder gesperrt."),
            5000);
}

void MainWindow::setzePremium(bool an)
{
    Abo::setzeStufe(an ? Abo::Stufe::Premium : Abo::Stufe::Kostenlos);
}

void MainWindow::aboAnwenden()
{
    const bool werbefrei = Abo::werbefrei();
    if (m_werbungsBereich)
        m_werbungsBereich->setVisible(!werbefrei);
    if (m_werbeSteuerung)
        m_werbeSteuerung->setAktiv(!werbefrei);
    aktualisiereAboChip();
    aktualisiereAboMenue();
    if (Abo::stufe() == Abo::Stufe::Ultimate)
        return;
    // ohne Ultimate: gesperrte Funktionen auf die freie Einstellung zurück
    QSettings einstellungen;
    if (!einstellungen.value(QStringLiteral("intro"), true).toBool())
        einstellungen.setValue(QStringLiteral("intro"), true);
    if (Abo::istPaketSprache(Sprache::aktuell())) {
        einstellungen.setValue(QStringLiteral("sprache"), QStringLiteral("de"));
        Sprache::setze(QStringLiteral("de"));
    }
    if (Stil::istDunkel())
        setzeDesign(false);
}

void MainWindow::aktualisiereAboChip()
{
    if (!m_premiumChip)
        return;
    const auto st = Abo::stufe();
    const bool ultimate = st == Abo::Stufe::Ultimate;
    m_premiumChip->setVisible(st != Abo::Stufe::Kostenlos);
    m_premiumChip->setText(ultimate ? tr("ULTIMATE") : tr("PREMIUM"));
    m_premiumChip->setToolTip(ultimate ? tr("SolarMonitor Ultimate ist aktiv – werbefrei, alle Funktionen frei")
                                       : tr("SolarMonitor Premium ist aktiv – werbefrei"));
    m_premiumChip->setProperty("ultimate", ultimate);
    m_premiumChip->style()->unpolish(m_premiumChip);
    m_premiumChip->style()->polish(m_premiumChip);
}

void MainWindow::aktualisiereAboMenue()
{
    if (!m_aboMenue)
        return;
    const auto st = Abo::stufe();
    m_aboMenue->setTitle(tr("&Abo: %1").arg(st == Abo::Stufe::Kostenlos ? tr("Kostenlos")
                                            : st == Abo::Stufe::Premium  ? QStringLiteral("Premium")
                                                                         : QStringLiteral("Ultimate")));
    for (QAction* a : m_aboMenue->actions())
        a->setChecked(a->data().toInt() == int(st));
}

void MainWindow::zeigePaywall(Abo::Funktion f)
{
    // Paywall: Funktion gibt es nur mit Ultimate – Upgrade anbieten
    QMessageBox frage(this);
    frage.setWindowTitle(QStringLiteral("SolarMonitor Ultimate"));
    frage.setIconPixmap(Stil::icon(Stil::Symbol::Schloss, QColor(109, 40, 217)).pixmap(48, 48));
    frage.setText(tr("<b>„%1“ gibt es nur mit SolarMonitor Ultimate.</b>").arg(Abo::funktionsName(f)));
    QString liste;
    for (const QString& v : Abo::vorteile(Abo::Stufe::Ultimate))
        liste += QStringLiteral("✔ %1<br>").arg(v.toHtmlEscaped());
    frage.setInformativeText(tr("Ultimate enthält:<br>%1<br>Für nur %2 im Monat (Spielgeld).")
                                 .arg(liste, Abo::preis(Abo::Stufe::Ultimate)));
    QPushButton* holen =
        frage.addButton(tr("Ultimate holen – %1").arg(Abo::preis(Abo::Stufe::Ultimate)), QMessageBox::AcceptRole);
    frage.addButton(tr("Vielleicht später"), QMessageBox::RejectRole);
    frage.setDefaultButton(holen);
    frage.exec();
    if (frage.clickedButton() == holen)
        aboKaufen(int(Abo::Stufe::Ultimate));
}

void MainWindow::meldung(const QString& text, int dauerMs, bool hinweis)
{
    // Kurze Einblendung unten rechts statt einer Statuszeile
    if (!m_toast) {
        m_toast = new QLabel(this);
        m_toast->setObjectName(QStringLiteral("Toast"));
        m_toast->setWordWrap(true);
        m_toast->setTextFormat(Qt::RichText);
        m_toast->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
        m_toast->setOpenExternalLinks(false);
        connect(m_toast, &QLabel::linkActivated, this, [this] {
            const auto aktion = m_toastAktion;
            m_toastAktion = {};
            m_toast->hide();
            if (aktion)
                aktion();
        });
        m_toastTimer = new QTimer(this);
        m_toastTimer->setSingleShot(true);
        connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);
    }
    m_toastAktion = {};
    m_toast->setText(text.toHtmlEscaped());
    // Hinweise (z. B. "erst Anlage auswählen") mit orangem Balken links
    if (m_toast->property("hinweis").toBool() != hinweis) {
        m_toast->setProperty("hinweis", hinweis);
        m_toast->style()->unpolish(m_toast);
        m_toast->style()->polish(m_toast);
    }
    // kurze Texte in einer Zeile, lange umbrechen
    m_toast->setFixedWidth(qMin(460, m_toast->fontMetrics().horizontalAdvance(text) + 48));
    m_toast->adjustSize();
    m_toast->move(width() - m_toast->width() - 28, height() - m_toast->height() - 24);
    m_toast->raise();
    m_toast->show();
    m_toastTimer->start(dauerMs);
}

void MainWindow::meldungMitAktion(const QString& text, const QString& linkText, std::function<void()> aktion,
                                  int dauerMs)
{
    // Text mit Abstand als Link anhängen; Breite für beides reservieren
    meldung(text + QStringLiteral("      ") + linkText, dauerMs);
    m_toast->setText(QStringLiteral("%1&nbsp;&nbsp;&nbsp;<a href=\"aktion\" style=\"color:%2; font-weight:600;\">%3</a>")
                         .arg(text.toHtmlEscaped(), Stil::istDunkel() ? QStringLiteral("#fbbf24") : QStringLiteral("#d97706"),
                              linkText.toHtmlEscaped()));
    m_toastAktion = std::move(aktion);
}

void MainWindow::fuehreAus(QUndoCommand* befehl)
{
    m_undoQuelle = UndoQuelle::Neu;
    m_undo->push(befehl); // ruft redo() auf und meldet den neuen Stand (undoStandGeaendert)
    m_undoQuelle = UndoQuelle::Keine;
}

void MainWindow::undoStandGeaendert(int index)
{
    // Rückmeldung mit Link zum direkten Zurücknehmen bzw. Wiederholen
    switch (m_undoQuelle) {
    case UndoQuelle::Neu:
        meldungMitAktion(tr("✓ %1 – gespeichert").arg(m_undo->text(index - 1)), tr("Rückgängig"), [this] {
            m_undoQuelle = UndoQuelle::Rueckgaengig;
            m_undo->undo();
            m_undoQuelle = UndoQuelle::Keine;
        });
        break;
    case UndoQuelle::Rueckgaengig:
        meldungMitAktion(tr("↶ Rückgängig gemacht: %1").arg(m_undo->text(index)), tr("Wiederholen"), [this] {
            m_undoQuelle = UndoQuelle::Wiederholen;
            m_undo->redo();
            m_undoQuelle = UndoQuelle::Keine;
        });
        break;
    case UndoQuelle::Wiederholen:
        meldungMitAktion(tr("↷ Wiederholt: %1").arg(m_undo->text(index - 1)), tr("Rückgängig"), [this] {
            m_undoQuelle = UndoQuelle::Rueckgaengig;
            m_undo->undo();
            m_undoQuelle = UndoQuelle::Keine;
        });
        break;
    case UndoQuelle::Keine:
        break; // z. B. Sprung im Änderungsverlauf: dort sieht man den Stand ohnehin
    }
}

void MainWindow::aktualisiereFilterKnopf()
{
    if (!m_filterReset)
        return;
    const int aktiv = int(!m_filter.suche.isEmpty()) + int(!m_filter.status.isEmpty()) + int(!m_filter.typ.isEmpty())
                    + int(!m_filter.ort.isEmpty()) + int(m_filter.minKwp > 0) + int(m_filter.maxKwp > 0)
                    + int(m_filter.nurAuffaellig) + int(m_filter.nurOffen) + int(m_filter.nurFavoriten);
    m_filterReset->setText(aktiv > 0 ? tr("Filter zurücksetzen (%1)").arg(aktiv) : tr("Filter zurücksetzen"));
    m_filterReset->setEnabled(aktiv > 0);
}

QWidget* MainWindow::baueAnlagenSeite()
{
    auto* seite = new QWidget;

    // Suchfeld sitzt rechts in der Kopfzeile (siehe baueKopfzeile)
    m_suche = new QLineEdit(m_filter.suche);
    m_suche->setPlaceholderText(tr("Anlagen durchsuchen (Strg+F)"));
    m_suche->setToolTip(tr("Sucht in Anlagen-ID, Ort und Wetterstation"));
    m_suche->setClearButtonEnabled(true);
    m_suche->setFixedWidth(300);

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
    m_nurFavoriten = new QCheckBox(tr("nur Favoriten"), seite);
    m_nurFavoriten->setIcon(Stil::icon(Stil::Symbol::SternVoll, QColor(234, 179, 8)));
    m_nurFavoriten->setChecked(m_filter.nurFavoriten);
    m_nurFavoriten->setToolTip(tr("Nur angepinnte Anlagen zeigen (anpinnen mit Strg+D oder dem Stern im Detailbereich)"));
    m_anlageFilter->setNurFavoriten(m_filter.nurFavoriten);

    auto* zuruecksetzen = new QPushButton(tr("Filter zurücksetzen"), seite);
    m_filterReset = zuruecksetzen;

    // Filter in einer Karte über der Tabelle (zwei Zeilen, damit das
    // Fenster auch auf Laptop-Bildschirmen Platz hat)
    auto* filterKarte = new QFrame(seite);
    filterKarte->setObjectName(QStringLiteral("Filterleiste"));
    auto* zeile1 = new QHBoxLayout;
    zeile1->setSpacing(6);
    zeile1->addWidget(new QLabel(tr("Status:"), seite));
    zeile1->addWidget(status, 1);
    zeile1->addSpacing(10);
    zeile1->addWidget(new QLabel(tr("Typ:"), seite));
    zeile1->addWidget(typ, 1);
    zeile1->addSpacing(10);
    zeile1->addWidget(new QLabel(tr("Ort:"), seite));
    zeile1->addWidget(ort, 1);
    zeile1->addSpacing(10);
    zeile1->addWidget(new QLabel(tr("Leistung:"), seite));
    zeile1->addWidget(minKwp);
    zeile1->addWidget(new QLabel(QStringLiteral("–"), seite));
    zeile1->addWidget(maxKwp);
    auto* zeile2 = new QHBoxLayout;
    zeile2->setSpacing(16);
    zeile2->addWidget(nurAuffaellig);
    zeile2->addWidget(nurOffen);
    zeile2->addWidget(m_nurFavoriten);
    zeile2->addStretch();
    zeile2->addWidget(zuruecksetzen);
    for (QComboBox* c : {status, typ, ort}) {
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(9);
    }
    auto* filterV = new QVBoxLayout(filterKarte);
    filterV->setContentsMargins(14, 10, 14, 10);
    filterV->setSpacing(8);
    filterV->addLayout(zeile1);
    filterV->addLayout(zeile2);

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
    m_anlagenTabelle->verticalHeader()->setDefaultSectionSize(32);
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
    connect(m_nurFavoriten, &QCheckBox::toggled, this, [this](bool an) {
        m_filter.nurFavoriten = an;
        m_anlageFilter->setNurFavoriten(an);
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
        m_nurFavoriten->setChecked(false);
    });

    m_anlagenLeer = new QLabel(tr("Keine Anlage entspricht den gewählten Filtern."), seite);
    m_anlagenLeer->setObjectName(QStringLiteral("Leerzustand"));
    m_anlagenLeer->setAlignment(Qt::AlignCenter);
    m_anlagenLeer->hide();

    // Tabelle links, Detailkarte rechts (Breite verstellbar)
    auto* links = new QWidget(seite);
    auto* lv = new QVBoxLayout(links);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);
    lv->addWidget(m_anlagenTabelle, 1);
    lv->addWidget(m_anlagenLeer, 1);
    auto* teiler = new QSplitter(Qt::Horizontal, seite);
    teiler->setChildrenCollapsible(false);
    teiler->setHandleWidth(16);
    teiler->addWidget(links);
    teiler->addWidget(m_anlageInfo);
    teiler->setStretchFactor(0, 1);
    teiler->setSizes({1000, 330});

    auto* v = new QVBoxLayout(seite);
    v->setSpacing(14);
    v->addWidget(filterKarte);
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


    auto* filterKarte = new QFrame(seite);
    filterKarte->setObjectName(QStringLiteral("Filterleiste"));
    auto* leiste = new QHBoxLayout(filterKarte);
    leiste->setContentsMargins(14, 10, 14, 10);
    leiste->setSpacing(6);
    suche->addAction(Stil::icon(Stil::Symbol::Suche, Stil::textGedimmt()), QLineEdit::LeadingPosition);
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
    m_stoerungsTabelle->verticalHeader()->setDefaultSectionSize(32);
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
    v->setSpacing(14);
    v->addWidget(filterKarte);
    v->addWidget(m_stoerungsTabelle, 1);
    v->addWidget(m_stoerungenLeer, 1);
    m_stoerungsTabelle->resizeColumnsToContents();
    return seite;
}

void MainWindow::erzeugeAktionen()
{
    // Aktionen werden bei jedem Neuaufbau neu erzeugt (Texte in aktueller Sprache)
    for (QAction* alt : {m_aktDetails, m_aktBearbeiten, m_aktStoerung, m_aktExportAnlagen, m_aktExportStoerungen,
                         m_aktExportBericht, m_aktPdfAnlage, m_aktPdfFlotte, m_aktRueckgaengig, m_aktWiederholen,
                         m_aktAnalyse, m_aktVerlauf, m_aktFavorit, m_aktVergleich})
        if (alt)
            alt->deleteLater();

    const QColor f = Stil::text();
    m_aktDetails = new QAction(Stil::icon(Stil::Symbol::Details, f), tr("&Details …"), this);
    m_aktDetails->setShortcut(Qt::Key_Return);
    m_aktDetails->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_aktDetails, &QAction::triggered, this, [this] {
        if (const QString id = markierteAnlageOderHinweis(); !id.isEmpty())
            oeffneAnlage(id);
    });

    m_aktBearbeiten = new QAction(Stil::icon(Stil::Symbol::Bearbeiten, f), tr("Stammdaten &bearbeiten …"), this);
    m_aktBearbeiten->setShortcut(Qt::Key_F2);
    connect(m_aktBearbeiten, &QAction::triggered, this, [this] {
        if (const QString id = markierteAnlageOderHinweis(); !id.isEmpty())
            bearbeiteAnlage(id);
    });
    m_aktFavorit = new QAction(Stil::icon(Stil::Symbol::Stern, f), tr("&Favorit"), this);
    m_aktFavorit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_aktFavorit->setToolTip(tr("Markierte Anlage zu den Favoriten hinzufügen oder entfernen (Strg+D)"));
    connect(m_aktFavorit, &QAction::triggered, this, [this] {
        if (const QString id = markierteAnlageOderHinweis(); !id.isEmpty())
            favoritUmschalten(id);
    });
    m_aktVergleich = new QAction(Stil::icon(Stil::Symbol::Vergleich, f), tr("&Vergleichen …"), this);
    m_aktVergleich->setToolTip(tr("Zwei Anlagen nebeneinander vergleichen"));
    connect(m_aktVergleich, &QAction::triggered, this, [this] { oeffneVergleich(markierteAnlage()); });

    m_aktStoerung = new QAction(Stil::icon(Stil::Symbol::Plus, f), tr("Störung &erfassen …"), this);
    connect(m_aktStoerung, &QAction::triggered, this, [this] {
        if (const QString id = markierteAnlageOderHinweis(); !id.isEmpty())
            erfasseStoerung(id);
    });

    m_aktExportAnlagen = new QAction(Stil::icon(Stil::Symbol::Export, f), tr("&Anlagen exportieren …"), this);
    connect(m_aktExportAnlagen, &QAction::triggered, this, &MainWindow::exportiereAnlagen);
    m_aktExportStoerungen = new QAction(Stil::icon(Stil::Symbol::Export, f), tr("&Störungen exportieren …"), this);
    connect(m_aktExportStoerungen, &QAction::triggered, this, &MainWindow::exportiereStoerungen);
    m_aktExportBericht = new QAction(Stil::icon(Stil::Symbol::Bericht, f), tr("&Ertragsbericht exportieren …"), this);
    connect(m_aktExportBericht, &QAction::triggered, this, &MainWindow::exportiereBericht);

    m_aktPdfAnlage = new QAction(Stil::icon(Stil::Symbol::Pdf, f), tr("Anlagenbericht als &PDF …"), this);
    connect(m_aktPdfAnlage, &QAction::triggered, this, [this] {
        if (const QString id = markierteAnlageOderHinweis(); !id.isEmpty())
            exportiereAnlagenPdf(id);
    });
    m_aktPdfFlotte = new QAction(Stil::icon(Stil::Symbol::Pdf, f), tr("&Flottenbericht als PDF …"), this);
    connect(m_aktPdfFlotte, &QAction::triggered, this, &MainWindow::exportiereFlottenPdf);

    m_aktStoerung->setProperty("primaer", true); // Hauptbefehl der Seite, orange hervorgehoben
    m_aktAnalyse = new QAction(Stil::icon(Stil::Symbol::Neuladen, f), tr("Analyse neu starten"), this);
    m_aktAnalyse->setToolTip(tr("Verläufe aller Anlagen erneut auswerten"));
    connect(m_aktAnalyse, &QAction::triggered, this, [this] { m_kontext.analyse->starte(); });
    m_aktVerlauf = new QAction(Stil::icon(Stil::Symbol::Liste, f), tr("Änderungsverlauf …"), this);
    connect(m_aktVerlauf, &QAction::triggered, this, &MainWindow::zeigeAenderungsverlauf);

    // Rückgängig/Wiederholen: Text zeigt, welche Änderung betroffen ist
    m_aktRueckgaengig = new QAction(Stil::icon(Stil::Symbol::Rueckgaengig, f), tr("&Rückgängig"), this);
    m_aktRueckgaengig->setShortcut(QKeySequence::Undo);
    m_aktWiederholen = new QAction(Stil::icon(Stil::Symbol::Wiederholen, f), tr("&Wiederholen"), this);
    m_aktWiederholen->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(m_aktRueckgaengig, &QAction::triggered, this, [this] {
        m_undoQuelle = UndoQuelle::Rueckgaengig;
        m_undo->undo();
        m_undoQuelle = UndoQuelle::Keine;
    });
    connect(m_aktWiederholen, &QAction::triggered, this, [this] {
        m_undoQuelle = UndoQuelle::Wiederholen;
        m_undo->redo();
        m_undoQuelle = UndoQuelle::Keine;
    });
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
    // Keine Menü- oder Symbolleiste: Befehle stehen in der Aktionszeile der
    // Seiten, im Benutzermenü und in den Kontextmenüs.
    m_anlagenTabelle->addAction(m_aktDetails);
    // Tastenkürzel gelten im ganzen Fenster, auch wenn die Seite mit dem
    // passenden Knopf gerade nicht sichtbar ist
    addActions({m_aktRueckgaengig, m_aktWiederholen, m_aktBearbeiten, m_aktFavorit});

    auto* sucheKurz = new QShortcut(QKeySequence::Find, m_tabs);
    connect(sucheKurz, &QShortcut::activated, this, [this] {
        m_tabs->setCurrentIndex(ReiterAnlagen);
        m_suche->setFocus();
        m_suche->selectAll();
    });
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

QString MainWindow::markierteAnlageOderHinweis()
{
    const QString id = markierteAnlage();
    if (id.isEmpty())
        meldung(tr("Bitte zuerst eine Anlage in der Liste auswählen."), 3500, true);
    return id;
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
    const bool favorit = Favoriten::instanz()->ist(id);
    QAction* fav = menue.addAction(favorit ? tr("Aus den Favoriten entfernen") : tr("Zu den Favoriten hinzufügen"), this,
                                   [this, id] { favoritUmschalten(id); });
    fav->setIcon(favorit ? Stil::icon(Stil::Symbol::Stern, Stil::text())
                         : Stil::icon(Stil::Symbol::SternVoll, QColor(234, 179, 8)));
    fav->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    menue.addAction(Stil::icon(Stil::Symbol::Vergleich, Stil::text()), tr("Vergleichen mit …"), this,
                    [this, id] { oeffneVergleich(id); });
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
    fuehreAus(new AnlageAendern(m_kontext.daten, *vorher, dlg.anlage(),
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
    fuehreAus(befehl);
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
    fuehreAus(new StoerungAendern(m_kontext.daten, *vorher, dlg.stoerung(),
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
    fuehreAus(new StoerungAendern(m_kontext.daten, *alt, neu,
                                  [this](const QString& id) { nachStoerungsAenderung(id); }));
}

void MainWindow::speichereAnlagen()
{
    const QString pfad = m_kontext.daten->ausgabePfad(QStringLiteral("anlagen_out.csv"));
    QString fehler;
    if (m_kontext.daten->exportiereAnlagen(pfad, &fehler))
        meldung(tr("Änderung gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 6000);
    else
        QMessageBox::warning(this, tr("Automatisches Speichern fehlgeschlagen"),
                             tr("Die Änderung ist im Speicher, wurde aber nicht gesichert:\n%1").arg(fehler));
}

void MainWindow::speichereStoerungen()
{
    const QString pfad = m_kontext.daten->ausgabePfad(QStringLiteral("stoerungen_out.csv"));
    QString fehler;
    if (m_kontext.daten->exportiereStoerungen(pfad, &fehler))
        meldung(tr("Änderung gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 6000);
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
        meldung(tr("%1 Anlagen exportiert nach %2").arg(m_kontext.daten->anlagen().size())
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
        meldung(tr("Störungen exportiert nach %1").arg(QDir::toNativeSeparators(pfad)), 6000);
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
        meldung(tr("Ertragsbericht gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 6000);
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
    meldung(tr("Bericht gespeichert: %1").arg(QDir::toNativeSeparators(pfad)), 8000);
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
    // Anmeldung im selben Fenster; "Zurück" führt ohne Wechsel zum Programm
    zeigeAnmeldung(true);
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
    const bool laeuft = gesamt > 0 && erledigt < gesamt;
    m_fortschritt->setRange(0, qMax(1, gesamt));
    m_fortschritt->setValue(erledigt);
    m_fortschritt->setVisible(laeuft);
    const bool live = m_kontext.verlaeufe && m_kontext.verlaeufe->quelle() == VerlaufRepository::Quelle::Live;
    if (laeuft)
        m_analyseText->setText(tr("Flottenanalyse läuft: %1 von %2 Anlagen").arg(erledigt).arg(gesamt));
    else if (m_kontext.analyse->istFertig())
        m_analyseText->setText(QStringLiteral("<span style='color:#22c55e'>●</span> ")
                               + tr("Analyse aktuell · %1 Anlagen").arg(m_kontext.analyse->kennzahlen().size())
                               + (live ? QStringLiteral("<br><span style='color:#f59e0b'>") + tr("Live-Generator (simuliert)")
                                           + QStringLiteral("</span>")
                                       : QString()));
    else
        m_analyseText->setText(tr("Flottenanalyse wird vorbereitet …"));
    m_analyseText->setToolTip(tr("Verläufe aller Anlagen werden im Hintergrund ausgewertet (%1 von %2).")
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

    meldung(tr("Flottenanalyse abgeschlossen: %1 Anlagen ausgewertet, %2 mit Fehlerbild, "
                                "%3 Verlaufsdateien defekt.")
                                 .arg(m_kontext.analyse->kennzahlen().size())
                                 .arg(auffaellig)
                                 .arg(meldungen.size()),
                             10000);
}
