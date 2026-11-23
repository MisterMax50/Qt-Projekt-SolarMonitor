#include "verlauf_repository.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

#include "repo/csv_parser.h"
#include "util/zeit.h"

namespace {

const QString kMasterIndex = QStringLiteral("master_verlauf_index.csv");
const QString kBatchIndex  = QStringLiteral("verlauf_index.csv");

/// Zahlenarray lesen; null -> NaN (Lücke), Nicht-Zahlen -> NaN + Zähler.
QVector<double> zahlenArray(const QJsonArray& arr, int* nichtZahlen)
{
    QVector<double> erg;
    erg.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (v.isDouble()) {
            erg.append(v.toDouble());
        } else {
            if (!v.isNull())
                ++*nichtZahlen;
            erg.append(Verlauf::luecke());
        }
    }
    return erg;
}

} // namespace

VerlaufRepository::VerlaufRepository(QObject* parent)
    : QObject(parent)
{
}

VerlaufRepository::~VerlaufRepository()
{
    // Laufende Hintergrundjobs abwarten, damit kein Thread auf ein
    // zerstörtes Objekt zurückmeldet.
    for (auto* w : std::as_const(m_laufend)) {
        w->disconnect(this);
        w->waitForFinished();
    }
}

// -------------------------------------------------------------------
//  Index
// -------------------------------------------------------------------

