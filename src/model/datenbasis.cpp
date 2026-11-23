#include "datenbasis.h"

#include <algorithm>

#include <QDir>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

#include "repo/csv_parser.h"
#include "util/zeit.h"

namespace {

const QString kDateiAnlagen         = QStringLiteral("anlagen.csv");
const QString kDateiWechselrichter  = QStringLiteral("wechselrichter.csv");
const QString kDateiWetterstationen = QStringLiteral("wetterstationen.csv");
const QString kDateiStoerungen      = QStringLiteral("stoerungen.csv");

const QString kDatumFormat = QStringLiteral("yyyy-MM-dd");

/// Zahl für die CSV-Ausgabe: Punkt als Dezimaltrenner, ohne überflüssige Nullen.
QString zahlText(double w, int maxNachkomma = 5)
{
    QString s = QString::number(w, 'f', maxNachkomma);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

/// Aus Stationskoordinaten ableiten, ob lat/lon offensichtlich vertauscht sind.
bool koordinatenVertauscht(double lat, double lon)
{
    const bool latOk = lat >= 46.5 && lat <= 49.0;
    const bool lonOk = lon >= 7.5 && lon <= 11.0;
    const bool latAlsLon = lat >= 7.5 && lat <= 11.0;
    const bool lonAlsLat = lon >= 46.5 && lon <= 49.0;
    return !latOk && !lonOk && latAlsLon && lonAlsLat;
}

} // namespace

// -------------------------------------------------------------------
//  Konstanten
// -------------------------------------------------------------------

const QStringList& Datenbasis::gueltigeTypen()
{
    static const QStringList l{QStringLiteral("dach_privat"), QStringLiteral("dach_gewerbe"),
                               QStringLiteral("freiflaeche"), QStringLiteral("balkon")};
    return l;
}

const QStringList& Datenbasis::gueltigeStatus()
{
    static const QStringList l{QStringLiteral("in_betrieb"), QStringLiteral("gestoert"),
                               QStringLiteral("wartung"), QStringLiteral("abgeschaltet")};
    return l;
}

const QStringList& Datenbasis::gueltigeSchweren()
{
    static const QStringList l{QStringLiteral("info"), QStringLiteral("warnung"),
                               QStringLiteral("fehler"), QStringLiteral("kritisch")};
    return l;
}

const QStringList& Datenbasis::gueltigeStoerungsStatus()
{
    static const QStringList l{QStringLiteral("offen"), QStringLiteral("in_bearbeitung"),
                               QStringLiteral("behoben")};
    return l;
}

Datenbasis::Datenbasis(QObject* parent)
    : QObject(parent)
{
}

void Datenbasis::melde(Pruefmeldung::Stufe stufe, const QString& datei, int zeile,
                       const QString& objektId, const QString& text)
{
    m_meldungen.append(Pruefmeldung{stufe, datei, zeile, objektId, text});
}

// -------------------------------------------------------------------
//  Laden
// -------------------------------------------------------------------

bool Datenbasis::lade(const QString& datenOrdner)
{
    m_datenOrdner = QDir(datenOrdner).absolutePath();
    m_anlagen.clear();
    m_anlageIndex.clear();
    m_wechselrichter.clear();
    m_wetterstationen.clear();
    m_stoerungen.clear();
    m_stoerungIndex.clear();
    m_meldungen.clear();

    // Referenzdaten zuerst, damit Verweise der Anlagen prüfbar sind.
    ladeWetterstationen();
    ladeWechselrichter();
    ladeAnlagen();
    ladeStoerungen();
    baueStoerungsIndex();

    return !m_anlagen.isEmpty();
}

void Datenbasis::ladeWetterstationen()
{
    CsvReader::Tabelle t;
    try {
        t = CsvReader::lade(QDir(m_datenOrdner).filePath(kDateiWetterstationen),
                            {QStringLiteral("station_id"), QStringLiteral("lat"), QStringLiteral("lon")});
    } catch (const DateiFehler& e) {
        melde(Pruefmeldung::Stufe::Fehler, kDateiWetterstationen, 0, QString(),
              tr("%1 – Wetterstationsdaten fehlen; Verweise werden nicht aufgelöst.").arg(e.meldung()));
        return;
    }

    for (const auto& z : t.zeilen) {
        Wetterstation w;
        w.stationId = t.wert(z, QStringLiteral("station_id"));
        w.name      = t.wert(z, QStringLiteral("name"));
        w.ort       = t.wert(z, QStringLiteral("ort"));
        if (w.stationId.isEmpty()) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, QString(),
                  tr("Zeile ohne station_id übersprungen."));
            continue;
        }
        if (m_wetterstationen.contains(w.stationId)) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, w.stationId,
                  tr("Doppelte station_id – Zeile übersprungen."));
            continue;
        }
        w.lat    = CsvReader::zahl(t.wert(z, QStringLiteral("lat"))).value_or(0.0);
        w.lon    = CsvReader::zahl(t.wert(z, QStringLiteral("lon"))).value_or(0.0);
        w.hoeheM = CsvReader::zahl(t.wert(z, QStringLiteral("hoehe_m"))).value_or(0.0);

        if (koordinatenVertauscht(w.lat, w.lon)) {
            std::swap(w.lat, w.lon);
            melde(Pruefmeldung::Stufe::Info, t.dateiname, z.zeilenNr, w.stationId,
                  tr("Breiten- und Längengrad waren vertauscht und wurden getauscht."));
        } else if (w.lat < 46.5 || w.lat > 49.0 || w.lon < 7.5 || w.lon > 11.0) {
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, w.stationId,
                  tr("Koordinaten (%1, %2) liegen außerhalb der Bodenseeregion.")
                      .arg(zahlText(w.lat), zahlText(w.lon)));
        }
        m_wetterstationen.insert(w.stationId, w);
    }
}

