#include "kachel_lader.h"

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

namespace {
constexpr int kMaxParallel = 2;           ///< OSM-Richtlinie: höchstens 2 Verbindungen
constexpr int kMaxFehlerInFolge = 6;      ///< danach Online-Zugriff pausieren
const char* kUserAgent = "SolarMonitor/2.0 (Hochschulprojekt, Qt-Desktopanwendung)";
} // namespace

KachelLader::KachelLader(const QString& kachelOrdner, QObject* parent)
    : QObject(parent)
    , m_ordner(kachelOrdner)
    , m_netz(new QNetworkAccessManager(this))
{
    m_speicher.setMaxCost(400); // Kacheln im Arbeitsspeicher (je ≈ 256 kB dekodiert)
}

KachelLader* KachelLader::gemeinsam(const QString& kachelOrdner)
{
    static QHash<QString, KachelLader*> lader;
    KachelLader*& l = lader[kachelOrdner];
    if (!l)
        l = new KachelLader(kachelOrdner, QCoreApplication::instance());
    return l;
}

QString KachelLader::schluessel(int z, int x, int y)
{
    return QStringLiteral("%1/%2/%3").arg(z).arg(x).arg(y);
}

QString KachelLader::dateiPfad(int z, int x, int y) const
{
    return QStringLiteral("%1/%2/%3/%4.png").arg(m_ordner).arg(z).arg(x).arg(y);
}

QPixmap KachelLader::ausSpeicherOderDatei(int z, int x, int y)
{
    const QString k = schluessel(z, x, y);
    if (QPixmap* p = m_speicher.object(k))
        return *p;
    // Fehlende Dateien merken: sonst fragt jedes Neuzeichnen die Festplatte
    // erneut ab (unter Windows spürbar langsam, die Karte ruckelt)
    if (m_nichtAufPlatte.contains(k))
        return QPixmap();
    const QString pfad = dateiPfad(z, x, y);
    QPixmap pm;
    if (pm.load(pfad, "PNG")) {
        m_speicher.insert(k, new QPixmap(pm));
        return pm;
    }
    if (QFileInfo::exists(pfad))
        QFile::remove(pfad); // beschädigte Datei beim nächsten Mal neu laden
    m_nichtAufPlatte.insert(k);
    return QPixmap();
}

QPixmap KachelLader::kachelOhneDownload(int z, int x, int y)
{
    return ausSpeicherOderDatei(z, x, y);
}

QPixmap KachelLader::kachel(int z, int x, int y)
{
    const QPixmap pm = ausSpeicherOderDatei(z, x, y);
    if (!pm.isNull())
        return pm;

    const QString k = schluessel(z, x, y);
    if (m_onlineErlaubt && !m_pausiert && !m_aktiv.contains(k) && !m_fehlgeschlagen.contains(k)
        && !m_warteschlange.contains(k)) {
        m_warteschlange.append(k);
        naechsteStarten();
    }
    return QPixmap();
}

void KachelLader::neueAnforderungsrunde()
{
    m_warteschlange.clear();
}

void KachelLader::setOnlineErlaubt(bool an)
{
    m_onlineErlaubt = an;
    if (!an)
        m_warteschlange.clear();
    emit statusGeaendert();
}

void KachelLader::erneutVersuchen()
{
    m_pausiert = false;
    m_fehlerInFolge = 0;
    m_fehlgeschlagen.clear();
    emit statusGeaendert();
}

void KachelLader::naechsteStarten()
{
    while (m_aktiv.size() < kMaxParallel && !m_warteschlange.isEmpty()) {
        const QString k = m_warteschlange.takeFirst();
        const QStringList teile = k.split(QLatin1Char('/'));
        const int z = teile.at(0).toInt(), x = teile.at(1).toInt(), y = teile.at(2).toInt();

        QNetworkRequest anfrage(QUrl(QStringLiteral("https://tile.openstreetmap.org/%1.png").arg(k)));
        anfrage.setHeader(QNetworkRequest::UserAgentHeader, QByteArray(kUserAgent));
        anfrage.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        anfrage.setTransferTimeout(15000);

        m_aktiv.insert(k);
        QNetworkReply* antwort = m_netz->get(anfrage);
        connect(antwort, &QNetworkReply::finished, this, [=] { fertig(antwort, k, z, x, y); });
    }
}

void KachelLader::fertig(QNetworkReply* antwort, const QString& k, int z, int x, int y)
{
    antwort->deleteLater();
    m_aktiv.remove(k);

    const QByteArray daten = antwort->readAll();
    QPixmap pm;
    if (antwort->error() == QNetworkReply::NoError && pm.loadFromData(daten, "PNG")) {
        m_fehlerInFolge = 0;
        ++m_geladen;
        m_nichtAufPlatte.remove(k);
        m_speicher.insert(k, new QPixmap(pm));

        // dauerhaft ablegen (atomar über QSaveFile)
        QDir().mkpath(QFileInfo(dateiPfad(z, x, y)).absolutePath());
        QSaveFile datei(dateiPfad(z, x, y));
        if (datei.open(QIODevice::WriteOnly)) {
            datei.write(daten);
            datei.commit();
        }
        emit kachelGeladen();
    } else {
        m_fehlgeschlagen.insert(k);
        if (++m_fehlerInFolge >= kMaxFehlerInFolge) {
            m_pausiert = true;
            m_warteschlange.clear();
        }
    }
    emit statusGeaendert();
    naechsteStarten();
}

QString KachelLader::status() const
{
    if (!m_onlineErlaubt)
        return tr("Hintergrundkarte: nur gespeicherte Kacheln (offline)");
    if (m_pausiert)
        return tr("Hintergrundkarte: Kartenserver nicht erreichbar – es werden nur gespeicherte Kacheln angezeigt");
    if (!m_aktiv.isEmpty() || !m_warteschlange.isEmpty())
        return tr("Hintergrundkarte wird geladen …");
    return QString();
}
