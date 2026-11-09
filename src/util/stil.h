#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QRectF>

class QPainter;

/**
 * @brief Einheitliches Erscheinungsbild der Anwendung.
 *
 * - Qt-Stil "Fusion" (sieht auf allen Systemen gleich aus) mit eigener
 *   Farbpalette und Stylesheet für ein helles und ein dunkles Design.
 *   Selbst gezeichnete Widgets (Diagramme) lesen ihre Farben aus der
 *   Palette und passen sich dadurch automatisch an.
 * - Eigene, im Code gezeichnete Linien-Icons (keine Bilddateien, keine
 *   zusätzlichen Qt-Module nötig), die in jeder Farbe erzeugt werden können.
 */
class Stil {
public:
    enum class Symbol {
        Liste, Warnung, Karte, Statistik, Werkzeug, Pruefung, Details, Bearbeiten,
        Plus, Export, Bericht, Neuladen, Sonne, Mond, Benutzer, Info, Haken, Fehler,
        Rueckgaengig, Wiederholen, Pdf, Uebersicht, Vollbild
    };

    static void anwenden(bool dunkel);
    static bool istDunkel();

    static QColor akzent();       ///< Akzentfarbe (Windows-Blau), z. B. für Links
    static QColor text();
    static QColor textGedimmt();
    static QColor flaeche();      ///< Karten-/Tabellenhintergrund
    static QColor rahmen();

    static QIcon icon(Symbol symbol, const QColor& farbe = QColor());
    /**
     * @brief Logo der Anwendung: aufgehende Sonne über einer Messkurve auf
     *        einer Kachel mit Amber-Orange-Verlauf.
     *
     * Für sehr kleine Größen (≤ 20 px) wird automatisch eine vereinfachte
     * Variante gezeichnet. Dieselbe Zeichnung liefert auch die Windows-
     * Programmdatei (resources/SolarMonitor.ico).
     */
    static void zeichneLogo(QPainter& p, const QRectF& ziel);
    static QPixmap logo(int groesse);
    /// Schriftzug "SolarMonitor" (Rich Text): "Solar" fett, "Monitor" normal.
    static QString wortmarke();
    /// Logo in allen gängigen Größen, z. B. für Fenster, Taskleiste und Login.
    static QIcon appIcon();
};
