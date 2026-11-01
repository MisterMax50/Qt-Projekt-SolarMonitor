#pragma once

#include <functional>

#include <QCoreApplication>
#include <QUndoCommand>

#include "model/anlage.h"
#include "model/stoerung.h"

class Datenbasis;

/**
 * @brief Änderungen als Befehle für Rückgängig/Wiederholen (QUndoStack).
 *
 * Jede Änderung an den Daten läuft über einen dieser Befehle. Er merkt sich
 * den Zustand davor und danach; redo() setzt den neuen Zustand, undo() den
 * alten. QUndoStack ruft redo() beim Hinzufügen automatisch auf.
 *
 * Nach jeder Ausführung (auch bei Rückgängig) wird @p nachher aufgerufen,
 * damit das Hauptfenster die *_out.csv speichert und die Kennzahlen der
 * Anlage neu berechnet.
 */
using NachAenderung = std::function<void(const QString& anlageId)>;

/// Stammdaten einer Anlage ändern.
class AnlageAendern : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(AnlageAendern)
public:
    AnlageAendern(Datenbasis* daten, const Anlage& alt, const Anlage& neu, NachAenderung nachher);
    void redo() override;
    void undo() override;

private:
    Datenbasis*   m_daten;
    Anlage        m_alt, m_neu;
    NachAenderung m_nachher;
};

/// Eine bestehende Störung ändern (Dialog oder Status per Kontextmenü).
class StoerungAendern : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(StoerungAendern)
public:
    StoerungAendern(Datenbasis* daten, const Stoerung& alt, const Stoerung& neu, NachAenderung nachher);
    void redo() override;
    void undo() override;

private:
    Datenbasis*   m_daten;
    Stoerung      m_alt, m_neu;
    NachAenderung m_nachher;
};

/// Neue Störung erfassen; Rückgängig entfernt sie wieder.
class StoerungErfassen : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(StoerungErfassen)
public:
    StoerungErfassen(Datenbasis* daten, const Stoerung& neu, NachAenderung nachher);
    void redo() override;
    void undo() override;
    /// Vergebene Störungs-ID (nach dem ersten redo()).
    QString stoerungId() const { return m_stoerung.stoerungId; }

private:
    Datenbasis*   m_daten;
    Stoerung      m_stoerung;
    NachAenderung m_nachher;
};