void Datenbasis::ladeWechselrichter()
{
    CsvReader::Tabelle t;
    try {
        t = CsvReader::lade(QDir(m_datenOrdner).filePath(kDateiWechselrichter),
                            {QStringLiteral("wechselrichter_id"), QStringLiteral("nennleistung_kw")});
    } catch (const DateiFehler& e) {
        melde(Pruefmeldung::Stufe::Fehler, kDateiWechselrichter, 0, QString(),
              tr("%1 – Wechselrichterdaten fehlen; Clipping-Erkennung eingeschränkt.").arg(e.meldung()));
        return;
    }

    for (const auto& z : t.zeilen) {
        Wechselrichter w;
        w.wechselrichterId = t.wert(z, QStringLiteral("wechselrichter_id"));
        w.hersteller       = t.wert(z, QStringLiteral("hersteller"));
        w.modell           = t.wert(z, QStringLiteral("modell"));
        if (w.wechselrichterId.isEmpty()) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, QString(),
                  tr("Zeile ohne wechselrichter_id übersprungen."));
            continue;
        }
        if (m_wechselrichter.contains(w.wechselrichterId)) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, w.wechselrichterId,
                  tr("Doppelte wechselrichter_id – Zeile übersprungen."));
            continue;
        }
        w.nennleistungKw         = CsvReader::zahl(t.wert(z, QStringLiteral("nennleistung_kw"))).value_or(0.0);
        w.maxWirkungsgradProzent = CsvReader::zahl(t.wert(z, QStringLiteral("max_wirkungsgrad_prozent"))).value_or(0.0);
        w.phasen     = qRound(CsvReader::zahl(t.wert(z, QStringLiteral("phasen"))).value_or(0.0));
        w.mppTracker = qRound(CsvReader::zahl(t.wert(z, QStringLiteral("mpp_tracker"))).value_or(0.0));

        if (w.nennleistungKw <= 0.0)
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, w.wechselrichterId,
                  tr("Nennleistung %1 kW ist nicht positiv.").arg(zahlText(w.nennleistungKw)));
        if (w.maxWirkungsgradProzent <= 0.0 || w.maxWirkungsgradProzent > 100.0)
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, w.wechselrichterId,
                  tr("Wirkungsgrad %1 % ist physikalisch unmöglich (0–100 %).")
                      .arg(zahlText(w.maxWirkungsgradProzent)));
        if (w.phasen != 1 && w.phasen != 3)
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, w.wechselrichterId,
                  tr("Ungewöhnliche Phasenzahl %1.").arg(w.phasen));
        m_wechselrichter.insert(w.wechselrichterId, w);
    }
}

