#include "sonnenstand.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QStringList>
#include <QTimeZone>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kGrad = kPi / 180.0;

double normiere(double grad)
{
    grad = std::fmod(grad, 360.0);
    return grad < 0 ? grad + 360.0 : grad;
}

} // namespace

namespace Sonnenstand {

Position berechne(const QDateTime& zeit, double latGrad, double lonGrad)
{
    // Tage seit J2000.0 (1. Januar 2000, 12:00 UT)
    const double jd = zeit.toMSecsSinceEpoch() / 86400000.0 + 2440587.5;
    const double n = jd - 2451545.0;

    // mittlere Länge und mittlere Anomalie der Sonne
    const double L = normiere(280.460 + 0.9856474 * n);
    const double g = normiere(357.528 + 0.9856003 * n) * kGrad;
    // ekliptikale Länge und Schiefe der Ekliptik
    const double lambda = (L + 1.915 * std::sin(g) + 0.020 * std::sin(2 * g)) * kGrad;
    const double eps = (23.439 - 0.0000004 * n) * kGrad;

    const double rektaszension = std::atan2(std::cos(eps) * std::sin(lambda), std::cos(lambda));
    const double deklination = std::asin(std::sin(eps) * std::sin(lambda));

    // Sternzeit Greenwich -> Ortssternzeit -> Stundenwinkel
    const double gmst = normiere(280.46061837 + 360.98564736629 * n);
    const double stundenwinkel = (normiere(gmst + lonGrad) * kGrad) - rektaszension;

    const double phi = latGrad * kGrad;
    const double sinH = std::sin(phi) * std::sin(deklination)
                      + std::cos(phi) * std::cos(deklination) * std::cos(stundenwinkel);
    const double hoehe = std::asin(std::clamp(sinH, -1.0, 1.0));
    const double azimut = std::atan2(-std::sin(stundenwinkel),
                                     std::tan(deklination) * std::cos(phi) - std::sin(phi) * std::cos(stundenwinkel));
    return {hoehe / kGrad, normiere(azimut / kGrad)};
}

Tag tag(const QDate& datum, double latGrad, double lonGrad, const QTimeZone& zone)
{
    // Minutenweise abtasten: robust (auch bei Zeitumstellung) und schnell genug
    constexpr double kHorizont = -0.833;
    Tag t;
    const QDateTime beginn(datum, QTime(0, 0), zone);
    double vorher = berechne(beginn, latGrad, lonGrad).hoeheGrad;
    double maxHoehe = -90.0;
    int tageslicht = 0;
    for (int m = 1; m <= 24 * 60; ++m) {
        const QDateTime z = beginn.addSecs(m * 60);
        if (z.date() != datum)
            break;
        const double h = berechne(z, latGrad, lonGrad).hoeheGrad;
        if (vorher < kHorizont && h >= kHorizont && !t.aufgang.isValid())
            t.aufgang = z;
        if (vorher >= kHorizont && h < kHorizont)
            t.untergang = z;
        if (h > maxHoehe) {
            maxHoehe = h;
            t.mittag = z;
        }
        if (h >= kHorizont)
            ++tageslicht;
        vorher = h;
    }
    t.mittagsHoeheGrad = maxHoehe;
    t.tageslichtMinuten = tageslicht;
    return t;
}

QString richtung(double azimutGrad)
{
    static const char* const namen[] = {QT_TRANSLATE_NOOP("Sonnenstand", "N"),   QT_TRANSLATE_NOOP("Sonnenstand", "NNO"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "NO"),  QT_TRANSLATE_NOOP("Sonnenstand", "ONO"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "O"),   QT_TRANSLATE_NOOP("Sonnenstand", "OSO"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "SO"),  QT_TRANSLATE_NOOP("Sonnenstand", "SSO"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "S"),   QT_TRANSLATE_NOOP("Sonnenstand", "SSW"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "SW"),  QT_TRANSLATE_NOOP("Sonnenstand", "WSW"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "W"),   QT_TRANSLATE_NOOP("Sonnenstand", "WNW"),
                                        QT_TRANSLATE_NOOP("Sonnenstand", "NW"),  QT_TRANSLATE_NOOP("Sonnenstand", "NNW")};
    const int i = int(std::floor(normiere(azimutGrad) / 22.5 + 0.5)) % 16;
    return QCoreApplication::translate("Sonnenstand", namen[i]);
}

QTimeZone zone()
{
    const QTimeZone mez(QByteArrayLiteral("Europe/Berlin"));
    return mez.isValid() ? mez : QTimeZone::systemTimeZone();
}

} // namespace Sonnenstand
