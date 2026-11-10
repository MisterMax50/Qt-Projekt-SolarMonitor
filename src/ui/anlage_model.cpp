#include "anlage_model.h"

#include <QApplication>
#include <QBrush>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

#include "eval/flotten_analyse.h"
#include "model/datenbasis.h"
#include "model/favoriten.h"
#include "util/anzeige.h"
#include "util/stil.h"

// -------------------------------------------------------------------
//  AnlageModel
// -------------------------------------------------------------------

AnlageModel::AnlageModel(const AppKontext& kontext, QObject* parent)
    : QAbstractTableModel(parent)
    , m_kontext(kontext)
    , m_warnIcon(Stil::icon(Stil::Symbol::Warnung, QColor(217, 119, 6)))
{
    // Favoriten: goldener Stern vor der Anlagen-ID (mit Warnhinweis kombiniert)
    const QColor gold(234, 179, 8);
    m_sternIcon = Stil::icon(Stil::Symbol::SternVoll, gold);
    QIcon kombiniert;
    for (int g : {16, 24, 32}) {
        // Stern mit kleinem Warndreieck unten rechts (gleiche Größe wie die Einzel-Symbole)
        QPixmap pm(g, g);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        m_sternIcon.paint(&p, QRect(0, 0, g, g));
        const int w = g * 3 / 5;
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawEllipse(QRect(g - w - 1, g - w - 1, w + 2, w + 2));
        m_warnIcon.paint(&p, QRect(g - w, g - w, w, w));
        p.end();
        kombiniert.addPixmap(pm);
    }
    m_sternWarnIcon = kombiniert;
    connect(Favoriten::instanz(), &Favoriten::geaendert, this, [this] {
        if (rowCount() > 0)
            emit dataChanged(index(0, SpId), index(rowCount() - 1, SpId), {Qt::DecorationRole, Qt::ToolTipRole});
    });
    connect(m_kontext.daten, &Datenbasis::anlageGeaendert, this, &AnlageModel::anlageGeaendert);
    connect(m_kontext.daten, &Datenbasis::stoerungenGeaendert, this, &AnlageModel::anlageGeaendert);
    connect(m_kontext.analyse, &FlottenAnalyse::fertig, this, &AnlageModel::kennzahlenGeaendert);
    connect(m_kontext.analyse, &FlottenAnalyse::kennzahlGeaendert, this, &AnlageModel::anlageGeaendert);
}

int AnlageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_kontext.daten->anlagen().size());
}

int AnlageModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : SpAnzahl;
}

const Anlage* AnlageModel::anlageAt(int zeile) const
{
    const auto& alle = m_kontext.daten->anlagen();
    return (zeile >= 0 && zeile < alle.size()) ? &alle.at(zeile) : nullptr;
}

bool AnlageModel::istAuffaellig(int zeile) const
{
    const Anlage* a = anlageAt(zeile);
    if (!a)
        return false;
    const Kennzahl* k = m_kontext.analyse->kennzahl(a->anlageId);
    return (k && (!k->ok || k->auswertung.istAuffaellig()));
}