void Datenbasis::ladeAnlagen()
{
    CsvReader::Tabelle t;
    try {
        t = CsvReader::lade(QDir(m_datenOrdner).filePath(kDateiAnlagen),
                            {QStringLiteral("anlage_id"), QStringLiteral("typ"),
                             QStringLiteral("leistung_kwp"), QStringLiteral("status")});
    } catch (const DateiFehler& e) {
        melde(Pruefmeldung::Stufe::Fehler, kDateiAnlagen, 0, QString(),
              tr("%1 – Ohne Anlagendaten ist kein Betrieb möglich.").arg(e.meldung()));
        return;
    }

    m_anlagen.reserve(t.zeilen.size());
    for (const auto& z : t.zeilen) {
        const QString id = t.wert(z, QStringLiteral("anlage_id"));

        if (z.felder.size() != t.kopf.size()) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, id,
                  tr("Zeile hat %1 statt %2 Spalten – Zuordnung unsicher, Zeile übersprungen.")
                      .arg(z.felder.size()).arg(t.kopf.size()));
            continue;
        }
        if (id.isEmpty()) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, QString(),
                  tr("Zeile ohne anlage_id übersprungen."));
            continue;
        }
        if (m_anlageIndex.contains(id)) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, id,
                  tr("Doppelte anlage_id – Zeile übersprungen, der erste Eintrag bleibt gültig."));
            continue;
        }

        Anlage a;
        a.anlageId         = id;
        a.typ              = t.wert(z, QStringLiteral("typ"));
        a.ort              = t.wert(z, QStringLiteral("ort"));
        a.wechselrichterId = t.wert(z, QStringLiteral("wechselrichter_id"));
        a.wetterstationId  = t.wert(z, QStringLiteral("wetterstation_id"));
        a.status           = t.wert(z, QStringLiteral("status"));

        // Zahlenfelder: nicht lesbare Werte melden, Dezimalkomma tolerieren.
        auto lies = [&](const QString& spalte, double standard) -> double {
            const QString roh = t.wert(z, spalte);
            bool komma = false;
            const std::optional<double> w = CsvReader::zahl(roh, &komma);
            if (!w) {
                melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, id,
                      tr("%1: \"%2\" ist keine gültige Zahl.").arg(spalte, roh));
                return standard;
            }
            if (komma)
                melde(Pruefmeldung::Stufe::Info, t.dateiname, z.zeilenNr, id,
                      tr("%1: Dezimalkomma in \"%2\" als Punkt gelesen.").arg(spalte, roh));
            return *w;
        };
        a.lat             = lies(QStringLiteral("lat"), 0.0);
        a.lon             = lies(QStringLiteral("lon"), 0.0);
        a.leistungKwp     = lies(QStringLiteral("leistung_kwp"), 0.0);
        a.ausrichtungGrad = qRound(lies(QStringLiteral("ausrichtung_grad"), 180.0));
        a.neigungGrad     = qRound(lies(QStringLiteral("neigung_grad"), 30.0));
        a.anzahlWr        = qRound(lies(QStringLiteral("anzahl_wr"), 1.0));
        a.verguetungCtKwh = lies(QStringLiteral("verguetung_ct_kwh"), 0.0);

        const QString ib = t.wert(z, QStringLiteral("inbetriebnahme"));
        a.inbetriebnahme = QDate::fromString(ib, kDatumFormat);
        if (!a.inbetriebnahme.isValid())
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, id,
                  tr("inbetriebnahme \"%1\" ist kein gültiges Datum (JJJJ-MM-TT).").arg(ib));

        for (Pruefmeldung m : pruefeAnlage(a)) {
            m.datei = t.dateiname;
            m.zeile = z.zeilenNr;
            m_meldungen.append(m);
        }

        m_anlageIndex.insert(id, m_anlagen.size());
        m_anlagen.append(a);
    }

    if (m_anlagen.isEmpty())
        melde(Pruefmeldung::Stufe::Fehler, t.dateiname, 0, QString(),
              tr("Keine gültigen Anlagen in %1.").arg(t.dateiname));
}

