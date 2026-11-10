#pragma once

#include <memory>

#include <functional>

#include <QMainWindow>
#include <QPointer>

#include "ui/app_kontext.h"
#include "util/abo.h"

class AnlageFilterModel;
class Datensatz;
class AnlageModel;
class KarteView;
class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class AnlageInfo;
class QStackedWidget;
class QProgressBar;
class QPushButton;
class QTableView;
class QUndoStack;
class StoerungFilterModel;
class StoerungModel;

/**
 * @brief Hauptfenster von SolarMonitor.
 *
 * Alles läuft in diesem einen Fenster: zuerst das Logo-Intro, dann die
 * Anmeldung, falls keine Daten gefunden wurden die Seite zum Festlegen des
 * Datenordners, danach das Programm (QStackedWidget mit Ebenen).
 *
 * Aufbau wie ein Monitoring-Portal für Solarparks: dunkle Seitenleiste
 * mit den Seiten (Übersicht · Anlagen · Störungen · Karte · Statistik ·
 * Serviceplanung · Datenprüfung), Kopfzeile mit Seitentitel, Anlagensuche,
 * Alarmglocke und Benutzermenü, darunter der Inhalt als Karten. Fenstergröße, Sprache und Design werden
 * in QSettings gespeichert.
 *
 * Das Fenster koordiniert alle Aktionen, die Daten ändern (Bearbeiten,
 * Störung erfassen/ändern, Export), und prüft dabei das Rollenrecht.
 * Modelle und Filterzustand bleiben erhalten, wenn die Oberfläche beim
 * Sprachwechsel oder Rollenwechsel neu aufgebaut wird.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /**
     * @param datensatz geladene Daten. Konnten sie nicht geladen werden
     *        (datensatz->ok() == false), folgt nach der Anmeldung die Seite
     *        "Datenordner festlegen".
     */
    explicit MainWindow(std::unique_ptr<Datensatz> datensatz, QWidget* parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief Fenster anzeigen (Vollbild oder maximiert, je nach letzter Wahl)
     *        und mit dem Intro bzw. direkt mit der Anmeldung beginnen.
     */
    void starte(bool mitIntro);

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void oeffneAnlage(const QString& anlageId);
    void bearbeiteAnlage(const QString& anlageId, QWidget* parent = nullptr);
    void erfasseStoerung(const QString& anlageId, QWidget* parent = nullptr);
    void oeffneStoerung(const QString& stoerungId, QWidget* parent = nullptr);
    void setzeStoerungsStatus(const QString& stoerungId, const QString& status);

    void exportiereAnlagen();
    void exportiereStoerungen();
    void exportiereBericht();
    void exportiereAnlagenPdf(const QString& anlageId, QWidget* parent = nullptr);
    void exportiereFlottenPdf();
    void zeigeAenderungsverlauf();
    void benutzerWechseln();
    void zeigeUeber();

    void analyseFortschritt(int erledigt, int gesamt);
    void analyseFertig();
    void anlagenKontextmenue(const QPoint& pos);
    void stoerungenKontextmenue(const QPoint& pos);
    void aktualisiereZaehler();
    void aktualisiereNavigation();
    void setzeDesign(bool dunkel);
    void setzeVollbild(bool an);
    void seiteGewechselt(int index);
    /// Abo-Modus (Spaß-Funktion): Werbung aus- bzw. einblenden und merken.
    void setzePremium(bool an);
    /// Abo-Stufe wählen: Upgrade über den Bezahlablauf, Downgrade sofort.
    void waehleAbo(int stufe);
    /// Upgrade auf die Stufe über den erfundenen Bezahlablauf "SolarMonitor Pay".
    bool aboKaufen(int stufe);
    void aboAnwenden();                      ///< Werbung, Abzeichen, Menü und Sperren an die Stufe anpassen
    void zeigePaywall(Abo::Funktion f);      ///< Ultimate-Angebot für eine gesperrte Funktion
    /// Abo abschließen über den erfundenen Bezahlablauf "SolarMonitor Pay".
    void premiumKaufen();
    /// Zwei Anlagen nebeneinander vergleichen (@p erste vorbelegt).
    void oeffneVergleich(const QString& erste = QString());
    /// Markierte Anlage an- oder abpinnen.
    void favoritUmschalten(const QString& anlageId);
    /// Einführungstour (Hinweisblasen) starten.
    void starteEinfuehrung();
    void angemeldet(const QString& kennung);

private:
    void aktualisiereAboChip();
    void aktualisiereAboMenue();
    /// Merkt sich die Filtereinstellungen über einen Neuaufbau der Oberfläche hinweg.
    struct FilterZustand {
        QString suche, status, typ, ort;
        double  minKwp = 0.0, maxKwp = 0.0;
        bool    nurAuffaellig = false, nurOffen = false, nurFavoriten = false;
        QString stoerSuche, stoerSchwere, stoerAnlage;
        int     stoerStatus = 0;
    };

    void baueUi();
    void neueLoginSeite();
    void zeigeAnmeldung(bool wechsel);
    void zeigeProgramm();
    void zeigeDatenordner(const QString& fehler, bool zurueckMoeglich);
    /// Daten und Modelle an das Fenster binden (beim Start und nach einem Ordnerwechsel).
    void uebernimmDaten(std::unique_ptr<Datensatz> datensatz);
    /// Programmoberfläche und Modelle entfernen, bevor die Daten ersetzt werden.
    void trenneDaten();
    /// Anderen Datenordner laden, ohne das Fenster zu schließen.
    void wechsleDaten(const QString& ordner);
    void erzeugeAktionen();
    /// Dunkle Seitenleiste mit Marke, Navigation und Analysezustand.
    QWidget* baueNavigation();
    /// Kopfzeile mit Seitentitel, Suche, Alarmglocke und Benutzermenü.
    QWidget* baueKopfzeile();
    /**
     * Seite mit Aktionszeile über dem Inhalt: links optional ein Widget
     * (z. B. Zähler), rechts Knöpfe für @p aktionen (nullptr = Abstand) und
     * ein Menüknopf "Exportieren" für @p exportieren.
     */
    QWidget* seite(const QString& titel, const QString& hinweis, QWidget* inhalt,
                   const QList<QAction*>& aktionen = {}, const QList<QAction*>& exportieren = {},
                   QWidget* links = nullptr);
    /// Kurze Einblendung unten rechts (ersetzt die Statusleiste).
    void meldung(const QString& text, int dauerMs = 5000, bool hinweis = false);
    /// Einblendung mit einem Link am Ende (z. B. "Rückgängig"), der @p aktion ausführt.
    void meldungMitAktion(const QString& text, const QString& linkText, std::function<void()> aktion,
                          int dauerMs = 7000);
    /// Markierte Anlage; ist keine markiert, erscheint ein kurzer Hinweis.
    QString markierteAnlageOderHinweis();
    void aktualisiereFilterKnopf();
    QWidget* baueAnlagenSeite();
    QWidget* baueStoerungsSeite();
    void baueMenues();
    void aktualisiereRechte();
    QString markierteAnlage() const;
    QString markierteStoerung() const;
    void speichereAnlagen();
    void speichereStoerungen();
    /// Nach jeder Änderung (auch Rückgängig/Wiederholen): sichern und neu bewerten.
    void nachAnlagenAenderung(const QString& anlageId);
    void nachStoerungsAenderung(const QString& anlageId);
    bool pdfZiel(const QString& titel, const QString& vorschlag, QString* pfad);
    void pdfFertig(bool ok, const QString& pfad, const QString& fehler);

    std::unique_ptr<Datensatz> m_datensatz; ///< Daten des aktuellen Ordners (gehören dem Fenster)
    AppKontext m_kontext;
    FilterZustand m_filter;

    // dauerhaft (überleben den Neuaufbau)
    AnlageModel*         m_anlageModel = nullptr;
    AnlageFilterModel*   m_anlageFilter = nullptr;
    StoerungModel*       m_stoerungModel = nullptr;
    StoerungFilterModel* m_stoerungFilter = nullptr;
    QUndoStack*          m_undo = nullptr; ///< Rückgängig/Wiederholen für alle Datenänderungen
    int  m_fortschrittWert = 0;
    int  m_fortschrittMax = 0;
    bool m_neuaufbauGeplant = false;

    // Ebenen des Fensters: Intro → Anmeldung → Programm
    QStackedWidget*    m_ebenen = nullptr;
    class IntroAnimation* m_intro = nullptr;
    class LoginSeite*  m_login = nullptr;
    QWidget*           m_programm = nullptr; ///< Oberfläche nach der Anmeldung (wird neu aufgebaut)
    class DatenordnerSeite* m_ordnerSeite = nullptr;
    QString            m_datenFehler;
    QString            m_datenOrdner;

    // werden bei baueUi() neu erzeugt
    QStackedWidget* m_tabs = nullptr;     ///< Seiten, gesteuert über die Seitenleiste
    QLabel*       m_seitenTitel = nullptr;
    QLabel*       m_seitenHinweis = nullptr;
    QLabel*       m_analyseText = nullptr;
    class QToolButton* m_glocke = nullptr;
    class QToolButton* m_benutzerKnopf = nullptr;
    QWidget*      m_werbungsBereich = nullptr;  ///< Spaß-Werbung (ohne Premium)
    class WerbeSteuerung* m_werbeSteuerung = nullptr;  ///< Popup-Werbung im Minutentakt (lebt mit dem Fenster)
    QLabel*       m_premiumChip = nullptr;
    QMenu*        m_aboMenue = nullptr;   ///< Kontomenü > Abo (Kostenlos/Premium/Ultimate)
    class Benutzerverwaltung* m_benutzer = nullptr;
    QString       m_angemeldet;   ///< Benutzername des angemeldeten Kontos
    QLabel*       m_toast = nullptr;
    class QTimer* m_toastTimer = nullptr;
    std::function<void()> m_toastAktion; ///< Link in der Einblendung (leer = keiner)

    // Rückmeldung zu Rückgängig/Wiederholen: woher kam die letzte Änderung am Stapel?
    enum class UndoQuelle { Keine, Neu, Rueckgaengig, Wiederholen };
    UndoQuelle m_undoQuelle = UndoQuelle::Keine;
    void fuehreAus(class QUndoCommand* befehl); ///< push() mit Rückmeldung
    void undoStandGeaendert(int index);
    QListWidget*  m_navigation = nullptr;
    AnlageInfo*   m_anlageInfo = nullptr;
    QPushButton*  m_filterReset = nullptr;
    QLabel*       m_anlagenLeer = nullptr;
    QLabel*       m_stoerungenLeer = nullptr;
    QTableView*   m_anlagenTabelle = nullptr;
    QTableView*   m_stoerungsTabelle = nullptr;
    KarteView*    m_karte = nullptr;
    QLineEdit*    m_suche = nullptr;
    QLineEdit*    m_stoerAnlage = nullptr;
    QComboBox*    m_stoerStatus = nullptr;
    QLabel*       m_anlagenZaehler = nullptr;
    QLabel*       m_stoerungsZaehler = nullptr;
    QProgressBar* m_fortschritt = nullptr;

    QPointer<class Einfuehrung> m_einfuehrung; ///< laufende Tour (sonst leer)
    QAction* m_aktFavorit = nullptr;   ///< markierte Anlage an-/abpinnen (Strg+D)
    QAction* m_aktVergleich = nullptr; ///< zwei Anlagen vergleichen
    class QCheckBox* m_nurFavoriten = nullptr;
    QAction* m_aktDetails = nullptr;
    QAction* m_aktBearbeiten = nullptr;
    QAction* m_aktStoerung = nullptr;
    QAction* m_aktExportAnlagen = nullptr;
    QAction* m_aktExportStoerungen = nullptr;
    QAction* m_aktExportBericht = nullptr;
    QAction* m_aktPdfAnlage = nullptr;
    QAction* m_aktPdfFlotte = nullptr;
    QAction* m_aktRueckgaengig = nullptr;
    QAction* m_aktWiederholen = nullptr;
    QAction* m_aktAnalyse = nullptr;
    QAction* m_aktVerlauf = nullptr;
};
