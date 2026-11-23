#pragma once

#include <QDateTime>

#include "model/anlage.h"
#include "model/stoerung.h"
#include "model/verlauf.h"

/**
 * @brief Erzeugt simulierte Messverläufe für die Datenquelle "Live-Generator".
 *
 * Ein erzeugter Verlauf hat dasselbe Format wie eine Verlaufsdatei
 * (14 Tage im 15-Minuten-Raster), sodass Auswertung, Diagramme, Detail- und
 * PDF-Ansicht ihn ohne Änderung verarbeiten. Die Werte folgen einem
 * vereinfachten Modell: Der Sonnenstand am Standort bestimmt die
 * Einstrahlung, eine Wetterlage (für alle Anlagen gleich) und eine eigene
 * Wolkenabweichung der Anlage dämpfen sie, Modultemperatur und
 * Systemwirkungsgrad wirken auf die Leistung. Nachts Standby, selten eine
 * Störung über einige Viertelstunden.
 *
 * Die Erzeugung ist reine Funktion (ohne gemeinsamen Zustand): Dasselbe
 * Ergebnis für dieselbe Anlage und denselben Endzeitpunkt, und sie läuft
 * gefahrlos im Hintergrund-Thread der Analyse.
 */
class LiveGenerator {
public:
    static constexpr int kTage = 14;
    static constexpr int kIntervallMin = 15;

    /// Verlauf der Anlage, der mit dem letzten vollen Viertelstundenwert bei @p ende endet.
    static Verlauf erzeuge(const Anlage& anlage, const QDateTime& ende = QDateTime::currentDateTime());

    /**
     * @brief Störungsmeldungen aus einem erzeugten Verlauf: jede zusammenhängende
     *        Phase mit wr_status "stoerung" wird eine Störung. Dauer bestimmt die
     *        Schwere; läuft die Phase bis zum Ende des Verlaufs, ist sie offen.
     */
    static QVector<Stoerung> stoerungenAus(const Verlauf& verlauf);
};
