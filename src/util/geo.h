#pragma once

#include <cmath>

/**
 * @brief Geografische Hilfsfunktionen.
 */
namespace Geo {

/// Großkreisentfernung zweier Punkte (Haversine-Formel) in km.
inline double entfernungKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kErdradiusKm = 6371.0;
    constexpr double kGrad = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * kGrad;
    const double dLon = (lon2 - lon1) * kGrad;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(lat1 * kGrad) * std::cos(lat2 * kGrad)
                         * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * kErdradiusKm * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

} // namespace Geo
