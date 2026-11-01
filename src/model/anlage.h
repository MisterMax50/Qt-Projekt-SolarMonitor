#pragma once

#include <QDate>
#include <QString>

/**
 * @brief Stammdaten einer PV-Anlage (eine Zeile aus anlagen.csv).
 *
 * Wertklasse: billig zu kopieren, in Containern speicherbar. Die Felder
 * entsprechen den CSV-Spalten, die Typen wurden fachlich gewählt
 * (z. B. QDate statt QString für die Inbetriebnahme).
 */
struct Anlage {
    QString anlageId;              ///< z. B. "P00001"
    QString typ;                   ///< dach_privat | dach_gewerbe | freiflaeche | balkon
    QString ort;
    double  lat = 0.0;             ///< Breitengrad (Dezimalgrad)
    double  lon = 0.0;             ///< Längengrad (Dezimalgrad)
    double  leistungKwp = 0.0;     ///< installierte Modulleistung in kWp
    int     ausrichtungGrad = 180; ///< Azimut: 90 = Ost, 180 = Süd, 270 = West
    int     neigungGrad = 30;
    QString wechselrichterId;      ///< Typ-ID aus wechselrichter.csv, z. B. "W05"
    int     anzahlWr = 1;
    QString wetterstationId;       ///< z. B. "WS03"
    QDate   inbetriebnahme;
    double  verguetungCtKwh = 0.0; ///< Einspeisevergütung in ct/kWh
    QString status;                ///< in_betrieb | gestoert | wartung | abgeschaltet

    /// Leistung > 0 ist Voraussetzung für spezifischen Ertrag und PR.
    bool hatGueltigeLeistung() const { return leistungKwp > 0.0; }

    /// Koordinaten grob plausibel (Bodenseeregion inkl. Umland)?
    bool hatGueltigeKoordinaten() const
    {
        return lat >= 46.5 && lat <= 49.0 && lon >= 7.5 && lon <= 11.0;
    }
};
