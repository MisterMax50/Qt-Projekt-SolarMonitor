#pragma once

#include <functional>

#include <QPointer>
#include <QVector>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;

/**
 * @brief Einführungstour: Hinweisblasen, die Schritt für Schritt die
 *        wichtigsten Stellen der Oberfläche zeigen.
 *
 * Liegt als halbdurchsichtige Ebene über dem Programm. Das jeweilige Ziel
 * bleibt hell ausgespart und orange umrandet, daneben steht eine Blase mit
 * Titel, Text und den Knöpfen Zurück/Weiter/Überspringen. Vor einem Schritt
 * kann eine Aktion laufen (z. B. auf die passende Seite wechseln).
 * Tastatur: → / Enter weiter, ← zurück, Esc beendet.
 */
class Einfuehrung : public QWidget {
    Q_OBJECT
public:
    struct Schritt {
        std::function<QWidget*()> ziel;   ///< hervorzuhebendes Widget (nullptr = Blase in der Mitte)
        QString                   titel;
        QString                   text;
        std::function<void()>     vorher = {}; ///< optional, läuft beim Betreten des Schritts
    };

    Einfuehrung(QWidget* wirt, QVector<Schritt> schritte);

    void starte();
    int schritt() const { return m_index; }
    int anzahl() const { return int(m_schritte.size()); }
    void weiter();
    void zurueck();
    void beende(bool vollstaendig);

signals:
    /// @param vollstaendig true = bis zum letzten Schritt durchgeklickt
    void beendet(bool vollstaendig);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void zeige(int index);
    void platziere();
    QRect zielRechteck() const;

    QVector<Schritt>  m_schritte;
    int               m_index = -1;
    QPointer<QWidget> m_ziel;
    QFrame*      m_blase = nullptr;
    QLabel*      m_zaehler = nullptr;
    QLabel*      m_titel = nullptr;
    QLabel*      m_text = nullptr;
    QPushButton* m_zurueck = nullptr;
    QPushButton* m_weiter = nullptr;
    QPushButton* m_ueberspringen = nullptr;
};
