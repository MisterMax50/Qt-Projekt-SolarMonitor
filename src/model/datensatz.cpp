#include "datensatz.h"

#include <QDir>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "repo/verlauf_repository.h"

Datensatz::Datensatz() = default;

Datensatz::~Datensatz()
{
    m_analyse.reset(); // zuerst: wartet auf laufende Auswertungen
    m_verlaeufe.reset();
    m_daten.reset();
}

std::unique_ptr<Datensatz> Datensatz::lade(const QString& ordner)
{
    auto d = std::make_unique<Datensatz>();
    d->m_ordner = ordner;
    d->m_daten = std::make_unique<Datenbasis>();
    d->m_verlaeufe = std::make_unique<VerlaufRepository>();

    if (ordner.isEmpty()) {
        d->m_fehler = tr("Es wurde kein Datenordner mit anlagen.csv gefunden.");
    } else if (!d->m_daten->lade(ordner)) {
        QStringList zeilen;
        for (const Pruefmeldung& m : d->m_daten->pruefmeldungen())
            if (m.stufe == Pruefmeldung::Stufe::Fehler && m.zeile == 0)
                zeilen.append(QStringLiteral("• ") + m.text);
        d->m_fehler = tr("In %1 konnten keine Anlagen geladen werden:\n%2")
                          .arg(QDir::toNativeSeparators(ordner), zeilen.join(QLatin1Char('\n')));
    } else {
        // Verlaufsindex (die JSON-Dateien selbst werden erst bei Bedarf geladen)
        d->m_verlaeufe->ladeIndex(d->m_daten->datenOrdner());
        d->m_verlaeufe->setLiveAnlagen(d->m_daten->anlagen());
        QVector<Pruefmeldung> meldungen = d->m_verlaeufe->indexMeldungen();
        for (const Anlage& a : d->m_daten->anlagen())
            if (!d->m_verlaeufe->istEingetragen(a.anlageId))
                meldungen.append({Pruefmeldung::Stufe::Info, QStringLiteral("master_verlauf_index.csv"), 0, a.anlageId,
                                  tr("Kein Verlauf im Index – nur Stammdaten verfügbar.")});
        for (const QString& id : d->m_verlaeufe->anlagenMitVerlauf())
            if (!d->m_daten->anlage(id))
                meldungen.append({Pruefmeldung::Stufe::Info, QStringLiteral("master_verlauf_index.csv"), 0, id,
                                  tr("Verlauf vorhanden, aber keine Stammdaten in anlagen.csv.")});
        d->m_daten->ergaenzeMeldungen(meldungen);
    }
    d->m_analyse = std::make_unique<FlottenAnalyse>(d->m_daten.get(), d->m_verlaeufe.get());
    return d;
}
