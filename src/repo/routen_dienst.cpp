#include "routen_dienst.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

namespace {
const char* kUserAgent = "SolarMonitor/2.0 (Hochschulprojekt, Qt-Desktopanwendung)";
/// Öffentlicher OSRM-Demoserver; per Umgebungsvariable durch einen eigenen ersetzbar.
QString server()
{
    return qEnvironmentVariable("SOLARMONITOR_OSRM_URL", QStringLiteral("https://router.project-osrm.org"));
}
} // namespace

RoutenDienst::RoutenDienst(const QString& cacheOrdner, QObject* parent)
    : QObject(parent)
    , m_cacheOrdner(cacheOrdner)
    , m_netz(new QNetworkAccessManager(this))
{
}

QString RoutenDienst::cachePfad(const QString& url) const
{
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(m_cacheOrdner).filePath(QString::fromLatin1(hash) + QStringLiteral(".json"));
}

void RoutenDienst::berechne(const TourStopp& start, const QVector<TourStopp>& ziele)
{
    const int nr = ++m_anfrageNr;
    if (ziele.isEmpty()) {
        StrassenRoute r;
        r.fehler = tr("Keine Ziele.");
        emit routeBerechnet(r);
        return;
    }

    // Koordinaten im OSRM-Format lon,lat; Start zuerst
    QStringList punkte{QStringLiteral("%1,%2").arg(start.lon, 0, 'f', 5).arg(start.lat, 0, 'f', 5)};
    for (const TourStopp& z : ziele)
        punkte.append(QStringLiteral("%1,%2").arg(z.lon, 0, 'f', 5).arg(z.lat, 0, 'f', 5));
    // Fähren ausschließen: Die Tour soll nicht über den See führen (passend zum
    // Umkreis, der ebenfalls um den See herum gemessen wird).
    const QString basis = QStringLiteral("%1/trip/v1/driving/%2?roundtrip=true&source=first"
                                         "&geometries=geojson&overview=full")
                              .arg(server(), punkte.join(QLatin1Char(';')));
    const QString url = basis + QStringLiteral("&exclude=ferry");

    // 1) Cache: identische Tour wurde schon einmal berechnet
    for (const QString& u : {url, basis}) {
        QFile cache(cachePfad(u));
        if (cache.open(QIODevice::ReadOnly)) {
            StrassenRoute r = auswerten(cache.readAll(), int(ziele.size()));
            if (r.ok) {
                r.ausCache = true;
                emit routeBerechnet(r);
                return;
            }
        }
    }

    // 2) Online anfragen
    frage(url, basis, nr, int(ziele.size()));
}

void RoutenDienst::frage(const QString& url, const QString& ersatzUrl, int nr, int anzahl)
{
    QNetworkRequest anfrage{QUrl(url)};
    anfrage.setHeader(QNetworkRequest::UserAgentHeader, QByteArray(kUserAgent));
    anfrage.setTransferTimeout(20000);
    QNetworkReply* antwort = m_netz->get(anfrage);
    connect(antwort, &QNetworkReply::finished, this, [this, antwort, url, ersatzUrl, nr, anzahl] {
        antwort->deleteLater();
        if (nr != m_anfrageNr)
            return; // inzwischen wurde eine neuere Tour angefordert
        const int http = antwort->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray daten = antwort->readAll();
        StrassenRoute r;
        if (http == 400 && !ersatzUrl.isEmpty()) {
            // Server kennt "exclude=ferry" nicht (anderes Profil) -> ohne Ausschluss erneut
            frage(ersatzUrl, QString(), nr, anzahl);
            return;
        }
        if (antwort->error() != QNetworkReply::NoError) {
            r.fehler = tr("Routingdienst nicht erreichbar (%1).").arg(antwort->errorString());
        } else {
            r = auswerten(daten, anzahl);
            if (r.ok) {
                QDir().mkpath(m_cacheOrdner);
                QSaveFile datei(cachePfad(url));
                if (datei.open(QIODevice::WriteOnly)) {
                    datei.write(daten);
                    datei.commit();
                }
            }
        }
        emit routeBerechnet(r);
    });
}

StrassenRoute RoutenDienst::auswerten(const QByteArray& json, int anzahlZiele)
{
    StrassenRoute r;
    QJsonParseError pe;
    const QJsonObject root = QJsonDocument::fromJson(json, &pe).object();
    if (pe.error != QJsonParseError::NoError) {
        r.fehler = tr("Ungültige Antwort des Routingdienstes.");
        return r;
    }
    if (root.value(QStringLiteral("code")).toString() != QLatin1String("Ok")) {
        r.fehler = tr("Routingdienst meldet: %1")
                       .arg(root.value(QStringLiteral("message")).toString(root.value(QStringLiteral("code")).toString()));
        return r;
    }

    const QJsonArray wegpunkte = root.value(QStringLiteral("waypoints")).toArray();
    const QJsonArray trips = root.value(QStringLiteral("trips")).toArray();
    if (wegpunkte.size() != anzahlZiele + 1 || trips.isEmpty()) {
        r.fehler = tr("Unvollständige Antwort des Routingdienstes.");
        return r;
    }

    // waypoint_index = Position des Eingabepunkts in der optimierten Tour
    QVector<QPair<int, int>> position; // (Tourposition, Zielindex)
    for (int i = 1; i < wegpunkte.size(); ++i)
        position.append({wegpunkte.at(i).toObject().value(QStringLiteral("waypoint_index")).toInt(), i - 1});
    std::sort(position.begin(), position.end());
    for (const auto& p : position)
        r.reihenfolge.append(p.second);

    const QJsonObject trip = trips.first().toObject();
    for (const QJsonValue& leg : trip.value(QStringLiteral("legs")).toArray()) {
        r.etappenKm.append(leg.toObject().value(QStringLiteral("distance")).toDouble() / 1000.0);
        r.etappenMin.append(leg.toObject().value(QStringLiteral("duration")).toDouble() / 60.0);
    }
    r.gesamtKm = trip.value(QStringLiteral("distance")).toDouble() / 1000.0;
    r.gesamtMin = trip.value(QStringLiteral("duration")).toDouble() / 60.0;
    const QJsonArray koordinaten =
        trip.value(QStringLiteral("geometry")).toObject().value(QStringLiteral("coordinates")).toArray();
    for (const QJsonValue& k : koordinaten) {
        const QJsonArray lonLat = k.toArray();
        if (lonLat.size() >= 2)
            r.verlauf.append(QPointF(lonLat.at(1).toDouble(), lonLat.at(0).toDouble()));
    }
    if (r.etappenKm.size() != anzahlZiele + 1 || r.verlauf.size() < 2) {
        r.fehler = tr("Unvollständige Antwort des Routingdienstes.");
        return r;
    }
    r.ok = true;
    return r;
}
