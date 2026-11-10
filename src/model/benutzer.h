#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include "model/rolle.h"

/// Ein Benutzerkonto. Die PIN wird nie gespeichert, nur Salz und Hash.
struct Benutzer {
    QString    kennung;        ///< Benutzername für die Anmeldung (eindeutig, klein geschrieben)
    QString    name;           ///< Anzeigename, z. B. "Max Kahl"
    Rolle      rolle = Rolle::Betrieb;
    bool       aktiv = true;   ///< gesperrte Konten können sich nicht anmelden
    QByteArray salz;           ///< zufällig je Konto (hex)
    QByteArray pinHash;        ///< SHA-256(salz + PIN) (hex)
    QDateTime  erstellt;
    QDateTime  letzteAnmeldung;
};

/**
 * @brief Benutzerverwaltung (Erweiterung des Rollenkonzepts).
 *
 * Konten mit Name, Rolle und eigener PIN. Gespeichert wird in
 * benutzer.json im Programmdatenordner des Betriebssystems
 * (QStandardPaths::AppDataLocation), also unabhängig vom Datenordner der
 * PV-Flotte. Fehlt die Datei, werden die drei Demo-Konten angelegt
 * (betrieb/0000, service/5678, admin/1234).
 *
 * Schutzregeln: Es bleibt immer mindestens ein aktiver Administrator; das
 * eigene Konto kann nicht gelöscht, gesperrt oder herabgestuft werden.
 * PINs bestehen aus 4–8 Ziffern und werden gesalzen gehasht (SHA-256).
 */
class Benutzerverwaltung : public QObject {
    Q_OBJECT
public:
    explicit Benutzerverwaltung(QObject* parent = nullptr);
    /// Für Tests: eigene Datei statt des Programmdatenordners.
    Benutzerverwaltung(const QString& datei, QObject* parent);

    const QList<Benutzer>& alle() const { return m_benutzer; }
    const Benutzer* finde(const QString& kennung) const;
    QString datei() const { return m_datei; }

    /// Prüft Benutzername und PIN; bei Erfolg wird die Anmeldung vermerkt.
    bool anmelden(const QString& kennung, const QString& pin);

    // Alle Änderungen liefern eine leere Zeichenkette oder den Fehlergrund.
    QString anlegen(const QString& kennung, const QString& name, Rolle rolle, const QString& pin);
    QString aendern(const QString& kennung, const QString& name, Rolle rolle, bool aktiv, const QString& angemeldet);
    QString setzePin(const QString& kennung, const QString& pin);
    QString loeschen(const QString& kennung, const QString& angemeldet);

    static bool pinGueltig(const QString& pin);
    static bool kennungGueltig(const QString& kennung);

signals:
    void geaendert();

private:
    void laden();
    bool speichern(QString* fehler = nullptr);
    void demoKontenAnlegen();
    int aktiveAdmins(const QString& ohne = QString()) const;
    static QByteArray hash(const QByteArray& salz, const QString& pin);
    static QByteArray neuesSalz();
    Benutzer* findeVeraenderbar(const QString& kennung);

    QString         m_datei;
    QList<Benutzer> m_benutzer;
};
