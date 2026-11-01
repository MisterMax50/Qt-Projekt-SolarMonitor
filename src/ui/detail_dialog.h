#pragma once

#include <QDialog>
#include <QVector>

#include "repo/verlauf_repository.h"
#include "ui/app_kontext.h"

class BalkenDiagramm;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTableWidget;
class VerlaufPlot;

/**
 * @brief Detailansicht einer Anlage (Pflichtpunkte 2, 5, 6, 7).
 *
 * Reiter:
 *  1. Stammdaten inkl. verknüpftem Wechselrichter und Wetterstation
 *     sowie den Prüfmeldungen zu dieser Anlage
 *  2. Verlauf: Leistung, Einstrahlung und Modultemperatur über 14 Tage
 *  3. Ertrag & PR: Kennzahlen, Tageserträge (Diagramm + Tabelle),
 *     erkannte Fehlerbilder
 *  4. Störungen der Anlage
 *
 * Der Verlauf wird erst beim Öffnen des Dialogs angefordert (Lazy
 * Loading) und im Hintergrund geladen. Fehlt er oder ist er defekt,
 * bleiben die Reiter 2 und 3 gesperrt und ein Hinweis erklärt den Grund
 * (Degradationsverhalten).
 */
class DetailDialog : public QDialog {
    Q_OBJECT
public:
    DetailDialog(const QString& anlageId, const AppKontext& kontext, QWidget* parent = nullptr);

    QString anlageId() const { return m_anlageId; }

signals:
    void bearbeitenGewuenscht(const QString& anlageId);
    void berichtGewuenscht(const QString& anlageId);
    void stoerungErfassenGewuenscht(const QString& anlageId);
    void stoerungOeffnenGewuenscht(const QString& stoerungId);

private slots:
    void verlaufGeladen(const QString& anlageId, const VerlaufLadeErgebnis& ergebnis);
    void zeigeStammdaten();
    void zeigeStoerungen();

private:
    QWidget* baueVerlaufSeite();
    QWidget* baueAuswertungSeite();
    QWidget* baueStoerungSeite();
    void zeigeAuswertung(const Verlauf& verlauf, const QStringList& dateiHinweise);
    void setzeBanner(const QString& text, bool fehler);

    QString    m_anlageId;
    AppKontext m_kontext;

    QLabel*      m_kopf = nullptr;
    QLabel*      m_statusChip = nullptr;
    QLabel*      m_banner = nullptr;
    QTabWidget*  m_tabs = nullptr;
    QWidget*     m_stammSeite = nullptr;

    VerlaufPlot* m_plotLeistung = nullptr;
    VerlaufPlot* m_plotEinstrahlung = nullptr;
    VerlaufPlot* m_plotTemperatur = nullptr;
    QLabel*      m_verlaufInfo = nullptr;

    QLabel*         m_auswertungHinweis = nullptr;
    QVector<QLabel*> m_kpiWerte;   ///< Kennzahl-Kacheln: Wert
    QVector<QLabel*> m_kpiNotizen; ///< Kennzahl-Kacheln: Zusatzinfo
    BalkenDiagramm* m_tagesDiagramm = nullptr;
    QTableWidget*   m_tagesTabelle = nullptr;
    QListWidget*    m_anomalien = nullptr;

    QTableWidget* m_stoerungen = nullptr;
    QPushButton*  m_btnBearbeiten = nullptr;
    QPushButton*  m_btnStoerung = nullptr;
    QPushButton*  m_btnBericht = nullptr;
};
