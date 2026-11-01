#pragma once

#include <optional>
#include <stdexcept>

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Ausnahme für Fehler, die eine ganze Datei unbrauchbar machen
 *        (fehlt, nicht lesbar, leer, Pflichtspalte fehlt).
 *
 * Einzelne fehlerhafte Zeilen werfen keine Ausnahme, sondern werden vom
 * Aufrufer als Prüfmeldung protokolliert.
 */
class DateiFehler : public std::runtime_error {
public:
    explicit DateiFehler(const QString& meldung)
        : std::runtime_error(meldung.toStdString())
        , m_meldung(meldung)
    {
    }
    const QString& meldung() const { return m_meldung; }

private:
    QString m_meldung;
};

/**
 * @brief Robuster CSV-Leser für die SolarMonitor-Dateien.
 *
 * UTF-8 (mit/ohne BOM), Komma als Trenner, Felder optional in
 * Anführungszeichen (inkl. "" als Maskierung), \n und \r\n.
 */
class CsvReader {
    Q_DECLARE_TR_FUNCTIONS(CsvReader)
public:
    struct Zeile {
        int         zeilenNr = 0; ///< 1-basiert, Kopfzeile = 1
        QStringList felder;
    };

    struct Tabelle {
        QString            dateiname; ///< nur der Dateiname (für Meldungen)
        QStringList        kopf;
        QVector<Zeile>     zeilen;
        QHash<QString,int> spalte;    ///< Spaltenname -> Index

        /// Wert einer Spalte (getrimmt); leerer String, wenn nicht vorhanden.
        QString wert(const Zeile& z, const QString& name) const;
        bool hatSpalte(const QString& name) const { return spalte.contains(name); }
    };

    /**
     * @brief Liest eine CSV-Datei vollständig ein.
     * @param pflichtSpalten Spalten, die im Kopf vorhanden sein müssen
     * @throws DateiFehler wenn die Datei fehlt, nicht lesbar oder leer ist
     *         bzw. eine Pflichtspalte fehlt
     */
    static Tabelle lade(const QString& pfad, const QStringList& pflichtSpalten = {});

    /// Zerlegt eine einzelne CSV-Zeile in Felder.
    static QStringList aufteilen(const QString& zeile);

    /// Maskiert ein Feld für die Ausgabe (Anführungszeichen bei Bedarf).
    static QString maskiere(const QString& feld);

    /**
     * @brief Zahl im CSV-Format lesen (Punkt als Dezimaltrenner).
     *
     * Ein Dezimalkomma (z. B. "58,4") wird toleriert; @p kommaKorrigiert
     * wird dann gesetzt, damit der Aufrufer eine Info-Meldung erzeugen kann.
     * @return std::nullopt bei leerem oder nicht numerischem Text
     */
    static std::optional<double> zahl(const QString& text, bool* kommaKorrigiert = nullptr);
};
