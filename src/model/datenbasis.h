#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "model/anlage.h"
#include "model/pruefmeldung.h"
#include "model/stoerung.h"
#include "model/wechselrichter.h"
#include "model/wetterstation.h"

/**
 * @brief Zentrale Datenhaltung der globalen CSV-Daten (Pflichtpunkt 1).
 *
 * Lädt wetterstationen.csv, wechselrichter.csv, anlagen.csv und
 * stoerungen.csv (in dieser Reihenfolge, damit Verweise geprüft werden
 * können), führt Plausibilitätsprüfungen durch und sammelt dabei
 * strukturierte @ref Pruefmeldung "Prüfmeldungen".
 *
 * Fehlerhafte Zeilen werden je nach Schwere korrigiert, markiert oder
 * übersprungen; fehlende Dateien führen nur bei anlagen.csv zum Abbruch
 * (Degradationsverhalten, Pflichtpunkt 7).
 *
 * Für schnelle Zugriffe werden Hash-Indizes gepflegt (ID -> Position,
 * offene Störungen je Anlage). Änderungen laufen ausschließlich über die
 * Methoden dieser Klasse und werden per Signal gemeldet.
 */
class Datenbasis : public QObject {
    Q_OBJECT
public:
    explicit Datenbasis(QObject* parent = nullptr);

    /**
     * @brief Lädt alle globalen CSV-Dateien aus @p datenOrdner.
     * @return true, wenn mindestens eine Anlage geladen wurde
     */
    bool lade(const QString& datenOrdner);
    QString datenOrdner() const { return m_datenOrdner; }

    // ---------- Lesezugriff ----------
    const QVector<Anlage>& anlagen() const { return m_anlagen; }
    const Anlage* anlage(const QString& anlageId) const;
    const Wechselrichter* wechselrichter(const QString& id) const;
    const Wetterstation* wetterstation(const QString& id) const;
    QVector<Wechselrichter> wechselrichterListe() const; ///< nach ID sortiert
    QVector<Wetterstation> wetterstationen() const;     ///< nach ID sortiert
    QStringList orte() const;                           ///< alphabetisch

    const QVector<Stoerung>& stoerungen() const { return m_stoerungen; }
    const Stoerung* stoerung(const QString& stoerungId) const;
    QVector<Stoerung> stoerungenVon(const QString& anlageId) const;
    int offeneStoerungen(const QString& anlageId) const { return m_offen.value(anlageId); }
    int offeneKritischeStoerungen(const QString& anlageId) const { return m_offenKritisch.value(anlageId); }

    /// Bekannte Fehlercodes mit Beschreibung (aus stoerungen.csv).
    QHash<QString, QString> fehlercodes() const;

    // ---------- Prüfmeldungen ----------
    const QVector<Pruefmeldung>& pruefmeldungen() const { return m_meldungen; }
    QVector<Pruefmeldung> pruefmeldungenFuer(const QString& objektId) const;
    int anzahlMeldungen(Pruefmeldung::Stufe stufe) const;
    /// Ergänzt Meldungen anderer Komponenten (z. B. Verlaufsindex).
    void ergaenzeMeldungen(const QVector<Pruefmeldung>& meldungen);
    /// Entfernt alle Meldungen zu einer Quelle (vor erneutem Ergänzen).
    void entferneMeldungen(const QString& datei);

    /**
     * @brief Plausibilitätsprüfung einer Anlage (Wertebereiche, Verweise).
     *        Wird beim Laden und im Bearbeitungsdialog verwendet.
     */
    QVector<Pruefmeldung> pruefeAnlage(const Anlage& a) const;

    // ---------- Ändern ----------
    /**
     * @brief Übernimmt geänderte Stammdaten (ID bleibt unverändert).
     * @return false + *fehler, wenn die Anlage unbekannt ist
     */
    bool aktualisiereAnlage(const Anlage& neu, QString* fehler = nullptr);
    bool aktualisiereStoerung(const Stoerung& neu, QString* fehler = nullptr);
    /// Legt eine neue Störung an; vergibt die ID und liefert sie zurück.
    QString erfasseStoerung(Stoerung neu);
    /// Fügt eine Störung mit vorhandener ID wieder ein (Wiederholen nach Rückgängig).
    bool fuegeStoerungEin(const Stoerung& stoerung);
    /// Entfernt eine Störung (Rückgängig von "Störung erfassen").
    bool entferneStoerung(const QString& stoerungId);

    // ---------- Export ----------
    bool exportiereAnlagen(const QString& pfad, QString* fehler = nullptr) const;
    bool exportiereStoerungen(const QString& pfad, QString* fehler = nullptr) const;
    /// Pfad im Datenordner, z. B. für anlagen_out.csv
    QString ausgabePfad(const QString& dateiname) const;

    static const QStringList& gueltigeTypen();
    static const QStringList& gueltigeStatus();
    static const QStringList& gueltigeSchweren();
    static const QStringList& gueltigeStoerungsStatus();

signals:
    void meldungenGeaendert();
    void anlageGeaendert(const QString& anlageId);
    void stoerungenGeaendert(const QString& anlageId);

private:
    void ladeWetterstationen();
    void ladeWechselrichter();
    void ladeAnlagen();
    void ladeStoerungen();
    void baueStoerungsIndex();
    void melde(Pruefmeldung::Stufe stufe, const QString& datei, int zeile,
               const QString& objektId, const QString& text);

    QString                        m_datenOrdner;
    QVector<Anlage>                m_anlagen;
    QHash<QString, int>            m_anlageIndex;       ///< ID -> Position in m_anlagen
    QHash<QString, Wechselrichter> m_wechselrichter;
    QHash<QString, Wetterstation>  m_wetterstationen;
    QVector<Stoerung>              m_stoerungen;
    QHash<QString, int>            m_stoerungIndex;     ///< ID -> Position
    QHash<QString, int>            m_offen;             ///< anlageId -> offene Störungen
    QHash<QString, int>            m_offenKritisch;
    QVector<Pruefmeldung>          m_meldungen;
};