QVector<Pruefmeldung> Datenbasis::pruefeAnlage(const Anlage& a) const
{
    QVector<Pruefmeldung> m;
    auto warnung = [&](const QString& text) {
        m.append(Pruefmeldung{Pruefmeldung::Stufe::Warnung, kDateiAnlagen, 0, a.anlageId, text});
    };

    if (!gueltigeTypen().contains(a.typ))
        warnung(tr("Unbekannter Typ \"%1\".").arg(a.typ));
    if (!gueltigeStatus().contains(a.status))
        warnung(tr("Unbekannter Status \"%1\".").arg(a.status));
    if (a.leistungKwp <= 0.0)
        warnung(tr("Leistung %1 kWp ist nicht positiv – Ertragskennzahlen werden nicht berechnet.")
                    .arg(zahlText(a.leistungKwp)));
    else if (a.typ == QLatin1String("balkon") && a.leistungKwp > 2.0)
        warnung(tr("Balkonkraftwerk mit %1 kWp ist unplausibel groß.").arg(zahlText(a.leistungKwp)));
    if (a.ausrichtungGrad < 0 || a.ausrichtungGrad >= 360)
        warnung(tr("Ausrichtung %1° liegt außerhalb 0–359°.").arg(a.ausrichtungGrad));
    if (a.neigungGrad < 0 || a.neigungGrad > 90)
        warnung(tr("Neigung %1° liegt außerhalb 0–90°.").arg(a.neigungGrad));
    if (a.anzahlWr < 1)
        warnung(tr("Anzahl Wechselrichter %1 ist kleiner als 1.").arg(a.anzahlWr));
    if (a.verguetungCtKwh < 0.0 || a.verguetungCtKwh > 100.0)
        warnung(tr("Vergütung %1 ct/kWh ist unplausibel.").arg(zahlText(a.verguetungCtKwh)));
    if (a.inbetriebnahme.isValid() && a.inbetriebnahme > QDate::currentDate())
        warnung(tr("Inbetriebnahme %1 liegt in der Zukunft.")
                    .arg(a.inbetriebnahme.toString(kDatumFormat)));
    if (!a.hatGueltigeKoordinaten())
        warnung(tr("Koordinaten (%1, %2) liegen außerhalb der Bodenseeregion.")
                    .arg(zahlText(a.lat), zahlText(a.lon)));

    if (!m_wetterstationen.isEmpty() && !m_wetterstationen.contains(a.wetterstationId))
        warnung(tr("Wetterstation \"%1\" existiert nicht in wetterstationen.csv.").arg(a.wetterstationId));

    const Wechselrichter* wr = wechselrichter(a.wechselrichterId);
    if (!m_wechselrichter.isEmpty() && !wr) {
        warnung(tr("Wechselrichter \"%1\" existiert nicht in wechselrichter.csv.").arg(a.wechselrichterId));
    } else if (wr && wr->nennleistungKw > 0.0 && a.anzahlWr > 0 && a.leistungKwp > 0.0) {
        const double acKw = wr->nennleistungKw * a.anzahlWr;
        if (a.leistungKwp / acKw > 2.0)
            warnung(tr("Wechselrichter stark unterdimensioniert: %1 kWp an %2 kW AC.")
                        .arg(zahlText(a.leistungKwp), zahlText(acKw)));
    }
    return m;
}

