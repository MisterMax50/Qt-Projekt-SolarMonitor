#include "badge_delegate.h"

#include <QApplication>
#include <QPainter>


void BadgeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QVariant farbWert = index.data(Qt::DecorationRole);
    const QString text = index.data(Qt::DisplayRole).toString();
    if (!farbWert.canConvert<QColor>() || text.isEmpty() || text == QStringLiteral("–")) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Hintergrund (Auswahl, Wechselfarbe) wie gewohnt, aber ohne Text/Icon
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();
    opt.icon = QIcon();
    opt.features &= ~QStyleOptionViewItem::HasDecoration;
    const QWidget* w = option.widget;
    QStyle* st = w ? w->style() : QApplication::style();
    st->drawControl(QStyle::CE_ItemViewItem, &opt, painter, w);

    // Statuspunkt + normaler Text, wie Statusspalten in Explorer/Outlook
    const QColor farbe = farbWert.value<QColor>();
    const Qt::Alignment ausr = Qt::Alignment(index.data(Qt::TextAlignmentRole).toInt());
    const QFontMetrics fm(option.font);
    const int breite = fm.horizontalAdvance(text) + 14;
    const int x = (ausr & Qt::AlignRight) ? option.rect.right() - breite - 6 : option.rect.left() + 6;
    const QPointF punkt(x + 4, option.rect.center().y() + 0.5);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(farbe);
    painter->drawEllipse(punkt, 3.5, 3.5);
    const bool ausgewaehlt = option.state & QStyle::State_Selected;
    painter->setPen(option.palette.color(ausgewaehlt ? QPalette::HighlightedText : QPalette::Text));
    painter->drawText(QRectF(x + 14, option.rect.top(), breite, option.rect.height()),
                      Qt::AlignVCenter | Qt::AlignLeft, text);
    painter->restore();
}

QSize BadgeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(QFontMetrics(option.font).horizontalAdvance(index.data(Qt::DisplayRole).toString()) + 28);
    return s;
}
