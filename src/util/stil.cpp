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

/// Farben nach dem Vorbild von Monitoring-Portalen für PV-Anlagen:
/// dunkelblaue Seitenleiste, helle Arbeitsfläche mit weißen Karten und
/// Sonnen-Orange als Akzent. Statusfarben (grün/orange/rot) bleiben
/// für den Zustand der Anlagen reserviert.
struct Farben {
    QColor fenster;      ///< Hintergrund der Arbeitsfläche (zwischen den Karten)
    QColor seite;        ///< Seitenleiste
    QColor flaeche;      ///< Karten, Listen, Eingaben
    QColor text, gedimmt;
    QColor linie, linieStark, feldRand;
    QColor hover, gedrueckt;
    QColor auswahl;      ///< markierte Zeile (Akzent-Tönung)
    QColor akzent, akzentHover, link, akzentFlaeche; ///< Akzent, Verweise, Hauptknopf
    QColor kopf;         ///< Spaltenköpfe
};

bool g_dunkel = false;
constexpr double kPiStil = 3.14159265358979323846;

Farben farben()
{
    if (g_dunkel)
        return {QColor("#0e1622"), QColor("#09111b"), QColor("#16212f"), QColor("#e5ebf3"), QColor("#8d9bb0"),
                QColor("#243245"), QColor("#31435b"), QColor("#3a4d66"), QColor("#1d2a3b"), QColor("#253549"),
                QColor("#3b2c12"), QColor("#fbbf24"), QColor("#b45309"), QColor("#7cb3ff"), QColor("#d97706"),
                QColor("#1a2636")};
    return {QColor("#f2f4f7"), QColor("#0f1e33"), QColor("#ffffff"), QColor("#1d2939"), QColor("#667085"),
            QColor("#e4e7ec"), QColor("#d0d5dd"), QColor("#c4cad4"), QColor("#f5f7fa"), QColor("#e9edf2"),
            QColor("#fff2dc"), QColor("#f59e0b"), QColor("#b45309"), QColor("#1d5fd1"), QColor("#d97706"),
            QColor("#f8f9fb")};
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

/* ---------- Seitenleiste (immer dunkel, wie in Monitoring-Portalen) ---------- */
QWidget#Seitenleiste { background: %SEITE%; }
QLabel#Marke { color: #ffffff; font-size: 13pt; }
QLabel#MarkeText { color: #8293ab; font-size: 8pt; }
QListWidget#Navigation { background: transparent; border: none; }
QLabel#AnalyseText { color: #8293ab; font-size: 8pt; }
QWidget#Seitenleiste QProgressBar { background: #1f3350; border: none; border-radius: 2px; max-height: 4px;
    color: transparent; }
QWidget#Seitenleiste QProgressBar::chunk { background: %AKZ%; border-radius: 2px; }

/* ---------- Kopfzeile ---------- */
QWidget#Kopfzeile { background: %FLAECHE%; border-bottom: 1px solid %LINIE%; }
QLabel#SeitenTitel { font-size: 15pt; font-weight: 600; color: %TEXT%; }
QLabel#SeitenHinweis { color: %GED%; }
QLineEdit#Suchfeld { background: %FENSTER%; border: 1px solid %FENSTER%; border-radius: 17px; padding: 6px 12px;
    min-width: 260px; }
QLineEdit#Suchfeld:hover { border-color: %LINIESTARK%; }
QLineEdit#Suchfeld:focus { background: %FLAECHE%; border: 1px solid %AKZ%; }
QToolButton#Glocke { border: none; border-radius: 8px; padding: 6px; }
QToolButton#Glocke:hover, QToolButton#BenutzerKnopf:hover { background: %HOVER%; }
QToolButton#BenutzerKnopf { border: none; border-radius: 6px; padding: 4px 8px; color: %TEXT%; }
QToolButton#BenutzerKnopf::menu-indicator { image: url(%PFEILUNTEN%); width: 10px; height: 10px;
    subcontrol-position: right center; right: 4px; }
