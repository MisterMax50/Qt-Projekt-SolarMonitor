#include "badge_delegate.h"

#include <QApplication>
#include <QPainter>

#include "util/stil.h"

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

    const QColor farbe = farbWert.value<QColor>();
    const QFontMetrics fm(option.font);
    const int breite = fm.horizontalAdvance(text) + 22;
    const int hoehe = qMin(option.rect.height() - 6, fm.height() + 6);
    const Qt::Alignment ausr = Qt::Alignment(index.data(Qt::TextAlignmentRole).toInt());
    const int x = (ausr & Qt::AlignRight) ? option.rect.right() - breite - 6 : option.rect.left() + 6;
    const QRectF pille(x, option.rect.center().y() - hoehe / 2.0 + 0.5, breite, hoehe);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QColor hinten = farbe;
    hinten.setAlpha(Stil::istDunkel() ? 70 : 38);
    painter->setPen(Qt::NoPen);
    painter->setBrush(hinten);
    painter->drawRoundedRect(pille, hoehe / 2.0, hoehe / 2.0);
    painter->setBrush(farbe);
    painter->drawEllipse(QPointF(pille.left() + 9, pille.center().y()), 3, 3);
    painter->setPen(Stil::istDunkel() ? farbe.lighter(150) : farbe.darker(140));
    painter->drawText(pille.adjusted(16, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    painter->restore();
}

QSize BadgeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(QFontMetrics(option.font).horizontalAdvance(index.data(Qt::DisplayRole).toString()) + 36);
    return s;
}
