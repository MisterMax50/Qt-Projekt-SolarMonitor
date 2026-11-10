#pragma once

#include <QWidget>

#include "ui/app_kontext.h"

class QFormLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;

/**
 * @brief Detailbereich rechts neben der Anlagentabelle.
 *
 * Wie der Lesebereich in Outlook oder der Detailbereich im Windows-Explorer:
 * Beim Markieren einer Zeile erscheinen die wichtigsten Angaben zur Anlage
 * (Stammdaten, Auswertung, Störungen), ohne dass ein Dialog geöffnet werden
 * muss. Für alles Weitere gibt es "Details …" (öffnet den Detaildialog).
 *
 * Liest nur aus Datenbasis und Flottenanalyse (keine eigenen Berechnungen,
 * kein Nachladen von Verlaufsdateien) und ist daher beim Durchblättern schnell.
 */
class AnlageInfo : public QWidget {
    Q_OBJECT
public:
    explicit AnlageInfo(const AppKontext& kontext, QWidget* parent = nullptr);

    void zeige(const QString& anlageId);
    QString anlageId() const { return m_anlageId; }

signals:
    void detailsGewuenscht(const QString& anlageId);
    void bearbeitenGewuenscht(const QString& anlageId);

private:
    void aktualisiere();
    void aktualisiereFavorit();
    QLabel* zeile(QFormLayout* form, const QString& titel);

    AppKontext m_kontext;
    QString    m_anlageId;

    QStackedWidget* m_seiten = nullptr;
    QLabel* m_titel = nullptr;
    QLabel* m_untertitel = nullptr;
    QLabel *m_typ = nullptr, *m_status = nullptr, *m_leistung = nullptr, *m_ausrichtung = nullptr,
           *m_wr = nullptr, *m_station = nullptr, *m_inbetrieb = nullptr;
    QLabel *m_energie = nullptr, *m_spez = nullptr, *m_pr = nullptr, *m_verlust = nullptr, *m_fehlerbilder = nullptr;
    QLabel *m_offen = nullptr, *m_letzte = nullptr;
    QPushButton* m_details = nullptr;
    QPushButton* m_bearbeiten = nullptr;
    QToolButton* m_favorit = nullptr;
};
