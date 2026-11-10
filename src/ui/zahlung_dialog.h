#pragma once

#include <QDialog>

#include "util/abo.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTimer;

/**
 * @brief Spaß-Funktion: erfundener Bezahlablauf für SolarMonitor Premium/Ultimate.
 *
 * "SolarMonitor Pay" ist frei erfunden und läuft komplett im Programm:
 * keine Verbindung nach außen, nichts wird gespeichert, es fließt kein Geld.
 * Schritte: Bestellung -> Anmelden (beliebige vierstellige "Sonnen-PIN") ->
 * Zahlart -> Prüfen und AGB -> Verarbeitung -> Quittung.
 * exec() liefert Accepted, wenn das "Abo" abgeschlossen wurde.
 */
class ZahlungDialog : public QDialog {
    Q_OBJECT
public:
    ZahlungDialog(const QString& kontoName, Abo::Stufe stufe, QWidget* parent = nullptr);

    /// Dauer der "Verarbeitung" in ms (für Tests verkürzbar)
    static int verarbeitungMs;

    int schritt() const;

private:
    enum Schritt { Bestellung, Anmelden, Zahlart, Pruefen, Verarbeitung, Fertig };

    QWidget* baueBestellung();
    QWidget* baueAnmelden(const QString& kontoName);
    QWidget* baueZahlart();
    QWidget* bauePruefen();
    QWidget* baueVerarbeitung();
    QWidget* baueFertig();

    void geheZu(int schritt);
    void weiter();
    void zurueck();
    void starteVerarbeitung();
    QString zahlartText() const;

    QStackedWidget* m_seiten = nullptr;
    QLabel*         m_schrittText = nullptr;
    QPushButton*    m_zurueck = nullptr;
    QPushButton*    m_abbrechen = nullptr;
    QPushButton*    m_weiter = nullptr;

    QLineEdit*      m_pin = nullptr;
    QLabel*         m_pinFehler = nullptr;
    QButtonGroup*   m_zahlarten = nullptr;
    QLabel*         m_pruefZahlart = nullptr;
    QCheckBox*      m_agb = nullptr;
    QProgressBar*   m_fortschritt = nullptr;
    QLabel*         m_status = nullptr;
    QLabel*         m_quittung = nullptr;
    QTimer*         m_takt = nullptr;
    int             m_verarbeitet = 0;
    Abo::Stufe      m_stufe;
};
