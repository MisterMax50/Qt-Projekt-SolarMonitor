#pragma once

#include <QAbstractButton>
#include <QDialog>
#include <QList>
#include <QPixmap>
#include <QWidget>

class QFrame;
class QPushButton;

/*
 * Spaß-Funktion "Keks-Banner" (Parodie auf Cookie-Banner)
 * ========================================================
 * Ohne Abo erscheint nach der Anmeldung einmal pro Programmstart ein
 * Banner, wie man ihn von Webseiten kennt: ein riesiger Knopf
 * "Alle akzeptieren", ein unscheinbarer Link "Einstellungen" – und dort
 * 47 Schalter in sechs Kategorien, alle vorab eingeschaltet, darunter das
 * berühmte "berechtigte Interesse". Es werden keine Daten erhoben oder
 * gespeichert; die Auswahl wird nur kommentiert.
 */

/// Kleiner Schiebeschalter (an/aus), wie in Einstellungs-Apps.
class KeksSchalter : public QAbstractButton {
    Q_OBJECT
public:
    explicit KeksSchalter(QWidget* parent = nullptr);
    QSize sizeHint() const override { return {40, 22}; }

protected:
    void paintEvent(QPaintEvent*) override;
};

/// Ein gezeichneter Keks (Schokostückchen, angebissen).
QPixmap keksBild(int groesse, qreal dpr);

/// Einstellungen mit 47 Schaltern.
class KeksEinstellungen : public QDialog {
    Q_OBJECT
public:
    explicit KeksEinstellungen(QWidget* parent = nullptr);

    int anzahlAn() const;
    int anzahl() const { return int(m_schalter.size()); }
    static constexpr int kAnzahlKekse = 47;

private:
    void setzeAlle(bool an);

    QList<KeksSchalter*> m_schalter;   ///< alle (auch die notwendigen)
    QList<KeksSchalter*> m_waehlbar;   ///< ohne die "unbedingt notwendigen"
};

/// Der Banner selbst: liegt unten über dem Programm und sperrt es bis zur Wahl.
class KeksBanner : public QWidget {
    Q_OBJECT
public:
    explicit KeksBanner(QWidget* wirt);
    void zeige();

    QPushButton* akzeptieren() const { return m_alle; }
    QPushButton* einstellungen() const { return m_einstellungen; }

signals:
    /// @param akzeptiert wie viele der Kekse am Ende an sind
    void entschieden(int akzeptiert, int gesamt);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void anpassen();
    void fertig(int akzeptiert, int gesamt);

    QFrame*      m_karte = nullptr;
    QPushButton* m_alle = nullptr;
    QPushButton* m_einstellungen = nullptr;
};
