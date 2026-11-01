#pragma once

#include <QWidget>

#include "ui/app_kontext.h"

class BalkenDiagramm;
class QLabel;
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
    QLabel* kpiKachel(const QString& titel, QWidget* parent, QLayout* layout);
    void fuelleTypen();
    void fuelleOrte();
    void fuelleStoerungen();
    void fuelleAlter();
    void fuelleStationen();

    AppKontext m_kontext;
    QLabel* m_kpiAnlagen = nullptr;
    QLabel* m_kpiLeistung = nullptr;
    QLabel* m_kpiStoerungen = nullptr;
    QLabel* m_kpiPr = nullptr;
    QLabel* m_kpiErtrag = nullptr;
    QLabel* m_kpiAuffaellig = nullptr;
    QTableWidget*   m_typen = nullptr;
    QTableWidget*   m_orte = nullptr;
    QTableWidget*   m_stoerungen = nullptr;
    QTableWidget*   m_stationen = nullptr;
    BalkenDiagramm* m_alter = nullptr;
};