void VerlaufRepository::ladeIndex(const QString& datenOrdner)
{
    m_datenOrdner = datenOrdner;
    m_index.clear();
    m_fehlend.clear();
    m_meldungen.clear();

    const QDir dir(datenOrdner);
    const QStringList batches = dir.entryList({QStringLiteral("anlagen_verlauf_batch_*")},
                                              QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    bool masterOk = false;
    // Ordner fehlt komplett: bei vielen Einträgen eine Sammelmeldung statt
    // hunderter Einzelwarnungen (z. B. wenn nur ein Batch mitgeliefert wird).
    QHash<QString, QVector<Pruefmeldung>> fehlendeOrdner;
    try {
        const CsvReader::Tabelle t = CsvReader::lade(dir.filePath(kMasterIndex),
                                                     {QStringLiteral("anlage_id"), QStringLiteral("pfad")});
        for (const auto& z : t.zeilen) {
            const QString id = t.wert(z, QStringLiteral("anlage_id"));
            const QString rel = t.wert(z, QStringLiteral("pfad"));
            if (id.isEmpty() || rel.isEmpty()) {
                m_meldungen.append({Pruefmeldung::Stufe::Fehler, kMasterIndex, z.zeilenNr, id,
                                    tr("Indexzeile ohne anlage_id oder pfad übersprungen.")});
                continue;
            }
            if (m_index.contains(id) || m_fehlend.contains(id)) {
                m_meldungen.append({Pruefmeldung::Stufe::Warnung, kMasterIndex, z.zeilenNr, id,
                                    tr("Doppelter Indexeintrag ignoriert.")});
                continue;
            }
            const QString pfad = QDir::cleanPath(dir.filePath(rel));
            if (QFileInfo::exists(pfad)) {
                m_index.insert(id, pfad);
            } else {
                m_fehlend.insert(id, pfad);
                const QString ordner = QFileInfo(pfad).absolutePath();
                const Pruefmeldung m{Pruefmeldung::Stufe::Warnung, kMasterIndex, z.zeilenNr, id,
                                     tr("Verlaufsdatei %1 fehlt.").arg(rel)};
                if (QFileInfo::exists(ordner))
                    m_meldungen.append(m);
                else
                    fehlendeOrdner[QDir(datenOrdner).relativeFilePath(ordner)].append(m);
            }
        }
        masterOk = true;
        for (auto it = fehlendeOrdner.cbegin(); it != fehlendeOrdner.cend(); ++it) {
            if (it.value().size() < 10)
                m_meldungen += it.value();
            else
                m_meldungen.append({Pruefmeldung::Stufe::Info, kMasterIndex, 0, QString(),
                                    tr("Ordner %1 ist nicht vorhanden – %2 Anlagen ohne Verlauf.")
                                        .arg(it.key())
                                        .arg(it.value().size())});
        }
    } catch (const DateiFehler& e) {
        m_meldungen.append({Pruefmeldung::Stufe::Warnung, kMasterIndex, 0, QString(),
                            tr("%1 – Es werden die Indexdateien der Batch-Ordner verwendet.")
                                .arg(e.meldung())});
    }

    // Batch-Indizes: als Ersatz (Master fehlt) oder zur Ergänzung.
    for (const QString& b : batches)
        ladeBatchIndex(dir.filePath(b), masterOk);

    if (m_index.isEmpty())
        m_meldungen.append({Pruefmeldung::Stufe::Fehler, kMasterIndex, 0, QString(),
                            tr("Keine Verlaufsdateien gefunden – Verlaufsanzeige und "
                                     "Auswertung sind nicht verfügbar.")});
}

void VerlaufRepository::ladeBatchIndex(const QString& batchOrdner, bool nurErgaenzen)
{
    const QDir bdir(batchOrdner);
    const QString name = bdir.dirName() + QLatin1Char('/') + kBatchIndex;
    CsvReader::Tabelle t;
    try {
        t = CsvReader::lade(bdir.filePath(kBatchIndex),
                            {QStringLiteral("anlage_id"), QStringLiteral("pfad")});
    } catch (const DateiFehler& e) {
        m_meldungen.append({Pruefmeldung::Stufe::Warnung, name, 0, QString(), e.meldung()});
        return;
    }

    for (const auto& z : t.zeilen) {
        const QString id = t.wert(z, QStringLiteral("anlage_id"));
        const QString rel = t.wert(z, QStringLiteral("pfad"));
        if (id.isEmpty() || rel.isEmpty() || m_index.contains(id))
            continue;
        const QString pfad = QDir::cleanPath(bdir.filePath(rel));

        // Master-Pfad falsch, Batch-Pfad gültig -> reparieren
        if (m_fehlend.contains(id)) {
            if (!QFileInfo::exists(pfad))
                continue;
            m_fehlend.remove(id);
            m_index.insert(id, pfad);
            m_meldungen.erase(std::remove_if(m_meldungen.begin(), m_meldungen.end(),
                                             [&](const Pruefmeldung& m) {
                                                 return m.objektId == id && m.datei == kMasterIndex;
                                             }),
                              m_meldungen.end());
            m_meldungen.append({Pruefmeldung::Stufe::Info, name, z.zeilenNr, id,
                                tr("Pfad im Master-Index existiert nicht; Pfad aus dem Batch-Index verwendet.")});
            continue;
        }
        if (!QFileInfo::exists(pfad)) {
            m_fehlend.insert(id, pfad);
            if (!nurErgaenzen)
                m_meldungen.append({Pruefmeldung::Stufe::Warnung, name, z.zeilenNr, id,
                                    tr("Verlaufsdatei %1 fehlt.").arg(rel)});
            continue;
        }
        m_index.insert(id, pfad);
        if (nurErgaenzen)
            m_meldungen.append({Pruefmeldung::Stufe::Info, name, z.zeilenNr, id,
                                tr("Fehlte im Master-Index, aus dem Batch-Index ergänzt.")});
    }
}

bool VerlaufRepository::hatVerlauf(const QString& anlageId) const
{
    return m_index.contains(anlageId);
}

QString VerlaufRepository::fehlgrund(const QString& anlageId) const
{
    if (m_index.contains(anlageId))
        return QString();
    if (m_fehlend.contains(anlageId))
        return tr("Die Verlaufsdatei ist im Index eingetragen, aber nicht vorhanden:\n%1")
            .arg(QDir::toNativeSeparators(m_fehlend.value(anlageId)));
    return tr("Für diese Anlage gibt es keinen Eintrag in der Verlaufs-Indexdatei.");
}

QString VerlaufRepository::pfadFuer(const QString& anlageId) const
{
    return m_index.value(anlageId);
}

// -------------------------------------------------------------------
//  Cache und asynchrones Laden
// -------------------------------------------------------------------

std::shared_ptr<const Verlauf> VerlaufRepository::ausCache(const QString& anlageId)
{
    const auto it = m_cache.constFind(anlageId);
    if (it == m_cache.constEnd())
        return nullptr;
    m_lru.remove(anlageId);
    m_lru.push_front(anlageId);
    return it.value();
}

void VerlaufRepository::inCache(const QString& anlageId, std::shared_ptr<const Verlauf> verlauf)
{
    m_lru.remove(anlageId);
    m_lru.push_front(anlageId);
    m_cache.insert(anlageId, std::move(verlauf));
    while (int(m_lru.size()) > m_cacheMax) {
        m_cache.remove(m_lru.back());
        m_lru.pop_back();
    }
}

void VerlaufRepository::setCacheGroesse(int anzahl)
{
    m_cacheMax = qMax(1, anzahl);
    while (int(m_lru.size()) > m_cacheMax) {
        m_cache.remove(m_lru.back());
        m_lru.pop_back();
    }
}

void VerlaufRepository::ladeAsync(const QString& anlageId)
{
    if (auto v = ausCache(anlageId)) {
        VerlaufLadeErgebnis e;
        e.verlauf = std::move(v);
        emit verlaufGeladen(anlageId, e);
        return;
    }
    if (!m_index.contains(anlageId)) {
        VerlaufLadeErgebnis e;
        e.fehler = fehlgrund(anlageId);
        emit verlaufGeladen(anlageId, e);
        return;
    }
    if (m_laufend.contains(anlageId))
        return; // Ergebnis kommt ohnehin

    auto* watcher = new QFutureWatcher<VerlaufLadeErgebnis>(this);
    m_laufend.insert(anlageId, watcher);
    connect(watcher, &QFutureWatcher<VerlaufLadeErgebnis>::finished, this, [this, anlageId, watcher]() {
        m_laufend.remove(anlageId);
        const VerlaufLadeErgebnis e = watcher->result();
        watcher->deleteLater();
        if (e.ok())
            inCache(anlageId, e.verlauf);
        emit verlaufGeladen(anlageId, e);
    });
    watcher->setFuture(QtConcurrent::run(&VerlaufRepository::lese, m_index.value(anlageId), anlageId));
}

// -------------------------------------------------------------------
//  JSON lesen und prüfen
// -------------------------------------------------------------------

VerlaufLadeErgebnis VerlaufRepository::lese(const QString& pfad, const QString& erwarteteId)
{
    VerlaufLadeErgebnis erg;
    const QString name = QFileInfo(pfad).fileName();

    QFile datei(pfad);
    if (!datei.open(QIODevice::ReadOnly)) {
        erg.fehler = tr("%1 konnte nicht geöffnet werden: %2").arg(name, datei.errorString());
        return erg;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(datei.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        erg.fehler = tr("%1 ist kein gültiges JSON (%2 bei Zeichen %3) – "
                              "die Datei ist vermutlich beschädigt oder abgeschnitten.")
                         .arg(name, pe.errorString())
                         .arg(pe.offset);
        return erg;
    }
    if (!doc.isObject()) {
        erg.fehler = tr("%1 enthält kein JSON-Objekt.").arg(name);
        return erg;
    }

    const QJsonObject root = doc.object();
    const QJsonObject anlageObj = root.value(QStringLiteral("anlage")).toObject();
    const QJsonObject vObj = root.value(QStringLiteral("verlauf")).toObject();

    auto v = std::make_shared<Verlauf>();
    v->anlageId = anlageObj.value(QStringLiteral("anlage_id")).toString().trimmed();
    if (!erwarteteId.isEmpty() && !v->anlageId.isEmpty() && v->anlageId != erwarteteId) {
        erg.fehler = tr("%1 gehört zu Anlage %2, erwartet wurde %3.")
                         .arg(name, v->anlageId, erwarteteId);
        return erg;
    }
    if (v->anlageId.isEmpty())
        v->anlageId = erwarteteId;

    if (vObj.isEmpty()) {
        erg.fehler = tr("%1 enthält keinen Block \"verlauf\".").arg(name);
        return erg;
    }

    const QStringList pflicht{QStringLiteral("zeit"), QStringLiteral("leistung_w"),
                              QStringLiteral("einstrahlung_wm2"), QStringLiteral("modultemperatur_c"),
                              QStringLiteral("wr_status")};
    QStringList fehlend;
    for (const QString& k : pflicht)
        if (!vObj.value(k).isArray())
            fehlend.append(k);
    if (!fehlend.isEmpty()) {
        erg.fehler = tr("%1: Arrays fehlen: %2").arg(name, fehlend.join(QStringLiteral(", ")));
        return erg;
    }

    v->intervallMin = vObj.value(QStringLiteral("intervall_min")).toInt(15);
    if (v->intervallMin <= 0)
        v->intervallMin = 15;

    // Zeitstempel
    int ungueltigeZeiten = 0;
    for (const QJsonValue& z : vObj.value(QStringLiteral("zeit")).toArray()) {
        const QDateTime dt = Zeit::parse(z.toString());
        if (!dt.isValid())
            ++ungueltigeZeiten;
        v->zeit.append(dt);
    }
    if (ungueltigeZeiten > 0) {
        erg.fehler = tr("%1: %2 ungültige Zeitstempel.").arg(name).arg(ungueltigeZeiten);
        return erg;
    }

    int nichtZahlen = 0;
    v->leistungW        = zahlenArray(vObj.value(QStringLiteral("leistung_w")).toArray(), &nichtZahlen);
    v->einstrahlungWm2  = zahlenArray(vObj.value(QStringLiteral("einstrahlung_wm2")).toArray(), &nichtZahlen);
    v->modultemperaturC = zahlenArray(vObj.value(QStringLiteral("modultemperatur_c")).toArray(), &nichtZahlen);

    static const QSet<QString> kStatus{QStringLiteral("ok"), QStringLiteral("standby"),
                                       QStringLiteral("stoerung"), QStringLiteral("aus"),
                                       QStringLiteral("keine_daten")};
    int unbekannterStatus = 0;
    for (const QJsonValue& s : vObj.value(QStringLiteral("wr_status")).toArray()) {
        const QString st = s.toString();
        if (!kStatus.contains(st))
            ++unbekannterStatus;
        v->wrStatus.append(st);
    }

    // Negative Einstrahlung ist physikalisch unmöglich -> als Lücke behandeln.
    int negativ = 0;
    for (double& g : v->einstrahlungWm2) {
        if (!Verlauf::istLuecke(g) && g < 0.0) {
            g = Verlauf::luecke();
            ++negativ;
        }
    }

    if (!v->istKonsistent()) {
        erg.fehler = tr("%1: Arrays unterschiedlich lang (zeit %2, leistung %3, "
                              "einstrahlung %4, temperatur %5, status %6).")
                         .arg(name)
                         .arg(v->zeit.size())
                         .arg(v->leistungW.size())
                         .arg(v->einstrahlungWm2.size())
                         .arg(v->modultemperaturC.size())
                         .arg(v->wrStatus.size());
        return erg;
    }

    const int anzahl = vObj.value(QStringLiteral("anzahl")).toInt(-1);
    if (anzahl >= 0 && anzahl != v->anzahl())
        erg.hinweise.append(tr("Feld \"anzahl\" (%1) passt nicht zur Arraylänge (%2).")
                                .arg(anzahl)
                                .arg(v->anzahl()));
    if (nichtZahlen > 0)
        erg.hinweise.append(tr("%1 nicht numerische Messwerte als Lücke behandelt.").arg(nichtZahlen));
    if (negativ > 0)
        erg.hinweise.append(tr("%1 negative Einstrahlungswerte als Lücke behandelt.").arg(negativ));
    if (unbekannterStatus > 0)
        erg.hinweise.append(tr("%1 unbekannte Wechselrichter-Statuswerte.").arg(unbekannterStatus));
    for (int i = 1; i < v->anzahl(); ++i) {
        if (v->zeit.at(i) <= v->zeit.at(i - 1)) {
            erg.hinweise.append(tr("Zeitstempel sind nicht aufsteigend sortiert."));
            break;
        }
    }

    erg.verlauf = std::move(v);
    return erg;
}
