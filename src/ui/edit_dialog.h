#pragma once

#include <QDialog>

#include "model/anlage.h"

class Datenbasis;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/**
 * @brief Bearbeitung der Stammdaten einer Anlage (Pflichtpunkt 4).
 *
 * Harte Fehler (z. B. leerer Ort) verhindern das Speichern; weiche
 * Plausibilitätswarnungen (z. B. unterdimensionierter Wechselrichter)
 * werden live angezeigt, das Speichern bleibt aber möglich.
 */
class EditDialog : public QDialog {
    Q_OBJECT
public:
    EditDialog(const Anlage& anlage, const Datenbasis* datenbasis, QWidget* parent = nullptr);

    /// Ergebnis nach accept()
    Anlage anlage() const;

private slots:
    void pruefen();
    void uebernehmen();

private:
    Anlage m_original;
    const Datenbasis* m_daten;

    QComboBox*      m_typ = nullptr;
    QLineEdit*      m_ort = nullptr;
    QDoubleSpinBox* m_lat = nullptr;
    QDoubleSpinBox* m_lon = nullptr;
    QDoubleSpinBox* m_leistung = nullptr;
    QSpinBox*       m_ausrichtung = nullptr;
    QSpinBox*       m_neigung = nullptr;
    QComboBox*      m_wechselrichter = nullptr;
    QSpinBox*       m_anzahlWr = nullptr;
    QComboBox*      m_station = nullptr;
    QDateEdit*      m_inbetrieb = nullptr;
    QDoubleSpinBox* m_verguetung = nullptr;
    QComboBox*      m_status = nullptr;
    QLabel*         m_hinweise = nullptr;
};