QToolButton#Schliessen { border: none; border-radius: 8px; padding: 7px; }
QToolButton#Schliessen:hover { background: #dc2626; }
QToolButton#Schliessen:pressed { background: #b91c1c; }
QFrame#KopfTrenner { background: %LINIE%; max-width: 1px; margin: 10px 4px; }

/* ---------- Seitenaktionen ---------- */
QToolButton#SeitenAktion, QToolButton#SeitenAktionPrimaer { background: %FLAECHE%; color: %TEXT%;
    border: 1px solid %LINIESTARK%; border-radius: 6px; padding: 5px 12px; }
QToolButton#SeitenAktion:hover { background: %HOVER%; }
QToolButton#SeitenAktion:pressed { background: %GEDR%; }
QToolButton#SeitenAktion:disabled { color: %GED%; background: %FENSTER%; border-color: %LINIE%; }
QToolButton#SeitenAktion[nurSymbol="true"] { padding: 5px 7px; }
QToolButton#SeitenAktion::menu-indicator { image: url(%PFEILUNTEN%); width: 10px; height: 10px;
    subcontrol-position: right center; right: 6px; }
QToolButton#SeitenAktion[menue="true"] { padding-right: 24px; }
QToolButton#SeitenAktionPrimaer { background: %AKZFL%; border-color: %AKZFL%; color: #ffffff; font-weight: 600; }
QToolButton#SeitenAktionPrimaer:hover { background: %AKZH%; border-color: %AKZH%; }
QToolButton#SeitenAktionPrimaer:disabled { background: %LINIE%; border-color: %LINIE%; color: %GED%; }
QLabel#Zaehler { color: %GED%; }

/* ---------- Karten ---------- */
QGroupBox { background: %FLAECHE%; border: 1px solid %LINIE%; border-radius: 8px; margin-top: 0;
    padding: 34px 6px 6px 6px; font-weight: 600; color: %TEXT%; }
QGroupBox::title { subcontrol-origin: padding; subcontrol-position: top left; left: 14px; top: 11px;
    color: %TEXT%; font-size: 10pt; }
QGroupBox QTreeView, QGroupBox QTreeWidget, QGroupBox QTableView, QGroupBox QTableWidget, QGroupBox QListWidget {
    border: none; border-top: 1px solid %LINIE%; }
QFrame#Kachel, QFrame#Filterleiste { background: %FLAECHE%; border: 1px solid %LINIE%; border-radius: 8px; }
QFrame#Kachel[klickbar="true"]:hover { border-color: %AKZ%; }
QLabel#KachelTitel { color: %GED%; }
QLabel#KachelWert { font-size: 19pt; font-weight: 600; color: %TEXT%; }
QLabel#KachelNotiz { color: %GED%; font-size: 8.5pt; }

/* Detailbereich: eine Karte, darin Abschnitte statt weiterer Karten */
QWidget#Detailbereich { background: %FLAECHE%; border: 1px solid %LINIE%; border-radius: 8px; }
QWidget#Detailbereich QGroupBox { border: none; border-top: 1px solid %LINIE%; border-radius: 0; margin-top: 18px;
    padding: 10px 0 0 0; background: transparent; }
QWidget#Detailbereich QGroupBox::title { subcontrol-origin: margin; left: 0; top: 0; color: %GED%; font-size: 8.5pt; }
QLabel#DetailTitel { font-size: 14pt; font-weight: 600; }
QLabel#Gedimmt, QLabel#KpiTitel, QLabel#KpiNotiz { color: %GED%; }
QLabel#KpiWert { font-size: 13pt; font-weight: 600; }
QLabel#Leerzustand { color: %GED%; padding: 24px; background: %FLAECHE%; border: 1px solid %LINIE%; border-radius: 8px; }
QSplitter::handle { background: transparent; }
QFrame#Platzhalter { background: transparent; border: 2px dashed %LINIESTARK%; border-radius: 8px; }
QFrame#Ablageziel { background: rgba(245, 158, 11, 0.10); border: 2px solid %AKZ%; border-radius: 8px; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }

/* ---------- Knöpfe ---------- */
QPushButton { background: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIESTARK%; border-radius: 6px; padding: 5px 16px;
    min-height: 18px; }
