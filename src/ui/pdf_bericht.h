#pragma once

#include <QCoreApplication>
#include <QString>

#include "ui/app_kontext.h"

/**
 * @brief PDF-Berichte (A4) für eine einzelne Anlage und für die Flotte.
 *
 * Aufbau: Der Inhalt wird als HTML in ein QTextDocument geschrieben (Tabellen,
 * Kennzahl-Kacheln, Diagramme als eingebettete Bilder). QTextDocument bricht
 * den Inhalt selbst auf Seiten um; jede Seite wird mit QPainter auf einen
 * QPdfWriter gezeichnet und bekommt eine eigene Kopfzeile (Logo, Titel) und
 * Fußzeile (Erstellungsdatum, Rolle, "Seite x von y").
 *
 * Die Diagramme sind dieselben Widgets wie in der Oberfläche (BalkenDiagramm,
 * VerlaufPlot). Sie werden unsichtbar in ein Bild gezeichnet, immer mit hellen
 * Farben – auch wenn gerade das dunkle Design aktiv ist.
 *
 * Alles nur mit QtGui/QtWidgets, kein zusätzliches Qt-Modul nötig.
 */
class PdfBericht {
    Q_DECLARE_TR_FUNCTIONS(PdfBericht)
public:
    /// Bericht zu einer Anlage: Stammdaten, Kennzahlen, Diagramme, Fehlerbilder, Störungen.
    static bool anlage(const AppKontext& kontext, const QString& anlageId, const QString& pfad,
                       QString* fehler = nullptr);

    /// Flottenbericht: Übersicht, Stationsvergleich, größte Verluste, offene Störungen, Datenqualität.
    /// Setzt eine abgeschlossene Flottenanalyse voraus.
    static bool flotte(const AppKontext& kontext, const QString& pfad, QString* fehler = nullptr);
};
