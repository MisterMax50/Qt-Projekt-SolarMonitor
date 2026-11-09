#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

/**
 * @brief Startanimation des Logos, wie das Intro beim Start eines Spiels.
 *
 * Ablauf (etwa 3,4 Sekunden) auf dunkelblauem Vollbild:
 *  1. Die Logokachel springt mit leichtem Überschwingen auf, dahinter
 *     leuchtet ein warmer Schein.
 *  2. Die Sonne geht hinter dem Horizont auf, danach wachsen die Strahlen
 *     nacheinander nach außen.
 *  3. Die Messkurve zeichnet sich von links nach rechts.
 *  4. Der Schriftzug "SolarMonitor" und der Untertitel blenden ein.
 *  5. Alles blendet aus.
 *
 * Ein Mausklick oder eine beliebige Taste überspringt das Intro. Gezeichnet
 * wird mit QPainter (Stil::zeichneLogo mit Fortschrittswerten), es werden
 * keine Bild- oder Videodateien benötigt.
 */
class IntroAnimation : public QWidget {
    Q_OBJECT
public:
    explicit IntroAnimation(QWidget* parent = nullptr);

    /// Zeigt das Intro im Vollbild und kehrt zurück, wenn es fertig oder übersprungen ist.
    static void abspielen();

    /// Bild zum Zeitpunkt @p sekunden zeichnen (auch für Tests/Screenshots).
    void setzeZeit(double sekunden);

signals:
    void fertig();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void beenden();

    QTimer*       m_takt = nullptr;
    QElapsedTimer m_uhr;
    double        m_zeit = 0.0;   ///< aktuelle Zeit in Sekunden
    bool          m_beendet = false;
};
