#pragma once

#include <QIcon>
#include <QWidget>

#include "ui/app_kontext.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

/**
 * @brief Übersicht aller Prüfmeldungen beim Einlesen (Pflichtpunkte 1 und 7).
 *
 * Zeigt Fehler, Warnungen und automatische Korrekturen aus den CSV-
 * Dateien, dem Verlaufsindex und der Flottenanalyse (defekte JSON-Dateien)
 * mit Datei und Zeilennummer, damit Datenfehler nachvollziehbar sind.
 */
class PruefprotokollWidget : public QWidget {
    Q_OBJECT
public:
    explicit PruefprotokollWidget(const AppKontext& kontext, QWidget* parent = nullptr);

signals:
    void anlageOeffnen(const QString& anlageId);

public slots:
    void aktualisiere();

private:
    AppKontext    m_kontext;
    QComboBox*    m_stufe = nullptr;
    QLineEdit*    m_suche = nullptr;
    QTableWidget* m_tabelle = nullptr;
    QLabel*       m_zusammenfassung = nullptr;
    QIcon         m_iconFehler, m_iconWarnung, m_iconInfo;
};
