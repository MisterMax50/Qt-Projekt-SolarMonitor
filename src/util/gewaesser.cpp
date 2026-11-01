#include "gewaesser.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QRectF>

#include "util/gewaesser_daten.h"

namespace Gewaesser {

namespace {

constexpr double kBezugLat = 47.65;
constexpr double kBezugLon = 9.40;
constexpr double kKmProGradLat = 110.574;
const double kKmProGradLon = 111.320 * std::cos(kBezugLat * 3.14159265358979323846 / 180.0);
constexpr double kUnendlich = std::numeric_limits<double>::infinity();

QPointF zuKm(double lat, double lon)
{
    return {(lon - kBezugLon) * kKmProGradLon, (lat - kBezugLat) * kKmProGradLat};
}

QPointF zuGrad(const QPointF& km)
{
    return {kBezugLat + km.y() / kKmProGradLat, kBezugLon + km.x() / kKmProGradLon};
}

double abstand(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

double kreuz(const QPointF& a, const QPointF& b, const QPointF& c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

/// Echte Kreuzung zweier Strecken (Berühren an Endpunkten zählt nicht).
bool kreuzen(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d)
{
    constexpr double eps = 1e-9;
    const double o1 = kreuz(a, b, c), o2 = kreuz(a, b, d);
    const double o3 = kreuz(c, d, a), o4 = kreuz(c, d, b);
    return ((o1 > eps && o2 < -eps) || (o1 < -eps && o2 > eps))
        && ((o3 > eps && o4 < -eps) || (o3 < -eps && o4 > eps));
}

bool innen(const QVector<QPointF>& poly, const QPointF& p)
{
    bool drin = false;
    for (int i = 0, j = int(poly.size()) - 1; i < poly.size(); j = i++) {
        const QPointF& a = poly.at(i);
        const QPointF& b = poly.at(j);
        if ((a.y() > p.y()) != (b.y() > p.y())
            && p.x() < (b.x() - a.x()) * (p.y() - a.y()) / (b.y() - a.y()) + a.x())
            drin = !drin;
    }
    return drin;
}

/// Nächster Punkt auf der Strecke ab zu p.
QPointF lotfuss(const QPointF& a, const QPointF& b, const QPointF& p)
{
    const QPointF ab = b - a;
    const double l2 = QPointF::dotProduct(ab, ab);
    const double t = l2 > 0 ? std::clamp(QPointF::dotProduct(p - a, ab) / l2, 0.0, 1.0) : 0.0;
    return a + t * ab;
}

/// Seeufer in km, Hüllrechtecke und Sichtbarkeitsgraph der Eckpunkte.
struct Netz {
    QVector<QVector<QPointF>> seen;      ///< km
    QVector<QRectF>           huelle;
    QVector<QVector<QPointF>> uferGrad;  ///< (lat, lon)
    QVector<QPointF>          knoten;    ///< alle Eckpunkte, km
    QVector<QVector<QPair<int, double>>> nachbarn;

    Netz()
    {
        for (const GewaesserDaten::See& see : GewaesserDaten::kSeen) {
            QVector<QPointF> km, grad;
            for (int i = 0; i < see.anzahl; ++i) {
                km.append(zuKm(see.punkte[i].lat, see.punkte[i].lon));
                grad.append(QPointF(see.punkte[i].lat, see.punkte[i].lon));
            }
            double x0 = kUnendlich, y0 = kUnendlich, x1 = -kUnendlich, y1 = -kUnendlich;
            for (const QPointF& q : std::as_const(km)) {
                x0 = std::min(x0, q.x()); x1 = std::max(x1, q.x());
                y0 = std::min(y0, q.y()); y1 = std::max(y1, q.y());
            }
            huelle.append(QRectF(QPointF(x0, y0), QPointF(x1, y1)));
            seen.append(km);
            uferGrad.append(grad);
        }
        QVector<QPair<int, int>> lage; // (See, Index) je Knoten
        for (int s = 0; s < seen.size(); ++s)
            for (int i = 0; i < seen.at(s).size(); ++i) {
                knoten.append(seen.at(s).at(i));
                lage.append({s, i});
            }
        nachbarn.resize(knoten.size());
        for (int i = 0; i < knoten.size(); ++i) {
            for (int j = i + 1; j < knoten.size(); ++j) {
                const bool amUfer = lage.at(i).first == lage.at(j).first
                    && (std::abs(lage.at(i).second - lage.at(j).second) == 1
                        || std::abs(lage.at(i).second - lage.at(j).second)
                               == seen.at(lage.at(i).first).size() - 1);
                if (amUfer || frei(knoten.at(i), knoten.at(j))) {
                    const double d = abstand(knoten.at(i), knoten.at(j));
                    nachbarn[i].append({j, d});
                    nachbarn[j].append({i, d});
                }
            }
        }
    }

    bool imSee(const QPointF& p) const
    {
        for (int s = 0; s < seen.size(); ++s)
            if (huelle.at(s).contains(p) && innen(seen.at(s), p))
                return true;
        return false;
    }

    /// Verläuft die Strecke a-b vollständig an Land (Ufer berühren erlaubt)?
    bool frei(const QPointF& a, const QPointF& b) const
    {
        const QRectF strecke = QRectF(a, b).normalized();
        for (int s = 0; s < seen.size(); ++s) {
            if (!huelle.at(s).intersects(strecke.adjusted(-1e-6, -1e-6, 1e-6, 1e-6)))
                continue;
            const QVector<QPointF>& poly = seen.at(s);
            for (int i = 0, j = int(poly.size()) - 1; i < poly.size(); j = i++)
                if (kreuzen(a, b, poly.at(i), poly.at(j)))
                    return false;
            // Sehne zwischen zwei Uferpunkten quer durch den See?
            for (double t : {0.25, 0.5, 0.75})
                if (innen(poly, a + t * (b - a)))
                    return false;
        }
        return true;
    }

    /// Punkte im See (grober Uferumriss) auf das nächste Ufer schieben.
    QPointF anLand(const QPointF& p) const
    {
        for (int s = 0; s < seen.size(); ++s) {
            if (!huelle.at(s).contains(p) || !innen(seen.at(s), p))
                continue;
            const QVector<QPointF>& poly = seen.at(s);
            QPointF best = poly.first();
            double bestD = kUnendlich;
            for (int i = 0, j = int(poly.size()) - 1; i < poly.size(); j = i++) {
                const QPointF q = lotfuss(poly.at(j), poly.at(i), p);
                const double d = abstand(p, q);
                if (d < bestD) {
                    bestD = d;
                    best = q;
                }
            }
            // ein paar Meter nach außen, damit der Punkt sicher an Land liegt
            const QPointF r = bestD > 1e-9 ? (best - p) / bestD : QPointF(0, 1);
            for (double schritt : {0.01, 0.05, 0.2}) {
                const QPointF q = best + schritt * r;
                if (!imSee(q))
                    return q;
            }
            return best;
        }
        return p;
    }
};

const Netz& netz()
{
    static const Netz n; // threadsicher initialisiert (C++11)
    return n;
}

} // namespace

const QVector<QVector<QPointF>>& ufer()
{
    return netz().uferGrad;
}

bool imWasser(double lat, double lon)
{
    return netz().imSee(zuKm(lat, lon));
}

QString quelle()
{
    return QStringLiteral("Natural Earth");
}

// -------------------------------------------------------------------------

Umweg::Umweg(double lat, double lon)
    : m_startGrad(lat, lon)
{
    const Netz& n = netz();
    m_start = n.anLand(zuKm(lat, lon));
    const int anzahl = int(n.knoten.size());
    m_dist.fill(kUnendlich, anzahl);
    m_vorg.fill(-1, anzahl);
    for (int i = 0; i < anzahl; ++i)
        if (n.frei(m_start, n.knoten.at(i)))
            m_dist[i] = abstand(m_start, n.knoten.at(i));

    // Dijkstra; bei ~100 Knoten reicht die einfache O(n²)-Variante
    QVector<bool> fertig(anzahl, false);
    for (int runde = 0; runde < anzahl; ++runde) {
        int u = -1;
        for (int i = 0; i < anzahl; ++i)
            if (!fertig.at(i) && (u < 0 || m_dist.at(i) < m_dist.at(u)))
                u = i;
        if (u < 0 || m_dist.at(u) == kUnendlich)
            break;
        fertig[u] = true;
        for (const auto& [v, d] : n.nachbarn.at(u)) {
            if (m_dist.at(u) + d < m_dist.at(v)) {
                m_dist[v] = m_dist.at(u) + d;
                m_vorg[v] = u;
            }
        }
    }
    m_sortiert.resize(anzahl);
    for (int i = 0; i < anzahl; ++i)
        m_sortiert[i] = i;
    std::sort(m_sortiert.begin(), m_sortiert.end(), [this](int a, int b) { return m_dist.at(a) < m_dist.at(b); });
}

double Umweg::km(QPointF ziel, int* letzterKnoten) const
{
    const Netz& n = netz();
    ziel = n.anLand(ziel);
    *letzterKnoten = -1;
    if (n.frei(m_start, ziel))
        return abstand(m_start, ziel);
    double best = kUnendlich;
    for (int v : m_sortiert) {
        if (m_dist.at(v) >= best)
            break; // alle weiteren Knoten sind schon weiter entfernt
        const double kandidat = m_dist.at(v) + abstand(n.knoten.at(v), ziel);
        if (kandidat < best && n.frei(n.knoten.at(v), ziel)) {
            best = kandidat;
            *letzterKnoten = v;
        }
    }
    return best;
}

double Umweg::km(double lat, double lon) const
{
    int letzter = -1;
    return km(zuKm(lat, lon), &letzter);
}

bool Umweg::umUfer(double lat, double lon) const
{
    const Netz& n = netz();
    return !n.frei(m_start, n.anLand(zuKm(lat, lon)));
}

QVector<QPointF> Umweg::pfad(double lat, double lon) const
{
    int letzter = -1;
    km(zuKm(lat, lon), &letzter);
    QVector<QPointF> zwischen;
    for (int v = letzter; v >= 0; v = m_vorg.at(v))
        zwischen.prepend(zuGrad(netz().knoten.at(v)));
    // Originalpunkte als Enden, damit die Linie genau an den Markern ansetzt
    QVector<QPointF> erg{m_startGrad};
    erg += zwischen;
    erg.append(QPointF(lat, lon));
    return erg;
}

double wegKm(double lat1, double lon1, double lat2, double lon2)
{
    return Umweg(lat1, lon1).km(lat2, lon2);
}

} // namespace Gewaesser
