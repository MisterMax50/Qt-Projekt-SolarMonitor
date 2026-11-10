#pragma once

#include <QCache>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief Lädt OpenStreetMap-Kartenkacheln und speichert sie dauerhaft.
 *
 * - Kacheln werden zuerst im Speicher (QCache), dann im Kachelordner auf
 *   der Festplatte gesucht ({ordner}/{z}/{x}/{y}.png). Nur wenn beides
 *   fehlt und Online-Zugriff erlaubt ist, wird die Kachel von
 *   tile.openstreetmap.org geladen und abgelegt.
 * - Einmal angesehene Bereiche funktionieren danach vollständig offline.
 *   Liegt der Kachelordner im Datenordner, kann er mit abgegeben werden.
 * - Nutzungsrichtlinie von OSM: eindeutiger User-Agent, höchstens zwei
 *   gleichzeitige Downloads, kein Vorab-Massendownload, Quellenangabe
 *   "© OpenStreetMap-Mitwirkende" auf der Karte.
 * - Fehlgeschlagene Kacheln werden nicht endlos neu angefragt; nach
 *   mehreren Fehlern in Folge wird der Online-Zugriff pausiert
 *   (z. B. ohne Internet), bis die Ansicht zurückgesetzt wird.
 */
class KachelLader : public QObject {
    Q_OBJECT
public:
    explicit KachelLader(const QString& kachelOrdner, QObject* parent = nullptr);

    /// Gemeinsamer Lader je Kachelordner: alle Karten (Übersicht, Kartenseite)
    /// teilen sich Arbeitsspeicher-Cache und Downloads.
    static KachelLader* gemeinsam(const QString& kachelOrdner);

    /// Kachel liefern, falls vorhanden; sonst Download anstoßen und Null-Pixmap.
    QPixmap kachel(int z, int x, int y);
    /// Nur aus Speicher/Festplatte (für Ersatzdarstellung, löst keinen Download aus).
    QPixmap kachelOhneDownload(int z, int x, int y);

    /// Warteschlange leeren (vor jedem Neuzeichnen, damit nur Sichtbares geladen wird).
    void neueAnforderungsrunde();

    void setOnlineErlaubt(bool an);
    bool onlineErlaubt() const { return m_onlineErlaubt; }
    /// Fehlerzähler zurücksetzen und Online-Zugriff erneut versuchen.
    void erneutVersuchen();

    QString kachelOrdner() const { return m_ordner; }
    QString status() const;

signals:
    void kachelGeladen();
    void statusGeaendert();

private:
    static QString schluessel(int z, int x, int y);
    QString dateiPfad(int z, int x, int y) const;
    QPixmap ausSpeicherOderDatei(int z, int x, int y);
    void naechsteStarten();
    void fertig(QNetworkReply* antwort, const QString& schluessel, int z, int x, int y);

    QString                m_ordner;
    QNetworkAccessManager* m_netz = nullptr;
    QCache<QString, QPixmap> m_speicher;
    QStringList            m_warteschlange;
    QSet<QString>          m_aktiv;
    QSet<QString>          m_fehlgeschlagen;
    QSet<QString>          m_nichtAufPlatte;   ///< bekannt fehlende Dateien (spart Festplattenzugriffe)
    bool m_onlineErlaubt = true;
    bool m_pausiert = false;
    int  m_fehlerInFolge = 0;
    int  m_geladen = 0;
};
