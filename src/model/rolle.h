#pragma once

#include <QCoreApplication>
#include <QString>

/**
 * @brief Rollen- und Rechtekonzept (Erweiterung 3).
 *
 *  - Betrieb : Lesen, Auswertungen, Berichte exportieren
 *  - Service : zusätzlich Störungen erfassen und bearbeiten
 *  - Admin   : zusätzlich Stammdaten bearbeiten und exportieren
 *
 * Alle Rechteabfragen der Oberfläche laufen über @ref darf, damit das
 * Konzept an genau einer Stelle definiert ist.
 */
enum class Rolle { Betrieb, Service, Admin };

enum class Recht {
    BerichtExportieren,
    StoerungBearbeiten,   ///< Störung erfassen / Status ändern
    StammdatenBearbeiten,
    StammdatenExportieren
};

class Rollen {
    Q_DECLARE_TR_FUNCTIONS(Rollen)
public:
    static bool darf(Rolle rolle, Recht recht);

    static QString kennung(Rolle rolle);          ///< "betrieb" | "service" | "admin"
    static QString anzeigename(Rolle rolle);      ///< übersetzter Name
    static bool ausKennung(const QString& kennung, Rolle* rolle);

    /// Prüft die PIN gegen den hinterlegten SHA-256-Hash.
    static bool pinKorrekt(Rolle rolle, const QString& pin);
};
