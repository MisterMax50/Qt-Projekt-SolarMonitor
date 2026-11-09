#include "karte_view.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QDir>
#include <QHash>
#include <QFileInfo>
#include <QStandardPaths>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "ui/kachel_lader.h"
#include "util/anzeige.h"
#include "util/geo.h"
#include "util/gewaesser.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

/// Web-Mercator (wie OSM): Weltkoordinaten im Bereich [0, 1] × [0, 1].
QPointF mercator(double lat, double lon)
{
    const double phi = qBound(-85.0511, lat, 85.0511) * kPi / 180.0;
    return QPointF((lon + 180.0) / 360.0, (1.0 - std::log(std::tan(phi) + 1.0 / std::cos(phi)) / kPi) / 2.0);
}

double breiteAusMercatorY(double y)
{
    return std::atan(std::sinh(kPi * (1.0 - 2.0 * y))) * 180.0 / kPi;
}

/// Erreichbare Fläche um den Startpunkt, alle Punkte als (lat, lon).
struct UmkreisFlaeche {
    QVector<QPolygonF> flaechen; ///< Füllung (innere Zeilenstücke + Randzellen)
    QVector<QLineF>    kontur;   ///< Randlinie
};

/**
 * @brief Fläche, die vom Start auf dem Landweg um die Seen herum
 *        höchstens @p radiusKm entfernt ist.
 *
 * Auf einem Raster wird die Umweg-Entfernung berechnet und die Grenze
 * "Entfernung = Radius" per Marching Squares als Linie bestimmt. Beim
 * Zeichnen werden die Seen ausgespart, dadurch endet die Fläche am Ufer.
 */
UmkreisFlaeche berechneUmkreis(const QPointF& start, double radiusKm)
{
    constexpr int n = 140; // Zellen je Achse
    const double dLat = radiusKm / 110.574 * 1.04;
    const double dLon = radiusKm / (111.320 * std::cos(start.x() * kPi / 180.0)) * 1.04;
    const double lat0 = start.x() - dLat, lon0 = start.y() - dLon;
    const double hLat = 2.0 * dLat / n, hLon = 2.0 * dLon / n;

    const Gewaesser::Umweg umweg(start.x(), start.y());
    QVector<double> f((n + 1) * (n + 1));
    for (int i = 0; i <= n; ++i) {
        for (int j = 0; j <= n; ++j) {
            const double lat = lat0 + i * hLat, lon = lon0 + j * hLon;
            const double luft = Geo::entfernungKm(start.x(), start.y(), lat, lon);
            // Umweg ist nie kürzer als die Luftlinie -> nur nahe Punkte genau rechnen.
            // Punkte im See erhalten den Wert ihres nächsten Ufers; die Grenze zwischen
            // diesseitigem und jenseitigem Ufer liegt damit im Wasser und wird ausgespart.
            f[i * (n + 1) + j] = luft > radiusKm ? luft : umweg.km(lat, lon);
        }
    }
    auto wert = [&](int i, int j) { return f.at(i * (n + 1) + j) - radiusKm; }; // ≤ 0: erreichbar
    auto punkt = [&](double i, double j) { return QPointF(lat0 + i * hLat, lon0 + j * hLon); };

    UmkreisFlaeche erg;
    for (int i = 0; i < n; ++i) {
        int laufStart = -1;
        for (int j = 0; j <= n; ++j) {
            // Ecken im Umlauf: (i,j) (i,j+1) (i+1,j+1) (i+1,j)
            const bool zelle = j < n;
            const double w[4] = {zelle ? wert(i, j) : 1, zelle ? wert(i, j + 1) : 1,
                                 zelle ? wert(i + 1, j + 1) : 1, zelle ? wert(i + 1, j) : 1};
            const bool voll = zelle && w[0] <= 0 && w[1] <= 0 && w[2] <= 0 && w[3] <= 0;
            if (voll) {
                if (laufStart < 0)
                    laufStart = j;
                continue;
            }
            if (laufStart >= 0) { // zusammenhängende volle Zellen einer Zeile als ein Rechteck
                QPolygonF r;
                r << punkt(i, laufStart) << punkt(i, j) << punkt(i + 1, j) << punkt(i + 1, laufStart);
                erg.flaechen.append(r);
                laufStart = -1;
            }
            if (!zelle || (w[0] > 0 && w[1] > 0 && w[2] > 0 && w[3] > 0))
                continue;

            // Randzelle: erreichbaren Teil der Zelle ausschneiden
            const QPointF ecke[4] = {punkt(i, j), punkt(i, j + 1), punkt(i + 1, j + 1), punkt(i + 1, j)};
            QPolygonF teil;
            QVector<QPointF> schnitt;
            for (int k = 0; k < 4; ++k) {
                const int m = (k + 1) % 4;
                if (w[k] <= 0)
                    teil << ecke[k];
                if ((w[k] <= 0) != (w[m] <= 0)) {
                    const double t = w[k] / (w[k] - w[m]);
                    const QPointF q = ecke[k] + t * (ecke[m] - ecke[k]);
                    teil << q;
                    schnitt.append(q);
                }
            }
            erg.flaechen.append(teil);
            if (schnitt.size() == 2) {
                erg.kontur.append(QLineF(schnitt.at(0), schnitt.at(1)));
            } else if (schnitt.size() == 4) { // Sattelpunkt
                erg.kontur.append(QLineF(schnitt.at(0), schnitt.at(1)));
                erg.kontur.append(QLineF(schnitt.at(2), schnitt.at(3)));
            }
        }
    }
    return erg;
}
} // namespace

