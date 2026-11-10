#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Spaß-Funktion: Abo-Stufen von SolarMonitor (alles Spielgeld).
 *
 *  - Kostenlos: mit (erfundener) Werbung
 *  - Premium  (9,99 €):  werbefrei
 *  - Ultimate (19,99 €): werbefrei + Ultimate-Funktionen (Paywall)
 *
 * Die Stufe steht in QSettings ("abo"). Gesperrte Funktionen fragen
 * Abo::pruefe(); fehlt Ultimate, sendet das Abo-Objekt paywall(), und
 * das Hauptfenster bietet das Upgrade an.
 */
class Abo : public QObject {
    Q_OBJECT
public:
    enum class Stufe { Kostenlos = 0, Premium = 1, Ultimate = 2 };
    Q_ENUM(Stufe)

    /// Funktionen hinter der Paywall (nur mit Ultimate)
    enum class Funktion { DunklesDesign, Anordnungen, Sprachpaket, IntroAus };
    Q_ENUM(Funktion)

    static Abo* instanz();

    static Stufe stufe();
    static void setzeStufe(Stufe s);
    static bool werbefrei() { return stufe() != Stufe::Kostenlos; }
    static bool erlaubt(Funktion) { return stufe() == Stufe::Ultimate; }
    /// true, wenn erlaubt; sonst wird paywall(f) gesendet und false geliefert.
    static bool pruefe(Funktion f);
    /// Sprachen aus dem Ultimate-Sprachpaket
    static bool istPaketSprache(const QString& kuerzel);

    static QString name(Stufe s);        ///< "SolarMonitor Premium" …
    static QString preis(Stufe s);       ///< "9,99 €" …
    static QStringList vorteile(Stufe s);
    static QString funktionsName(Funktion f);

signals:
    void stufeGeaendert(Abo::Stufe neu);
    void paywall(Abo::Funktion f);

private:
    using QObject::QObject;
};
