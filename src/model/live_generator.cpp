#include "model/live_generator.h"

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QHash>
#include <QRandomGenerator>
#include <QTime>

#include "util/sonnenstand.h"

namespace {
constexpr double kSystemWirkungsgrad = 0.85;
constexpr double kTemperaturBeiwert = 0.0035;     ///< Leistungsverlust je Kelvin über 25 °C
constexpr double kStoerWahrscheinlichkeit = 0.004; ///< je Anlage und Viertelstunde bei Sonne
constexpr double kNachtGrenzeWm2 = 5.0;
constexpr double kPi = 3.14159265358979323846;
constexpr quint32 kWetterSeed = 20260601u;         ///< gemeinsame Wetterlage aller Anlagen
constexpr int kSchrittSek = LiveGenerator::kIntervallMin * 60;

double clamp(double wert, double lo, double hi)
{
    return std::max(lo, std::min(hi, wert));
}
} // namespace

Verlauf LiveGenerator::erzeuge(const Anlage& anlage, const QDateTime& ende)
{
    Verlauf v;
    v.anlageId = anlage.anlageId;
    v.intervallMin = kIntervallMin;

    // Endzeitpunkt auf den Viertelstundenraster abrunden
    QDateTime letzter = ende;
    letzter.setTime(QTime(letzter.time().hour(), (letzter.time().minute() / kIntervallMin) * kIntervallMin, 0, 0));

    const int n = kTage * 24 * 60 / kIntervallMin;
    const QDateTime start = letzter.addSecs(-qint64(n - 1) * kSchrittSek);

    // Gemeinsame Wetterlage: gleicher Seed, also für alle Anlagen dieselbe Folge
    QRandomGenerator wetter(kWetterSeed);
    // Eigene Abweichung der Anlage: Seed aus der Anlagen-ID
    QRandomGenerator lokal(qHash(anlage.anlageId.toUtf8(), 0x5eedu));

    double wetterWert = 0.6 + wetter.generateDouble() * 0.3;
    double lokalWert = (lokal.generateDouble() - 0.5) * 0.2;
    int stoerRest = 0;
    const bool abgeschaltet = (anlage.status == QLatin1String("abgeschaltet"));
    const double kwp = anlage.leistungKwp;

    v.zeit.reserve(n);
    v.leistungW.reserve(n);
    v.einstrahlungWm2.reserve(n);
    v.modultemperaturC.reserve(n);
    v.wrStatus.reserve(n);

    for (int i = 0; i < n; ++i) {
        const QDateTime zeit = start.addSecs(qint64(i) * kSchrittSek);
        wetterWert = clamp(wetterWert + (wetter.generateDouble() - 0.5) * 0.08, 0.3, 1.0);
        lokalWert = clamp(lokalWert + (lokal.generateDouble() - 0.5) * 0.05, -0.15, 0.15);

        // Sonnenstand bestimmt die Einstrahlung in der Modulebene (vereinfachtes Modell)
        const double hoehe = Sonnenstand::berechne(zeit, anlage.lat, anlage.lon).hoeheGrad;
        double einstrahlung = hoehe > 0.0 ? 950.0 * std::pow(std::sin(hoehe * kPi / 180.0), 1.15) : 0.0;
        einstrahlung *= clamp(wetterWert + lokalWert, 0.15, 1.0);
        einstrahlung = std::max(0.0, einstrahlung + (lokal.generateDouble() - 0.5) * 12.0);

        const double temperatur = 15.0 + einstrahlung / 1000.0 * 30.0;
        double leistung = 0.0;
        QString status;

        if (abgeschaltet) {
            status = QStringLiteral("aus");
        } else if (einstrahlung < kNachtGrenzeWm2) {
            status = QStringLiteral("standby");
        } else if (stoerRest > 0) {
            --stoerRest;
            status = QStringLiteral("stoerung");
        } else if (lokal.generateDouble() < kStoerWahrscheinlichkeit) {
            stoerRest = 1 + int(lokal.bounded(7)); // Dauer: 1 bis 7 Viertelstunden
            status = QStringLiteral("stoerung");
        } else {
            const double temperaturFaktor = 1.0 - kTemperaturBeiwert * std::max(0.0, temperatur - 25.0);
            const double rauschen = 1.0 + (lokal.generateDouble() - 0.5) * 0.03;
            leistung = clamp(kwp * 1000.0 * (einstrahlung / 1000.0) * kSystemWirkungsgrad * temperaturFaktor * rauschen,
                             0.0, kwp * 1000.0);
            status = QStringLiteral("ok");
        }

        v.zeit.append(zeit);
        v.leistungW.append(leistung);
        v.einstrahlungWm2.append(einstrahlung);
        v.modultemperaturC.append(temperatur);
        v.wrStatus.append(status);
    }
    return v;
}