// ===================================================================
//  KartenFlaeche: Zeichenfläche mit Projektion, Zoom und Pan
// ===================================================================

class KartenFlaeche : public QWidget {
public:
    struct Punkt {
        QString id;
        double  lat = 0.0, lon = 0.0, kwp = 0.0;
        QColor  farbe;
        QString tooltip;
        int     rang = 0; ///< höher = wichtiger, wird zuletzt (also oben) gezeichnet
    };
    struct Station {
        QString id, name;
        double  lat = 0.0, lon = 0.0;
    };

    std::function<void(const QString&)> beiKlick;

    explicit KartenFlaeche(KachelLader* lader, QWidget* parent)
        : QWidget(parent)
        , m_lader(lader)
    {
        connect(m_lader, &KachelLader::kachelGeladen, this, [this] { update(); });
        connect(m_lader, &KachelLader::statusGeaendert, this, [this] { update(); });
        setMouseTracking(true);
        setMinimumSize(320, 240);
        setCursor(Qt::OpenHandCursor);
    }

    void setDaten(QVector<Punkt> punkte, QVector<Station> stationen, QVector<QPair<QColor, QString>> legende)
    {
        // Wichtige Punkte zuletzt (oben); innerhalb gleicher Wichtigkeit große
        // Anlagen zuerst zeichnen, damit kleine sichtbar bleiben.
        std::sort(punkte.begin(), punkte.end(), [](const Punkt& a, const Punkt& b) {
            return a.rang != b.rang ? a.rang < b.rang : a.kwp > b.kwp;
        });
        m_punkte = std::move(punkte);
        m_stationen = std::move(stationen);
        m_legende = std::move(legende);
        berechneAusschnitt();
        update();
    }
    void setStationenSichtbar(bool an) { m_zeigeStationen = an; update(); }
    void markiere(const QString& id) { m_markiert = id; update(); }
    void setTour(const QVector<QPointF>& stopps, const QVector<QPointF>& verlauf, bool strasse, double umkreisKm)
    {
        m_tour = stopps;
        m_tourVerlauf = verlauf;
        m_tourStrasse = strasse;
        m_umkreis = (stopps.size() >= 2 && umkreisKm > 0.0) ? berechneUmkreis(stopps.first(), umkreisKm)
                                                            : UmkreisFlaeche{};
        update();
    }
    void ansichtZuruecksetzen()
    {
        m_zoom = 1.0;
        m_pan = QPointF();
        m_lader->erneutVersuchen();
        update();
    }
    void setHintergrund(bool an) { m_hintergrund = an; update(); }

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent* e) override
    {
        const double faktor = std::pow(1.2, e->angleDelta().y() / 120.0);
        const double neu = qBound(0.5, m_zoom * faktor, 64.0);
        const QPointF mitte(width() / 2.0, height() / 2.0);
        const QPointF maus = e->position();
        m_pan = maus - mitte - (maus - mitte - m_pan) * (neu / m_zoom);
        m_zoom = neu;
        update();
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        m_druckPos = e->position();
        m_letztePos = e->position();
        m_zieht = false;
        setCursor(Qt::ClosedHandCursor);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (e->buttons() & Qt::LeftButton) {
            if ((e->position() - m_druckPos).manhattanLength() > 4)
                m_zieht = true;
            m_pan += e->position() - m_letztePos;
            m_letztePos = e->position();
            update();
            return;
        }
        const Punkt* p = punktBei(e->position());
        if (p)
            QToolTip::showText(e->globalPosition().toPoint(), p->tooltip, this);
        else if (const Station* s = stationBei(e->position()))
            QToolTip::showText(e->globalPosition().toPoint(), QStringLiteral("%1 – %2").arg(s->id, s->name), this);
        else
            QToolTip::hideText();
        setCursor(p ? Qt::PointingHandCursor : Qt::OpenHandCursor);
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        setCursor(Qt::OpenHandCursor);
        if (!m_zieht && beiKlick)
            if (const Punkt* p = punktBei(e->position()))
                beiKlick(p->id);
    }

