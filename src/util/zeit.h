#pragma once

#include <QDateTime>
#include <QStringView>
#include <QTimeZone>

/**
 * @brief Zeitstempel der Datendateien ("JJJJ-MM-TT hh:mm:ss", Ortszeit).
 *
 * Die Zeitstempel werden bewusst als "naive" Zeit (ohne Zeitzone, intern
 * als UTC markiert) gespeichert:
 *  - Uhrzeiten in der Lücke der Sommerzeitumstellung (z. B. 31.03. 02:30)
 *    wären als Ortszeit ungültig, stehen aber so in den Daten;
 *  - Tagesgrenzen und 15-Minuten-Abstände bleiben unabhängig von der
 *    Zeitzone des Rechners stabil;
 *  - das Parsen per Hand ist um ein Vielfaches schneller als
 *    QDateTime::fromString (wichtig bei 1.500 × 1.344 Zeitstempeln).
 */
namespace Zeit {

inline QDateTime naiv(const QDate& d, const QTime& t)
{
    return QDateTime(d, t, QTimeZone::utc());
}

inline QDateTime jetzt()
{
    const QDateTime lokal = QDateTime::currentDateTime();
    return naiv(lokal.date(), QTime(lokal.time().hour(), lokal.time().minute(), lokal.time().second()));
}

/// "JJJJ-MM-TT hh:mm:ss" oder "JJJJ-MM-TT" lesen; ungültig bei Formatfehler.
inline QDateTime parse(QStringView s)
{
    s = s.trimmed();
    if (s.size() != 10 && s.size() != 19)
        return {};
    if (s.at(4) != QLatin1Char('-') || s.at(7) != QLatin1Char('-'))
        return {};
    bool ok1 = false, ok2 = false, ok3 = false;
    const QDate d(s.mid(0, 4).toInt(&ok1), s.mid(5, 2).toInt(&ok2), s.mid(8, 2).toInt(&ok3));
    if (!(ok1 && ok2 && ok3) || !d.isValid())
        return {};
    if (s.size() == 10)
        return naiv(d, QTime(0, 0));
    if (s.at(10) != QLatin1Char(' ') || s.at(13) != QLatin1Char(':') || s.at(16) != QLatin1Char(':'))
        return {};
    const QTime t(s.mid(11, 2).toInt(&ok1), s.mid(14, 2).toInt(&ok2), s.mid(17, 2).toInt(&ok3));
    if (!(ok1 && ok2 && ok3) || !t.isValid())
        return {};
    return naiv(d, t);
}

inline QString text(const QDateTime& dt)
{
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")) : QString();
}

/// Existiert die Uhrzeit als Ortszeit in Deutschland (nicht in der Sommerzeit-Lücke)?
inline bool existiertAlsOrtszeit(const QDateTime& naiv)
{
    static const QTimeZone berlin("Europe/Berlin");
    if (!berlin.isValid())
        return true;
    const QDateTime lokal(naiv.date(), naiv.time(), berlin);
    return lokal.isValid() && lokal.time() == naiv.time();
}

} // namespace Zeit
