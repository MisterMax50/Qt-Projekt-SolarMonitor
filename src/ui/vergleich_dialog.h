#pragma once

#include <QDate>
#include <QDialog>
#include <QMap>

#include "eval/ertrags_auswertung.h"
#include "ui/app_kontext.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
struct VerlaufLadeErgebnis;
class TagesVergleich;

/**
 * @brief Zwei Anlagen nebeneinander vergleichen.
 *
 * Oben die Auswahl (durchsuchbar), darunter eine Tabelle mit Stammdaten und
 * Kennzahlen der Flottenanalyse; der jeweils bessere Wert ist grün markiert.
 * Unten der spezifische Tagesertrag (kWh/kWp) beider Anlagen als
 * gruppiertes Balkendiagramm – spezifisch, damit unterschiedlich große
 * Anlagen vergleichbar sind. Die Tageswerte werden aus den Verläufen
 * berechnet, die dazu im Hintergrund geladen werden.
 */
class VergleichDialog : public QDialog {
    Q_OBJECT
public:
    VergleichDialog(const AppKontext& kontext, const QString& erste, QWidget* parent = nullptr);

    void waehle(const QString& a, const QString& b); ///< (für Tests und Vorbelegung)

signals:
    void anlageOeffnen(const QString& anlageId);

private:
    QString gewaehlt(int seite) const;
    QString vorschlagFuer(const QString& a) const;
    void aktualisiere();
    void fuelleTabelle();
    void aktualisiereHinweis();
    void verlaufGeladen(const QString& anlageId, const VerlaufLadeErgebnis& e);

    AppKontext    m_kontext;
    QComboBox*    m_wahl[2] = {nullptr, nullptr};
    QPushButton*  m_oeffnen[2] = {nullptr, nullptr};
    QTableWidget* m_tabelle = nullptr;
    TagesVergleich* m_diagramm = nullptr;
    QLabel*       m_diagrammHinweis = nullptr;
    QMap<QDate, double> m_tage[2];  ///< spezifischer Tagesertrag (kWh/kWp)
    bool          m_tageGeladen[2] = {false, false};
    QString       m_tageFuer[2];
};

/// Gruppiertes Balkendiagramm: je Tag ein Balken für Anlage A und B.
class TagesVergleich : public QWidget {
    Q_OBJECT
public:
    explicit TagesVergleich(QWidget* parent = nullptr);
    void setDaten(const QString& nameA, const QMap<QDate, double>& a, const QString& nameB,
                  const QMap<QDate, double>& b);
    QSize sizeHint() const override { return {760, 230}; }
    QSize minimumSizeHint() const override { return {320, 160}; }

    static QColor farbe(int seite);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    QRectF plot() const;
    QList<QDate> tage() const;

    QString m_name[2];
    QMap<QDate, double> m_werte[2];
};
