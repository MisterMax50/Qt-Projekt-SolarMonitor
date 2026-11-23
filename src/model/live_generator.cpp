#include "model/live_generator.h"

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QHash>

#include "util/sonnenstand.h"

namespace {
constexpr double kSystemWirkungsgrad = 0.85;
constexpr double kTemperaturBeiwert = 0.0035;      ///< Leistungsverlust je Kelvin über 25 °C
constexpr double kStoerWahrscheinlichkeit = 0.004;  ///< je Anlage und Viertelstunde, Störungsbeginn
constexpr int    kStoerMaxSchritte = 7;             ///< längste Störung in Viertelstunden (ohne den ersten)
constexpr double kNachtGrenzeWm2 = 5.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int    kSchrittSek = LiveGenerator::kIntervallMin * 60;
constexpr int    kStuetzAbstand = 24;              ///< Stützstellen der Wetterkurve: alle 6 Stunden

constexpr quint64 kWetterSeed = 0x5057455452ull;   ///< gemeinsame Wetterlage aller Anlagen

double clamp(double wert, double lo, double hi)
{
    return std::max(lo, std::min(hi, wert));
}

/// Mischfunktion (SplitMix64): aus einer Zahl eine gleichmäßig verteilte Bitfolge.
quint64 mische(quint64 x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

/// Zufallszahl in [0,1), die nur von Seed und Zeitschritt abhängt – nicht von der Reihenfolge der Aufrufe.
double zufall(quint64 seed, qint64 schritt)
{
    return double(mische(seed ^ mische(quint64(schritt))) >> 11) * (1.0 / 9007199254740992.0);
}

/// Glatte Zufallskurve in [0,1): Stützstellen mit Zufall, dazwischen weich interpoliert.
double glatt(quint64 seed, qint64 schritt)
{
    const qint64 k = schritt / kStuetzAbstand;
    const double f = double(schritt - k * kStuetzAbstand) / kStuetzAbstand;
    const double s = (1.0 - std::cos(f * kPi)) / 2.0;
    return zufall(seed, k) * (1.0 - s) + zufall(seed, k + 1) * s;
}

quint64 seedAusId(const QString& id, quint64 salz)
{
    return mische(qHash(id.toUtf8(), 0x5eedu) ^ salz);
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

    // Alle Zufallswerte hängen an der absoluten Zeit (Viertelstunden seit 1970).
    // Verschiebt sich das Zeitfenster, bleiben die Werte für gleiche Zeitpunkte gleich.
    const quint64 wolkenSeed = seedAusId(anlage.anlageId, 0xA1);
    const quint64 rauschSeed = seedAusId(anlage.anlageId, 0xB2);
    const quint64 stoerSeed = seedAusId(anlage.anlageId, 0xC3);
    const quint64 stoerDauerSeed = seedAusId(anlage.anlageId, 0xD4);
    const bool abgeschaltet = (anlage.status == QLatin1String("abgeschaltet"));
    const double kwp = anlage.leistungKwp;

    // Störungsbeginn: eine Störung hält 1 bis 8 Viertelstunden, Beginn ist zufällig
    auto stoerungLaeuft = [&](qint64 schritt) {
        for (qint64 s = schritt - kStoerMaxSchritte; s <= schritt; ++s) {
            if (zufall(stoerSeed, s) >= kStoerWahrscheinlichkeit)
                continue;
            const qint64 dauer = 1 + qint64(zufall(stoerDauerSeed, s) * (kStoerMaxSchritte + 1));
            if (s + dauer > schritt)
                return true;
        }
        return false;
    };

    v.zeit.reserve(n);
    v.leistungW.reserve(n);
    v.einstrahlungWm2.reserve(n);
    v.modultemperaturC.reserve(n);
    v.wrStatus.reserve(n);

    for (int i = 0; i < n; ++i) {
        const QDateTime zeit = start.addSecs(qint64(i) * kSchrittSek);
        const qint64 schritt = zeit.toSecsSinceEpoch() / kSchrittSek;

        // Wetter: gemeinsam für alle Anlagen; eigene Abweichung je Anlage
        const double wetterWert = 0.3 + 0.7 * glatt(kWetterSeed, schritt);
        const double lokalWert = (glatt(wolkenSeed, schritt) - 0.5) * 0.3;

        // Sonnenstand bestimmt die Einstrahlung in der Modulebene (vereinfachtes Modell)
        const double hoehe = Sonnenstand::berechne(zeit, anlage.lat, anlage.lon).hoeheGrad;
        double einstrahlung = hoehe > 0.0 ? 950.0 * std::pow(std::sin(hoehe * kPi / 180.0), 1.15) : 0.0;
        einstrahlung *= clamp(wetterWert + lokalWert, 0.15, 1.0);
        einstrahlung = std::max(0.0, einstrahlung + (zufall(rauschSeed, schritt) - 0.5) * 12.0);

        const double temperatur = 15.0 + einstrahlung / 1000.0 * 30.0;
        double leistung = 0.0;
        QString status;

        if (abgeschaltet) {
            status = QStringLiteral("aus");
        } else if (einstrahlung < kNachtGrenzeWm2) {
            status = QStringLiteral("standby");
        } else if (stoerungLaeuft(schritt)) {
            status = QStringLiteral("stoerung");
        } else {
            const double temperaturFaktor = 1.0 - kTemperaturBeiwert * std::max(0.0, temperatur - 25.0);
            const double rauschen = 1.0 + (zufall(rauschSeed ^ 0x77, schritt) - 0.5) * 0.03;
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

QVector<Stoerung> LiveGenerator::stoerungenAus(const Verlauf& v)
{
    QVector<Stoerung> erg;
    const int n = v.anzahl();
    for (int i = 0; i < n;) {
        if (v.wrStatus.at(i) != QLatin1String("stoerung")) {
            ++i;
            continue;
        }
        int j = i;
        while (j + 1 < n && v.wrStatus.at(j + 1) == QLatin1String("stoerung"))
            ++j;

        const int schritte = j - i + 1;
        const bool offen = (j == n - 1);

        Stoerung s;
        s.anlageId = v.anlageId;
        s.zeitpunkt = v.zeit.at(i);
        // Dauer bestimmt die Schwere; Fehlercode und Text passen zu den Codes in stoerungen.csv
        if (schritte >= 6) {
            s.fehlercode = QStringLiteral("E601");
            s.beschreibung = QStringLiteral("Wechselrichter ausgefallen");
            s.schwere = QStringLiteral("kritisch");
        } else if (schritte >= 3) {
            s.fehlercode = QStringLiteral("E512");
            s.beschreibung = QStringLiteral("String-Ausfall erkannt");
            s.schwere = QStringLiteral("fehler");
        } else {
            s.fehlercode = QStringLiteral("E101");
            s.beschreibung = QStringLiteral("Netzspannung außerhalb Toleranz");
            s.schwere = QStringLiteral("warnung");
        }
        s.status = offen ? QStringLiteral("offen") : QStringLiteral("behoben");
        if (!offen)
            s.behobenAm = v.zeit.at(j).addSecs(v.intervallMin * 60);
        // Beginn und Anlage bestimmen die ID, damit sie über verschobene Zeitfenster gleich bleibt
        s.stoerungId = QStringLiteral("SIM-%1-%2").arg(v.anlageId).arg(s.zeitpunkt.toSecsSinceEpoch());
        erg.append(s);
        i = j + 1;
    }
    return erg;
}