private:
    void berechneAusschnitt()
    {
        double xMin = 1, xMax = 0, yMin = 1, yMax = 0;
        auto erweitere = [&](double lat, double lon) {
            const QPointF w = mercator(lat, lon);
            xMin = qMin(xMin, w.x()); xMax = qMax(xMax, w.x());
            yMin = qMin(yMin, w.y()); yMax = qMax(yMax, w.y());
        };
        for (const Punkt& p : std::as_const(m_punkte))
            erweitere(p.lat, p.lon);
        for (const Station& s : std::as_const(m_stationen))
            erweitere(s.lat, s.lon);
        if (xMin > xMax) { // keine Daten: Bodensee
            erweitere(47.4, 8.9);
            erweitere(48.0, 10.1);
        }
        m_mitte = QPointF((xMin + xMax) / 2.0, (yMin + yMax) / 2.0);
        m_breiteWelt = qMax(1e-5, xMax - xMin) * 1.08;
        m_hoeheWelt = qMax(1e-5, yMax - yMin) * 1.08;
    }

    /// Pixel je Welteinheit (Welt = 1 × 1 bei Web-Mercator).
    double skala() const
    {
        return qMin(width() / m_breiteWelt, height() / m_hoeheWelt) * m_zoom;
    }

    QPointF weltZuBild(const QPointF& w) const
    {
        return QPointF(width() / 2.0, height() / 2.0) + (w - m_mitte) * skala() + m_pan;
    }

    QPointF bildZuWelt(const QPointF& b) const
    {
        return m_mitte + (b - QPointF(width() / 2.0, height() / 2.0) - m_pan) / skala();
    }

    QPointF projiziere(double lat, double lon) const { return weltZuBild(mercator(lat, lon)); }

    void zeichneKacheln(QPainter& p);
    void zeichneGitter(QPainter& p, const QFont& klein);

    double radius(double kwp) const
    {
        return 2.5 + 1.3 * std::log10(qMax(0.0, kwp) + 1.0) + (m_zoom > 4 ? 1.5 : 0.0);
    }

    const Punkt* punktBei(const QPointF& pos) const
    {
        const Punkt* best = nullptr;
        double bestD = 1e9;
        for (const Punkt& p : m_punkte) {
            const QPointF d = projiziere(p.lat, p.lon) - pos;
            const double dist = std::hypot(d.x(), d.y());
            if (dist <= radius(p.kwp) + 4 && dist < bestD) {
                bestD = dist;
                best = &p;
            }
        }
        return best;
    }

    const Station* stationBei(const QPointF& pos) const
    {
        if (!m_zeigeStationen)
            return nullptr;
        for (const Station& s : m_stationen) {
            const QPointF d = projiziere(s.lat, s.lon) - pos;
            if (std::hypot(d.x(), d.y()) <= 8)
                return &s;
        }
        return nullptr;
    }

    QVector<Punkt>   m_punkte;
    QVector<Station> m_stationen;
    QVector<QPair<QColor, QString>> m_legende;
    QVector<QPointF> m_tour;        ///< Start + Stopps (+ Start), (lat, lon)
    QVector<QPointF> m_tourVerlauf; ///< Linienzug (lat, lon); leer = gerade Linien zwischen den Stopps
    bool             m_tourStrasse = false; ///< Straßenverlauf (OSRM) statt Luftlinie
    UmkreisFlaeche   m_umkreis;
    QString m_markiert;
    bool    m_zeigeStationen = true;

    KachelLader* m_lader;
    bool    m_hintergrund = true;
    QPointF m_mitte{0.5, 0.5};
    double  m_breiteWelt = 1.0, m_hoeheWelt = 1.0;
    double  m_zoom = 1.0;
    QPointF m_pan;
    QPointF m_druckPos, m_letztePos;
    bool    m_zieht = false;
    /// Kacheln, die bereits auf die exakte Zielgröße in Gerätepixeln
    /// herunterskaliert wurden (Schlüssel: Bild + Größe)
    QHash<QString, QPixmap> m_scharf;
    QPixmap scharf(const QPixmap& quelle, const QRectF& ausschnitt, const QSize& ziel, qreal dpr);
};