QPushButton:hover { background: %HOVER%; }
QPushButton:pressed { background: %GEDR%; }
QPushButton:default, QPushButton#Primaer { background: %AKZFL%; color: #ffffff; border-color: %AKZFL%; font-weight: 600; }
QPushButton:default:hover, QPushButton#Primaer:hover { background: %AKZH%; border-color: %AKZH%; }
QPushButton:disabled { color: %GED%; background: %FENSTER%; border-color: %LINIE%; }
QPushButton:flat { border: none; background: transparent; }
QToolButton { color: %TEXT%; border: 1px solid transparent; border-radius: 6px; padding: 3px 5px; }
QToolButton:hover { background: %HOVER%; }

/* ---------- Eingaben ---------- */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QPlainTextEdit, QTextEdit {
    background: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIESTARK%; border-radius: 6px; padding: 4px 8px;
    selection-background-color: %AKZFL%; selection-color: #ffffff; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit { min-height: 18px; }
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QDateEdit:hover, QDateTimeEdit:hover {
    border-color: %FELD%; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus,
QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid %AKZ%; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color: %GED%; background: %FENSTER%;
    border-color: %LINIE%; }
QComboBox::drop-down, QDateEdit::drop-down, QDateTimeEdit::drop-down { border: none; width: 24px; }
QComboBox::down-arrow, QDateEdit::down-arrow, QDateTimeEdit::down-arrow { image: url(%PFEILUNTEN%); width: 11px; height: 11px; }
QComboBox QAbstractItemView { background: %FLAECHE%; border: 1px solid %LINIESTARK%; selection-background-color: %AUSW%;
    selection-color: %TEXT%; outline: 0; }
QAbstractSpinBox { padding-right: 20px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { subcontrol-origin: border; width: 20px; border: none;
    background: transparent; }
QAbstractSpinBox::up-button { subcontrol-position: top right; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: %HOVER%; }
QAbstractSpinBox::up-arrow { image: url(%PFEILOBEN%); width: 9px; height: 9px; }
QAbstractSpinBox::down-arrow { image: url(%PFEILUNTEN%); width: 9px; height: 9px; }
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off,
QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off { image: none; }
QCheckBox { spacing: 7px; color: %TEXT%; }
QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid %FELD%; border-radius: 4px; background: %FLAECHE%; }
QCheckBox::indicator:hover { border-color: %AKZ%; }
QCheckBox::indicator:checked { background: %AKZFL%; border-color: %AKZFL%; image: url(%HAKEN%); }
QCheckBox::indicator:disabled { border-color: %LINIE%; background: %FENSTER%; }

/* ---------- Tabellen und Listen ---------- */
QTableView, QTableWidget, QTreeView, QTreeWidget, QListWidget {
    background: %FLAECHE%; alternate-background-color: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIE%;
    border-radius: 8px; gridline-color: %LINIE%; selection-background-color: %AUSW%; selection-color: %TEXT%; }
QTableView::item, QTreeView::item { padding: 0 8px; border: none; border-bottom: 1px solid %LINIE%; }
QTableView::item:hover, QTreeView::item:hover { background: %HOVER%; }
QTableView::item:selected, QTreeView::item:selected, QListWidget::item:selected { background: %AUSW%; color: %TEXT%; }
QHeaderView { background: transparent; }
QHeaderView::section { background: %KOPF%; color: %GED%; border: none; border-bottom: 1px solid %LINIE%;
    padding: 7px 8px; font-weight: 600; font-size: 8.5pt; }
QHeaderView::section:first { border-top-left-radius: 8px; }
QHeaderView::section:last { border-top-right-radius: 8px; }
QHeaderView::section:hover { color: %TEXT%; }
QHeaderView::down-arrow { image: url(%PFEILUNTEN%); width: 9px; height: 9px; }
QHeaderView::up-arrow { image: url(%PFEILOBEN%); width: 9px; height: 9px; }
QTableCornerButton::section { background: %KOPF%; border: none; }

/* ---------- Reiter (Dialoge) ---------- */
QTabWidget::pane { border: none; border-top: 1px solid %LINIE%; background: %FENSTER%; top: -1px; }
QTabBar::tab { background: transparent; color: %GED%; padding: 8px 16px; border: none; border-bottom: 2px solid transparent;
    font-weight: 600; }
QTabBar::tab:hover { color: %TEXT%; }
QTabBar::tab:selected { color: %TEXT%; border-bottom: 2px solid %AKZ%; }
QTabBar::tab:disabled { color: %LINIESTARK%; }

/* ---------- Kopf in Dialogen ---------- */
QWidget#DialogKopf { background: %SEITE%; }
QLabel#DialogKopfTitel { color: #ffffff; font-size: 16pt; font-weight: 600; }
QLabel#DialogKopfText { color: #9fb0c6; }
QLabel#AnmeldenTitel { font-size: 15pt; font-weight: 600; }
QWidget#Anmeldeseite { background: qradialgradient(cx:0.5, cy:0.45, radius:0.8, fx:0.5, fy:0.45,
    stop:0 #16294a, stop:1 #08111f); }
QFrame#Anmeldekarte { background: %FLAECHE%; border: none; border-radius: 12px; }
QFrame#Anmeldekarte QWidget#DialogKopf { border-top-left-radius: 12px; border-bottom-left-radius: 12px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1e3a64, stop:1 #13253f); }

/* ---------- Menüs, Hinweise ---------- */
QMenu { background: %FLAECHE%; color: %TEXT%; border: 1px solid %LINIE%; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 10px; border-radius: 4px; }
QMenu::item:selected { background: %HOVER%; }
QMenu::item:disabled { color: %GED%; }
QMenu::separator { height: 1px; background: %LINIE%; margin: 5px 4px; }
QMenu::icon { padding-left: 8px; }
QToolTip { background: %SEITE%; color: #ffffff; border: none; padding: 5px 8px; }
QFrame#Werbung { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #fff7d6, stop:0.55 #ffe08a, stop:1 #ffb347);
    border: 2px solid #e11d48; border-radius: 8px; }
QLabel#WerbungHinweis { color: #7c2d12; font-size: 7.5pt; font-weight: 700; border: 1px solid #7c2d12;
    border-radius: 3px; padding: 0 4px; background: transparent; }
QLabel#WerbungSticker { color: #ffffff; background: #e11d48; font-size: 8pt; font-weight: 900; border-radius: 4px; padding: 1px 8px; }
QLabel#WerbungSticker[blink="true"] { color: #b91c1c; background: #facc15; }
QLabel#WerbungTitel { color: #7c2d12; font-size: 14pt; font-weight: 900; background: transparent; }
QLabel#WerbungText { color: #431407; background: transparent; }
QLabel#WerbungCountdown { color: #b91c1c; font-weight: 800; background: transparent; }
QPushButton#WerbungAktion { background: #e11d48; color: #ffffff; border: 2px solid #9f1239; border-radius: 16px;
    padding: 8px 22px; font-weight: 900; font-size: 11pt; }
