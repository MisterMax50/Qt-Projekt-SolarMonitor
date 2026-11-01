#include "aenderungen.h"

#include "model/datenbasis.h"
#include "util/anzeige.h"

// -------------------------------------------------------------------
//  Anlage ändern
// -------------------------------------------------------------------

AnlageAendern::AnlageAendern(Datenbasis* daten, const Anlage& alt, const Anlage& neu, NachAenderung nachher)
    : m_daten(daten)
    , m_alt(alt)
    , m_neu(neu)
    , m_nachher(std::move(nachher))
{
    setText(tr("Stammdaten von %1 ändern").arg(neu.anlageId));
}

void AnlageAendern::redo()
{
    m_daten->aktualisiereAnlage(m_neu);
    if (m_nachher)
        m_nachher(m_neu.anlageId);
}

void AnlageAendern::undo()
{
    m_daten->aktualisiereAnlage(m_alt);
    if (m_nachher)
        m_nachher(m_alt.anlageId);
}

// -------------------------------------------------------------------
//  Störung ändern
// -------------------------------------------------------------------

StoerungAendern::StoerungAendern(Datenbasis* daten, const Stoerung& alt, const Stoerung& neu, NachAenderung nachher)
    : m_daten(daten)
    , m_alt(alt)
    , m_neu(neu)
    , m_nachher(std::move(nachher))
{
    if (alt.status != neu.status)
        setText(tr("Status von %1 auf „%2“ setzen").arg(neu.stoerungId, Anzeige::stoerungsStatusName(neu.status)));
    else
        setText(tr("Störung %1 ändern").arg(neu.stoerungId));
}

void StoerungAendern::redo()
{
    m_daten->aktualisiereStoerung(m_neu);
    if (m_nachher)
        m_nachher(m_neu.anlageId);
}

void StoerungAendern::undo()
{
    m_daten->aktualisiereStoerung(m_alt);
    if (m_nachher)
        m_nachher(m_alt.anlageId);
}

// -------------------------------------------------------------------
//  Störung erfassen
// -------------------------------------------------------------------

StoerungErfassen::StoerungErfassen(Datenbasis* daten, const Stoerung& neu, NachAenderung nachher)
    : m_daten(daten)
    , m_stoerung(neu)
    , m_nachher(std::move(nachher))
{
    setText(tr("Störung für %1 erfassen").arg(neu.anlageId));
}

void StoerungErfassen::redo()
{
    if (m_stoerung.stoerungId.isEmpty()) {
        // erstes Ausführen: Datenbasis vergibt die nächste freie ID
        m_stoerung.stoerungId = m_daten->erfasseStoerung(m_stoerung);
        setText(tr("Störung %1 für %2 erfassen").arg(m_stoerung.stoerungId, m_stoerung.anlageId));
    } else {
        // Wiederholen: mit derselben ID wieder einfügen
        m_daten->fuegeStoerungEin(m_stoerung);
    }
    if (m_nachher)
        m_nachher(m_stoerung.anlageId);
}

void StoerungErfassen::undo()
{
    m_daten->entferneStoerung(m_stoerung.stoerungId);
    if (m_nachher)
        m_nachher(m_stoerung.anlageId);
}
