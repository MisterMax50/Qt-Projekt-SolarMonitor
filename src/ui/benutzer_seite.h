#pragma once

#include <QDialog>
#include <QWidget>

#include "model/rolle.h"

class Benutzerverwaltung;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QPushButton;

/**
 * @brief Formular zum Anlegen oder Bearbeiten eines Benutzers.
 *
 * Beim Anlegen sind Benutzername und PIN Pflicht; beim Bearbeiten ist der
 * Benutzername fest und die PIN wird nur geändert, wenn eine neue
 * eingetragen wird. Fehler werden direkt im Formular angezeigt.
 */
class BenutzerDialog : public QDialog {
    Q_OBJECT
public:
    /// @param kennung leer = neuen Benutzer anlegen
    BenutzerDialog(Benutzerverwaltung* verwaltung, const QString& kennung, const QString& angemeldet,
                   QWidget* parent = nullptr);

private:
    void speichern();

    Benutzerverwaltung* m_verwaltung;
    QString    m_kennung;
    QString    m_angemeldet;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_benutzername = nullptr;
    QComboBox* m_rolle = nullptr;
    QCheckBox* m_aktiv = nullptr;
    QCheckBox* m_hello = nullptr;
    QLineEdit* m_pin = nullptr;
    QLineEdit* m_pin2 = nullptr;
    QLabel*    m_fehler = nullptr;
};

/**
 * @brief Seite "Benutzerverwaltung" (nur für Administratoren).
 *
 * Liste aller Konten mit Name, Benutzername, Rolle, Status und letzter
 * Anmeldung. Befehle: Neuer Benutzer, Bearbeiten (auch Doppelklick),
 * PIN zurücksetzen, Löschen.
 */
class BenutzerSeite : public QWidget {
    Q_OBJECT
public:
    BenutzerSeite(Benutzerverwaltung* verwaltung, const QString& angemeldet, QWidget* parent = nullptr);

    QAction* aktionNeu() const { return m_neu; }
    QAction* aktionBearbeiten() const { return m_bearbeiten; }
    QAction* aktionPin() const { return m_pin; }
    QAction* aktionLoeschen() const { return m_loeschen; }

signals:
    void meldung(const QString& text, bool hinweis);

private:
    void aktualisiere();
    QString markiert() const;
    void neu();
    void bearbeiten();
    void pinZuruecksetzen();
    void loeschen();
    void auswahlGeaendert();

    Benutzerverwaltung* m_verwaltung;
    QString      m_angemeldet;
    QTreeWidget* m_liste = nullptr;
    QLabel*      m_info = nullptr;
    QAction*     m_neu = nullptr;
    QAction*     m_bearbeiten = nullptr;
    QAction*     m_pin = nullptr;
    QAction*     m_loeschen = nullptr;
};
