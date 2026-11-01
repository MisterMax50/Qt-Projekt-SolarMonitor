#include "ertrags_auswertung.h"

#include <algorithm>

#include "eval/schwellwerte.h"
#include "util/anzeige.h"

using namespace Schwellwerte;

namespace {

bool gilt(double w) { return !std::isnan(w); }

QString zeitText(const QDateTime& dt) { return dt.toString(QStringLiteral("dd.MM. hh:mm")); }
QString stundeText(const QDateTime& dt) { return dt.toString(QStringLiteral("hh:mm")); }

} // namespace

// -------------------------------------------------------------------
//  Anomalie-Texte
// -------------------------------------------------------------------

QString Anomalie::typName(Typ typ)
{
    switch (typ) {
    case Typ::KeineEinspeisung: return tr("Keine Einspeisung");
    case Typ::Ausfall:          return tr("Ausfall");
    case Typ::WrStoerung:       return tr("WR-Störung");
    case Typ::Clipping:         return tr("Leistungsbegrenzung");
    case Typ::MorgenSchatten:   return tr("Morgenschatten");
    case Typ::AbendSchatten:    return tr("Abendschatten");
    case Typ::Minderleistung:   return tr("Minderleistung");
    case Typ::Datenluecke:      return tr("Datenlücken");
    }
    return QString();
}

QString Anomalie::text() const
{
    switch (typ) {
    case Typ::KeineEinspeisung:
        return tr("Keine Einspeisung im gesamten Zeitraum (%1) trotz Einstrahlung – "
                  "Anlage oder Wechselrichter ausgefallen.")
            .arg(Anzeige::kwh(wert));
    case Typ::Ausfall:
        if (von.date() == bis.date())
            return tr("Ausfall am %1, %2–%3 Uhr (%4 h): Einstrahlung > 300 W/m², Leistung ≈ 0.")
                .arg(von.toString(QStringLiteral("dd.MM.")), stundeText(von), stundeText(bis),
                     Anzeige::zahl(wert, 2));
        return tr("Ausfälle an %1 Tagen zwischen %2 und %3, zusammen %4 h.")
            .arg(anzahl)
            .arg(von.toString(QStringLiteral("dd.MM.")), bis.toString(QStringLiteral("dd.MM.")),
                 Anzeige::zahl(wert, 1));
    case Typ::WrStoerung:
        return tr("Wechselrichter meldet Störung in %1 Intervallen (%2 h), erstmals %3.")
            .arg(anzahl)
            .arg(Anzeige::zahl(wert, 2), zeitText(von));
    case Typ::Clipping:
        return tr("Leistungsbegrenzung durch den Wechselrichter an %1 Tagen "
                  "(bis %2 h/Tag an der Grenze von %3 kW).")
            .arg(anzahl)
            .arg(Anzeige::zahl(wert, 2), Anzeige::zahl(vergleich, 1));
    case Typ::MorgenSchatten:
        return tr("Mögliche Verschattung am Morgen: Wirkungsgrad vor 10 Uhr nur %1 des Mittagswerts.")
            .arg(Anzeige::prozent(wert, 0));
    case Typ::AbendSchatten:
        return tr("Mögliche Verschattung am Abend: Wirkungsgrad ab 16 Uhr nur %1 des Mittagswerts.")
            .arg(Anzeige::prozent(wert, 0));
    case Typ::Minderleistung:
        if (vergleich > 0.0)
            return tr("Minderleistung: PR %1 liegt deutlich unter dem Median der Wetterstation (%2).")
                .arg(Anzeige::pr(wert), Anzeige::pr(vergleich));
        return tr("Minderleistung: PR %1 ist auffällig niedrig (keine Vergleichsgruppe).")
            .arg(Anzeige::pr(wert));
    case Typ::Datenluecke:
        return tr("%1 von %2 Messpunkten fehlen (%3).")
            .arg(anzahl)
            .arg(int(vergleich))
            .arg(Anzeige::prozent(wert, 1));
    }
    return QString();
}

bool Auswertung::hat(Anomalie::Typ typ) const
{
    return std::any_of(anomalien.cbegin(), anomalien.cend(),
                       [typ](const Anomalie& a) { return a.typ == typ; });
}

bool Auswertung::istAuffaellig() const
{
    return std::any_of(anomalien.cbegin(), anomalien.cend(),
                       [](const Anomalie& a) { return !a.istHinweis(); });
}

// -------------------------------------------------------------------
//  Berechnung
// -------------------------------------------------------------------

double ErtragsAuswertung::median(QVector<double> werte)
{
    werte.erase(std::remove_if(werte.begin(), werte.end(), [](double w) { return !gilt(w); }),
                werte.end());
    if (werte.isEmpty())
        return std::nan("");
    std::sort(werte.begin(), werte.end());
    const int n = werte.size();
    return (n % 2 == 1) ? werte.at(n / 2) : 0.5 * (werte.at(n / 2 - 1) + werte.at(n / 2));
}