QPixmap KartenFlaeche::scharf(const QPixmap& quelle, const QRectF& ausschnitt, const QSize& ziel, qreal dpr)
{
    // Hochwertig verkleinern (statt beim Zeichnen nur bilinear zu
    // interpolieren) und das Ergebnis merken: beim Verschieben der Karte
    // bleiben die Größen gleich, es wird also nicht neu gerechnet.
    const QString schluessel = QStringLiteral("%1|%2,%3,%4|%5x%6")
                                   .arg(quelle.cacheKey())
                                   .arg(ausschnitt.x()).arg(ausschnitt.y()).arg(ausschnitt.width())
                                   .arg(ziel.width()).arg(ziel.height());
    auto it = m_scharf.constFind(schluessel);
    if (it != m_scharf.constEnd())
        return it.value();
    if (m_scharf.size() > 300)
        m_scharf.clear(); // nach vielen Zoomstufen Speicher freigeben
    QPixmap teil = ausschnitt == QRectF(quelle.rect()) ? quelle : quelle.copy(ausschnitt.toRect());
    QPixmap pm = teil.scaled(ziel, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    pm.setDevicePixelRatio(dpr);
    m_scharf.insert(schluessel, pm);
    return pm;
}

void KartenFlaeche::zeichneKacheln(QPainter& p)
{
    // Kachel-Zoomstufe passend zum Maßstab in Gerätepixeln (Windows-
    // Skalierung 125 %/150 % berücksichtigt): eine Kachel ist 256 px breit,
    // die Welt hat auf Stufe z also 256 · 2^z Pixel. Es wird aufgerundet,
    // damit Kacheln höchstens minimal vergrößert, sonst verkleinert werden –
    // Vergrößern macht die Karte unscharf, Verkleinern nicht.
    const qreal dpr = devicePixelRatioF();
    const int z = qBound(1, int(std::ceil(std::log2(skala() * dpr / 256.0) - 0.1)), 19);
    const int n = 1 << z;

    const QPointF w0 = bildZuWelt(QPointF(0, 0));
    const QPointF w1 = bildZuWelt(QPointF(width(), height()));
    const int x0 = qMax(0, int(std::floor(w0.x() * n))), x1 = qMin(n - 1, int(std::floor(w1.x() * n)));
    const int y0 = qMax(0, int(std::floor(w0.y() * n))), y1 = qMin(n - 1, int(std::floor(w1.y() * n)));

    // Kachelränder auf ganze Gerätepixel legen: Kacheln stoßen lückenlos
    // aneinander und werden ohne weiteres Umrechnen 1:1 gezeichnet.
    auto geraet = [&](double wx, double wy) {
        const QPointF b = weltZuBild(QPointF(wx, wy));
        return QPoint(int(std::lround(b.x() * dpr)), int(std::lround(b.y() * dpr)));
    };

    m_lader->neueAnforderungsrunde();
    p.save();
    p.setOpacity(0.8); // etwas aufgehellt, damit die Anlagenpunkte gut lesbar bleiben
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            const QPoint a = geraet(double(tx) / n, double(ty) / n);
            const QPoint e = geraet(double(tx + 1) / n, double(ty + 1) / n);
            const QSize groesse(e.x() - a.x(), e.y() - a.y());
            if (groesse.width() <= 0 || groesse.height() <= 0)
                continue;
            const QPointF ziel(a.x() / dpr, a.y() / dpr);

            const QPixmap pm = m_lader->kachel(z, tx, ty);
            if (!pm.isNull()) {
                p.drawPixmap(ziel, scharf(pm, QRectF(pm.rect()), groesse, dpr));
                continue;
            }
            // Ersatz, bis die Kachel geladen ist: vergrößerter Ausschnitt
            // einer gröberen, bereits gespeicherten Kachel
            for (int d = 1; d <= 4 && z - d >= 0; ++d) {
                const QPixmap grob = m_lader->kachelOhneDownload(z - d, tx >> d, ty >> d);
                if (grob.isNull())
                    continue;
                const int teile = 1 << d;
                const double gr = 256.0 / teile;
                const QRectF quelle((tx % teile) * gr, (ty % teile) * gr, gr, gr);
                p.drawPixmap(ziel, scharf(grob, quelle, groesse, dpr));
                break;
            }
        }
    }
    p.restore();
}

void KartenFlaeche::zeichneGitter(QPainter& p, const QFont& klein)
{
    const QPointF links = bildZuWelt(QPointF(0, height()));
    const QPointF rechts = bildZuWelt(QPointF(width(), 0));
    const double lonMin = links.x() * 360.0 - 180.0, lonMax = rechts.x() * 360.0 - 180.0;
    const double latMin = breiteAusMercatorY(links.y()), latMax = breiteAusMercatorY(rechts.y());
    const double spanne = lonMax - lonMin;
    const double schritt = spanne > 3 ? 0.5 : (spanne > 0.6 ? 0.1 : 0.05);

    p.setPen(QColor(200, 210, 200));
    p.setFont(klein);
    for (double lat = std::floor(latMin / schritt) * schritt; lat <= latMax; lat += schritt) {
        const double y = projiziere(lat, lonMin).y();
        p.drawLine(QPointF(0, y), QPointF(width(), y));
        p.drawText(QPointF(4, y - 2), Anzeige::zahl(lat, schritt < 0.1 ? 2 : 1) + QStringLiteral("° N"));
    }
    for (double lon = std::floor(lonMin / schritt) * schritt; lon <= lonMax; lon += schritt) {
        const double x = projiziere(latMin, lon).x();
        p.drawLine(QPointF(x, 0), QPointF(x, height()));
        p.drawText(QPointF(x + 2, height() - 4), Anzeige::zahl(lon, schritt < 0.1 ? 2 : 1) + QStringLiteral("° O"));
    }
}

