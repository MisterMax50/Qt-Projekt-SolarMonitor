#pragma once

#include <memory>

#include <QCoreApplication>
#include <QString>

class Datenbasis;
class VerlaufRepository;
class FlottenAnalyse;

/**
 * @brief Alle Daten eines Datenordners: Stammdaten, Verlaufsindex und
 *        Flottenanalyse.
 *
 * Wird beim Start und beim Wechsel des Datenordners erzeugt. Beim Wechsel
 * lädt das Hauptfenster erst einen neuen Datensatz und tauscht ihn nur bei
 * Erfolg gegen den alten – das Fenster bleibt dabei offen.
 */
class Datensatz {
    Q_DECLARE_TR_FUNCTIONS(Datensatz)
public:
    Datensatz();
    ~Datensatz();
    Datensatz(const Datensatz&) = delete;
    Datensatz& operator=(const Datensatz&) = delete;

    /// Lädt @p ordner (leer = kein Ordner gefunden). Fehler stehen danach in fehler().
    static std::unique_ptr<Datensatz> lade(const QString& ordner);

    bool ok() const { return m_fehler.isEmpty(); }
    QString fehler() const { return m_fehler; }
    QString ordner() const { return m_ordner; }

    Datenbasis*        daten() const { return m_daten.get(); }
    VerlaufRepository* verlaeufe() const { return m_verlaeufe.get(); }
    FlottenAnalyse*    analyse() const { return m_analyse.get(); }

private:
    // Reihenfolge wichtig: die Analyse greift auf Daten und Verläufe zu und
    // wird deshalb zuerst zerstört (wartet auf laufende Hintergrundarbeit).
    std::unique_ptr<Datenbasis>        m_daten;
    std::unique_ptr<VerlaufRepository> m_verlaeufe;
    std::unique_ptr<FlottenAnalyse>    m_analyse;
    QString m_fehler;
    QString m_ordner;
};