QPushButton#WerbungAktion[blink="true"] { background: #16a34a; border-color: #14532d; }
QPushButton#WerbungAktion:hover { background: #be123c; }
QToolButton#WerbungSchliessen { color: #7c2d12; font-size: 13pt; border: none; padding: 0 4px; background: transparent; }
QToolButton#WerbungSchliessen:hover { color: #000000; background: transparent; }
QFrame#WerbePopupKarte { background: #fffbea; border: 3px solid #e11d48; border-radius: 14px; }
QLabel#WerbePopupKopf { background: #e11d48; color: #ffffff; font-size: 13pt; font-weight: 900; padding: 12px 24px;
    border-top-left-radius: 10px; border-top-right-radius: 10px; }
QLabel#WerbePopupKopf[blink="true"] { background: #facc15; color: #b91c1c; }
QLabel#WerbePopupTitel { color: #7c2d12; font-size: 22pt; font-weight: 900; background: transparent; }
QLabel#WerbePopupText { color: #431407; font-size: 11pt; background: transparent; }
QLabel#WerbePopupPreis, QLabel#WerbeSeitePreis { color: #b91c1c; font-size: 15pt; background: transparent; }
QLabel#WerbePopupKlein { color: #a8a29e; font-size: 7.5pt; background: transparent; }
QPushButton#WerbePopupPremium { color: #78716c; background: transparent; border: none; text-decoration: underline;
    font-size: 8.5pt; padding: 2px; }
