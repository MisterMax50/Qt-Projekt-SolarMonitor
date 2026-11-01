#include "stoerung_model.h"

#include <QBrush>

#include "model/datenbasis.h"
#include "util/anzeige.h"

StoerungModel::StoerungModel(const AppKontext& kontext, QObject* parent)
    : QAbstractTableModel(parent)
    , m_kontext(kontext)
{
    m_anzahl = int(m_kontext.daten->stoerungen().size());
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &StoerungModel::neuLaden);
}

void StoerungModel::neuLaden()
{
    // Neue Störungen werden angehängt; bestehende ändern nur Werte.
    // Weniger Zeilen gibt es nur nach "Rückgängig" einer erfassten Störung.
    const int neu = int(m_kontext.daten->stoerungen().size());
    if (neu < m_anzahl) {
        beginResetModel();
        m_anzahl = neu;
        endResetModel();
        return;
    }
    if (neu > m_anzahl) {
        beginInsertRows({}, m_anzahl, neu - 1);
        m_anzahl = neu;
        endInsertRows();
    }
    if (m_anzahl > 0)
        emit dataChanged(index(0, 0), index(m_anzahl - 1, SpAnzahl - 1));
}

int StoerungModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_anzahl;
}

int StoerungModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : SpAnzahl;
}

const Stoerung* StoerungModel::stoerungAt(int zeile) const
{
    const auto& alle = m_kontext.daten->stoerungen();
    return (zeile >= 0 && zeile < alle.size()) ? &alle.at(zeile) : nullptr;
}

QVariant StoerungModel::data(const QModelIndex& index, int rolle) const
{
    const Stoerung* s = index.isValid() ? stoerungAt(index.row()) : nullptr;
    if (!s)
        return {};
    const int sp = index.column();

    if (rolle == IdRolle)
        return s->stoerungId;

    if (rolle == Qt::DisplayRole) {
        switch (sp) {
        case SpId:           return s->stoerungId;
        case SpAnlage:       return s->anlageId;
        case SpZeitpunkt:    return Anzeige::zeitpunkt(s->zeitpunkt);
        case SpFehlercode:   return s->fehlercode;
        case SpBeschreibung: return s->beschreibung;
        case SpSchwere:      return Anzeige::schwereName(s->schwere);
        case SpStatus:       return Anzeige::stoerungsStatusName(s->status);
        case SpBehobenAm:    return s->behobenAm.isValid() ? Anzeige::zeitpunkt(s->behobenAm) : QString();
        default:             return {};
        }
    }
    if (rolle == SortierRolle) {
        switch (sp) {
        case SpZeitpunkt: return s->zeitpunkt;
        case SpBehobenAm: return s->behobenAm;
        case SpSchwere:   return int(Datenbasis::gueltigeSchweren().indexOf(s->schwere));
        default:          return data(index, Qt::DisplayRole).toString().toLower();
        }
    }
    if (rolle == Qt::DecorationRole && sp == SpSchwere)
        return Anzeige::schwereFarbe(s->schwere);
    if (rolle == Qt::DecorationRole && sp == SpStatus)
        return s->status == QLatin1String("offen")            ? QColor(220, 38, 38)
             : s->status == QLatin1String("in_bearbeitung")   ? QColor(217, 119, 6)
                                                              : QColor(22, 163, 74);
    if (rolle == Qt::ToolTipRole && sp == SpAnlage && !m_kontext.daten->anlage(s->anlageId))
        return tr("Anlage ist in anlagen.csv nicht vorhanden.");
    return {};
}

QVariant StoerungModel::headerData(int spalte, Qt::Orientation o, int rolle) const
{
    if (o != Qt::Horizontal || rolle != Qt::DisplayRole)
        return {};
    switch (spalte) {
    case SpId:           return tr("Störung");
    case SpAnlage:       return tr("Anlage");
    case SpZeitpunkt:    return tr("Zeitpunkt");
    case SpFehlercode:   return tr("Code");
    case SpBeschreibung: return tr("Beschreibung");
    case SpSchwere:      return tr("Schwere");
    case SpStatus:       return tr("Status");
    case SpBehobenAm:    return tr("Behoben am");
    default:             return {};
    }
}

// -------------------------------------------------------------------
//  StoerungFilterModel
// -------------------------------------------------------------------

StoerungFilterModel::StoerungFilterModel(QObject* parent)
    : FilterProxy(parent)
{
    setSortRole(StoerungModel::SortierRolle);
    setDynamicSortFilter(true);
}

void StoerungFilterModel::setSuchtext(const QString& text)
{
    aendereFilter([&] {
        m_suche = text.trimmed();
    });
}

void StoerungFilterModel::setStatusFilter(StatusFilter f)
{
    aendereFilter([&] {
        m_status = f;
    });
}

void StoerungFilterModel::setSchwere(const QString& schwere)
{
    aendereFilter([&] {
        m_schwere = schwere;
    });
}

void StoerungFilterModel::setAnlage(const QString& anlageId)
{
    aendereFilter([&] {
        m_anlage = anlageId.trimmed();
    });
}

bool StoerungFilterModel::filterAcceptsRow(int quellZeile, const QModelIndex&) const
{
    const auto* m = qobject_cast<const StoerungModel*>(sourceModel());
    const Stoerung* s = m ? m->stoerungAt(quellZeile) : nullptr;
    if (!s)
        return false;

    switch (m_status) {
    case StatusFilter::Alle:          break;
    case StatusFilter::Offen:         if (!s->istOffen()) return false; break;
    case StatusFilter::NurOffen:      if (s->status != QLatin1String("offen")) return false; break;
    case StatusFilter::InBearbeitung: if (s->status != QLatin1String("in_bearbeitung")) return false; break;
    case StatusFilter::Behoben:       if (s->status != QLatin1String("behoben")) return false; break;
    }
    if (!m_schwere.isEmpty() && s->schwere != m_schwere)
        return false;
    if (!m_anlage.isEmpty() && s->anlageId.compare(m_anlage, Qt::CaseInsensitive) != 0)
        return false;
    if (!m_suche.isEmpty()) {
        const bool treffer = s->stoerungId.contains(m_suche, Qt::CaseInsensitive)
                          || s->anlageId.contains(m_suche, Qt::CaseInsensitive)
                          || s->fehlercode.contains(m_suche, Qt::CaseInsensitive)
                          || s->beschreibung.contains(m_suche, Qt::CaseInsensitive);
        if (!treffer)
            return false;
    }
    return true;
}
