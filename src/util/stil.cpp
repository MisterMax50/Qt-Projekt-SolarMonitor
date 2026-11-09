#include "stil.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleHints>
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QDir>
#include <QStandardPaths>
#include <QStyleFactory>

namespace {

/// Farben im Stil üblicher Windows-Desktopprogramme (Explorer, Outlook):
/// neutrale Grautöne, weiße Listen, ein einziger Akzent (Windows-Blau).
struct Farben {
    QColor fenster, liste, text, gedimmt, linie, schalter, hover, navAuswahl, akzent, akzentText;
};

bool g_dunkel = false;
constexpr double kPiStil = 3.14159265358979323846;

Farben farben()
{
    if (g_dunkel)
        return {QColor("#202020"), QColor("#2b2b2b"), QColor("#f0f0f0"), QColor("#a8a8a8"), QColor("#3f3f3f"),
                QColor("#2d2d2d"), QColor("#353535"), QColor("#3a3a3a"), QColor("#4cc2ff"), QColor("#000000")};
    return {QColor("#f3f3f3"), QColor("#ffffff"), QColor("#1b1b1b"), QColor("#5f5f5f"), QColor("#d6d6d6"),
            QColor("#fdfdfd"), QColor("#e8e8e8"), QColor("#dcdcdc"), QColor("#005fb8"), QColor("#ffffff")};
}

/// Nur wenige, gezielte Regeln. Alles andere zeichnet der Plattform-Stil
/// (unter Windows 11 "windows11", sonst "Fusion"), damit Knöpfe, Eingaben,
/// Tabellen und Menüs aussehen wie in anderen Programmen auch.
QString stylesheet(const Farben& f)
{
    QString s = QStringLiteral(R"(
QWidget#Navigationsbereich { background: %FENSTER%; border-right: 1px solid %LINIE%; }
QListWidget#Navigation { background: transparent; border: none; outline: 0; }
QListWidget#Navigation::item { color: %TEXT%; padding: 5px 6px; margin: 1px 4px; border-radius: 4px; }
QListWidget#Navigation::item:hover { background: %HOVER%; }
QListWidget#Navigation::item:selected { background: %NAVAUSW%; color: %TEXT%; }
QLabel#Bereichstitel { color: %GED%; padding: 8px 10px 2px 10px; }
QToolBar#Befehlsleiste { background: %FENSTER%; border: none; border-bottom: 1px solid %LINIE%; padding: 2px 4px; spacing: 2px; }
QLabel#Gedimmt, QLabel#KpiTitel, QLabel#KpiNotiz { color: %GED%; }
QLabel#KpiWert { font-weight: 600; }
QLabel#Leerzustand { color: %GED%; padding: 24px; }
QFrame#Karte, QFrame#KarteKlick { background: %LISTE%; border: 1px solid %LINIE%; border-radius: 3px; }
QWidget#Detailbereich { background: %LISTE%; border-left: 1px solid %LINIE%; }
QLabel#DetailTitel { font-size: 11pt; font-weight: 600; }
)");
    s.replace(QStringLiteral("%FENSTER%"), f.fenster.name());
    s.replace(QStringLiteral("%LISTE%"), f.liste.name());
    s.replace(QStringLiteral("%TEXT%"), f.text.name());
    s.replace(QStringLiteral("%GED%"), f.gedimmt.name());
    s.replace(QStringLiteral("%LINIE%"), f.linie.name());
    s.replace(QStringLiteral("%HOVER%"), f.hover.name());
    s.replace(QStringLiteral("%NAVAUSW%"), f.navAuswahl.name());
    return s;
}

} // namespace

void Stil::anwenden(bool dunkel)
{
    g_dunkel = dunkel;
    const Farben f = farben();

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Native Stile (z. B. windows11) richten sich nach dem Farbschema
    QGuiApplication::styleHints()->setColorScheme(dunkel ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
    // Windows 11: nativer Stil (abgerundete Fluent-Steuerelemente, Systemschrift).
    // Überall sonst "Fusion" mit einer an Windows angelehnten Palette.
    QStyle* stil = nullptr;
    if (QStyleFactory::keys().contains(QStringLiteral("windows11"), Qt::CaseInsensitive))
        stil = QStyleFactory::create(QStringLiteral("windows11"));
    const bool nativ = stil != nullptr;
    if (!stil)
        stil = QStyleFactory::create(QStringLiteral("Fusion"));
    QApplication::setStyle(stil);

    QPalette p = nativ ? stil->standardPalette() : QPalette();
    if (!nativ) {
        p.setColor(QPalette::Window, f.fenster);
        p.setColor(QPalette::WindowText, f.text);
        p.setColor(QPalette::Base, f.liste);
        p.setColor(QPalette::AlternateBase, f.liste.darker(dunkel ? 90 : 102));
        p.setColor(QPalette::Text, f.text);
        p.setColor(QPalette::Button, f.schalter);
        p.setColor(QPalette::ButtonText, f.text);
        p.setColor(QPalette::ToolTipBase, f.liste);
        p.setColor(QPalette::ToolTipText, f.text);
        p.setColor(QPalette::Highlight, dunkel ? QColor("#0078d4") : f.akzent);
        p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        p.setColor(QPalette::PlaceholderText, f.gedimmt);
        p.setColor(QPalette::Light, dunkel ? QColor("#3a3a3a") : QColor("#ffffff"));
        p.setColor(QPalette::Midlight, dunkel ? QColor("#333333") : QColor("#ececec"));
        p.setColor(QPalette::Mid, dunkel ? QColor("#4a4a4a") : QColor("#c8c8c8"));
        p.setColor(QPalette::Dark, dunkel ? QColor("#151515") : QColor("#a0a0a0"));
        p.setColor(QPalette::Shadow, QColor(0, 0, 0, dunkel ? 160 : 60));
        p.setColor(QPalette::Link, f.akzent);
        p.setColor(QPalette::LinkVisited, f.akzent);
        p.setColor(QPalette::Disabled, QPalette::Text, f.gedimmt);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, f.gedimmt);
        p.setColor(QPalette::Disabled, QPalette::WindowText, f.gedimmt);
    }
    QApplication::setPalette(p);
    qApp->setStyleSheet(stylesheet(f));
    QApplication::setWindowIcon(appIcon());
}

bool Stil::istDunkel() { return g_dunkel; }
QColor Stil::akzent() { return farben().akzent; }
QColor Stil::text() { return farben().text; }
QColor Stil::textGedimmt() { return farben().gedimmt; }
QColor Stil::flaeche() { return farben().liste; }
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