void Datenbasis::ladeStoerungen()
{
    CsvReader::Tabelle t;
    try {
        t = CsvReader::lade(QDir(m_datenOrdner).filePath(kDateiStoerungen),
                            {QStringLiteral("stoerung_id"), QStringLiteral("anlage_id"),
                             QStringLiteral("status")});
    } catch (const DateiFehler& e) {
        melde(Pruefmeldung::Stufe::Fehler, kDateiStoerungen, 0, QString(),
              tr("%1 – Die Störungsliste bleibt leer.").arg(e.meldung()));
        return;
    }

    m_stoerungen.reserve(t.zeilen.size());
    for (const auto& z : t.zeilen) {
        Stoerung s;
        s.stoerungId   = t.wert(z, QStringLiteral("stoerung_id"));
        s.anlageId     = t.wert(z, QStringLiteral("anlage_id"));
        s.fehlercode   = t.wert(z, QStringLiteral("fehlercode"));
        s.beschreibung = t.wert(z, QStringLiteral("beschreibung"));
        s.schwere      = t.wert(z, QStringLiteral("schwere"));
        s.status       = t.wert(z, QStringLiteral("status"));

        if (s.stoerungId.isEmpty() || s.anlageId.isEmpty()) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("Störung ohne stoerung_id oder anlage_id übersprungen."));
            continue;
        }
        if (m_stoerungIndex.contains(s.stoerungId)) {
            melde(Pruefmeldung::Stufe::Fehler, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("Doppelte stoerung_id – Zeile übersprungen."));
            continue;
        }

        const QString zp = t.wert(z, QStringLiteral("zeitpunkt"));
        s.zeitpunkt = Zeit::parse(zp);
        if (!s.zeitpunkt.isValid())
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("zeitpunkt \"%1\" ist ungültig.").arg(zp));
        else if (!Zeit::existiertAlsOrtszeit(s.zeitpunkt))
            melde(Pruefmeldung::Stufe::Info, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("zeitpunkt %1 liegt in der Lücke der Sommerzeitumstellung "
                     "(als Ortszeit nicht existent), wird unverändert übernommen.").arg(zp));

        const QString ba = t.wert(z, QStringLiteral("behoben_am"));
        if (!ba.isEmpty()) {
            s.behobenAm = Zeit::parse(ba);
            if (!s.behobenAm.isValid())
                melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                      tr("behoben_am \"%1\" ist ungültig.").arg(ba));
        }

        if (!m_anlageIndex.contains(s.anlageId))
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("Verweist auf unbekannte Anlage \"%1\".").arg(s.anlageId));
        if (!gueltigeSchweren().contains(s.schwere))
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("Unbekannte Schwere \"%1\".").arg(s.schwere));
        if (!gueltigeStoerungsStatus().contains(s.status))
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("Unbekannter Status \"%1\".").arg(s.status));
        if (s.status == QLatin1String("behoben") && !s.behobenAm.isValid())
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("Status \"behoben\", aber kein behoben_am."));
        if (s.status != QLatin1String("behoben") && s.behobenAm.isValid())
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("behoben_am gesetzt, obwohl die Störung nicht behoben ist."));
        if (s.behobenAm.isValid() && s.zeitpunkt.isValid() && s.behobenAm < s.zeitpunkt)
            melde(Pruefmeldung::Stufe::Warnung, t.dateiname, z.zeilenNr, s.stoerungId,
                  tr("behoben_am liegt vor dem Störungszeitpunkt."));

        m_stoerungIndex.insert(s.stoerungId, m_stoerungen.size());
        m_stoerungen.append(s);
    }
}

void Datenbasis::baueStoerungsIndex()
{
    m_offen.clear();
    m_offenKritisch.clear();
    for (const Stoerung& s : std::as_const(m_stoerungen)) {
        if (!s.istOffen())
            continue;
        ++m_offen[s.anlageId];
        if (s.istKritisch())
            ++m_offenKritisch[s.anlageId];
    }
}

// -------------------------------------------------------------------
//  Lesezugriff
// -------------------------------------------------------------------

const Anlage* Datenbasis::anlage(const QString& anlageId) const
{
    const auto it = m_anlageIndex.constFind(anlageId);
    return it == m_anlageIndex.constEnd() ? nullptr : &m_anlagen.at(it.value());
}

