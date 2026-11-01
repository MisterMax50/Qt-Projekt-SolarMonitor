#pragma once

#include <QString>

/**
 * @brief Laufzeit-Umschaltung der Sprache (Erweiterung 9).
 *
 * Quellsprache im Code ist Deutsch. Für Englisch wird die eingebettete
 * Übersetzung :/i18n/SolarMonitor_en.qm geladen (erzeugt beim Bauen per
 * lrelease, siehe qt_add_translations in CMakeLists.txt), dazu
 * die Qt-eigenen Übersetzungen (qtbase_xx) für Standarddialoge.
 *
 * Nach dem Umschalten schickt Qt allen Widgets ein LanguageChange-Ereignis.
 */
class Sprache {
public:
    /// "de" oder "en"; liefert false, wenn die Übersetzung fehlt.
    static bool setze(const QString& kuerzel);
    static QString aktuell();
};
