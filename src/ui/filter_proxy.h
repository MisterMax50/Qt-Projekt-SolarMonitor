#pragma once

#include <utility>

#include <QSortFilterProxyModel>
#include <QtGlobal>

/**
 * @brief Gemeinsame Basis der Filtermodelle.
 *
 * Kapselt das Neuberechnen des Filters nach einer Änderung der Kriterien.
 * Ab Qt 6.10 ist invalidateFilter() veraltet; stattdessen wird die
 * Änderung zwischen beginFilterChange() und endFilterChange() ausgeführt.
 * Ältere Qt-Versionen nutzen weiterhin invalidateFilter().
 */
class FilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    template <typename Aenderung>
    void aendereFilter(Aenderung&& aenderung)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        std::forward<Aenderung>(aenderung)();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        std::forward<Aenderung>(aenderung)();
        invalidateFilter();
#endif
    }
};
