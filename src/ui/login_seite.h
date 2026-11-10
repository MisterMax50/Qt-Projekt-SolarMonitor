#pragma once

#include <QWidget>

#include "model/rolle.h"

class QComboBox;
class Benutzerverwaltung;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

/**
 * @brief Anmeldung mit Benutzer und PIN (Erweiterung 3) als Seite im Hauptfenster.
 *
 * Kein eigenes Fenster: Die Anmeldung erscheint im selben Fenster wie das
 * Programm (nach dem Intro und beim Benutzerwechsel), als Karte in der Mitte
 * eines dunkelblauen Hintergrunds – wie die Anmeldeseite eines
 * Monitoring-Portals.
 *
 * Nach drei Fehlversuchen wird die Anmeldung für 30 Sekunden gesperrt
 * (Countdown). Die PINs werden gesalzen gehasht geprüft (Benutzerverwaltung).
 */
class LoginSeite : public QWidget {
    Q_OBJECT
public:
    explicit LoginSeite(Benutzerverwaltung* benutzer, QWidget* parent = nullptr);

    /**
     * @brief Für eine neue Anmeldung vorbereiten (PIN leeren, Fokus setzen).
     * @param zurueckMoeglich true beim Benutzerwechsel: der zweite Knopf heißt
     *        dann "Zurück" statt "Beenden".
     */
    void vorbereiten(bool zurueckMoeglich);

signals:
    /// Erfolgreich angemeldet; @p kennung ist der Benutzername.
    void angemeldet(const QString& kennung);
    /// "Beenden" bzw. "Zurück" gewählt.
    void abgebrochen();

private slots:
    void anmelden();
    void sperreTick();

private:
    void fuelleBenutzer();

    Benutzerverwaltung* m_benutzer = nullptr;
    QComboBox*   m_benutzerWahl = nullptr;
    QLineEdit*   m_pin = nullptr;
    QLabel*      m_meldung = nullptr;
    QPushButton* m_anmelden = nullptr;
    QPushButton* m_abbrechen = nullptr;
    QTimer*      m_sperrTimer = nullptr;
    int          m_fehlversuche = 0;
    int          m_restSekunden = 0;
};
