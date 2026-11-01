#include "tourenplanung.h"

#include <algorithm>
#include <limits>

#include "util/gewaesser.h"

Tour Tourenplanung::plane(const TourStopp& start, const QVector<TourStopp>& ziele, bool mitRueckfahrt)
{
    Tour tour;
    const int n = ziele.size();
    if (n == 0)
        return tour;

    // Punkte: 0 = Start, 1..n = Ziele
    QVector<TourStopp> p;
    p.reserve(n + 1);
    p.append(start);
    p += ziele;

    // Distanzmatrix einmalig berechnen: Luftlinie, aber um den See herum
    QVector<Gewaesser::Umweg> umweg;
    umweg.reserve(n + 1);
    for (const TourStopp& s : std::as_const(p))
        umweg.append(Gewaesser::Umweg(s.lat, s.lon));
    QVector<QVector<double>> d(n + 1, QVector<double>(n + 1, 0.0));
    for (int i = 0; i <= n; ++i)
        for (int j = i + 1; j <= n; ++j)
            d[i][j] = d[j][i] = umweg.at(i).km(p[j].lat, p[j].lon);

    // 1) Nearest Neighbour
    QVector<int> route{0};
    QVector<bool> besucht(n + 1, false);
    besucht[0] = true;
    for (int schritt = 0; schritt < n; ++schritt) {
        const int aktuell = route.last();
        int best = -1;
        double bestD = std::numeric_limits<double>::max();
        for (int j = 1; j <= n; ++j) {
            if (!besucht[j] && d[aktuell][j] < bestD) {
                bestD = d[aktuell][j];
                best = j;
            }
        }
        besucht[best] = true;
        route.append(best);
    }
    if (mitRueckfahrt)
        route.append(0);

    // 2) 2-opt: Teilstück [i..k] umkehren, wenn es die Strecke verkürzt.
    auto laenge = [&](const QVector<int>& r) {
        double s = 0.0;
        for (int i = 1; i < r.size(); ++i)
            s += d[r[i - 1]][r[i]];
        return s;
    };
    const int letzterBeweglicher = route.size() - (mitRueckfahrt ? 2 : 1);
    bool verbessert = true;
    int runden = 0;
    while (verbessert && runden++ < 100) {
        verbessert = false;
        for (int i = 1; i < letzterBeweglicher; ++i) {
            for (int k = i + 1; k <= letzterBeweglicher; ++k) {
                const int a = route[i - 1], b = route[i];
                const int c = route[k];
                const int nachK = (k + 1 < route.size()) ? route[k + 1] : -1;
                const double alt = d[a][b] + (nachK >= 0 ? d[c][nachK] : 0.0);
                const double neu = d[a][c] + (nachK >= 0 ? d[b][nachK] : 0.0);
                if (neu + 1e-9 < alt) {
                    std::reverse(route.begin() + i, route.begin() + k + 1);
                    verbessert = true;
                }
            }
        }
    }

    for (int i = 1; i < route.size(); ++i) {
        const double etappe = d[route[i - 1]][route[i]];
        tour.etappenKm.append(etappe);
        if (route[i] != 0)
            tour.reihenfolge.append(route[i] - 1);
    }
    tour.gesamtKm = laenge(route);

    // Linienzug für die Karte (Etappen ggf. um das Ufer herum)
    for (int i = 1; i < route.size(); ++i) {
        const QVector<QPointF> etappe = umweg.at(route[i - 1]).pfad(p[route[i]].lat, p[route[i]].lon);
        tour.verlauf += tour.verlauf.isEmpty() ? etappe : etappe.mid(1);
    }
    return tour;
}