QVariant AnlageModel::data(const QModelIndex& index, int rolle) const
{
    const Anlage* a = index.isValid() ? anlageAt(index.row()) : nullptr;
    if (!a)
        return {};
    const Kennzahl* k = m_kontext.analyse->kennzahl(a->anlageId);
    const bool hatKz = k && k->ok;
    const int sp = index.column();

    if (rolle == IdRolle)
        return a->anlageId;

    if (rolle == Qt::DisplayRole) {
        switch (sp) {
        case SpId:               return a->anlageId;
        case SpTyp:              return Anzeige::typName(a->typ);
        case SpOrt:              return a->ort;
        case SpLeistung:         return Anzeige::zahl(a->leistungKwp, 2);
        case SpStatus:           return Anzeige::statusName(a->status);
        case SpStation:          return a->wetterstationId;
        case SpWechselrichter:   return a->wechselrichterId;
        case SpInbetrieb:        return Anzeige::datum(a->inbetriebnahme);
        case SpOffeneStoerungen: return m_kontext.daten->offeneStoerungen(a->anlageId);
        case SpPr:               return hatKz ? Anzeige::pr(k->auswertung.pr) : QStringLiteral("–");
        case SpSpezErtrag:       return hatKz ? Anzeige::zahl(k->auswertung.spezErtragKwhKwp, 1) : QStringLiteral("–");
        case SpAuffaellig: {
            if (!k)
                return QString();
            if (!k->ok)
                return tr("Verlauf defekt");
            QStringList typen;
            for (const Anomalie& an : k->auswertung.anomalien)
                if (!typen.contains(Anomalie::typName(an.typ)))
                    typen.append(Anomalie::typName(an.typ));
            return typen.join(QStringLiteral(", "));
        }
        default: return {};
        }
    }

    if (rolle == SortierRolle) {
        switch (sp) {
        case SpLeistung:         return a->leistungKwp;
        case SpInbetrieb:        return a->inbetriebnahme;
        case SpOffeneStoerungen: return m_kontext.daten->offeneStoerungen(a->anlageId);
        case SpPr:               return hatKz && k->auswertung.pr > 0.0 ? k->auswertung.pr : -1.0;
        case SpSpezErtrag:       return hatKz ? k->auswertung.spezErtragKwhKwp : -1.0;
        case SpAuffaellig:       return k ? int(k->auswertung.anomalien.size()) + (k->ok ? 0 : 100) : -1;
        default:                 return data(index, Qt::DisplayRole).toString().toLower();
        }
    }

    if (rolle == Qt::TextAlignmentRole) {
        if (sp == SpLeistung || sp == SpOffeneStoerungen || sp == SpPr || sp == SpSpezErtrag)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    }

    if (rolle == Qt::DecorationRole) {
        if (sp == SpStatus)
            return Anzeige::statusFarbe(a->status);
        if (sp == SpPr && hatKz && k->auswertung.pr > 0.0)
            return Anzeige::prFarbe(k->auswertung.pr);
        if (sp == SpId) {
            const bool favorit = Favoriten::instanz()->ist(a->anlageId);
            const bool warnung = !m_kontext.daten->pruefmeldungenFuer(a->anlageId).isEmpty();
            if (favorit)
                return warnung ? m_sternWarnIcon : m_sternIcon;
            if (warnung)
                return m_warnIcon;
        }
        return {};
    }

    if (rolle == Qt::ForegroundRole && sp == SpOffeneStoerungen
        && m_kontext.daten->offeneKritischeStoerungen(a->anlageId) > 0)
        return QBrush(Anzeige::schwereFarbe(QStringLiteral("kritisch")));

    if (rolle == Qt::BackgroundRole && istAuffaellig(index.row()))
        return QBrush(QColor(255, 80, 80, 40));

    if (rolle == Qt::ToolTipRole) {
        if (sp == SpId) {
            QStringList zeilen;
            for (const Pruefmeldung& m : m_kontext.daten->pruefmeldungenFuer(a->anlageId))
                zeilen.append(QStringLiteral("• ") + m.text);
            QString text = zeilen.isEmpty() ? QString()
                                            : tr("Datenprüfung:") + QLatin1Char('\n') + zeilen.join(QLatin1Char('\n'));
            if (Favoriten::instanz()->ist(a->anlageId))
                text = tr("★ Favorit") + (text.isEmpty() ? QString() : QStringLiteral("\n\n") + text);
            return text;
        }
        if (sp == SpAuffaellig && k) {
            if (!k->ok)
                return k->fehler;
            QStringList zeilen;
            for (const Anomalie& an : k->auswertung.anomalien)
                zeilen.append(QStringLiteral("• ") + an.text());
            return zeilen.join(QLatin1Char('\n'));
        }
        if (sp == SpPr && hatKz && k->medianPrStation > 0.0)
            return tr("Median der Wetterstation %1: %2").arg(a->wetterstationId, Anzeige::pr(k->medianPrStation));
        if (sp == SpWechselrichter) {
            if (const Wechselrichter* wr = m_kontext.daten->wechselrichter(a->wechselrichterId))
                return tr("%1 × %2 (%3 kW)").arg(a->anzahlWr).arg(wr->bezeichnung(), Anzeige::zahl(wr->nennleistungKw, 1));
        }
        if (sp == SpStation) {
            if (const Wetterstation* ws = m_kontext.daten->wetterstation(a->wetterstationId))
                return ws->name;
        }
        return {};
    }
    return {};
}

