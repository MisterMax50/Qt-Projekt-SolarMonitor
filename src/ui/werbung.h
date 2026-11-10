#pragma once

#include <QFrame>

class QLabel;
class QPushButton;
class QTimer;

/**
 * @brief Spaß-Funktion: Werbebanner mit erfundenen Produkten.
 *
 * Solange der Abo-Modus ("SolarMonitor Premium") aus ist, zeigt das
 * Hauptfenster unten dieses Banner. Es wechselt alle paar Sekunden zwischen
 * frei erfundenen, augenzwinkernden Anzeigen rund um Photovoltaik. Es gibt
 * keine echten Marken, keine Links nach außen und nichts kostet Geld.
 * Klick auf die Anzeige oder auf "×" bietet an, Premium einzuschalten.
 */
class WerbeBanner : public QFrame {
    Q_OBJECT
public:
    explicit WerbeBanner(QWidget* parent = nullptr);

signals:
    /// Benutzer möchte die Werbung loswerden (Premium einschalten).
    void premiumGewuenscht();

private:
    void zeige(int index);
    void angeklickt();

    QLabel*      m_bild = nullptr;
    QLabel*      m_titel = nullptr;
    QLabel*      m_text = nullptr;
    QPushButton* m_aktion = nullptr;
    QTimer*      m_wechsel = nullptr;
    int          m_index = 0;
};