QPushButton#WerbePopupPremium:hover { color: #44403c; }
QToolButton#WerbePopupKreuz { color: rgba(255, 255, 255, 110); background: transparent; border: none; padding: 0;
    font-size: 7pt; }
QToolButton#WerbePopupKreuz:hover { color: rgba(255, 255, 255, 200); background: transparent; }
QWidget#WerbeSeite { background: #ffffff; }
QWidget#WerbeSeiteReiter { background: #dfe1e5; }
QLabel#WerbeSeiteTab { background: #ffffff; color: #202124; padding: 7px 14px; border-top-left-radius: 8px;
    border-top-right-radius: 8px; }
QWidget#WerbeSeiteLeiste { background: #ffffff; border-bottom: 1px solid #dadce0; }
QToolButton#WerbeSeiteNav { color: #9aa0a6; background: transparent; border: none; font-size: 12pt; padding: 2px 6px; }
QLineEdit#WerbeSeiteAdresse { background: #f1f3f4; color: #202124; border: none; border-radius: 14px; padding: 5px 14px; }
QScrollArea#WerbeSeiteScroll, QWidget#WerbeSeiteInhalt { background: #fffbea; }
QLabel#WerbeSeiteGewinn { background: #e11d48; color: #ffffff; font-size: 16pt; font-weight: 900; padding: 12px;
    border-radius: 8px; }
QLabel#WerbeSeiteGewinn[blink="true"] { background: #facc15; color: #b91c1c; }
QLabel#WerbeSeiteTitel { color: #7c2d12; font-size: 30pt; font-weight: 900; background: transparent; }
QLabel#WerbeSeiteText { color: #431407; font-size: 11pt; background: transparent; }
QFrame#WerbeSeiteBox { background: #ffffff; border: 2px dashed #f59e0b; border-radius: 10px; }
QLabel#WerbeSeiteBoxTitel { color: #15803d; font-size: 12pt; font-weight: 800; background: transparent; }
QFrame#WerbeSeiteFuss { background: #f5f5f4; border-radius: 8px; }
QFrame#WerbeSeiteFuss QLabel { color: #57534e; background: transparent; }
QPushButton#WerbeSeitePremium { background: #1f2937; color: #fde68a; border: none; border-radius: 6px; padding: 8px 16px;
    font-weight: 700; }
QLabel#PremiumChip { color: #3b2a05; font-size: 7.5pt; font-weight: 800; letter-spacing: 1px; border-radius: 10px;
    padding: 3px 10px; background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #fde68a, stop:0.5 #f59e0b, stop:1 #fbbf24); }
QLabel#PremiumChip[ultimate="true"] { color: #ffffff;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #a855f7, stop:0.5 #6d28d9, stop:1 #db2777); }
QLabel#Toast { background: %SEITE%; color: #ffffff; border-radius: 8px; padding: 10px 16px; }
QLabel#Toast[hinweis="true"] { border-left: 4px solid %AKZ%; padding-left: 14px; }
QProgressBar { background: %LINIE%; border: none; border-radius: 3px; max-height: 6px; color: transparent; }
QProgressBar::chunk { background: %AKZ%; border-radius: 3px; }

/* ---------- Bildlaufleisten ---------- */
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle { background: %LINIESTARK%; border: 3px solid transparent; border-radius: 6px; background-clip: padding;
    min-height: 30px; min-width: 30px; }
