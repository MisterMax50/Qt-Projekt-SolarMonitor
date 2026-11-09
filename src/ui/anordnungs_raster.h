#pragma once

#include <QCursor>
#include <QHash>
#include <QIcon>
#include <QPointer>
#include <QRect>
#include <QStringList>
#include <QWidget>

class QFrame;
class QGridLayout;

/**
 * @brief Raster mit festen Plätzen, auf denen sich Karten frei verschieben lassen.
 *
 * Wie bei den Startseiten von Monitoring-Portalen: Eine Karte wird mit der
 * Maus gezogen (bei Karten mit Überschrift nur an der Überschrift, damit
 * Listen und Karte darin bedienbar bleiben) und folgt dabei frei dem
 * Mauszeiger. Der Platz unter dem Mauszeiger wird hervorgehoben; beim
 * Loslassen rastet die Karte dort ein und tauscht den Platz mit der Karte,
 * die bisher dort lag. Der verlassene Platz zeigt währenddessen einen
 * gestrichelten Platzhalter.
 *
 * Die Plätze (Zeile, Spalte, Spannweite) sind vorgegeben. Zusätzlich kann
 * es mehrere vordefinierte Anordnungen (Vorlagen) mit eigenen Plätzen
 * geben, z. B. "Karte groß" oder "Listen nebeneinander". Gewählte Vorlage
 * und Belegung werden in QSettings unter dem übergebenen Schlüssel
 * gespeichert.
 */
class AnordnungsRaster : public QWidget {
    Q_OBJECT
public:
    explicit AnordnungsRaster(const QString& schluessel, QWidget* parent = nullptr);

    struct Platz {
        int zeile, spalte, zeilen = 1, spalten = 1;
    };
    /// Vordefinierte Anordnung: Plätze, Gewichtung der Spalten/Zeilen und Belegung.
    struct Vorlage {
        QString      id;
        QString      name;
        QList<Platz> plaetze;
        QList<int>   spaltenGewicht;
        QList<int>   zeilenGewicht;
        QStringList  belegung;   ///< Karten-ids je Platz
    };

    /// Vorgegebenen Platz anlegen; Reihenfolge der Aufrufe = Reihenfolge der Plätze.
    void neuerPlatz(int zeile, int spalte, int zeilen = 1, int spalten = 1);
    /// Vordefinierte Anordnung anbieten (die erste ist die Standardanordnung).
    void neueVorlage(const Vorlage& vorlage);
    /// Vordefinierte Anordnung anwenden und merken.
    void waehleVorlage(const QString& id);
    const QList<Vorlage>& vorlagen() const { return m_vorlagen; }
    QString aktuelleVorlage() const { return m_vorlage; }
    /// Kleines Vorschaubild einer Vorlage (Rechtecke der Plätze).
    QIcon vorschau(const QString& id) const;
    /**
     * @brief Karte hinzufügen; ohne gespeicherte Anordnung kommt sie auf den nächsten freien Platz.
     * @param griffHoehe Höhe des Bereichs oben, an dem gezogen werden kann (0 = ganze Karte)
     */
    void fuegeHinzu(const QString& id, QWidget* karte, int griffHoehe = 0);
    /// Gespeicherte Anordnung anwenden (nach dem Hinzufügen aller Karten aufrufen).
    void ladeAnordnung();
    /// Zur vorgegebenen Anordnung zurückkehren.
    void zuruecksetzen();

    QGridLayout* raster() const { return m_raster; }

signals:
    void anordnungGeaendert();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void ordneAn();
    void wendeVorlageAn(const Vorlage& v);
    void beginneZiehen();
    void beendeZiehen(const QPoint& pos);
    int platzBei(const QPoint& pos) const;
    QRect platzRechteck(int platz) const;

    QString        m_schluessel;
    QGridLayout*   m_raster = nullptr;
    QList<Platz>   m_plaetze;
    QList<Vorlage> m_vorlagen;
    QString        m_vorlage;           ///< id der gewählten Vorlage
    QStringList    m_standard;          ///< ids in der vorgegebenen Reihenfolge
    QStringList    m_reihenfolge;       ///< id je Platz
    QHash<QString, QWidget*> m_karten;
    QHash<QWidget*, int>     m_griff;

    // Zustand beim Ziehen
    QPointer<QWidget> m_kandidat;
    bool    m_zieht = false;
    QPoint  m_startGlobal;
    QPoint  m_versatz;
    int     m_startPlatz = -1;
    QList<QRect> m_rechtecke;           ///< Plätze in Rasterkoordinaten (beim Start gemessen)
    QCursor m_alterCursor;
    QFrame* m_platzhalter = nullptr;
    QFrame* m_ziel = nullptr;
};
