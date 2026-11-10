#pragma once

#include <QFrame>
#include <QList>
#include <QPointer>
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QTimer;
class QToolButton;

/*
 * Spaß-Funktion "Werbung" (Abo-Modus)
 * ===================================
 * Solange "SolarMonitor Premium" aus ist, zeigt das Hauptfenster frei
 * erfundene, augenzwinkernde Werbung rund um Photovoltaik:
 *  - WerbeBanner:   lautes Banner unter jeder Seite (Blinken, Countdown, Lauftext)
 *  - WerbePopup:    jede Minute eine Einblendung über dem Fenster; das winzige
 *                   Kreuz schließt erst beim dritten Klick, die Klicks davor
 *                   öffnen die "Webseite" der Anzeige
 *  - WerbeSeite:    die "Webseite" – ein eigenes Fenster im Browser-Look,
 *                   komplett im Programm gezeichnet (kein Internet, keine Links)
 *  - WerbeSteuerung: Minutentakt, verwaltet Popup und Webseiten-Fenster
 * Es gibt keine echten Marken, und nichts kostet Geld.
 */

/// Lautes Werbebanner unter dem Seiteninhalt.
class WerbeBanner : public QFrame {
    Q_OBJECT
public:
    explicit WerbeBanner(QWidget* parent = nullptr);

signals:
    /// Benutzer möchte die Werbung loswerden (Premium einschalten).
    void premiumGewuenscht();
    /// Benutzer hat auf die Aktion geklickt -> Webseite der Anzeige öffnen.
    void webseiteGewuenscht(int anzeige);

protected:
    void mousePressEvent(QMouseEvent* e) override;

private:
    void zeige(int index);
    void takt();
    void schliessenGeklickt();

    QLabel*      m_bild = nullptr;
    QLabel*      m_titel = nullptr;
    QLabel*      m_text = nullptr;
    QLabel*      m_sticker = nullptr;
    QLabel*      m_countdown = nullptr;
    QPushButton* m_aktion = nullptr;
    QTimer*      m_wechsel = nullptr;
    QTimer*      m_blink = nullptr;
    int          m_index = 0;
    int          m_schritt = 0;
    int          m_restSekunden = 299;
};

/// Popup-Werbung über dem ganzen Fenster (Kind-Widget, kein eigenes Fenster).
class WerbePopup : public QWidget {
    Q_OBJECT
public:
    WerbePopup(int anzeige, QWidget* wirt);

    QToolButton* kreuz() const { return m_kreuz; }
    int klicksBisZu() const { return kKlicksZumSchliessen - m_kreuzKlicks; }

    static constexpr int kKlicksZumSchliessen = 3;

signals:
    void webseiteGewuenscht(int anzeige);
    void premiumGewuenscht();

protected:
    bool eventFilter(QObject* o, QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void kreuzGeklickt();
    void anpassen();
    void takt();

    QFrame*      m_karte = nullptr;
    QLabel*      m_kopf = nullptr;
    QLabel*      m_bild = nullptr;
    QLabel*      m_countdown = nullptr;
    QPushButton* m_aktion = nullptr;
    QToolButton* m_kreuz = nullptr;
    QTimer*      m_blink = nullptr;
    int          m_anzeige = 0;
    int          m_kreuzKlicks = 0;
    int          m_schritt = 0;
    int          m_restSekunden = 59;
};

/// "Webseite" einer Anzeige in einem eigenen Fenster im Browser-Look.
class WerbeSeite : public QWidget {
    Q_OBJECT
public:
    WerbeSeite(int anzeige, QWidget* fenster);

signals:
    void premiumGewuenscht();

private:
    QLabel* m_gewinn = nullptr;
    QLabel* m_countdown = nullptr;
    QTimer* m_blink = nullptr;
    int     m_schritt = 0;
    int     m_restSekunden = 599;
};

/// Minutentakt für die Popup-Werbung; verwaltet Popup und Webseiten.
class WerbeSteuerung : public QObject {
    Q_OBJECT
public:
    /// @param fenster  Elternfenster der Webseiten-Fenster
    /// @param wirt     liefert das Widget, über dem das Popup liegen soll, oder
    ///                 nullptr, solange keins kommen darf (Anmeldung, Dialog offen)
    WerbeSteuerung(QWidget* fenster, std::function<QWidget*()> wirt);

    void setAktiv(bool an);          ///< false: alles schließen, Takt stoppen
    bool aktiv() const;
    void zeigePopup();               ///< sofort ein Popup zeigen (auch für Tests)
    void oeffneWebseite(int anzeige);
    WerbePopup* popup() const { return m_popup; }
    int offeneWebseiten() const;

    static constexpr int kTaktMs = 60 * 1000;

signals:
    void premiumGewuenscht();

private:
    QWidget*                    m_fenster;
    std::function<QWidget*()>   m_wirt;
    QTimer*                     m_takt = nullptr;
    QPointer<WerbePopup>        m_popup;
    QList<QPointer<WerbeSeite>> m_seiten;
};