QScrollBar::handle:hover { background: %FELD%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
)");
    s.replace(QStringLiteral("%FENSTER%"), f.fenster.name());
    s.replace(QStringLiteral("%SEITE%"), f.seite.name());
    s.replace(QStringLiteral("%FLAECHE%"), f.flaeche.name());
    s.replace(QStringLiteral("%TEXT%"), f.text.name());
    s.replace(QStringLiteral("%GED%"), f.gedimmt.name());
    s.replace(QStringLiteral("%LINIESTARK%"), f.linieStark.name());
    s.replace(QStringLiteral("%LINIE%"), f.linie.name());
    s.replace(QStringLiteral("%FELD%"), f.feldRand.name());
    s.replace(QStringLiteral("%HOVER%"), f.hover.name());
    s.replace(QStringLiteral("%GEDR%"), f.gedrueckt.name());
    s.replace(QStringLiteral("%AUSW%"), f.auswahl.name());
    s.replace(QStringLiteral("%AKZFL%"), f.akzentFlaeche.name());
    s.replace(QStringLiteral("%AKZH%"), f.akzentHover.name());
    s.replace(QStringLiteral("%AKZ%"), f.akzent.name());
    s.replace(QStringLiteral("%KOPF%"), f.kopf.name());
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
    p.setColor(QPalette::Link, f.link);
    p.setColor(QPalette::LinkVisited, f.link);
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
QColor Stil::hintergrund() { return farben().fenster; }
QColor Stil::seitenleiste() { return farben().seite; }

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
        case Symbol::Glocke: {
            QPainterPath g;
            g.moveTo(5, 17);
            g.lineTo(6.5, 15);
            g.lineTo(6.5, 10.5);
            g.cubicTo(6.5, 7, 9, 4.5, 12, 4.5);
            g.cubicTo(15, 4.5, 17.5, 7, 17.5, 10.5);
            g.lineTo(17.5, 15);
            g.lineTo(19, 17);
            g.closeSubpath();
            p.drawPath(g);
            p.drawArc(QRectF(9.5, 17.5, 5, 4), 180 * 16, 180 * 16);
            break;
        }
        case Symbol::Suche:
            p.drawEllipse(QPointF(10.5, 10.5), 6, 6);
            p.drawLine(QPointF(15, 15), QPointF(20, 20));
            break;
        case Symbol::Blitz:
            p.drawPolygon(QPolygonF({QPointF(13.5, 2.5), QPointF(5, 13.5), QPointF(11.5, 13.5), QPointF(10.5, 21.5),
                                     QPointF(19, 10.5), QPointF(12.5, 10.5)}));
            break;
        case Symbol::Euro:
            p.drawArc(QRectF(6, 4, 15, 16), 50 * 16, 260 * 16);
            p.drawLine(QPointF(4, 10.5), QPointF(13, 10.5));
            p.drawLine(QPointF(4, 14), QPointF(12, 14));
            break;
        case Symbol::Abmelden:
            p.drawPolyline(QPolygonF({QPointF(10, 4), QPointF(5, 4), QPointF(5, 20), QPointF(10, 20)}));
            p.drawLine(QPointF(10, 12), QPointF(20, 12));
            p.drawPolyline(QPolygonF({QPointF(16, 8), QPointF(20, 12), QPointF(16, 16)}));
            break;
        case Symbol::Fingerabdruck:
            // ineinanderliegende Bögen wie die Linien eines Fingerabdrucks
            p.drawArc(QRectF(4, 3, 16, 18), 20 * 16, 140 * 16);
            p.drawArc(QRectF(7, 6.5, 10, 12), 0, 180 * 16);
            p.drawLine(QPointF(7, 12.5), QPointF(7, 16));
            p.drawLine(QPointF(17, 12.5), QPointF(17, 17));
            p.drawArc(QRectF(10, 10, 4, 6), 0, 180 * 16);
            p.drawLine(QPointF(10, 13), QPointF(10, 20.5));
            p.drawLine(QPointF(14, 13), QPointF(14, 19));
            p.drawLine(QPointF(4.5, 14), QPointF(4.5, 17));
            break;
        case Symbol::Schloss:
            // Vorhängeschloss: Bügel + Körper mit Schlüsselloch
            p.drawArc(QRectF(8, 3.5, 8, 10), 0, 180 * 16);
            p.drawLine(QPointF(8, 8.5), QPointF(8, 11));
            p.drawLine(QPointF(16, 8.5), QPointF(16, 11));
            p.drawRoundedRect(QRectF(5, 11, 14, 9.5), 2, 2);
            p.drawLine(QPointF(12, 14.5), QPointF(12, 17));
            break;
        case Symbol::Schliessen:
            // Tür: Türblatt mit Klinke, darunter der Boden
            p.drawPolyline(QPolygonF({QPointF(6.5, 20.5), QPointF(6.5, 4.5), QPointF(17.5, 4.5), QPointF(17.5, 20.5)}));
            p.drawLine(QPointF(3.5, 20.5), QPointF(20.5, 20.5));
            p.drawLine(QPointF(13.5, 12.5), QPointF(14.5, 12.5));
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
    zeichneLogo(p, ziel, 1.0, 1.0, 1.0);
}

