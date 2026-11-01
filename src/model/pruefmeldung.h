#pragma once

#include <QString>

/**
 * @brief Ergebnis einer Plausibilitätsprüfung beim Einlesen.
 *
 * Statt Warnungen nur auf die Konsole zu schreiben, sammelt die
 * Datenbasis strukturierte Meldungen, die in der Oberfläche
 * (Tab "Datenprüfung") gefiltert angezeigt werden.
 */
struct Pruefmeldung {
    enum class Stufe {
        Info,    ///< automatisch korrigiert (z. B. Dezimalkomma)
        Warnung, ///< Wert unplausibel, Datensatz bleibt erhalten
        Fehler   ///< Datensatz/Datei konnte nicht verwendet werden
    };

    Stufe   stufe = Stufe::Warnung;
    QString datei;     ///< z. B. "anlagen.csv"
    int     zeile = 0; ///< 1-basierte Zeilennummer in der Datei (0 = ganze Datei)
    QString objektId;  ///< betroffene Anlage/Station/Störung (falls bekannt)
    QString text;
};
