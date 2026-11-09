#pragma once

#include <QWidget>

#include "ui/app_kontext.h"

class BalkenDiagramm;
class QGridLayout;
class QLabel;
class KennzahlKachel;
class QTableWidget;

/**
 * @brief Statistiken über die Flotte (Erweiterung 1).
 *
 *  - Kennzahlen (Anlagen, installierte Leistung, offene Störungen, Flotten-PR)
 *  - installierte Leistung je Typ und je Ort
 *  - Störungen je Fehlercode und Schwere
 *  - Altersverteilung (Inbetriebnahmejahr)
 *  - Performance Ratio je Wetterstation (nach der Flottenanalyse)
 */
class StatistikWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatistikWidget(const AppKontext& kontext, QWidget* parent = nullptr);

public slots:
    void aktualisiere();

private:
    void fuelleTypen();
    void fuelleOrte();
    void fuelleStoerungen();
    void fuelleAlter();
    void fuelleStationen();

    AppKontext m_kontext;
    KennzahlKachel* m_kpiAnlagen = nullptr;
    KennzahlKachel* m_kpiLeistung = nullptr;
    KennzahlKachel* m_kpiStoerungen = nullptr;
    KennzahlKachel* m_kpiPr = nullptr;
    KennzahlKachel* m_kpiErtrag = nullptr;
    KennzahlKachel* m_kpiAuffaellig = nullptr;
    QTableWidget*   m_typen = nullptr;
    QTableWidget*   m_orte = nullptr;
    QTableWidget*   m_stoerungen = nullptr;
    QTableWidget*   m_stationen = nullptr;
    BalkenDiagramm* m_alter = nullptr;
};