void KartenFlaeche::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), m_hintergrund ? QColor(242, 239, 233) : QColor(236, 242, 236));

    QFont klein = font();
    klein.setPointSizeF(klein.pointSizeF() * 0.8);
    if (m_hintergrund)
        zeichneKacheln(p);
    else
        zeichneGitter(p, klein);

    // Umkreis um den Startpunkt: auf dem Landweg erreichbare Fläche, Seen ausgespart
    if (!m_umkreis.flaechen.isEmpty()) {
        QPainterPath land;
        land.setFillRule(Qt::OddEvenFill);
        land.addRect(QRectF(rect()).adjusted(-50, -50, 50, 50));
        for (const QVector<QPointF>& see : Gewaesser::ufer()) {
            QPolygonF ufer;
            for (const QPointF& q : see)
                ufer << projiziere(q.x(), q.y());
            ufer << ufer.first();
            land.addPolygon(ufer);
        }
        QPainterPath flaeche;
        flaeche.setFillRule(Qt::WindingFill);
        for (const QPolygonF& teil : std::as_const(m_umkreis.flaechen)) {
            QPolygonF bild;
            for (const QPointF& q : teil)
                bild << projiziere(q.x(), q.y());
            flaeche.addPolygon(bild);
            flaeche.closeSubpath();
        }
        QVector<QLineF> rand;
        rand.reserve(m_umkreis.kontur.size());
        for (const QLineF& l : std::as_const(m_umkreis.kontur))
            rand.append(QLineF(projiziere(l.p1().x(), l.p1().y()), projiziere(l.p2().x(), l.p2().y())));
        p.save();
        p.setClipPath(land);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 80, 200, 26));
        p.drawPath(flaeche);
        p.setPen(QPen(QColor(40, 80, 200, 170), 1.6, Qt::DashLine, Qt::RoundCap));
        p.drawLines(rand);
        p.restore();
    }

    // Anlagen
    const QPointF* markiertPos = nullptr;
    QPointF markiertPunkt;
    for (const Punkt& pt : std::as_const(m_punkte)) {
        const QPointF pos = projiziere(pt.lat, pt.lon);
        if (!rect().adjusted(-20, -20, 20, 20).contains(pos.toPoint()))
            continue;
        const double r = radius(pt.kwp);
        p.setPen(QPen(QColor(255, 255, 255, 220), 0.8));
        p.setBrush(pt.farbe);
        p.drawEllipse(pos, r, r);
        if (pt.id == m_markiert) {
            markiertPunkt = pos;
            markiertPos = &markiertPunkt;
        }
    }
    if (markiertPos) {
        p.setPen(QPen(QColor(20, 20, 20), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(*markiertPos, 12, 12);
    }

    // Tour: Straßenverlauf (falls vorhanden), sonst Luftlinie
    if (m_tour.size() >= 2) {
        const QVector<QPointF>& linie = m_tourVerlauf.size() >= 2 ? m_tourVerlauf : m_tour;
        const bool strasse = m_tourStrasse && m_tourVerlauf.size() >= 2;
        QPainterPath pfad;
        pfad.moveTo(projiziere(linie.first().x(), linie.first().y()));
        for (int i = 1; i < linie.size(); ++i)
            pfad.lineTo(projiziere(linie.at(i).x(), linie.at(i).y()));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 220), 6.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(pfad); // heller Rand für Lesbarkeit auf der Karte
        p.setPen(QPen(QColor(37, 99, 235), strasse ? 3.5 : 2.5, strasse ? Qt::SolidLine : Qt::DashLine,
                      Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(pfad);
        p.setFont(font());
        for (int i = 1; i < m_tour.size(); ++i) {
            const QPointF pt = projiziere(m_tour.at(i).x(), m_tour.at(i).y());
            if (i == m_tour.size() - 1 && m_tour.last() == m_tour.first())
                break;
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(QColor(37, 99, 235));
            p.drawEllipse(pt, 10, 10);
            p.setPen(Qt::white);
            p.drawText(QRectF(pt.x() - 9, pt.y() - 9, 18, 18), Qt::AlignCenter, QString::number(i));
        }
        const QPointF start = projiziere(m_tour.first().x(), m_tour.first().y());
        p.setPen(QPen(Qt::black, 1.5));
        p.setBrush(QColor(255, 220, 0));
        p.drawRect(QRectF(start.x() - 6, start.y() - 6, 12, 12));
    }

    // Wetterstationen als Dreiecke
    if (m_zeigeStationen) {
        p.setFont(klein);
        for (const Station& s : std::as_const(m_stationen)) {
            const QPointF pos = projiziere(s.lat, s.lon);
            QPolygonF dreieck;
            dreieck << pos + QPointF(0, -7) << pos + QPointF(6, 5) << pos + QPointF(-6, 5);
            p.setPen(QPen(Qt::black, 1));
            p.setBrush(QColor(80, 160, 230));
            p.drawPolygon(dreieck);
            p.setPen(QColor(30, 30, 60));
            p.drawText(pos + QPointF(8, 4), s.id);
        }
    }

    // Legende
    p.setFont(font());
    const int zeilenHoehe = 18;
    double breite = 190;
    for (const auto& eintrag : std::as_const(m_legende))
        breite = std::max(breite, p.fontMetrics().horizontalAdvance(eintrag.second) + 40.0);
    const QRectF box(8, 8, breite, 10 + zeilenHoehe * (m_legende.size() + (m_zeigeStationen ? 1 : 0)));
    p.setPen(QColor(160, 160, 160));
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawRoundedRect(box, 4, 4);
    double y = box.top() + 6;
    for (const auto& eintrag : std::as_const(m_legende)) {
        p.setPen(Qt::NoPen);
        p.setBrush(eintrag.first);
        p.drawEllipse(QPointF(box.left() + 14, y + 8), 5, 5);
        p.setPen(Qt::black);
        p.drawText(QRectF(box.left() + 26, y, box.width() - 30, zeilenHoehe), Qt::AlignVCenter, eintrag.second);
        y += zeilenHoehe;
    }
    if (m_zeigeStationen) {
        QPolygonF d;
        const QPointF c(box.left() + 14, y + 8);
        d << c + QPointF(0, -6) << c + QPointF(5, 4) << c + QPointF(-5, 4);
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(QColor(80, 160, 230));
        p.drawPolygon(d);
        p.setPen(Qt::black);
        p.drawText(QRectF(box.left() + 26, y, box.width() - 30, zeilenHoehe), Qt::AlignVCenter,
                   KarteView::tr("Wetterstation"));
    }

    // Quellenangabe (Pflicht bei OSM-Kacheln) und Ladestatus
    double untenRand = height() - 4;
    if (m_hintergrund) {
        p.setFont(klein);
        const QString quelle = KarteView::tr("© OpenStreetMap-Mitwirkende");
        const QRectF qr(width() - p.fontMetrics().horizontalAdvance(quelle) - 12, height() - 18,
                        p.fontMetrics().horizontalAdvance(quelle) + 8, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 210));
        p.drawRect(qr);
        p.setPen(QColor(40, 40, 40));
        p.drawText(qr, Qt::AlignCenter, quelle);
        untenRand = qr.top() - 4;

        QString status = m_lader->status();
        if (m_tourStrasse && m_tourVerlauf.size() >= 2)
            status = (status.isEmpty() ? QString() : status + QStringLiteral(" · "))
                   + KarteView::tr("Route: OSRM, Daten © OpenStreetMap-Mitwirkende");
        if (!status.isEmpty()) {
            const QRectF sr(width() - p.fontMetrics().horizontalAdvance(status) - 18, 8,
                            p.fontMetrics().horizontalAdvance(status) + 10, 18);
            p.setPen(QColor(160, 160, 160));
            p.setBrush(QColor(255, 255, 255, 230));
            p.drawRoundedRect(sr, 3, 3);
            p.setPen(QColor(60, 60, 60));
            p.drawText(sr, Qt::AlignCenter, status);
        }
        p.setFont(font());
    }

    // Maßstab: Meter je Pixel hängen bei Mercator vom Breitengrad ab
    const double latMitte = breiteAusMercatorY(bildZuWelt(QPointF(width() / 2.0, height() / 2.0)).y());
    const double kmProPixel = 40075.016686 * std::cos(latMitte * kPi / 180.0) / skala();
    double km = 0.1;
    for (double k : {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0})
        if (k / kmProPixel <= 140)
            km = k;
    const double laenge = km / kmProPixel;
    const QPointF links(width() - laenge - 16, untenRand - 8);
    p.setPen(QPen(Qt::black, 2));
    p.drawLine(links, links + QPointF(laenge, 0));
    p.drawLine(links + QPointF(0, -4), links + QPointF(0, 4));
    p.drawLine(links + QPointF(laenge, -4), links + QPointF(laenge, 4));
    p.drawText(QRectF(links.x() - 10, links.y() - 20, laenge + 20, 16), Qt::AlignCenter,
               Anzeige::zahl(km, km < 1 ? 1 : 0) + QStringLiteral(" km"));
}

// ===================================================================
//  KarteView
// ===================================================================

KarteView::KarteView(const AppKontext& kontext, QWidget* parent, Modus modus)
    : QWidget(parent)
    , m_kontext(kontext)
{
    // Kachelordner im Datenordner, damit er mit abgegeben werden kann und
    // die Karte danach offline funktioniert; ersatzweise im Benutzer-Cache.
    QString ordner = QDir(m_kontext.daten->datenOrdner()).filePath(QStringLiteral("kartenkacheln"));
    QDir().mkpath(ordner);
    if (!QFileInfo(ordner).isWritable())
        ordner = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/kartenkacheln");
    m_lader = new KachelLader(ordner, this);

    m_flaeche = new KartenFlaeche(m_lader, this);
    m_flaeche->beiKlick = [this](const QString& id) { emit anlageGewaehlt(id); };

    m_faerbung = new QComboBox(this);
    m_faerbung->addItem(tr("Status"));
    m_faerbung->addItem(tr("Performance Ratio"));
    m_faerbung->addItem(tr("Auffälligkeiten"));
    m_faerbung->addItem(tr("Handlungsbedarf"));
    m_stationen = new QCheckBox(tr("Wetterstationen"), this);
    m_stationen->setChecked(true);
    auto* hintergrund = new QCheckBox(tr("Hintergrundkarte (OpenStreetMap)"), this);
    hintergrund->setChecked(true);
    hintergrund->setToolTip(tr("Kartenkacheln werden beim ersten Anzeigen aus dem Internet geladen und in\n%1\n"
                               "gespeichert; danach funktioniert die Karte auch offline.")
                                .arg(QDir::toNativeSeparators(ordner)));
    auto* online = new QCheckBox(tr("fehlende Kacheln herunterladen"), this);
    online->setChecked(true);
    online->setToolTip(tr("Aus: nur bereits gespeicherte Kacheln verwenden (vollständig offline)."));
    auto* zuruecksetzen = new QPushButton(tr("Ansicht zurücksetzen"), this);
    m_info = new QLabel(tr("Mausrad: Zoom · Ziehen: verschieben · Klick: Anlage öffnen"), this);
    m_info->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // bei schmalen Fenstern abschneiden
    m_info->setObjectName(QStringLiteral("Gedimmt"));

    auto* leistenWidget = new QWidget(this);
    auto* leiste = new QHBoxLayout(leistenWidget);
    leiste->setContentsMargins(0, 0, 0, 0);
    leiste->addWidget(new QLabel(tr("Färbung:"), this));
    leiste->addWidget(m_faerbung);
    leiste->addWidget(m_stationen);
    leiste->addWidget(hintergrund);
    leiste->addWidget(online);
    leiste->addWidget(zuruecksetzen);
    leiste->addStretch();
    leiste->addWidget(m_info);

    auto* v = new QVBoxLayout(this);
    v->addWidget(leistenWidget);
    v->addWidget(m_flaeche, 1);
    if (modus == Modus::Kompakt) {
        // Übersicht: nur die Karte, Fokus auf Handlungsbedarf
        leistenWidget->hide();
        v->setContentsMargins(0, 0, 0, 0);
        m_faerbung->setCurrentIndex(3);
        m_stationen->setChecked(false);
        m_flaeche->setStationenSichtbar(false);
    }

    connect(m_faerbung, &QComboBox::currentIndexChanged, this, &KarteView::aktualisiere);
    connect(m_stationen, &QCheckBox::toggled, this, [this](bool an) { m_flaeche->setStationenSichtbar(an); });
    connect(hintergrund, &QCheckBox::toggled, this, [this, online](bool an) {
        m_flaeche->setHintergrund(an);
        online->setEnabled(an);
    });
    connect(online, &QCheckBox::toggled, m_lader, &KachelLader::setOnlineErlaubt);
    connect(zuruecksetzen, &QPushButton::clicked, this, [this] { m_flaeche->ansichtZuruecksetzen(); });
    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &KarteView::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, &KarteView::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &KarteView::aktualisiere);

    aktualisiere();
}

