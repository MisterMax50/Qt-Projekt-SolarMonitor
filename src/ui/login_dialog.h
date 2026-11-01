#pragma once

#include <QDialog>

#include "model/rolle.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

/**
 * @brief Anmeldung mit Rolle und PIN (Erweiterung 3).
 *
 * Nach drei Fehlversuchen wird die Anmeldung für 30 Sekunden gesperrt
 * (Countdown im Dialog). Die PINs werden nur als SHA-256-Hash geprüft.
 */
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    Rolle rolle() const { return m_rolle; }

private slots:
    void anmelden();
    void sperreTick();

private:
    QComboBox*   m_rollen = nullptr;
    QLineEdit*   m_pin = nullptr;
    QLabel*      m_meldung = nullptr;
    QPushButton* m_anmelden = nullptr;
    QTimer*      m_sperrTimer = nullptr;
    int          m_fehlversuche = 0;
    int          m_restSekunden = 0;
    Rolle        m_rolle = Rolle::Betrieb;
};
