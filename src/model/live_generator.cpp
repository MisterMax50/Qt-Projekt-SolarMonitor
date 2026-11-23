#include "model/live_generator.h"

#include <algorithm>
#include <cmath>

#include "model/anlage.h"
#include "util/sonnenstand.h"

namespace {
constexpr int    kTaktMs = 1000;          ///< Wanduhr-Takt zwischen zwei Messungen
constexpr double kSystemWirkungsgrad = 0.85;
constexpr double kTemperaturBeiwert = 0.0035; ///< Leistungsverlust je Kelvin über 25 °C
constexpr double kStoerWahrscheinlichkeit = 0.0004; ///< je Anlage und Takt bei Sonne
constexpr double kNachtGrenzeWm2 = 5.0;
constexpr double kPi = 3.14159265358979323846;

double clamp(double wert, double lo, double hi)
{
    return std::max(lo, std::min(hi, wert));
}
} // namespace

LiveGenerator::LiveGenerator(QObject* parent)
    : QObject(parent)
{
    m_takt.setInterval(kTaktMs);
    connect(&m_takt, &QTimer::timeout, this, &LiveGenerator::schritt);
}

void LiveGenerator::setAnlagen(const QVector<Anlage>& anlagen)
{
    m_quellen.clear();
    m_quellen.reserve(anlagen.size());
    for (const Anlage& a : anlagen) {
        if (!a.hatGueltigeLeistung())
            continue;
        Quelle q;
        q.id = a.anlageId;
        q.kwp = a.leistungKwp;
        q.lat = a.lat;
        q.lon = a.lon;
        q.abgeschaltet = (a.status == QLatin1String("abgeschaltet"));
        q.lokal = (m_zufall.generateDouble() - 0.5) * 0.2;
        m_quellen.append(q);
    }
    // Simulationszeit beginnt mit der Wanduhr und läuft ab hier weiter
    m_zeit = QDateTime::currentDateTime();
}

void LiveGenerator::setFaktor(int faktor)
{
    m_faktor = std::max(1, faktor);
}

void LiveGenerator::starte()
{
    if (m_takt.isActive())
        return;
    m_takt.start();
    schritt(); // sofort ein erster Stand, nicht erst nach einer Sekunde
}

void LiveGenerator::stoppe()
{
    m_takt.stop();
}

void LiveGenerator::schritt()
{
    if (!m_zeit.isValid())
        m_zeit = QDateTime::currentDateTime();
    m_zeit = m_zeit.addMSecs(qint64(m_faktor) * kTaktMs);

    // Gemeinsame Wetterlage wandert langsam; jede Anlage hat zusätzlich eine eigene Abweichung
    m_wetter = clamp(m_wetter + (m_zufall.generateDouble() - 0.5) * 0.06, 0.3, 1.0);

    QVector<LiveMesswert> werte;
    werte.reserve(m_quellen.size());

    for (Quelle& q : m_quellen) {
        q.lokal = clamp(q.lokal + (m_zufall.generateDouble() - 0.5) * 0.04, -0.15, 0.15);

        LiveMesswert m;
        m.anlageId = q.id;

        if (q.abgeschaltet) {
            m.status = QStringLiteral("aus");
            werte.append(m);
            continue;
        }

        // Sonnenstand bestimmt die Einstrahlung in der Modulebene (vereinfachtes Modell)
        const double hoehe = Sonnenstand::berechne(m_zeit, q.lat, q.lon).hoeheGrad;
        const double sinH = std::sin(hoehe * kPi / 180.0);
        double einstrahlung = hoehe > 0.0 ? 950.0 * std::pow(sinH, 1.15) : 0.0;
        einstrahlung *= clamp(m_wetter + q.lokal, 0.15, 1.0);
        einstrahlung = std::max(0.0, einstrahlung + (m_zufall.generateDouble() - 0.5) * 12.0);

        m.einstrahlungWm2 = einstrahlung;
        m.modultemperaturC = 15.0 + einstrahlung / 1000.0 * 30.0;

        if (einstrahlung < kNachtGrenzeWm2) {
            // Nacht: Wechselrichter im Standby, keine Einspeisung
            m.status = QStringLiteral("standby");
            m.leistungW = 0.0;
            werte.append(m);
            continue;
        }

        // Gelegentlich eine Störung, die einige Takte anhält
        if (q.stoerRest > 0) {
            --q.stoerRest;
            m.status = QStringLiteral("stoerung");
            m.leistungW = 0.0;
            werte.append(m);
            continue;
        }
        if (m_zufall.generateDouble() < kStoerWahrscheinlichkeit) {
            q.stoerRest = 5 + int(m_zufall.bounded(30));
            m.status = QStringLiteral("stoerung");
            m.leistungW = 0.0;
            werte.append(m);
            continue;
        }

        const double temperaturFaktor = 1.0 - kTemperaturBeiwert * std::max(0.0, m.modultemperaturC - 25.0);
        const double rauschen = 1.0 + (m_zufall.generateDouble() - 0.5) * 0.03;
        double leistung = q.kwp * 1000.0 * (einstrahlung / 1000.0) * kSystemWirkungsgrad
                        * temperaturFaktor * rauschen;
        leistung = clamp(leistung, 0.0, q.kwp * 1000.0);

        m.leistungW = leistung;
        m.status = QStringLiteral("ok");
        werte.append(m);
    }

    emit neueMesswerte(werte, m_zeit);
}
