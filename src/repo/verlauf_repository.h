#pragma once

#include <list>
#include <memory>

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "model/anlage.h"
#include "model/pruefmeldung.h"
#include "model/verlauf.h"

/// Ergebnis eines Ladevorgangs (Erfolg: verlauf != nullptr).
struct VerlaufLadeErgebnis {
    std::shared_ptr<const Verlauf> verlauf;
    QString     fehler;   ///< Grund, warum der Verlauf unbrauchbar ist
    QStringList hinweise; ///< nicht fatale Auffälligkeiten (z. B. Nicht-Zahlen)

    bool ok() const { return verlauf != nullptr; }
};

/**
 * @brief Index der Verlaufsdateien und Lazy Loading (Pflichtpunkt 5).
 *
 * - Index: master_verlauf_index.csv (Pfade relativ zum Datenordner).
 *   Fehlt die Datei, werden ersatzweise die verlauf_index.csv der
 *   Batch-Ordner gelesen (Pfade relativ zum Batch-Ordner). Einträge der
 *   Batch-Indizes, die im Master fehlen, werden ergänzt.
 * - Lazy Loading: eine JSON wird erst geladen, wenn sie gebraucht wird,
 *   und zwar mit QtConcurrent in einem Pool-Thread; das Ergebnis kommt
 *   über das Signal @ref verlaufGeladen im GUI-Thread an.
 * - LRU-Cache: die zuletzt geladenen Verläufe (Standard 20) bleiben
 *   als std::shared_ptr<const Verlauf> im Speicher. Weil Cache und
 *   Watcher nur im GUI-Thread angefasst werden, ist kein Mutex nötig.
 */
class VerlaufRepository : public QObject {
    Q_OBJECT
public:
    /// Woher die Zeitreihen stammen: Verlaufsdateien (CSV-Ordner) oder der Live-Generator.
    enum class Quelle { Csv, Live };

    explicit VerlaufRepository(QObject* parent = nullptr);
    ~VerlaufRepository() override;

    Quelle quelle() const { return m_quelle; }
    /// Wechselt die Quelle, merkt sie sich und leert den Cache. Laufende Ladevorgänge der alten Quelle werden verworfen.
    void setQuelle(Quelle quelle);
    /// Stammdaten für den Live-Generator (Anlagen mit installierter Leistung).
    void setLiveAnlagen(const QVector<Anlage>& anlagen);

    /// Liest den Index; Auffälligkeiten stehen danach in @ref indexMeldungen.
    void ladeIndex(const QString& datenOrdner);
    const QVector<Pruefmeldung>& indexMeldungen() const { return m_meldungen; }

    bool hatVerlauf(const QString& anlageId) const;
    /// Im Index eingetragen (auch wenn die Datei fehlt)?
    bool istEingetragen(const QString& anlageId) const
    {
        return m_index.contains(anlageId) || m_fehlend.contains(anlageId);
    }
    /// Erklärung, warum kein Verlauf verfügbar ist (leer, wenn verfügbar).
    QString fehlgrund(const QString& anlageId) const;
    /// Pfad der Verlaufsdatei (nur Quelle CSV; sonst leer).
    QString pfadFuer(const QString& anlageId) const;
    QStringList anlagenMitVerlauf() const;
    int anzahlIndexEintraege() const { return int(m_index.size()); }

    /// Verlauf aus dem Cache (nullptr, wenn nicht geladen). Aktualisiert die LRU-Reihenfolge.
    std::shared_ptr<const Verlauf> ausCache(const QString& anlageId);

    /**
     * @brief Asynchron laden. Liegt der Verlauf im Cache, kommt das Signal
     *        sofort; läuft bereits ein Ladevorgang, wird keiner doppelt gestartet.
     *        Quelle Live: erzeugt den Verlauf sofort (Signal kommt im nächsten Takt).
     */
    void ladeAsync(const QString& anlageId);

    /// Verlauf der aktuellen Quelle, ohne Cache und ohne Signal (für Berichte).
    VerlaufLadeErgebnis ladeSofort(const QString& anlageId) const;

    /// Erzeugt einen Live-Verlauf zu einer Anlage (thread-sicher, ohne Zustand).
    static VerlaufLadeErgebnis erzeugeLive(const Anlage& anlage);

    void setCacheGroesse(int anzahl);
    int cacheGroesse() const { return m_cacheMax; }
    int cacheBelegung() const { return int(m_cache.size()); }

    /**
     * @brief Liest und prüft eine Verlaufsdatei (thread-sicher, ohne Zustand).
     * @param erwarteteId anlage_id, die in der Datei stehen muss (leer = egal)
     */
    static VerlaufLadeErgebnis lese(const QString& pfad, const QString& erwarteteId);

signals:
    void verlaufGeladen(const QString& anlageId, const VerlaufLadeErgebnis& ergebnis);

private:
    void ladeBatchIndex(const QString& batchOrdner, bool nurErgaenzen);
    void inCache(const QString& anlageId, std::shared_ptr<const Verlauf> verlauf);

    QString                 m_datenOrdner;
    QHash<QString, QString> m_index;        ///< anlageId -> absoluter Pfad (Datei existiert)
    QHash<QString, QString> m_fehlend;      ///< anlageId -> Pfad, Datei fehlt
    QVector<Pruefmeldung>   m_meldungen;

    QHash<QString, std::shared_ptr<const Verlauf>> m_cache;
    std::list<QString>      m_lru;          ///< vorne = zuletzt benutzt
    int                     m_cacheMax = 20;

    QHash<QString, QFutureWatcher<VerlaufLadeErgebnis>*> m_laufend;

    Quelle                  m_quelle = Quelle::Csv;
    QHash<QString, Anlage>  m_live;         ///< Stammdaten für den Live-Generator
};
