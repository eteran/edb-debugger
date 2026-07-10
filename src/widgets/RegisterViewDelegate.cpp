/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "RegisterViewDelegate.h"
#include <QTreeView>

/**
 * @brief Constructs a RegisterViewDelegate with the specified QTreeView and parent widget.
 *
 * @param view The QTreeView that this delegate will be associated with.
 * @param parent The parent widget for this delegate.
 */
RegisterViewDelegate::RegisterViewDelegate(QTreeView *view, QWidget *parent)
	: QStyledItemDelegate(parent), view_(view) {
}

/**
 * @brief Paints the item at the given index in the view using the specified painter and style option.
 *
 * @param painter The QPainter used to draw the item.
 * @param option The style options for the item.
 * @param index The model index of the item to be painted.
 */
void RegisterViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

	const QAbstractItemModel *const model = index.model();
	Q_ASSERT(model);

	if (!model->parent(index).isValid()) {
		// this is a top-level item.
		QStyleOptionButton buttonOption;

		buttonOption.state = option.state;
#ifdef Q_WS_MAC
		buttonOption.state |= QStyle::State_Raised;
#endif
		buttonOption.state &= ~QStyle::State_HasFocus;

		buttonOption.rect     = option.rect;
		buttonOption.palette  = option.palette;
		buttonOption.features = QStyleOptionButton::None;
		view_->style()->drawControl(QStyle::CE_PushButton, &buttonOption, painter, view_);

		static const int i = 9; // ### hardcoded in qcommonstyle.cpp
		const QRect &r     = option.rect;

		QStyleOption branchOption;
		branchOption.rect    = QRect(r.left() + i / 2, r.top() + (r.height() - i) / 2, i, i);
		branchOption.palette = option.palette;
		branchOption.state   = QStyle::State_Children;

		if (view_->isExpanded(index)) {
			branchOption.state |= QStyle::State_Open;
		}

		view_->style()->drawPrimitive(QStyle::PE_IndicatorBranch, &branchOption, painter, view_);

		// draw text
		const QRect textrect = QRect(r.left() + i * 2, r.top(), r.width() - ((5 * i) / 2), r.height());
		const QString text   = option.fontMetrics.elidedText(model->data(index, Qt::DisplayRole).toString(), Qt::ElideMiddle, textrect.width());
		view_->style()->drawItemText(painter, textrect, Qt::AlignCenter, option.palette, view_->isEnabled(), text);

	} else {
		QStyledItemDelegate::paint(painter, option, index);
	}
}

/**
 * @brief Returns the size hint for the item at the given index, taking into account whether it is a top-level item or not.
 *
 * @param opt The style options for the item.
 * @param index The model index of the item for which to return the size hint.
 * @return The size hint for the item at the given index.
 */
QSize RegisterViewDelegate::sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const {
	const QSize defaultHint = QStyledItemDelegate::sizeHint(opt, index) + QSize(2, 2);
	if (!index.model()->parent(index).isValid()) {
		QStyleOptionButton optButton;
		optButton.rect.setSize(opt.fontMetrics.size(Qt::TextShowMnemonic, QStringLiteral("X")));
		const QSize buttonHint = view_->style()->sizeFromContents(QStyle::CT_PushButton, &optButton, optButton.rect.size());
		return QSize(defaultHint.width(), buttonHint.height());
	}

	return defaultHint;
}