const Wechselrichter* Datenbasis::wechselrichter(const QString& id) const
{
    const auto it = m_wechselrichter.constFind(id);
    return it == m_wechselrichter.constEnd() ? nullptr : &it.value();
}

const Wetterstation* Datenbasis::wetterstation(const QString& id) const
{
    const auto it = m_wetterstationen.constFind(id);
    return it == m_wetterstationen.constEnd() ? nullptr : &it.value();
}

QVector<Wechselrichter> Datenbasis::wechselrichterListe() const
{
    QVector<Wechselrichter> l(m_wechselrichter.cbegin(), m_wechselrichter.cend());
    std::sort(l.begin(), l.end(), [](const Wechselrichter& a, const Wechselrichter& b) {
        return a.wechselrichterId < b.wechselrichterId;
    });
    return l;
}

QVector<Wetterstation> Datenbasis::wetterstationen() const
{
    QVector<Wetterstation> l(m_wetterstationen.cbegin(), m_wetterstationen.cend());
    std::sort(l.begin(), l.end(), [](const Wetterstation& a, const Wetterstation& b) {
        return a.stationId < b.stationId;
    });
    return l;
}

QStringList Datenbasis::orte() const
{
    QSet<QString> s;
    for (const Anlage& a : m_anlagen)
        if (!a.ort.isEmpty())
            s.insert(a.ort);
    QStringList l(s.cbegin(), s.cend());
    l.sort(Qt::CaseInsensitive);
    return l;
}

const Stoerung* Datenbasis::stoerung(const QString& stoerungId) const
{
    const auto it = m_stoerungIndex.constFind(stoerungId);
    return it == m_stoerungIndex.constEnd() ? nullptr : &m_stoerungen.at(it.value());
}

QVector<Stoerung> Datenbasis::stoerungenVon(const QString& anlageId) const
{
    QVector<Stoerung> l;
    for (const Stoerung& s : m_stoerungen)
        if (s.anlageId == anlageId)
            l.append(s);
    std::sort(l.begin(), l.end(), [](const Stoerung& a, const Stoerung& b) {
        return a.zeitpunkt > b.zeitpunkt; // neueste zuerst
    });
    return l;
}

QHash<QString, QString> Datenbasis::fehlercodes() const
{
    QHash<QString, QString> h;
    for (const Stoerung& s : m_stoerungen)
        if (!s.fehlercode.isEmpty() && !h.contains(s.fehlercode))
            h.insert(s.fehlercode, s.beschreibung);
    return h;
}

QVector<Pruefmeldung> Datenbasis::pruefmeldungenFuer(const QString& objektId) const
{
    QVector<Pruefmeldung> l;
    for (const Pruefmeldung& m : m_meldungen)
        if (m.objektId == objektId)
            l.append(m);
    return l;
}

int Datenbasis::anzahlMeldungen(Pruefmeldung::Stufe stufe) const
{
    return int(std::count_if(m_meldungen.cbegin(), m_meldungen.cend(),
                             [stufe](const Pruefmeldung& m) { return m.stufe == stufe; }));
}

void Datenbasis::ergaenzeMeldungen(const QVector<Pruefmeldung>& meldungen)
{
    m_meldungen += meldungen;
    emit meldungenGeaendert();
}

void Datenbasis::entferneMeldungen(const QString& datei)
{
    m_meldungen.erase(std::remove_if(m_meldungen.begin(), m_meldungen.end(),
                                     [&](const Pruefmeldung& m) { return m.datei == datei; }),
                      m_meldungen.end());
    emit meldungenGeaendert();
}

// -------------------------------------------------------------------
//  Ändern
// -------------------------------------------------------------------

