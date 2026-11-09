#include "stil.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QDir>
#include <QStandardPaths>
#include <QStyleFactory>

namespace {

/// Farben nach dem Vorbild von Microsoft Office (Fluent): neutrale Grautöne,
/// weiße Arbeitsfläche und eine Programmfarbe. Grün steht für Energie –
/// so wie jedes Office-Programm seine eigene Farbe hat (Word blau, Excel grün).
struct Farben {
    QColor fenster;      ///< Hintergrund außerhalb der Arbeitsfläche
    QColor band;         ///< Menüband
    QColor flaeche;      ///< Arbeitsfläche, Listen, Eingaben
    QColor text, gedimmt;
    QColor linie, linieStark, feldRand;
    QColor hover, gedrueckt;
    QColor auswahl;      ///< markierte Zeile (Akzent-Tönung)
    QColor akzent, akzentHover, akzentText, akzentFlaeche; ///< Akzent als Fläche (Statusleiste, Datei)
};

bool g_dunkel = false;
constexpr double kPiStil = 3.14159265358979323846;

Farben farben()
{
    if (g_dunkel)
        return {QColor("#1b1a19"), QColor("#292827"), QColor("#252423"), QColor("#f3f2f1"), QColor("#b3b0ad"),
                QColor("#3b3a39"), QColor("#605e5c"), QColor("#8a8886"), QColor("#3b3a39"), QColor("#484644"),
                QColor("#1e4d32"), QColor("#4bbf7f"), QColor("#1f7a49"), QColor("#ffffff"), QColor("#185c37")};
    return {QColor("#f3f2f1"), QColor("#faf9f8"), QColor("#ffffff"), QColor("#201f1e"), QColor("#605e5c"),
            QColor("#e1dfdd"), QColor("#c8c6c4"), QColor("#8a8886"), QColor("#edebe9"), QColor("#e1dfdd"),
            QColor("#d9eedf"), QColor("#107c41"), QColor("#0c6634"), QColor("#ffffff"), QColor("#107c41")};
}

/// Kleine Grafik für Pfeile/Häkchen erzeugen und als Datei für das Stylesheet ablegen.
QString grafik(const QString& name, const QColor& farbe, int art)
{
    const QString ordner = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/solarmonitor_stil");
    QDir().mkpath(ordner);
    const QString pfad = QStringLiteral("%1/%2_%3.png").arg(ordner, name, farbe.name().mid(1));
    QPixmap pm(24, 24);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(farbe, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (art == 0) // Pfeil nach unten
        p.drawPolyline(QPolygonF({QPointF(6, 9.5), QPointF(12, 15.5), QPointF(18, 9.5)}));
    else if (art == 1) // Pfeil nach oben
        p.drawPolyline(QPolygonF({QPointF(6, 14.5), QPointF(12, 8.5), QPointF(18, 14.5)}));
    else // Häkchen
        p.drawPolyline(QPolygonF({QPointF(5.5, 12.5), QPointF(10, 17), QPointF(18.5, 7.5)}));
    p.end();
    pm.save(pfad, "PNG");
    return pfad;
}

QString stylesheet(const Farben& f)
{
    QString s = QStringLiteral(R"(
* { outline: 0; }
QMainWindow, QDialog { background: %FENSTER%; }
QWidget#Inhalt { background: %FENSTER%; }

/* ---------- Menüband ---------- */
QWidget#Menueband { background: %BAND%; border-bottom: 1px solid %LINIE%; }
QToolButton#DateiKnopf { background: %AKZFL%; color: #ffffff; border: none; padding: 7px 18px; margin: 0; }
QToolButton#DateiKnopf:hover { background: %AKZH%; }
QToolButton#DateiKnopf::menu-indicator { image: none; width: 0; }
QTabBar#BandReiter { background: transparent; }
QTabBar#BandReiter::tab { background: transparent; color: %TEXT%; border: none; padding: 7px 12px 6px 12px; margin: 0 1px;
    border-bottom: 3px solid transparent; }
QTabBar#BandReiter::tab:hover { background: %HOVER%; }
QTabBar#BandReiter::tab:selected { color: %AKZ%; border-bottom: 3px solid %AKZ%; }
QStackedWidget#BandFlaeche, QWidget#BandSeite { background: %FLAECHE%; }
QStackedWidget#BandFlaeche { border-top: 1px solid %LINIE%; min-height: 96px; max-height: 96px; }
QLabel#BandGruppenTitel { color: %GED%; font-size: 8pt; padding: 1px 0 3px 0; }
QFrame#BandTrenner { color: %LINIE%; background: %LINIE%; margin: 6px 3px; }
QToolButton#BandGross { border: 1px solid transparent; border-radius: 3px; padding: 3px 6px; color: %TEXT%; min-width: 46px; }
QToolButton#BandKlein { border: 1px solid transparent; border-radius: 3px; padding: 2px 6px; color: %TEXT%; text-align: left; }
QToolButton#BandGross:hover, QToolButton#BandKlein:hover { background: %HOVER%; border-color: %LINIE%; }
QToolButton#BandGross:pressed, QToolButton#BandKlein:pressed { background: %GEDR%; }
QToolButton#BandGross:checked, QToolButton#BandKlein:checked { background: %AUSW%; border-color: %AKZ%; }
QToolButton#BandGross:disabled, QToolButton#BandKlein:disabled { color: %GED%; }
QToolButton#BandGross::menu-indicator { subcontrol-position: bottom center; subcontrol-origin: padding; bottom: -2px;
    image: url(%PFEILUNTEN%); width: 9px; height: 9px; }

/* ---------- Navigationsbereich (wie in Outlook) ---------- */
QWidget#Navigationsbereich { background: %FENSTER%; border-right: 1px solid %LINIE%; }
QLabel#Bereichstitel { color: %GED%; font-size: 8pt; font-weight: 600; padding: 10px 14px 4px 14px; }
QListWidget#Navigation { background: transparent; border: none; }
QListWidget#Navigation::item { color: %TEXT%; padding: 7px 10px; border-left: 3px solid transparent; }
QListWidget#Navigation::item:hover { background: %HOVER%; }
QListWidget#Navigation::item:selected { background: %AUSW%; color: %TEXT%; border-left: 3px solid %AKZ%; font-weight: 600; }

/* ---------- Knöpfe ---------- */
QPushButton { background: %FLAECHE%; color: %TEXT%; border: 1px solid %FELD%; border-radius: 2px; padding: 4px 16px;
    min-height: 18px; }
QPushButton:hover { background: %HOVER%; }
QPushButton:pressed { background: %GEDR%; }
QPushButton:default, QPushButton#Primaer { background: %AKZFL%; color: #ffffff; border-color: %AKZFL%; }
QPushButton:default:hover, QPushButton#Primaer:hover { background: %AKZH%; border-color: %AKZH%; }
QPushButton:disabled { color: %GED%; background: %FENSTER%; border-color: %LINIE%; }
QPushButton:flat { border: none; background: transparent; }
QToolButton { color: %TEXT%; border: 1px solid transparent; border-radius: 2px; padding: 2px 4px; }
QToolButton:hover { background: %HOVER%; border-color: %LINIE%; }

/* ---------- Eingaben ---------- */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QPlainTextEdit, QTextEdit {
    background: %FLAECHE%; color: %TEXT%; border: 1px solid %FELD%; border-radius: 2px; padding: 3px 6px;
    selection-background-color: %AKZ%; selection-color: #ffffff; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit { min-height: 18px; }
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QDateEdit:hover, QDateTimeEdit:hover {
    border-color: %TEXT%; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus,
QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid %AKZ%; border-bottom: 2px solid %AKZ%; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color: %GED%; background: %FENSTER%;
    border-color: %LINIE%; }
QLineEdit#Suchfeld { border-radius: 2px; padding: 4px 8px; min-width: 240px; }
QComboBox::drop-down, QDateEdit::drop-down, QDateTimeEdit::drop-down { border: none; width: 22px; }
QComboBox::down-arrow, QDateEdit::down-arrow, QDateTimeEdit::down-arrow { image: url(%PFEILUNTEN%); width: 11px; height: 11px; }
QComboBox QAbstractItemView { background: %FLAECHE%; border: 1px solid %FELDLINIE%; selection-background-color: %AUSW%;
    selection-color: %TEXT%; outline: 0; }
QAbstractSpinBox { padding-right: 18px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { subcontrol-origin: border; width: 18px; border: none;
    background: transparent; }
QAbstractSpinBox::up-button { subcontrol-position: top right; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: %HOVER%; }
QAbstractSpinBox::up-arrow { image: url(%PFEILOBEN%); width: 9px; height: 9px; }
QAbstractSpinBox::down-arrow { image: url(%PFEILUNTEN%); width: 9px; height: 9px; }
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off,
QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off { image: none; }
QCheckBox { spacing: 6px; color: %TEXT%; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %TEXT%; border-radius: 2px; background: %FLAECHE%; }
QCheckBox::indicator:hover { border-color: %AKZ%; }
QCheckBox::indicator:checked { background: %AKZFL%; border-color: %AKZFL%; image: url(%HAKEN%); }
QCheckBox::indicator:disabled { border-color: %LINIESTARK%; background: %FENSTER%; }

/* ---------- Tabellen und Listen ---------- */
QTableView, QTableWidget, QTreeView, QTreeWidget, QListWidget {
    background: %FLAECHE%; alternate-background-color: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIE%;
    gridline-color: %LINIE%; selection-background-color: %AUSW%; selection-color: %TEXT%; }
QTableView::item, QTreeView::item { padding: 0 6px; border: none; }
QTableView::item:hover, QTreeView::item:hover { background: %HOVER%; }
QTableView::item:selected, QTreeView::item:selected, QListWidget::item:selected { background: %AUSW%; color: %TEXT%; }
QHeaderView { background: %FLAECHE%; }
QHeaderView::section { background: %FLAECHE%; color: %GED%; border: none; border-right: 1px solid %LINIE%;
    border-bottom: 1px solid %LINIESTARK%; padding: 4px 8px; font-weight: 600; }
QHeaderView::section:hover { background: %HOVER%; color: %TEXT%; }
QHeaderView::down-arrow { image: url(%PFEILUNTEN%); width: 9px; height: 9px; }
QHeaderView::up-arrow { image: url(%PFEILOBEN%); width: 9px; height: 9px; }
QTableCornerButton::section { background: %FLAECHE%; border: none; }

/* ---------- Reiter (Dialoge) ---------- */
QTabWidget::pane { border: none; border-top: 1px solid %LINIE%; background: %FENSTER%; top: -1px; }
QTabBar::tab { background: transparent; color: %TEXT%; padding: 6px 14px; border: none; border-bottom: 3px solid transparent; }
QTabBar::tab:hover { background: %HOVER%; }
QTabBar::tab:selected { color: %AKZ%; border-bottom: 3px solid %AKZ%; font-weight: 600; }
QTabBar::tab:disabled { color: %GED%; }

/* ---------- Gruppen als Abschnitte (Überschrift mit Linie) ---------- */
QGroupBox { background: transparent; border: none; border-top: 1px solid %LINIE%; margin-top: 22px; padding-top: 8px;
    font-weight: 600; color: %TEXT%; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 0; top: 2px; padding: 0 6px 0 0;
    color: %AKZ%; }

/* ---------- Kopfband in Dialogen ---------- */
QWidget#DialogKopf { background: %AKZFL%; }
QLabel#DialogKopfTitel { color: #ffffff; font-size: 14pt; font-weight: 600; }
QLabel#DialogKopfText { color: #ffffff; }

/* ---------- Detailbereich, Karten ---------- */
QWidget#Detailbereich { background: %FLAECHE%; border-left: 1px solid %LINIE%; }
QLabel#DetailTitel { font-size: 13pt; font-weight: 600; }
QLabel#Gedimmt, QLabel#KpiTitel, QLabel#KpiNotiz { color: %GED%; }
QLabel#KpiWert { font-weight: 600; }
QLabel#Leerzustand { color: %GED%; padding: 24px; }
QFrame#Karte, QFrame#KarteKlick { background: %FLAECHE%; border: 1px solid %LINIE%; }
QSplitter::handle { background: %LINIE%; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }

/* ---------- Menüs ---------- */
QMenu { background: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIESTARK%; padding: 4px 0; }
QMenu::item { padding: 6px 28px 6px 12px; }
QMenu::item:selected { background: %HOVER%; }
QMenu::item:disabled { color: %GED%; }
QMenu::separator { height: 1px; background: %LINIE%; margin: 4px 0; }
QMenu::icon { padding-left: 8px; }
QToolTip { background: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIESTARK%; padding: 4px 6px; }

/* ---------- Statusleiste in der Programmfarbe (wie Word/Excel) ---------- */
QStatusBar { background: %AKZFL%; color: #ffffff; min-height: 24px; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: #ffffff; padding: 0 6px; }
QStatusBar QPushButton { color: #ffffff; background: transparent; border: none; padding: 1px 8px; min-height: 0; }
QStatusBar QPushButton:hover { background: rgba(255,255,255,0.18); }
QProgressBar { background: rgba(255,255,255,0.25); border: none; max-height: 6px; color: transparent; }
QProgressBar::chunk { background: #ffffff; }

/* ---------- Bildlaufleisten ---------- */
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle { background: %LINIESTARK%; border: 3px solid transparent; background-clip: padding; min-height: 30px;
    min-width: 30px; }
QScrollBar::handle:hover { background: %FELD%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
)");
    s.replace(QStringLiteral("%FENSTER%"), f.fenster.name());
    s.replace(QStringLiteral("%BAND%"), f.band.name());
    s.replace(QStringLiteral("%FLAECHE%"), f.flaeche.name());
    s.replace(QStringLiteral("%TEXT%"), f.text.name());
    s.replace(QStringLiteral("%GED%"), f.gedimmt.name());
    s.replace(QStringLiteral("%LINIESTARK%"), f.linieStark.name());
    s.replace(QStringLiteral("%LINIE%"), f.linie.name());
    s.replace(QStringLiteral("%FELDLINIE%"), f.linieStark.name());
    s.replace(QStringLiteral("%FELD%"), f.feldRand.name());
    s.replace(QStringLiteral("%HOVER%"), f.hover.name());
    s.replace(QStringLiteral("%GEDR%"), f.gedrueckt.name());
    s.replace(QStringLiteral("%AUSW%"), f.auswahl.name());
    s.replace(QStringLiteral("%AKZFL%"), f.akzentFlaeche.name());
    s.replace(QStringLiteral("%AKZH%"), f.akzentHover.name());
    s.replace(QStringLiteral("%AKZ%"), f.akzent.name());
    s.replace(QStringLiteral("%PFEILUNTEN%"), grafik(QStringLiteral("unten"), f.gedimmt, 0));
    s.replace(QStringLiteral("%PFEILOBEN%"), grafik(QStringLiteral("oben"), f.gedimmt, 1));
    s.replace(QStringLiteral("%HAKEN%"), grafik(QStringLiteral("haken"), QColor("#ffffff"), 2));
    return s;
}

} // namespace

void Stil::anwenden(bool dunkel)
{
    g_dunkel = dunkel;
    const Farben f = farben();

    // Immer "Fusion" als Grundlage: Das Stylesheet bestimmt das Aussehen
    // vollständig, dadurch sieht das Programm überall gleich aus.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, f.fenster);
    p.setColor(QPalette::WindowText, f.text);
    p.setColor(QPalette::Base, f.flaeche);
    p.setColor(QPalette::AlternateBase, f.flaeche);
    p.setColor(QPalette::Text, f.text);
    p.setColor(QPalette::Button, f.flaeche);
    p.setColor(QPalette::ButtonText, f.text);
    p.setColor(QPalette::ToolTipBase, f.flaeche);
    p.setColor(QPalette::ToolTipText, f.text);
    p.setColor(QPalette::Highlight, f.akzentFlaeche);
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::PlaceholderText, f.gedimmt);
    p.setColor(QPalette::Light, f.flaeche);
    p.setColor(QPalette::Midlight, f.hover);
    p.setColor(QPalette::Mid, f.linieStark);
    p.setColor(QPalette::Dark, f.feldRand);
    p.setColor(QPalette::Shadow, QColor(0, 0, 0, dunkel ? 160 : 60));
    p.setColor(QPalette::Link, f.akzent);
    p.setColor(QPalette::LinkVisited, f.akzent);
    p.setColor(QPalette::Disabled, QPalette::Text, f.gedimmt);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, f.gedimmt);
    p.setColor(QPalette::Disabled, QPalette::WindowText, f.gedimmt);
    QApplication::setPalette(p);
    qApp->setStyleSheet(stylesheet(f));
    QApplication::setWindowIcon(appIcon());
}

bool Stil::istDunkel() { return g_dunkel; }
QColor Stil::akzent() { return farben().akzent; }
QColor Stil::text() { return farben().text; }
QColor Stil::textGedimmt() { return farben().gedimmt; }
QColor Stil::flaeche() { return farben().flaeche; }
QColor Stil::rahmen() { return farben().linie; }

// -------------------------------------------------------------------
//  Icons: 24er-Raster, Linienstärke 2, abgerundete Enden
// -------------------------------------------------------------------

QIcon Stil::icon(Symbol symbol, const QColor& farbeWunsch)
{
    const QColor farbe = farbeWunsch.isValid() ? farbeWunsch : text();
    QIcon icon;
    for (int groesse : {16, 24, 32, 48}) {
        QPixmap pm(groesse, groesse);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(groesse / 24.0, groesse / 24.0);
        QPen stift(farbe, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(stift);
        p.setBrush(Qt::NoBrush);

        switch (symbol) {
        case Symbol::Liste:
            for (int y : {6, 12, 18}) {
                p.drawLine(QPointF(9, y), QPointF(20, y));
                p.drawPoint(QPointF(4.5, y));
            }
            break;
        case Symbol::Warnung: {
            QPainterPath d;
            d.moveTo(12, 3.5);
            d.lineTo(21.5, 20);
            d.lineTo(2.5, 20);
            d.closeSubpath();
            p.drawPath(d);
            p.drawLine(QPointF(12, 9.5), QPointF(12, 14));
            p.drawPoint(QPointF(12, 17));
            break;
        }
        case Symbol::Karte: {
            QPainterPath pin;
            pin.moveTo(12, 21);
            pin.cubicTo(12, 21, 5, 14, 5, 9.5);
            pin.arcTo(QRectF(5, 2.5, 14, 14), 180, -180);
            pin.cubicTo(19, 14, 12, 21, 12, 21);
            p.drawPath(pin);
            p.drawEllipse(QPointF(12, 9.5), 2.5, 2.5);
            break;
        }
        case Symbol::Statistik:
            p.drawLine(QPointF(3, 20.5), QPointF(21, 20.5));
            p.drawLine(QPointF(6.5, 17), QPointF(6.5, 12));
            p.drawLine(QPointF(12, 17), QPointF(12, 5));
            p.drawLine(QPointF(17.5, 17), QPointF(17.5, 9));
            break;
        case Symbol::Werkzeug: {
            QPainterPath w;
            w.moveTo(14.5, 6);
            w.arcTo(QRectF(13, 3, 8, 8), 200, 280);
            w.lineTo(9, 18);
            w.arcTo(QRectF(4, 16, 4, 4), 45, 180);
            w.closeSubpath();
            p.drawPath(w);
            break;
        }
        case Symbol::Pruefung: {
            QPainterPath s;
            s.moveTo(12, 3);
            s.lineTo(20, 6);
            s.lineTo(20, 12);
            s.cubicTo(20, 16.5, 16.5, 19.5, 12, 21);
            s.cubicTo(7.5, 19.5, 4, 16.5, 4, 12);
            s.lineTo(4, 6);
            s.closeSubpath();
            p.drawPath(s);
            p.drawPolyline(QPolygonF({QPointF(8.5, 12), QPointF(11, 14.5), QPointF(15.5, 9.5)}));
            break;
        }
        case Symbol::Details:
        case Symbol::Info:
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawLine(QPointF(12, 11), QPointF(12, 16.5));
            p.drawPoint(QPointF(12, 7.5));
            break;
        case Symbol::Bearbeiten:
            p.drawPolygon(QPolygonF({QPointF(15.5, 4.5), QPointF(19.5, 8.5), QPointF(8.5, 19.5), QPointF(4, 20),
                                     QPointF(4.5, 15.5)}));
            p.drawLine(QPointF(13, 7), QPointF(17, 11));
            break;
        case Symbol::Plus:
            p.drawLine(QPointF(12, 5), QPointF(12, 19));
            p.drawLine(QPointF(5, 12), QPointF(19, 12));
            break;
        case Symbol::Export:
            p.drawLine(QPointF(12, 3.5), QPointF(12, 15));
            p.drawPolyline(QPolygonF({QPointF(7.5, 10.5), QPointF(12, 15), QPointF(16.5, 10.5)}));
            p.drawPolyline(QPolygonF({QPointF(4, 15), QPointF(4, 20), QPointF(20, 20), QPointF(20, 15)}));
            break;
        case Symbol::Bericht:
            p.drawPolygon(QPolygonF({QPointF(6, 3), QPointF(14, 3), QPointF(19, 8), QPointF(19, 21), QPointF(6, 21)}));
            p.drawLine(QPointF(9, 13), QPointF(16, 13));
            p.drawLine(QPointF(9, 17), QPointF(14, 17));
            break;
        case Symbol::Neuladen: {
            p.drawArc(QRectF(4, 4, 16, 16), 60 * 16, 290 * 16);
            p.drawPolyline(QPolygonF({QPointF(16, 3), QPointF(16.5, 6.5), QPointF(13, 7.5)}));
            break;
        }
        case Symbol::Rueckgaengig:
        case Symbol::Wiederholen: {
            // gebogener Pfeil; "Wiederholen" ist das Spiegelbild
            if (symbol == Symbol::Wiederholen) {
                p.translate(24, 0);
                p.scale(-1, 1);
            }
            p.drawPolyline(QPolygonF({QPointF(9, 14), QPointF(4, 9), QPointF(9, 4)}));
            QPainterPath bogen;
            bogen.moveTo(4, 9);
            bogen.lineTo(14.5, 9);
            bogen.arcTo(QRectF(9, 9, 11, 11), 90, -180);
            bogen.lineTo(11, 20);
            p.drawPath(bogen);
            break;
        }
        case Symbol::Uebersicht:
            // vier Kacheln wie auf der Übersichtsseite
            p.drawRoundedRect(QRectF(3.5, 3.5, 7.5, 9), 1.5, 1.5);
            p.drawRoundedRect(QRectF(13, 3.5, 7.5, 5.5), 1.5, 1.5);
            p.drawRoundedRect(QRectF(3.5, 15, 7.5, 5.5), 1.5, 1.5);
            p.drawRoundedRect(QRectF(13, 11, 7.5, 9.5), 1.5, 1.5);
            break;
        case Symbol::Vollbild:
            // vier Ecken
            p.drawPolyline(QPolygonF({QPointF(4, 9), QPointF(4, 4), QPointF(9, 4)}));
            p.drawPolyline(QPolygonF({QPointF(15, 4), QPointF(20, 4), QPointF(20, 9)}));
            p.drawPolyline(QPolygonF({QPointF(20, 15), QPointF(20, 20), QPointF(15, 20)}));
            p.drawPolyline(QPolygonF({QPointF(9, 20), QPointF(4, 20), QPointF(4, 15)}));
            break;
        case Symbol::Pdf: {
            // Dokument mit umgeknickter Ecke und Textzeilen
            QPainterPath blatt;
            blatt.moveTo(14, 3);
            blatt.lineTo(6, 3);
            blatt.lineTo(6, 21);
            blatt.lineTo(18, 21);
            blatt.lineTo(18, 7);
            blatt.closeSubpath();
            p.drawPath(blatt);
            p.drawPolyline(QPolygonF({QPointF(14, 3), QPointF(14, 7), QPointF(18, 7)}));
            p.drawLine(QPointF(9, 12), QPointF(15, 12));
            p.drawLine(QPointF(9, 16), QPointF(15, 16));
            break;
        }
        case Symbol::Sonne:
            p.drawEllipse(QPointF(12, 12), 4, 4);
            for (int i = 0; i < 8; ++i) {
                const double w = i * 3.14159265 / 4.0;
                p.drawLine(QPointF(12 + 7 * std::cos(w), 12 + 7 * std::sin(w)),
                           QPointF(12 + 9.5 * std::cos(w), 12 + 9.5 * std::sin(w)));
            }
            break;
        case Symbol::Mond: {
            QPainterPath m;
            m.moveTo(19.5, 14.5);
            m.arcTo(QRectF(3.5, 3.5, 17, 17), -20, -250);
            m.arcTo(QRectF(9, 1.5, 13, 13), 140, 160);
            p.drawPath(m);
            break;
        }
        case Symbol::Benutzer:
            p.drawEllipse(QPointF(12, 8), 4, 4);
            p.drawArc(QRectF(4.5, 14, 15, 13), 0, 180 * 16);
            break;
        case Symbol::Haken:
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawPolyline(QPolygonF({QPointF(8, 12.5), QPointF(11, 15.5), QPointF(16.5, 9)}));
            break;
        case Symbol::Fehler:
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawLine(QPointF(9, 9), QPointF(15, 15));
            p.drawLine(QPointF(15, 9), QPointF(9, 15));
            break;
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

void Stil::zeichneLogo(QPainter& p, const QRectF& ziel)
{
    // Entwurf auf einem 64er-Raster; Kacheln bis 20 px bekommen eine
    // vereinfachte Variante (weniger Strahlen, gerader Horizont), damit das
    // Symbol auch in der Titelleiste noch klar erkennbar ist.
    const double g = std::min(ziel.width(), ziel.height());
    const bool klein = g <= 20.0;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(ziel.center() - QPointF(g / 2.0, g / 2.0));
    p.scale(g / 64.0, g / 64.0);

    // Kachel mit Verlauf Amber -> Orange und leichtem Glanz oben
    QPainterPath kachel;
    kachel.addRoundedRect(QRectF(2, 2, 60, 60), 15, 15);
    QLinearGradient verlauf(QPointF(8, 4), QPointF(56, 60));
    verlauf.setColorAt(0.0, QColor("#fbbf24"));
    verlauf.setColorAt(0.55, QColor("#f59e0b"));
    verlauf.setColorAt(1.0, QColor("#ea580c"));
    p.fillPath(kachel, verlauf);
    QLinearGradient glanz(QPointF(0, 2), QPointF(0, 34));
    glanz.setColorAt(0.0, QColor(255, 255, 255, 60));
    glanz.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillPath(kachel, glanz);

    // Aufgehende Sonne über dem Horizont
    const QColor weiss(Qt::white);
    const QPointF m(32, klein ? 37 : 35);
    const double r = klein ? 11.0 : 10.0;
    QPainterPath sonne;
    sonne.moveTo(m.x() - r, m.y());
    sonne.arcTo(QRectF(m.x() - r, m.y() - r, 2 * r, 2 * r), 180, -180);
    sonne.closeSubpath();
    p.fillPath(sonne, weiss);

    p.setPen(QPen(weiss, klein ? 4.5 : 3.2, Qt::SolidLine, Qt::RoundCap));
    const int strahlen = klein ? 3 : 5;
    for (int i = 0; i < strahlen; ++i) {
        const double winkel = (klein ? 45.0 + i * 45.0 : 15.0 + i * 37.5) * kPiStil / 180.0;
        const double r1 = r + (klein ? 5.0 : 4.5), r2 = r + (klein ? 10.0 : 9.5);
        p.drawLine(QPointF(m.x() + r1 * std::cos(winkel), m.y() - r1 * std::sin(winkel)),
                   QPointF(m.x() + r2 * std::cos(winkel), m.y() - r2 * std::sin(winkel)));
    }

    // Horizont als Messkurve: steht für die Überwachung (Monitoring)
    p.setPen(QPen(weiss, klein ? 4.5 : 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (klein) {
        p.drawLine(QPointF(10, 44), QPointF(54, 44));
    } else {
        const QPolygonF kurve({QPointF(11, 42), QPointF(24.5, 42), QPointF(28.5, 37.5), QPointF(34, 50),
                               QPointF(38.5, 42), QPointF(53, 42)});
        p.drawPolyline(kurve);
    }
    p.restore();
}

QPixmap Stil::logo(int groesse)
{
    QPixmap pm(groesse, groesse);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    zeichneLogo(p, QRectF(0, 0, groesse, groesse));
    p.end();
    return pm;
}

QString Stil::wortmarke()
{
    return QStringLiteral("<span style='font-weight:700'>Solar</span><span style='font-weight:400'>Monitor</span>");
}

QIcon Stil::appIcon()
{
    QIcon icon;
    for (int g : {16, 20, 24, 32, 40, 48, 64, 128, 256})
        icon.addPixmap(logo(g));
    return icon;
}