Auswertung ErtragsAuswertung::berechne(const Verlauf& v, double ppeakKwp, double acGrenzeKw)
{
    Auswertung a;
    if (!v.istKonsistent()) {
        a.grund = tr("Der Verlauf ist unvollständig.");
        return a;
    }
    if (ppeakKwp <= 0.0) {
        a.grund = tr("Die installierte Leistung ist ungültig (≤ 0 kWp); "
                     "spezifischer Ertrag und PR sind nicht berechenbar.");
        return a;
    }

    const int n = v.anzahl();
    const double dtH = v.intervallStunden();
    const double ppeakW = ppeakKwp * 1000.0;
    const double ausfallGrenzeW = qMax(kAusfallLeistungMinW, kAusfallLeistungAnteil * ppeakW);

    a.messpunkte = n;
    double ePaar = 0.0; // Energie in Intervallen mit P und G [kWh]
    double hPaar = 0.0; // Einstrahlung in Intervallen mit P und G [kWh/m²]
    int paare = 0;

    // Tageswerte (die Zeitstempel sind aufsteigend; ein Tag = ein Block)
    QVector<double> tagEPaar, tagHPaar;
    for (int i = 0; i < n; ++i) {
        const double p = v.leistungW.at(i);
        const double g = v.einstrahlungWm2.at(i);
        const QDate tag = v.zeit.at(i).date();

        if (a.tage.isEmpty() || a.tage.last().tag != tag) {
            TagesErtrag t;
            t.tag = tag;
            a.tage.append(t);
            tagEPaar.append(0.0);
            tagHPaar.append(0.0);
        }
        TagesErtrag& t = a.tage.last();
        ++t.messpunkte;

        if (gilt(p)) {
            t.energieKwh += p * dtH / 1000.0;
            a.energieKwh += p * dtH / 1000.0;
        }
        if (gilt(g)) {
            t.einstrahlungKwhM2 += g * dtH / 1000.0;
            a.einstrahlungKwhM2 += g * dtH / 1000.0;
        }
        if (gilt(p) && gilt(g)) {
            ePaar += p * dtH / 1000.0;
            hPaar += g * dtH / 1000.0;
            tagEPaar.last() += p * dtH / 1000.0;
            tagHPaar.last() += g * dtH / 1000.0;
            ++paare;
        } else {
            ++t.luecken;
            ++a.luecken;
        }
    }
    for (int d = 0; d < a.tage.size(); ++d)
        if (tagHPaar.at(d) > 0.05)
            a.tage[d].pr = tagEPaar.at(d) / (tagHPaar.at(d) * ppeakKwp);

    a.abdeckung = n > 0 ? double(paare) / n : 0.0;
    a.spezErtragKwhKwp = a.energieKwh / ppeakKwp;
    if (hPaar > 0.1)
        a.pr = ePaar / (hPaar * ppeakKwp);
    a.gueltig = true;

    // ---------- 1) Keine Einspeisung im gesamten Zeitraum ----------
    const bool keineEinspeisung = a.einstrahlungKwhM2 > 1.0
        && a.energieKwh < kKeineEinspeisungAnteil * a.einstrahlungKwhM2 * ppeakKwp * kStandardPr;
    if (keineEinspeisung) {
        Anomalie an;
        an.typ = Anomalie::Typ::KeineEinspeisung;
        an.von = v.zeit.first();
        an.bis = v.zeit.last();
        an.wert = a.energieKwh;
        a.anomalien.append(an);
    }

    // ---------- 2) Ausfälle (je Tag zusammengefasst) ----------
    QVector<bool> imAusfall(n, false);
    if (!keineEinspeisung) {
        struct TagAusfall { QDateTime von, bis; int intervalle = 0; };
        QVector<TagAusfall> tage;
        for (int i = 0; i < n; ++i) {
            const double p = v.leistungW.at(i);
            const double g = v.einstrahlungWm2.at(i);
            if (!(gilt(p) && gilt(g) && g > kAusfallEinstrahlungWm2 && p < ausfallGrenzeW))
                continue;
            imAusfall[i] = true;
            if (tage.isEmpty() || tage.last().von.date() != v.zeit.at(i).date())
                tage.append(TagAusfall{v.zeit.at(i), v.zeit.at(i), 0});
            tage.last().bis = v.zeit.at(i);
            ++tage.last().intervalle;
        }
        if (!tage.isEmpty() && tage.size() <= 5) {
            for (const TagAusfall& t : tage) {
                Anomalie an;
                an.typ = Anomalie::Typ::Ausfall;
                an.von = t.von;
                an.bis = t.bis.addSecs(v.intervallMin * 60);
                an.anzahl = t.intervalle;
                an.wert = t.intervalle * dtH;
                a.anomalien.append(an);
            }
        } else if (!tage.isEmpty()) {
            Anomalie an;
            an.typ = Anomalie::Typ::Ausfall;
            an.von = tage.first().von;
            an.bis = tage.last().bis;
            an.anzahl = tage.size();
            for (const TagAusfall& t : tage)
                an.wert += t.intervalle * dtH;
            a.anomalien.append(an);
        }
    }

    // ---------- 3) Wechselrichter meldet Störung ----------
    {
        Anomalie an;
        an.typ = Anomalie::Typ::WrStoerung;
        for (int i = 0; i < n; ++i) {
            if (v.wrStatus.at(i) != QLatin1String("stoerung"))
                continue;
            if (an.anzahl == 0)
                an.von = v.zeit.at(i);
            an.bis = v.zeit.at(i);
            ++an.anzahl;
        }
        an.wert = an.anzahl * dtH;
        if (an.anzahl > 0 && !keineEinspeisung)
            a.anomalien.append(an);
    }

    // ---------- 4) Leistungsbegrenzung durch den Wechselrichter ----------
    if (acGrenzeKw > 0.0 && !keineEinspeisung) {
        const double grenzeW = kClippingAnteil * acGrenzeKw * 1000.0;
        int tageMitClipping = 0;
        int maxIntervalle = 0;
        QDate tag;
        int lauf = 0, maxLaufTag = 0;
        auto tagAbschliessen = [&]() {
            if (maxLaufTag >= kClippingMinIntervalle) {
                ++tageMitClipping;
                maxIntervalle = qMax(maxIntervalle, maxLaufTag);
            }
        };
        for (int i = 0; i < n; ++i) {
            const QDate d = v.zeit.at(i).date();
            if (d != tag) {
                tagAbschliessen();
                tag = d;
                lauf = 0;
                maxLaufTag = 0;
            }
            const double p = v.leistungW.at(i);
            lauf = (gilt(p) && p >= grenzeW) ? lauf + 1 : 0;
            maxLaufTag = qMax(maxLaufTag, lauf);
        }
        tagAbschliessen();
        if (tageMitClipping > 0) {
            Anomalie an;
            an.typ = Anomalie::Typ::Clipping;
            an.anzahl = tageMitClipping;
            an.wert = maxIntervalle * dtH;
            an.vergleich = acGrenzeKw;
            a.anomalien.append(an);
        }
    }

    // ---------- 5) Verschattung am Morgen / Abend ----------
    // Momentaner Wirkungsgrad r = P / (G · Ppeak) je Tageszeit; Ausfälle
    // und WR-Störungen werden ausgeklammert, damit sie nicht als Schatten
    // erscheinen.
    if (!keineEinspeisung) {
        QVector<double> morgen, mittag, abend;
        for (int i = 0; i < n; ++i) {
            const double p = v.leistungW.at(i);
            const double g = v.einstrahlungWm2.at(i);
            if (!gilt(p) || !gilt(g) || g < kSchattenMinEinstrahlungWm2 || imAusfall.at(i)
                || v.wrStatus.at(i) != QLatin1String("ok"))
                continue;
            const double r = p / (g * ppeakKwp);
            const int h = v.zeit.at(i).time().hour();
            if (h < kMorgenBisStunde)
                morgen.append(r);
            else if (h >= kMittagVonStunde && h < kMittagBisStunde)
                mittag.append(r);
            else if (h >= kAbendAbStunde)
                abend.append(r);
        }
        const double rMittag = median(mittag);
        if (mittag.size() >= kSchattenMinMesspunkte && rMittag > 0.2) {
            const double rMorgen = median(morgen);
            if (morgen.size() >= kSchattenMinMesspunkte && rMorgen < kSchattenVerhaeltnis * rMittag) {
                Anomalie an;
                an.typ = Anomalie::Typ::MorgenSchatten;
                an.wert = rMorgen / rMittag;
                a.anomalien.append(an);
            }
            const double rAbend = median(abend);
            if (abend.size() >= kSchattenMinMesspunkte && rAbend < kSchattenVerhaeltnis * rMittag) {
                Anomalie an;
                an.typ = Anomalie::Typ::AbendSchatten;
                an.wert = rAbend / rMittag;
                a.anomalien.append(an);
            }
        }
    }

    // ---------- 6) Datenlücken ----------
    if (n > 0 && a.luecken > kLueckenAnteil * n) {
        Anomalie an;
        an.typ = Anomalie::Typ::Datenluecke;
        an.anzahl = a.luecken;
        an.vergleich = n;
        an.wert = double(a.luecken) / n;
        a.anomalien.append(an);
    }

    return a;
}

void ErtragsAuswertung::bewerteGegenStation(Auswertung& a, double medianPr)
{
    a.anomalien.erase(std::remove_if(a.anomalien.begin(), a.anomalien.end(),
                                     [](const Anomalie& an) {
                                         return an.typ == Anomalie::Typ::Minderleistung;
                                     }),
                      a.anomalien.end());
    if (!a.gueltig || !(a.pr > 0.0) || a.hat(Anomalie::Typ::KeineEinspeisung))
        return;

    const bool vergleichbar = medianPr > 0.0;
    const bool auffaellig = vergleichbar ? a.pr < kMinderleistungRelativ * medianPr
                                         : a.pr < kMinderleistungAbsolut;
    if (!auffaellig)
        return;

    Anomalie an;
    an.typ = Anomalie::Typ::Minderleistung;
    an.wert = a.pr;
    an.vergleich = vergleichbar ? medianPr : 0.0;
    a.anomalien.append(an);
}