bool Datenbasis::aktualisiereAnlage(const Anlage& neu, QString* fehler)
{
    const auto it = m_anlageIndex.constFind(neu.anlageId);
    if (it == m_anlageIndex.constEnd()) {
        if (fehler)
            *fehler = tr("Anlage %1 ist unbekannt.").arg(neu.anlageId);
        return false;
    }
    m_anlagen[it.value()] = neu;

    // Alte Prüfmeldungen dieser Anlage aus anlagen.csv durch neue ersetzen.
    m_meldungen.erase(std::remove_if(m_meldungen.begin(), m_meldungen.end(),
                                     [&](const Pruefmeldung& m) {
                                         return m.objektId == neu.anlageId && m.datei == kDateiAnlagen;
                                     }),
                      m_meldungen.end());
    m_meldungen += pruefeAnlage(neu);

    emit anlageGeaendert(neu.anlageId);
    emit meldungenGeaendert();
    return true;
}

void Datenbasis::setzeSimulierteStoerungen(const QVector<Stoerung>& simulierte)
{
    if (!m_simuliert) {
        m_csvStoerungen = m_stoerungen;
        m_simuliert = true;
    }
    m_stoerungen = simulierte;
    m_stoerungIndex.clear();
    for (int i = 0; i < m_stoerungen.size(); ++i)
        m_stoerungIndex.insert(m_stoerungen.at(i).stoerungId, i);
    baueStoerungsIndex();
    emit stoerungenGeaendert(QString());
}

void Datenbasis::stelleCsvStoerungenWiederHer()
{
    if (!m_simuliert)
        return;
    m_stoerungen = m_csvStoerungen;
    m_csvStoerungen.clear();
    m_simuliert = false;
    m_stoerungIndex.clear();
    for (int i = 0; i < m_stoerungen.size(); ++i)
        m_stoerungIndex.insert(m_stoerungen.at(i).stoerungId, i);
    baueStoerungsIndex();
    emit stoerungenGeaendert(QString());
}

bool Datenbasis::aktualisiereStoerung(const Stoerung& neu, QString* fehler)
{
    if (m_simuliert) {
        if (fehler)
            *fehler = tr("Im Simulationsmodus sind Störungen schreibgeschützt.");
        return false;
    }
    const auto it = m_stoerungIndex.constFind(neu.stoerungId);
    if (it == m_stoerungIndex.constEnd()) {
        if (fehler)
            *fehler = tr("Störung %1 ist unbekannt.").arg(neu.stoerungId);
        return false;
    }
    m_stoerungen[it.value()] = neu;
    baueStoerungsIndex();
    emit stoerungenGeaendert(neu.anlageId);
    return true;
}

QString Datenbasis::erfasseStoerung(Stoerung neu)
{
    if (m_simuliert)
        return QString(); // schreibgeschützt: keine ID vergeben
    int maxNr = 0;
    for (const Stoerung& s : std::as_const(m_stoerungen)) {
        bool ok = false;
        const int nr = s.stoerungId.mid(2).toInt(&ok);
        if (ok)
            maxNr = qMax(maxNr, nr);
    }
    neu.stoerungId = QStringLiteral("ST%1").arg(maxNr + 1, 5, 10, QLatin1Char('0'));
    m_stoerungIndex.insert(neu.stoerungId, m_stoerungen.size());
    m_stoerungen.append(neu);
    baueStoerungsIndex();
    emit stoerungenGeaendert(neu.anlageId);
    return neu.stoerungId;
}

bool Datenbasis::fuegeStoerungEin(const Stoerung& stoerung)
{
    if (m_simuliert || stoerung.stoerungId.isEmpty() || m_stoerungIndex.contains(stoerung.stoerungId))
        return false;
    m_stoerungIndex.insert(stoerung.stoerungId, m_stoerungen.size());
    m_stoerungen.append(stoerung);
    baueStoerungsIndex();
    emit stoerungenGeaendert(stoerung.anlageId);
    return true;
}

