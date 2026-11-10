#pragma once

#include <QCoreApplication>
#include <QString>

class QWidget;

/**
 * @brief Anmeldung per Windows Hello (Fingerabdruck, Gesicht oder Windows-PIN).
 *
 * Fragt über die Windows-Schnittstelle UserConsentVerifier nach, ob die
 * Person am Rechner der angemeldete Windows-Benutzer ist. Die Abfrage
 * selbst (Fingerabdruck usw.) zeigt Windows in einem eigenen Dialog an.
 *
 * SolarMonitor verknüpft ein Konto mit dem Windows-Konto, unter dem Hello
 * eingerichtet wurde; nur an diesem Windows-Konto ist die Anmeldung per
 * Hello möglich. Auf anderen Systemen ist Hello nicht verfügbar.
 */
class WindowsHello {
    Q_DECLARE_TR_FUNCTIONS(WindowsHello)
public:
    enum class Ergebnis { Bestaetigt, Abgebrochen, NichtVerfuegbar, Fehler };

    /// Ist Windows Hello eingerichtet und nutzbar? (Ergebnis wird zwischengespeichert)
    static bool verfuegbar(QString* grund = nullptr);
    /// Windows-Hello-Dialog anzeigen; die Oberfläche bleibt währenddessen bedienbar.
    static Ergebnis pruefe(QWidget* fenster, const QString& nachricht, QString* fehler = nullptr);
    /// "DOMÄNE\\Benutzer" des angemeldeten Windows-Kontos (leer außerhalb von Windows).
    static QString windowsKonto();
};
