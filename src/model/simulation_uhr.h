#pragma once

#include <QDateTime>
#include <QElapsedTimer>

/**
 * @brief Simulationsuhr: läuft ab dem Start schneller als die Wanduhr.
 *
 * Simulationszeit = Startzeit + verstrichene Wanduhrzeit · Faktor.
 * Faktor 1 ist Echtzeit, 60 bedeutet eine Simulationsminute je Sekunde.
 * Ein Faktorwechsel setzt die Uhr an der aktuellen Stelle fort.
 */
class SimulationsUhr {
public:
    SimulationsUhr()
        : m_start(QDateTime::currentDateTime())
    {
        m_timer.start();
    }

    QDateTime jetzt() const
    {
        return m_start.addMSecs(qint64(m_timer.elapsed()) * m_faktor);
    }

    int faktor() const { return m_faktor; }

    void setFaktor(int faktor)
    {
        m_start = jetzt();
        m_timer.restart();
        m_faktor = qMax(1, faktor);
    }

private:
    QDateTime     m_start;
    QElapsedTimer m_timer;
    int           m_faktor = 1;
};