QVariant AnlageModel::headerData(int spalte, Qt::Orientation o, int rolle) const
{
    if (o != Qt::Horizontal)
        return {};
    if (rolle == Qt::TextAlignmentRole) // Zahlenspalten: Kopf rechtsbündig wie die Werte
        return int((spalte == SpLeistung || spalte == SpOffeneStoerungen || spalte == SpPr || spalte == SpSpezErtrag
                        ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    if (rolle == Qt::ToolTipRole) {
        switch (spalte) {
        case SpPr:         return tr("Performance Ratio PR = E / (H · Ppeak) im Zeitraum 01.–14.06.2026");
        case SpSpezErtrag: return tr("Spezifischer Ertrag Y = E / Ppeak in kWh/kWp (14 Tage)");
        case SpAuffaellig: return tr("Automatisch erkannte Fehlerbilder aus den Verläufen");
        default:           return {};
        }
    }
    if (rolle != Qt::DisplayRole)
        return {};
    switch (spalte) {
    case SpId:               return tr("Anlage");
    case SpTyp:              return tr("Typ");
    case SpOrt:              return tr("Ort");
    case SpLeistung:         return tr("Leistung (kWp)");
    case SpStatus:           return tr("Status");
    case SpStation:          return tr("Wetterstation");
    case SpWechselrichter:   return tr("WR");
    case SpInbetrieb:        return tr("Inbetriebnahme");
    case SpOffeneStoerungen: return tr("Offene Störungen");
    case SpPr:               return tr("PR");
    case SpSpezErtrag:       return tr("kWh/kWp");
    case SpAuffaellig:       return tr("Auffälligkeiten");
    default:                 return {};
    }
}

void AnlageModel::anlageGeaendert(const QString& anlageId)
{
    const auto& alle = m_kontext.daten->anlagen();
    for (int i = 0; i < alle.size(); ++i) {
        if (alle.at(i).anlageId == anlageId) {
            emit dataChanged(index(i, 0), index(i, SpAnzahl - 1));
            return;
        }
    }
}

void AnlageModel::kennzahlenGeaendert()
{
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, SpAnzahl - 1));
}

// -------------------------------------------------------------------
//  AnlageFilterModel
// -------------------------------------------------------------------

AnlageFilterModel::AnlageFilterModel(QObject* parent)
    : FilterProxy(parent)
{
    setSortRole(AnlageModel::SortierRolle);
    setDynamicSortFilter(true);
    // "nur Favoriten" sofort nachführen, wenn eine Anlage an- oder abgepinnt wird
    connect(Favoriten::instanz(), &Favoriten::geaendert, this, [this] {
        if (m_nurFavoriten)
            aendereFilter([] {});
    });
}

void AnlageFilterModel::setNurFavoriten(bool an)
{
    aendereFilter([&] {
        m_nurFavoriten = an;
    });
}

void AnlageFilterModel::setSuchtext(const QString& text)
{
    aendereFilter([&] {
        m_suche = text.trimmed();
    });
}

void AnlageFilterModel::setStatus(const QString& status)
{
    aendereFilter([&] {
        m_status = status;
    });
}

void AnlageFilterModel::setTyp(const QString& typ)
{
    aendereFilter([&] {
        m_typ = typ;
    });
}

void AnlageFilterModel::setOrt(const QString& ort)
{
    aendereFilter([&] {
        m_ort = ort;
    });
}

void AnlageFilterModel::setLeistungsbereich(double minKwp, double maxKwp)
{
    aendereFilter([&] {
        m_minKwp = minKwp;
        m_maxKwp = maxKwp;
    });
}

void AnlageFilterModel::setNurAuffaellige(bool an)
{
    aendereFilter([&] {
        m_nurAuffaellig = an;
    });
}

void AnlageFilterModel::setNurOffeneStoerungen(bool an)
{
    aendereFilter([&] {
        m_nurOffen = an;
    });
}

bool AnlageFilterModel::filterAcceptsRow(int quellZeile, const QModelIndex&) const
{
    const auto* m = qobject_cast<const AnlageModel*>(sourceModel());
    const Anlage* a = m ? m->anlageAt(quellZeile) : nullptr;
    if (!a)
        return false;

    if (!m_status.isEmpty() && a->status != m_status)
        return false;
    if (!m_typ.isEmpty() && a->typ != m_typ)
        return false;
    if (!m_ort.isEmpty() && a->ort != m_ort)
        return false;
    if (m_minKwp > 0.0 && a->leistungKwp < m_minKwp)
        return false;
    if (m_maxKwp > 0.0 && a->leistungKwp > m_maxKwp)
        return false;
    if (m_nurAuffaellig && !m->istAuffaellig(quellZeile))
        return false;
    if (m_nurFavoriten && !Favoriten::instanz()->ist(a->anlageId))
        return false;
    if (m_nurOffen && m->data(m->index(quellZeile, AnlageModel::SpOffeneStoerungen)).toInt() == 0)
        return false;
    if (!m_suche.isEmpty()) {
        const bool treffer = a->anlageId.contains(m_suche, Qt::CaseInsensitive)
                          || a->ort.contains(m_suche, Qt::CaseInsensitive)
                          || a->wetterstationId.contains(m_suche, Qt::CaseInsensitive);
        if (!treffer)
            return false;
    }
    return true;
}
