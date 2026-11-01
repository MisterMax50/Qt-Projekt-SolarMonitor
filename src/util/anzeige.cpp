#include "anzeige.h"

#include <cmath>

#include <QDateTime>
#include <QLocale>

namespace {
QLocale& locale()
{
    static QLocale l(QLocale::German, QLocale::Germany);
    return l;
}
bool englisch()
{
    return locale().language() == QLocale::English;
}
} // namespace

void Anzeige::setSprache(const QString& sprache)
{
    locale() = (sprache == QLatin1String("en")) ? QLocale(QLocale::English, QLocale::UnitedKingdom)
                                                : QLocale(QLocale::German, QLocale::Germany);
}

QString Anzeige::typName(const QString& roh)
{
    if (roh == QLatin1String("dach_privat"))  return tr("Dach (privat)");
    if (roh == QLatin1String("dach_gewerbe")) return tr("Dach (Gewerbe)");
    if (roh == QLatin1String("freiflaeche"))  return tr("Freifläche");
    if (roh == QLatin1String("balkon"))       return tr("Balkon");
    return roh.isEmpty() ? tr("(leer)") : roh;
}

QString Anzeige::statusName(const QString& roh)
{
    if (roh == QLatin1String("in_betrieb"))   return tr("In Betrieb");
    if (roh == QLatin1String("gestoert"))     return tr("Gestört");
    if (roh == QLatin1String("wartung"))      return tr("Wartung");
    if (roh == QLatin1String("abgeschaltet")) return tr("Abgeschaltet");
    return roh.isEmpty() ? tr("(leer)") : roh;
}

QString Anzeige::stoerungsStatusName(const QString& roh)
{
    if (roh == QLatin1String("offen"))          return tr("Offen");
    if (roh == QLatin1String("in_bearbeitung")) return tr("In Bearbeitung");
    if (roh == QLatin1String("behoben"))        return tr("Behoben");
    return roh.isEmpty() ? tr("(leer)") : roh;
}

QString Anzeige::schwereName(const QString& roh)
{
    if (roh == QLatin1String("info"))     return tr("Info");
    if (roh == QLatin1String("warnung"))  return tr("Warnung");
    if (roh == QLatin1String("fehler"))   return tr("Fehler");
    if (roh == QLatin1String("kritisch")) return tr("Kritisch");
    return roh.isEmpty() ? tr("(leer)") : roh;
}

QString Anzeige::wrStatusName(const QString& roh)
{
    if (roh == QLatin1String("ok"))          return tr("OK");
    if (roh == QLatin1String("standby"))     return tr("Standby");
    if (roh == QLatin1String("stoerung"))    return tr("Störung");
    if (roh == QLatin1String("aus"))         return tr("Aus");
    if (roh == QLatin1String("keine_daten")) return tr("Keine Daten");
    return roh;
}

QColor Anzeige::statusFarbe(const QString& status)
{
    if (status == QLatin1String("in_betrieb"))   return QColor(46, 160, 67);
    if (status == QLatin1String("gestoert"))     return QColor(207, 34, 46);
    if (status == QLatin1String("wartung"))      return QColor(230, 126, 34);
    if (status == QLatin1String("abgeschaltet")) return QColor(110, 110, 110);
    return QColor(160, 160, 160);
}

QColor Anzeige::schwereFarbe(const QString& schwere)
{
    if (schwere == QLatin1String("kritisch")) return QColor(207, 34, 46);
    if (schwere == QLatin1String("fehler"))   return QColor(230, 126, 34);
    if (schwere == QLatin1String("warnung"))  return QColor(191, 144, 0);
    return QColor(90, 110, 140);
}

QColor Anzeige::prFarbe(double pr)
{
    if (!(pr > 0.0))
        return QColor(160, 160, 160);
    // 0,50 -> rot, 0,70 -> gelb, ≥ 0,85 -> grün (lineare Interpolation im HSV-Farbton)
    const double t = qBound(0.0, (pr - 0.5) / 0.35, 1.0);
    return QColor::fromHsvF(float(t * 0.33), 0.85f, 0.85f);
}

QString Anzeige::zahl(double wert, int nachkommastellen)
{
    if (std::isnan(wert))
        return QStringLiteral("–");
    return locale().toString(wert, 'f', nachkommastellen);
}

QString Anzeige::prozent(double anteil, int nachkommastellen)
{
    if (std::isnan(anteil))
        return QStringLiteral("–");
    return locale().toString(anteil * 100.0, 'f', nachkommastellen) + QStringLiteral(" %");
}

QString Anzeige::pr(double pr)
{
    return pr > 0.0 ? prozent(pr, 1) : QStringLiteral("–");
}

QString Anzeige::kwh(double kwh)
{
    if (std::isnan(kwh))
        return QStringLiteral("–");
    if (std::abs(kwh) >= 10000.0)
        return zahl(kwh / 1000.0, 1) + QStringLiteral(" MWh");
    return zahl(kwh, 1) + QStringLiteral(" kWh");
}

QString Anzeige::leistungW(double w)
{
    if (std::isnan(w))
        return QStringLiteral("–");
    if (std::abs(w) >= 1.0e6)
        return zahl(w / 1.0e6, 2) + QStringLiteral(" MW");
    if (std::abs(w) >= 1000.0)
        return zahl(w / 1000.0, 1) + QStringLiteral(" kW");
    return zahl(w, 0) + QStringLiteral(" W");
}

QString Anzeige::euro(double euro)
{
    return zahl(euro, 2) + QStringLiteral(" €");
}

QString Anzeige::datum(const QDate& d)
{
    if (!d.isValid())
        return QStringLiteral("–");
    return d.toString(englisch() ? QStringLiteral("dd/MM/yyyy") : QStringLiteral("dd.MM.yyyy"));
}

QString Anzeige::zeitpunkt(const QDateTime& dt)
{
    if (!dt.isValid())
        return QStringLiteral("–");
    return dt.toString(englisch() ? QStringLiteral("dd/MM/yyyy hh:mm") : QStringLiteral("dd.MM.yyyy hh:mm"));
}
