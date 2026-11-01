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

struct Farben {
    QColor hintergrund, flaeche, rahmen, text, gedimmt, alternativ, eingabe, hover, auswahl, seite, seiteText,
        seiteGedimmt, scroll;
};

bool g_dunkel = false;

const QColor kAkzent(245, 158, 11); // Amber, "Sonne"
constexpr double kPiStil = 3.14159265358979323846;

Farben farben()
{
    if (g_dunkel)
        return {QColor("#0f172a"), QColor("#1e293b"), QColor("#334155"), QColor("#e2e8f0"), QColor("#94a3b8"),
                QColor("#223047"), QColor("#0f172a"), QColor("#2b3a52"), QColor(245, 158, 11, 70), QColor("#020617"),
                QColor("#cbd5e1"), QColor("#64748b"), QColor("#475569")};
    return {QColor("#f1f5f9"), QColor("#ffffff"), QColor("#e2e8f0"), QColor("#0f172a"), QColor("#64748b"),
            QColor("#f8fafc"), QColor("#ffffff"), QColor("#e2e8f0"), QColor("#fef3c7"), QColor("#0f172a"),
            QColor("#cbd5e1"), QColor("#64748b"), QColor("#cbd5e1")};
}

QString rgba(const QColor& c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
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
    p.setPen(QPen(farbe, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (art == 0) // Pfeil nach unten
        p.drawPolyline(QPolygonF({QPointF(6, 9), QPointF(12, 15), QPointF(18, 9)}));
    else if (art == 1) // Pfeil nach oben
        p.drawPolyline(QPolygonF({QPointF(6, 15), QPointF(12, 9), QPointF(18, 15)}));
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
QMainWindow, QDialog { background: %HG%; }
QWidget#Inhalt { background: %HG%; }

/* ---------- Seitenleiste ---------- */
QWidget#Seitenleiste { background: %SEITE%; }
QLabel#AppTitel { color: #ffffff; font-size: 14pt; font-weight: 600; }
QLabel#AppUntertitel, QLabel#SeitenleisteInfo { color: %SEITEGED%; }
QListWidget#Navigation { background: transparent; border: none; color: %SEITETEXT%; font-size: 10.5pt; }
QListWidget#Navigation::item { padding: 9px 10px; margin: 2px 10px; border-radius: 8px; }
QListWidget#Navigation::item:hover { background: rgba(255,255,255,0.07); }
QListWidget#Navigation::item:selected { background: rgba(245,158,11,0.18); color: #fbbf24; }
QPushButton#SeitenleisteKnopf { background: transparent; color: %SEITETEXT%; border: 1px solid rgba(255,255,255,0.15);
    border-radius: 8px; padding: 6px 10px; text-align: left; }
QPushButton#SeitenleisteKnopf:hover { background: rgba(255,255,255,0.07); }

/* ---------- Seitenkopf ---------- */
QLabel#Seitentitel { font-size: 16pt; font-weight: 600; color: %TEXT%; }
QLabel#Seitenhinweis { color: %GED%; }

/* ---------- Karten / Kennzahlen ---------- */
QFrame#Karte { background: %FL%; border: 1px solid %RAHMEN%; border-radius: 10px; }
QLabel#KpiTitel { color: %GED%; font-size: 9pt; }
QLabel#KpiWert { color: %TEXT%; font-size: 17pt; font-weight: 600; }
QLabel#KpiNotiz { color: %GED%; font-size: 8.5pt; }
QLabel#Gedimmt { color: %GED%; }
QLabel#Leerzustand { color: %GED%; font-size: 11pt; padding: 30px; }
QFrame#KarteKlick { background: %FL%; border: 1px solid %RAHMEN%; border-radius: 10px; }
QFrame#KarteKlick:hover { border-color: #f59e0b; }
QLabel#KartenTitel { color: %TEXT%; font-size: 11pt; font-weight: 600; }
QPushButton#Link { border: none; background: transparent; color: #d97706; padding: 2px 4px; font-weight: 600; }
QPushButton#Link:hover { color: #b45309; text-decoration: underline; }
QToolButton#Schnell { border: 1px solid %RAHMEN%; border-radius: 8px; padding: 6px 12px; text-align: left; }
QToolButton#Schnell:hover { border-color: #f59e0b; background: %HOVER%; }

/* ---------- Eingaben ---------- */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit {
    background: %EIN%; color: %TEXT%; border: 1px solid %RAHMEN%; border-radius: 7px; padding: 5px 8px; min-height: 20px; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus {
    border: 1px solid #f59e0b; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(%PFEILUNTEN%); width: 12px; height: 12px; }
QDateEdit::drop-down, QDateTimeEdit::drop-down { border: none; width: 24px; }
QDateEdit::down-arrow, QDateTimeEdit::down-arrow { image: url(%PFEILUNTEN%); width: 12px; height: 12px; }
QAbstractSpinBox { padding-right: 20px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { subcontrol-origin: border; width: 20px; border: none;
    background: transparent; }
QAbstractSpinBox::up-button { subcontrol-position: top right; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; }
QAbstractSpinBox::up-arrow { image: url(%PFEILOBEN%); width: 10px; height: 10px; }
QAbstractSpinBox::down-arrow { image: url(%PFEILUNTEN%); width: 10px; height: 10px; }
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off { image: none; }
QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off { image: none; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %GED%; border-radius: 4px; background: %EIN%; }
QCheckBox::indicator:hover { border-color: #f59e0b; }
QCheckBox::indicator:checked { background: #f59e0b; border-color: #f59e0b; image: url(%HAKEN%); }
QCheckBox::indicator:disabled { background: %HG%; border-color: %RAHMEN%; }
QComboBox QAbstractItemView { background: %FL%; border: 1px solid %RAHMEN%; selection-background-color: %AUSW%;
    selection-color: %TEXT%; padding: 4px; }

/* ---------- Knöpfe ---------- */
QPushButton { background: %FL%; color: %TEXT%; border: 1px solid %RAHMEN%; border-radius: 7px; padding: 6px 14px; }
QPushButton:hover { border-color: #f59e0b; }
QPushButton:pressed { background: %HOVER%; }
QPushButton:default, QPushButton#Primaer { background: #f59e0b; border-color: #f59e0b; color: #1f2937; font-weight: 600; }
QPushButton:default:hover, QPushButton#Primaer:hover { background: #fbbf24; }
QPushButton:disabled { color: %GED%; background: %HG%; border-color: %RAHMEN%; }
QToolButton { border: none; border-radius: 7px; padding: 6px 10px; color: %TEXT%; }
QToolButton:hover { background: %HOVER%; }
QToolButton:disabled { color: %GED%; }
QCheckBox { spacing: 6px; color: %TEXT%; }

/* ---------- Tabellen und Listen ---------- */
QTableView, QTableWidget, QListWidget, QTreeView {
    background: %FL%; alternate-background-color: %ALT%; color: %TEXT%; border: 1px solid %RAHMEN%;
    border-radius: 10px; gridline-color: transparent; selection-background-color: %AUSW%; selection-color: %TEXT%; }
QTableView::item, QTableWidget::item { padding: 0 6px; border: none; }
QTableWidget#Kompakt { border: none; border-radius: 0; background: transparent; }
QTableWidget#Kompakt::item { border-bottom: 1px solid %RAHMEN%; }
QTableWidget#Kompakt::item:hover { background: %HOVER%; }
QListWidget::item { padding: 5px; }
QHeaderView { background: transparent; }
QHeaderView::section { background: %FL%; color: %GED%; border: none; border-bottom: 1px solid %RAHMEN%;
    padding: 7px 8px; font-weight: 600; }
QHeaderView::section:hover { color: %TEXT%; }
QTableCornerButton::section { background: %FL%; border: none; }

/* ---------- Reiter, Gruppen ---------- */
QTabWidget::pane { border: 1px solid %RAHMEN%; border-radius: 10px; background: %FL%; top: -1px; }
QTabBar::tab { background: transparent; color: %GED%; padding: 8px 16px; border: none; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: %TEXT%; border-bottom: 2px solid #f59e0b; font-weight: 600; }
QTabBar::tab:hover { color: %TEXT%; }
QTabBar::tab:disabled { color: %RAHMEN%; }
QGroupBox { background: %FL%; border: 1px solid %RAHMEN%; border-radius: 10px; margin-top: 26px; padding: 10px 8px 8px 8px;
    font-weight: 600; color: %TEXT%; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 2px; top: 4px; padding: 0; }
QSplitter::handle { background: transparent; }

/* ---------- Menüs, Leisten ---------- */
QMenuBar { background: %HG%; color: %TEXT%; padding: 2px 6px; }
QMenuBar::item { padding: 5px 10px; border-radius: 6px; background: transparent; }
QMenuBar::item:selected { background: %HOVER%; }
QMenu { background: %FL%; color: %TEXT%; border: 1px solid %RAHMEN%; border-radius: 8px; padding: 5px; }
QMenu::item { padding: 6px 26px 6px 12px; border-radius: 5px; }
QMenu::item:selected { background: %AUSW%; }
QMenu::item:disabled { color: %GED%; }
QMenu::separator { height: 1px; background: %RAHMEN%; margin: 4px 8px; }
QToolBar { background: %HG%; border: none; padding: 6px 12px; spacing: 4px; }
QToolBar::separator { width: 1px; background: %RAHMEN%; margin: 4px 6px; }
QStatusBar { background: %FL%; border-top: 1px solid %RAHMEN%; color: %GED%; }
QStatusBar QLabel { color: %GED%; }
QProgressBar { background: %RAHMEN%; border: none; border-radius: 4px; max-height: 8px; color: transparent; }
QProgressBar::chunk { background: #f59e0b; border-radius: 4px; }
QToolTip { background: %FL%; color: %TEXT%; border: 1px solid %RAHMEN%; padding: 5px; border-radius: 6px; }

/* ---------- Bildlaufleisten ---------- */
QScrollBar:vertical { background: transparent; width: 11px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 2px; }
QScrollBar::handle { background: %SCROLL%; border-radius: 4px; min-height: 30px; min-width: 30px; }
QScrollBar::handle:hover { background: %GED%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollArea { background: transparent; border: none; }
)");
    s.replace(QStringLiteral("%HG%"), f.hintergrund.name());
    s.replace(QStringLiteral("%FL%"), f.flaeche.name());
    s.replace(QStringLiteral("%RAHMEN%"), f.rahmen.name());
    s.replace(QStringLiteral("%TEXT%"), f.text.name());
    s.replace(QStringLiteral("%GED%"), f.gedimmt.name());
    s.replace(QStringLiteral("%ALT%"), f.alternativ.name());
    s.replace(QStringLiteral("%EIN%"), f.eingabe.name());
    s.replace(QStringLiteral("%HOVER%"), f.hover.name());
    s.replace(QStringLiteral("%AUSW%"), rgba(f.auswahl));
    s.replace(QStringLiteral("%SEITE%"), f.seite.name());
    s.replace(QStringLiteral("%SEITETEXT%"), f.seiteText.name());
    s.replace(QStringLiteral("%SEITEGED%"), f.seiteGedimmt.name());
    s.replace(QStringLiteral("%SCROLL%"), f.scroll.name());
    s.replace(QStringLiteral("%PFEILUNTEN%"), grafik(QStringLiteral("unten"), f.gedimmt, 0));
    s.replace(QStringLiteral("%PFEILOBEN%"), grafik(QStringLiteral("oben"), f.gedimmt, 1));
    s.replace(QStringLiteral("%HAKEN%"), grafik(QStringLiteral("haken"), QColor("#1f2937"), 2));
    return s;
}

} // namespace

void Stil::anwenden(bool dunkel)
{
    g_dunkel = dunkel;
    const Farben f = farben();
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, f.hintergrund);
    p.setColor(QPalette::WindowText, f.text);
    p.setColor(QPalette::Base, f.flaeche);
    p.setColor(QPalette::AlternateBase, f.alternativ);
    p.setColor(QPalette::Text, f.text);
    p.setColor(QPalette::Button, f.flaeche);
    p.setColor(QPalette::ButtonText, f.text);
    p.setColor(QPalette::ToolTipBase, f.flaeche);
    p.setColor(QPalette::ToolTipText, f.text);
    p.setColor(QPalette::Highlight, kAkzent);
    p.setColor(QPalette::HighlightedText, QColor("#1f2937"));
    p.setColor(QPalette::PlaceholderText, f.gedimmt);
    p.setColor(QPalette::Mid, f.rahmen.darker(dunkel ? 70 : 115));
    p.setColor(QPalette::Light, f.flaeche);
    p.setColor(QPalette::Dark, f.rahmen);
    p.setColor(QPalette::Link, kAkzent.darker(120));
    p.setColor(QPalette::Disabled, QPalette::Text, f.gedimmt);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, f.gedimmt);
    p.setColor(QPalette::Disabled, QPalette::WindowText, f.gedimmt);
    QApplication::setPalette(p);

    qApp->setStyleSheet(stylesheet(f));
    QApplication::setWindowIcon(appIcon());
}

bool Stil::istDunkel() { return g_dunkel; }
QColor Stil::akzent() { return kAkzent; }
QColor Stil::text() { return farben().text; }
QColor Stil::textGedimmt() { return farben().gedimmt; }
QColor Stil::flaeche() { return farben().flaeche; }
QColor Stil::rahmen() { return farben().rahmen; }

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
