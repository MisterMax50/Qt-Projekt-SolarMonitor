#pragma once

#include <QStyledItemDelegate>

/**
 * @brief Zeichnet eine Tabellenzelle als farbiges Abzeichen ("Badge").
 *
 * Die Farbe kommt aus Qt::DecorationRole (QColor) des Modells, der Text
 * aus Qt::DisplayRole. So bleibt die Farbzuordnung im Modell und die
 * Ansicht entscheidet nur über die Darstellung.
 */
class BadgeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
