#pragma once

#include <QDialog>

#include "model/stoerung.h"

class Datenbasis;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;

/**
 * @brief Anzeigen, Bearbeiten und Erfassen einer Störung (Erweiterungen 1, 3, 4).
 *
 * - Neu: Anlage ist vorgegeben, Zeitpunkt = jetzt, Status = offen.
 * - Bestehend: nur mit dem Recht "Störung bearbeiten" änderbar; beim
 *   Wechsel auf "behoben" wird behoben_am automatisch gesetzt.
 */
class StoerungDialog : public QDialog {
    Q_OBJECT
public:
    /// Bestehende Störung anzeigen/bearbeiten
    StoerungDialog(const Stoerung& stoerung, const Datenbasis* daten, bool bearbeitbar,
                   QWidget* parent = nullptr);
    /// Neue Störung für eine Anlage erfassen
    StoerungDialog(const QString& anlageId, const Datenbasis* daten, QWidget* parent = nullptr);

    Stoerung stoerung() const;

private slots:
    void fehlercodeGewaehlt();
    void uebernehmen();

private:
    void baueLayout(bool neu, bool bearbeitbar);

    Stoerung          m_original;
    const Datenbasis* m_daten;

    QDateTimeEdit* m_zeitpunkt = nullptr;
    QComboBox*     m_code = nullptr;
    QLineEdit*     m_beschreibung = nullptr;
    QComboBox*     m_schwere = nullptr;
    QComboBox*     m_status = nullptr;
    QLabel*        m_behobenAm = nullptr;
};
