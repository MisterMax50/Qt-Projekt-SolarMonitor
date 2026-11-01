#pragma once

/**
 * @brief Fachliche Schwellwerte der Auswertung an einer Stelle.
 *
 * Die Werte sind bewusst konservativ gewählt, damit nur deutliche
 * Auffälligkeiten markiert werden (lieber übersehen als Fehlalarm).
 */
namespace Schwellwerte {

/// Ausfall: Einstrahlung über diesem Wert, aber praktisch keine Leistung.
constexpr double kAusfallEinstrahlungWm2 = 300.0;
/// "Praktisch keine Leistung": unter 0,5 % der Nennleistung bzw. 5 W.
constexpr double kAusfallLeistungAnteil = 0.005;
constexpr double kAusfallLeistungMinW = 5.0;

/// Clipping: Leistung ≥ 97 % der AC-Grenzleistung des Wechselrichters …
constexpr double kClippingAnteil = 0.97;
/// … über mindestens so viele aufeinanderfolgende Intervalle (45 min).
constexpr int kClippingMinIntervalle = 3;

/// Schatten: Mindesteinstrahlung für die Verhältnisbildung P/(G·Ppeak).
constexpr double kSchattenMinEinstrahlungWm2 = 150.0;
/// Morgen/Abend gilt als verschattet, wenn das Verhältnis unter 65 % des Mittags liegt.
constexpr double kSchattenVerhaeltnis = 0.65;
constexpr int kSchattenMinMesspunkte = 8;
constexpr int kMorgenBisStunde = 10;  ///< Morgen: vor 10:00
constexpr int kMittagVonStunde = 11;  ///< Mittag: 11:00–14:59
constexpr int kMittagBisStunde = 15;
constexpr int kAbendAbStunde = 16;    ///< Abend: ab 16:00

/// Minderleistung: PR unter 85 % des Medians der Station …
constexpr double kMinderleistungRelativ = 0.85;
/// … oder (ohne Vergleichsgruppe) PR absolut unter 0,65.
constexpr double kMinderleistungAbsolut = 0.65;
/// Mindestgröße der Vergleichsgruppe an einer Wetterstation.
constexpr int kMinVergleichsanlagen = 3;

/// Datenlücken: Warnung ab 5 % fehlender Messpunkte.
constexpr double kLueckenAnteil = 0.05;

/// PR-Referenz, wenn kein Stationsmedian bekannt ist.
constexpr double kStandardPr = 0.80;

/// Keine Einspeisung: gesamter Ertrag unter 1 % des Erwarteten.
constexpr double kKeineEinspeisungAnteil = 0.01;

} // namespace Schwellwerte