void KarteView::aktualisiere()
{
    const int modus = m_faerbung->currentIndex();
    QVector<KartenFlaeche::Punkt> punkte;
    int ohneKoordinaten = 0;
    for (const Anlage& a : m_kontext.daten->anlagen()) {
        if (!a.hatGueltigeKoordinaten()) {
            ++ohneKoordinaten;
            continue;
        }
        const Kennzahl* k = m_kontext.analyse->kennzahl(a.anlageId);
        KartenFlaeche::Punkt p;
        p.id = a.anlageId;
        p.lat = a.lat;
        p.lon = a.lon;
        p.kwp = a.leistungKwp;
        p.tooltip = tr("%1 – %2, %3\n%4 kWp, %5").arg(a.anlageId, a.ort, Anzeige::typName(a.typ),
                                                      Anzeige::zahl(a.leistungKwp, 2), Anzeige::statusName(a.status));
        if (k && k->ok)
            p.tooltip += tr("\nPR %1").arg(Anzeige::pr(k->auswertung.pr));

        if (modus == 0) {
            p.farbe = Anzeige::statusFarbe(a.status);
        } else if (modus == 1) {
            p.farbe = (k && k->ok) ? Anzeige::prFarbe(k->auswertung.pr) : QColor(170, 170, 170);
        } else if (modus == 3) {
            // Handlungsbedarf: braucht keine Verlaufsdaten, gilt also für alle Anlagen
            const bool kritisch = m_kontext.daten->offeneKritischeStoerungen(a.anlageId) > 0;
            const bool fehlerbild = k && k->ok && k->auswertung.istAuffaellig();
            const bool offen = m_kontext.daten->offeneStoerungen(a.anlageId) > 0;
            if (kritisch || fehlerbild || a.status == QLatin1String("gestoert")) {
                p.farbe = QColor(207, 34, 46);
                p.rang = 3;
            } else if (offen || a.status == QLatin1String("wartung")) {
                p.farbe = QColor(230, 126, 34);
                p.rang = 2;
            } else if (a.status == QLatin1String("abgeschaltet")) {
                p.farbe = QColor(150, 150, 150);
                p.rang = 1;
            } else {
                p.farbe = QColor(46, 160, 67);
            }
        } else {
            if (!k)
                p.farbe = QColor(170, 170, 170);
            else if (!k->ok || k->auswertung.istAuffaellig())
                p.farbe = QColor(207, 34, 46);
            else if (m_kontext.daten->offeneStoerungen(a.anlageId) > 0)
                p.farbe = QColor(230, 126, 34);
            else
                p.farbe = QColor(46, 160, 67);
        }
        punkte.append(p);
    }

    QVector<KartenFlaeche::Station> stationen;
    for (const Wetterstation& w : m_kontext.daten->wetterstationen())
        stationen.append({w.stationId, w.name, w.lat, w.lon});

    QVector<QPair<QColor, QString>> legende;
    if (modus == 0) {
        for (const QString& s : Datenbasis::gueltigeStatus())
            legende.append({Anzeige::statusFarbe(s), Anzeige::statusName(s)});
    } else if (modus == 1) {
        legende = {{Anzeige::prFarbe(0.88), tr("PR ≥ 85 %")}, {Anzeige::prFarbe(0.72), tr("PR ≈ 70 %")},
                   {Anzeige::prFarbe(0.5), tr("PR ≤ 50 %")}, {QColor(170, 170, 170), tr("keine Daten")}};
    } else if (modus == 3) {
        legende = {{QColor(207, 34, 46), tr("dringend (kritisch, gestört, Fehlerbild)")},
                   {QColor(230, 126, 34), tr("offene Störung / Wartung")},
                   {QColor(46, 160, 67), tr("in Ordnung")}, {QColor(150, 150, 150), tr("abgeschaltet")}};
    } else {
        legende = {{QColor(207, 34, 46), tr("Fehlerbild erkannt")}, {QColor(230, 126, 34), tr("offene Störung")},
                   {QColor(46, 160, 67), tr("unauffällig")}, {QColor(170, 170, 170), tr("keine Daten")}};
    }
    m_flaeche->setDaten(punkte, stationen, legende);
    m_flaeche->setStationenSichtbar(m_stationen->isChecked());
    if (ohneKoordinaten > 0)
        m_info->setText(tr("%1 Anlagen ohne gültige Koordinaten nicht dargestellt · Mausrad: Zoom · Klick: öffnen")
                            .arg(ohneKoordinaten));
}

void KarteView::markiere(const QString& anlageId)
{
    m_flaeche->markiere(anlageId);
}

void KarteView::setTour(const QPointF& startLatLon, const QStringList& anlagenIds,
                        const QVector<QPointF>& verlauf, bool strassenroute, double umkreisKm)
{
    QVector<QPointF> stopps{startLatLon};
    for (const QString& id : anlagenIds)
        if (const Anlage* a = m_kontext.daten->anlage(id))
            stopps.append(QPointF(a->lat, a->lon));
    stopps.append(startLatLon);
    m_flaeche->setTour(stopps, verlauf, strassenroute, umkreisKm);
}
