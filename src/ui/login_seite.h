#pragma once

#include <QWidget>

#include "model/rolle.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

/**
 * @brief Anmeldung mit Rolle und PIN (Erweiterung 3) als Seite im Hauptfenster.
 *
 * Kein eigenes Fenster: Die Anmeldung erscheint im selben Fenster wie das
 * Programm (nach dem Intro und beim Benutzerwechsel), als Karte in der Mitte
 * eines dunkelblauen Hintergrunds – wie die Anmeldeseite eines
 * Monitoring-Portals.
 *
 * Nach drei Fehlversuchen wird die Anmeldung für 30 Sekunden gesperrt
 * (Countdown). Die PINs werden nur als SHA-256-Hash geprüft.
 */
class LoginSeite : public QWidget {
    Q_OBJECT
public:
    explicit LoginSeite(QWidget* parent = nullptr);

    /**
     * @brief Für eine neue Anmeldung vorbereiten (PIN leeren, Fokus setzen).
     * @param zurueckMoeglich true beim Benutzerwechsel: der zweite Knopf heißt
     *        dann "Zurück" statt "Beenden".
     */
    void vorbereiten(bool zurueckMoeglich);

signals:
    void angemeldet(Rolle rolle);
    /// "Beenden" bzw. "Zurück" gewählt.
    void abgebrochen();

private slots:
    void anmelden();
    void sperreTick();

private:
    QComboBox*   m_rollen = nullptr;
    QLineEdit*   m_pin = nullptr;
    QLabel*      m_meldung = nullptr;
    QPushButton* m_anmelden = nullptr;
    QPushButton* m_abbrechen = nullptr;
    QTimer*      m_sperrTimer = nullptr;
    int          m_fehlversuche = 0;
    int          m_restSekunden = 0;
};
