#include "flotten_analyse.h"

#include <QtConcurrent/QtConcurrentMap>

#include "eval/schwellwerte.h"
#include "model/datenbasis.h"
#include "repo/verlauf_repository.h"

FlottenAnalyse::FlottenAnalyse(Datenbasis* datenbasis, VerlaufRepository* verlaeufe, QObject* parent)
    : QObject(parent)
    , m_datenbasis(datenbasis)
    , m_verlaeufe(verlaeufe)
{
    connect(&m_watcher, &QFutureWatcher<Kennzahl>::progressValueChanged, this,
            [this](int wert) { emit fortschritt(wert, m_watcher.progressMaximum()); });
    connect(&m_watcher, &QFutureWatcher<Kennzahl>::finished, this, &FlottenAnalyse::auswerten);
}

FlottenAnalyse::~FlottenAnalyse()
{
    abbrechen();
}

void FlottenAnalyse::abbrechen()
{
    if (m_watcher.isRunning()) {
        m_watcher.cancel();
        m_watcher.waitForFinished();
    }
}

FlottenAnalyse::Auftrag FlottenAnalyse::auftragFuer(const QString& anlageId) const
{
    Auftrag a;
    a.anlageId = anlageId;
    a.pfad = m_verlaeufe->pfadFuer(anlageId);
    if (const Anlage* an = m_datenbasis->anlage(anlageId)) {
        a.stationId = an->wetterstationId;
        a.ppeakKwp = an->leistungKwp;
        if (const Wechselrichter* wr = m_datenbasis->wechselrichter(an->wechselrichterId))
            a.acGrenzeKw = wr->nennleistungKw * qMax(1, an->anzahlWr);
    }
    return a;
}

void FlottenAnalyse::starte()
{
    abbrechen();
    m_fertig = false;

    QList<Auftrag> auftraege;
    for (const Anlage& a : m_datenbasis->anlagen())
        if (m_verlaeufe->hatVerlauf(a.anlageId))
            auftraege.append(auftragFuer(a.anlageId));

    emit fortschritt(0, int(auftraege.size()));
    m_watcher.setFuture(QtConcurrent::mapped(auftraege, &FlottenAnalyse::analysiere));
}

Kennzahl FlottenAnalyse::analysiere(const Auftrag& auftrag)
{
    Kennzahl k;
    k.anlageId = auftrag.anlageId;
    k.stationId = auftrag.stationId;

    const VerlaufLadeErgebnis e = VerlaufRepository::lese(auftrag.pfad, auftrag.anlageId);
    if (!e.ok()) {
        k.fehler = e.fehler;
        return k;
    }
    k.auswertung = ErtragsAuswertung::berechne(*e.verlauf, auftrag.ppeakKwp, auftrag.acGrenzeKw);
    k.auswertung.tage.clear(); // Tageswerte braucht nur der Detaildialog
    k.ok = k.auswertung.gueltig;
    if (!k.ok)
        k.fehler = k.auswertung.grund;
    return k;
}

void FlottenAnalyse::auswerten()
{
    if (m_watcher.isCanceled())
        return;

    m_kennzahlen.clear();
    const QList<Kennzahl> ergebnisse = m_watcher.future().results();
    for (const Kennzahl& k : ergebnisse)
        m_kennzahlen.insert(k.anlageId, k);

    // PR-Mediane je Station; Anlagen ohne Einspeisung verfälschen den
    // Median nicht, weil sie keine gültige PR besitzen.
    QHash<QString, QVector<double>> proStation;
    QVector<double> alle;
    for (const Kennzahl& k : std::as_const(m_kennzahlen)) {
        if (!k.ok || !(k.auswertung.pr > 0.0)
            || k.auswertung.hat(Anomalie::Typ::KeineEinspeisung))
            continue;
        proStation[k.stationId].append(k.auswertung.pr);
        alle.append(k.auswertung.pr);
    }
    m_medianStation.clear();
    for (auto it = proStation.cbegin(); it != proStation.cend(); ++it)
        if (it.value().size() >= Schwellwerte::kMinVergleichsanlagen)
            m_medianStation.insert(it.key(), ErtragsAuswertung::median(it.value()));
    m_medianFlotte = ErtragsAuswertung::median(alle);

    for (Kennzahl& k : m_kennzahlen)
        bewerte(k);

    m_fertig = true;
    emit fertig();
}

void FlottenAnalyse::bewerte(Kennzahl& k) const
{
    k.medianPrStation = medianPrStation(k.stationId);
    if (!k.ok)
        return;
    ErtragsAuswertung::bewerteGegenStation(k.auswertung, k.medianPrStation);

    const Anlage* a = m_datenbasis->anlage(k.anlageId);
    if (!a)
        return;
    const double prRef = k.medianPrStation > 0.0 ? k.medianPrStation : Schwellwerte::kStandardPr;
    k.sollKwh = k.auswertung.einstrahlungKwhM2 * a->leistungKwp * prRef;
    k.verlustKwh = qMax(0.0, k.sollKwh - k.auswertung.energieKwh);
    k.verlustEuro = k.verlustKwh * a->verguetungCtKwh / 100.0;
}

void FlottenAnalyse::aktualisiere(const QString& anlageId)
{
    if (!m_fertig || !m_verlaeufe->hatVerlauf(anlageId))
        return;
    Kennzahl k = analysiere(auftragFuer(anlageId));
    bewerte(k);
    m_kennzahlen.insert(anlageId, k);
    emit kennzahlGeaendert(anlageId);
}

const Kennzahl* FlottenAnalyse::kennzahl(const QString& anlageId) const
{
    const auto it = m_kennzahlen.constFind(anlageId);
    return it == m_kennzahlen.constEnd() ? nullptr : &it.value();
}

double FlottenAnalyse::medianPrStation(const QString& stationId) const
{
    return m_medianStation.value(stationId, std::nan(""));
}
