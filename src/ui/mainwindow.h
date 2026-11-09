#pragma once

#include <QMainWindow>

#include "ui/app_kontext.h"

class AnlageFilterModel;
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
 * Seiten (Navigation links): Anlagen · Störungen · Karte · Statistik ·
 * Serviceplanung · Datenprüfung. Fenstergröße, Sprache und Design werden
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
    explicit MainWindow(const AppKontext& kontext, QWidget* parent = nullptr);

    /// Fenster anzeigen: im Vollbild (Standard) oder maximiert, je nach letzter Wahl.
    void zeigeBeimStart();

protected:
    void changeEvent(QEvent* event) override;
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

private:
    /// Merkt sich die Filtereinstellungen über einen Neuaufbau der Oberfläche hinweg.
    struct FilterZustand {
        QString suche, status, typ, ort;
        double  minKwp = 0.0, maxKwp = 0.0;
        bool    nurAuffaellig = false, nurOffen = false;
        QString stoerSuche, stoerSchwere, stoerAnlage;
        int     stoerStatus = 0;
    };

    void baueUi();
    void erzeugeAktionen();
    QWidget* baueNavigation();
    void baueBefehlsleiste();
    /// Seite mit Titel, Erklärung und Aktionsknöpfen im Kopf.
    QWidget* seite(const QString& titel, const QString& hinweis, QWidget* inhalt,
                   const QList<QAction*>& aktionen = {});
    void aktualisiereFilterKnopf();
    QWidget* baueAnlagenSeite();
    QWidget* baueStoerungsSeite();
    void baueMenues();
    void baueStatusleiste();
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

    // werden bei baueUi() neu erzeugt
    QStackedWidget* m_tabs = nullptr;     ///< Seiten, gesteuert über die Navigation
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
    QPushButton*  m_meldungenKnopf = nullptr;

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
};
