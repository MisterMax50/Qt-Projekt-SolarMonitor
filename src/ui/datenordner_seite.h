#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief Seite im Hauptfenster zum Festlegen des Datenordners.
 *
 * Erscheint nach der Anmeldung, wenn beim Start keine Daten gefunden
 * wurden, und jederzeit über das Benutzermenü ("Datenordner ändern …").
 * Der Pfad kann eingetippt oder über "Durchsuchen …" gewählt werden; die
 * Seite prüft sofort, ob der Ordner eine anlagen.csv enthält, und zeigt,
 * welche weiteren Dateien gefunden wurden. Erst dann ist "Übernehmen"
 * möglich.
 */
class DatenordnerSeite : public QWidget {
    Q_OBJECT
public:
    explicit DatenordnerSeite(QWidget* parent = nullptr);

    /**
     * @brief Für die Anzeige vorbereiten.
     * @param ordner      vorgeschlagener Pfad (z. B. der aktuelle Datenordner)
     * @param fehler      Grund, warum keine Daten geladen werden konnten (leer = Ordner ändern)
     * @param zurueckMoeglich true: zweiter Knopf heißt "Zurück" statt "Beenden"
     */
    void vorbereiten(const QString& ordner, const QString& fehler, bool zurueckMoeglich);

    /// true, wenn @p ordner eine anlagen.csv enthält.
    static bool istDatenordner(const QString& ordner);

signals:
    void ordnerGewaehlt(const QString& ordner);
    void abgebrochen();

private:
    void pruefe();
    void durchsuchen();

    QLabel*      m_titel = nullptr;
    QLabel*      m_fehler = nullptr;
    QLineEdit*   m_pfad = nullptr;
    QLabel*      m_pruefung = nullptr;
    QPushButton* m_uebernehmen = nullptr;
    QPushButton* m_abbrechen = nullptr;
};