bool Datenbasis::entferneStoerung(const QString& stoerungId)
{
    if (m_simuliert)
        return false;
    const auto it = m_stoerungIndex.constFind(stoerungId);
    if (it == m_stoerungIndex.constEnd())
        return false;
    const QString anlageId = m_stoerungen.at(it.value()).anlageId;
    m_stoerungen.removeAt(it.value());
    m_stoerungIndex.clear();
    for (int i = 0; i < m_stoerungen.size(); ++i)
        m_stoerungIndex.insert(m_stoerungen.at(i).stoerungId, i);
    baueStoerungsIndex();
    emit stoerungenGeaendert(anlageId);
    return true;
}

// -------------------------------------------------------------------
//  Export
// -------------------------------------------------------------------

QString Datenbasis::ausgabePfad(const QString& dateiname) const
{
    return QDir(m_datenOrdner).filePath(dateiname);
}

bool Datenbasis::exportiereAnlagen(const QString& pfad, QString* fehler) const
{
    // QSaveFile schreibt in eine temporäre Datei und ersetzt erst bei
    // Erfolg: eine abgebrochene Ausgabe hinterlässt keine halbe CSV.
    QSaveFile datei(pfad);
    if (!datei.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (fehler)
            *fehler = tr("%1 konnte nicht geschrieben werden: %2").arg(pfad, datei.errorString());
        return false;
    }

    QTextStream out(&datei);
    out.setEncoding(QStringConverter::Utf8);
    out << "anlage_id,typ,ort,lat,lon,leistung_kwp,ausrichtung_grad,neigung_grad,"
           "wechselrichter_id,anzahl_wr,wetterstation_id,inbetriebnahme,verguetung_ct_kwh,status\n";
    for (const Anlage& a : m_anlagen) {
        out << CsvReader::maskiere(a.anlageId) << ',' << CsvReader::maskiere(a.typ) << ','
            << CsvReader::maskiere(a.ort) << ',' << zahlText(a.lat) << ',' << zahlText(a.lon) << ','
            << zahlText(a.leistungKwp, 3) << ',' << a.ausrichtungGrad << ',' << a.neigungGrad << ','
            << CsvReader::maskiere(a.wechselrichterId) << ',' << a.anzahlWr << ','
            << CsvReader::maskiere(a.wetterstationId) << ','
            << (a.inbetriebnahme.isValid() ? a.inbetriebnahme.toString(kDatumFormat) : QString()) << ','
            << zahlText(a.verguetungCtKwh, 2) << ',' << CsvReader::maskiere(a.status) << '\n';
    }
    out.flush();

    if (!datei.commit()) {
        if (fehler)
            *fehler = tr("%1 konnte nicht gespeichert werden: %2").arg(pfad, datei.errorString());
        return false;
    }
    return true;
}

bool Datenbasis::exportiereStoerungen(const QString& pfad, QString* fehler) const
{
    if (m_simuliert) {
        if (fehler)
            *fehler = tr("Im Simulationsmodus werden keine Störungen gespeichert.");
        return false;
    }
    QSaveFile datei(pfad);
    if (!datei.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (fehler)
            *fehler = tr("%1 konnte nicht geschrieben werden: %2").arg(pfad, datei.errorString());
        return false;
    }

    QTextStream out(&datei);
    out.setEncoding(QStringConverter::Utf8);
    out << "stoerung_id,anlage_id,zeitpunkt,fehlercode,beschreibung,schwere,status,behoben_am\n";
    for (const Stoerung& s : m_stoerungen) {
        out << CsvReader::maskiere(s.stoerungId) << ',' << CsvReader::maskiere(s.anlageId) << ','
            << (s.zeitpunkt.isValid() ? Zeit::text(s.zeitpunkt) : QString()) << ','
            << CsvReader::maskiere(s.fehlercode) << ',' << CsvReader::maskiere(s.beschreibung) << ','
            << CsvReader::maskiere(s.schwere) << ',' << CsvReader::maskiere(s.status) << ','
            << (s.behobenAm.isValid() ? Zeit::text(s.behobenAm) : QString()) << '\n';
    }
    out.flush();

    if (!datei.commit()) {
        if (fehler)
            *fehler = tr("%1 konnte nicht gespeichert werden: %2").arg(pfad, datei.errorString());
        return false;
    }
    return true;
}