void Stil::zeichneLogo(QPainter& p, const QRectF& ziel, double sonne, double strahlen, double kurve)
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
    // Sonne geht auf: nach unten verschoben und am Horizont abgeschnitten
    const double absenkung = (1.0 - std::clamp(sonne, 0.0, 1.0)) * (r + 2.0);
    QPainterPath halbkreis;
    halbkreis.moveTo(m.x() - r, m.y() + absenkung);
    halbkreis.arcTo(QRectF(m.x() - r, m.y() - r + absenkung, 2 * r, 2 * r), 180, -180);
    halbkreis.closeSubpath();
    p.save();
    p.setClipRect(QRectF(0, 0, 64, m.y()), Qt::IntersectClip);
    p.fillPath(halbkreis, weiss);
    p.restore();

    p.setPen(QPen(weiss, klein ? 4.5 : 3.2, Qt::SolidLine, Qt::RoundCap));
    const int anzahlStrahlen = klein ? 3 : 5;
    for (int i = 0; i < anzahlStrahlen; ++i) {
        // Strahlen erscheinen nacheinander und wachsen nach außen
        const double anteil = std::clamp(strahlen * anzahlStrahlen - i, 0.0, 1.0);
        if (anteil <= 0.0)
            continue;
        const double winkel = (klein ? 45.0 + i * 45.0 : 15.0 + i * 37.5) * kPiStil / 180.0;
        const double r1 = r + (klein ? 5.0 : 4.5);
        const double r2 = r1 + ((klein ? 10.0 : 9.5) - (klein ? 5.0 : 4.5)) * anteil;
        p.drawLine(QPointF(m.x() + r1 * std::cos(winkel), m.y() - r1 * std::sin(winkel)),
                   QPointF(m.x() + r2 * std::cos(winkel), m.y() - r2 * std::sin(winkel)));
    }

    // Horizont als Messkurve: steht für die Überwachung (Monitoring)
    p.setPen(QPen(weiss, klein ? 4.5 : 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (klein) {
        p.drawLine(QPointF(10, 44), QPointF(10 + 44 * std::clamp(kurve, 0.0, 1.0), 44));
    } else {
        const QPolygonF punkte({QPointF(11, 42), QPointF(24.5, 42), QPointF(28.5, 37.5), QPointF(34, 50),
                                QPointF(38.5, 42), QPointF(53, 42)});
        // nur den gewünschten Anteil der Kurve zeichnen (von links nach rechts)
        double gesamt = 0.0;
        for (int i = 1; i < punkte.size(); ++i)
            gesamt += QLineF(punkte[i - 1], punkte[i]).length();
        double rest = std::clamp(kurve, 0.0, 1.0) * gesamt;
        QPolygonF teil{punkte.first()};
        for (int i = 1; i < punkte.size() && rest > 0.0; ++i) {
            const QLineF strecke(punkte[i - 1], punkte[i]);
            if (strecke.length() <= rest) {
                teil.append(punkte[i]);
                rest -= strecke.length();
            } else {
                teil.append(strecke.pointAt(rest / strecke.length()));
                rest = 0.0;
            }
        }
        if (teil.size() > 1)
            p.drawPolyline(teil);
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
