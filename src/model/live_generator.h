#pragma once

#include <QDateTime>
#include <QObject>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>
#include <QVector>

struct Anlage;

/// Ein simulierter Messwert einer Anlage zu einem Simulationszeitpunkt.
struct LiveMesswert {
    QString anlageId;
    double  leistungW = 0.0;          ///< eingespeiste AC-Leistung in W
    double  einstrahlungWm2 = 0.0;    ///< Bestrahlungsstärke in W/m²
    double  modultemperaturC = 0.0;   ///< Modultemperatur in °C
    QString status;                   ///< ok | standby | stoerung | aus
};

/**
 * @brief Erzeugt im Takt neue Live-Messwerte für alle Anlagen.
 *
 * Die Werte sind simuliert, aber plausibel: Der Sonnenstand (aus
 * util/sonnenstand) bestimmt die Einstrahlung, eine langsam schwankende
 * Wolkenlage dämpft sie, Modultemperatur und Systemwirkungsgrad wirken
 * auf die Leistung, und gelegentlich fällt eine Anlage für einige Takte
 * aus. Nachts stehen die Wechselrichter auf Standby.
 *
 * Die Simulationszeit läuft schneller als die Wanduhr, wenn der Faktor
 * größer als 1 ist (z. B. 60 = eine Simulationsminute je Sekunde).
 */
class LiveGenerator : public QObject {
    Q_OBJECT
public:
    explicit LiveGenerator(QObject* parent = nullptr);

    /// Übernimmt die Anlagen mit installierter Leistung; setzt die Simulation zurück.
    void setAnlagen(const QVector<Anlage>& anlagen);
    int anzahl() const { return m_quellen.size(); }

    /// Simulationszeit je Takt in Vielfachen der Wanduhrzeit (mindestens 1).
    void setFaktor(int faktor);
    int faktor() const { return m_faktor; }

    bool laeuft() const { return m_takt.isActive(); }
    QDateTime simulationszeit() const { return m_zeit; }

public slots:
    void starte();
    void stoppe();
    /// Ein Takt: Zeit weiterschalten und allen Anlagen neue Werte geben.
    void schritt();

signals:
    void neueMesswerte(const QVector<LiveMesswert>& werte, const QDateTime& zeit);

private:
    struct Quelle {
        QString id;
        double  kwp = 0.0;
        double  lat = 0.0;
        double  lon = 0.0;
        double  lokal = 0.0;      ///< eigene Wolkenabweichung der Anlage
        int     stoerRest = 0;    ///< Takte, die die Störung noch dauert
        bool    abgeschaltet = false;
    };

    QVector<Quelle> m_quellen;
    QTimer          m_takt;
    QDateTime       m_zeit;
    int             m_faktor = 60;
    double          m_wetter = 0.8; ///< gemeinsame Wetterlage aller Anlagen (0..1)
    QRandomGenerator m_zufall{quint32(QDateTime::currentMSecsSinceEpoch())};
};
