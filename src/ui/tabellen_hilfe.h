#pragma once

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

/**
 * @brief Tabellenzelle, die formatierten Text anzeigt, aber nach dem
 *        hinterlegten Zahlenwert sortiert (z. B. "1.234,5 kWh").
 */
class ZahlItem : public QTableWidgetItem {
public:
    ZahlItem(const QString& text, double wert)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, wert);
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    bool operator<(const QTableWidgetItem& other) const override
    {
        return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
    }
};

namespace TabellenHilfe {

/// Einheitliche Grundeinstellung für reine Anzeige-Tabellen.
inline void einrichten(QTableWidget* t, const QStringList& kopf)
{
    t->setColumnCount(int(kopf.size()));
    t->setHorizontalHeaderLabels(kopf);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(false);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setShowGrid(false);
}

inline QTableWidgetItem* text(const QString& s)
{
    return new QTableWidgetItem(s);
}

} // namespace TabellenHilfe
