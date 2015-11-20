/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AnalyzerWidget.h"
#include "edb.h"
#include "IAnalyzer.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "State.h"
#include <QAbstractScrollArea>
#include <QDebug>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

namespace Analyzer {

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
AnalyzerWidget::AnalyzerWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f), mouse_pressed_(false) {
	setMinimumHeight(20);
	setMaximumHeight(20);
	QSizePolicy policy;

	policy.setHorizontalPolicy(QSizePolicy::Expanding);
	setSizePolicy(policy);

	connect(edb::v1::disassembly_widget(), SIGNAL(regionChanged()), this, SLOT(update()));

	if(auto scroll_area = qobject_cast<QAbstractScrollArea*>(edb::v1::disassembly_widget())) {
		if(QScrollBar *scrollbar = scroll_area->verticalScrollBar()) {
			connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(update()));
		}
	}

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void AnalyzerWidget::paintEvent(QPaintEvent *event) {

	Q_UNUSED(event);

	QPainter painter(this);
	painter.fillRect(0, 0, width(), height(), QBrush(Qt::black));
	QFontMetrics fm(font());

	if(const IRegion::pointer region = edb::v1::current_cpu_view_region()) {
		if(region->size() != 0) {
			const auto byte_width = static_cast<float>(width()) / region->size();

			const QSet<edb::address_t> specified_functions = edb::v1::analyzer()->specified_functions();

			const IAnalyzer::FunctionMap functions = edb::v1::analyzer()->functions(region);
			for(auto it = functions.begin(); it != functions.end(); ++it) {
				const Function &f = it.value();

				const int first_offset = (f.entry_address() - region->start()) * byte_width;
				const int last_offset  = (f.end_address() - region->start()) * byte_width;

				if(!specified_functions.contains(f.entry_address())) {
					painter.fillRect(first_offset, 0, last_offset - first_offset, height(), QBrush(Qt::darkGreen));
				} else {
					painter.fillRect(first_offset, 0, last_offset - first_offset, height(), QBrush(Qt::darkRed));
				}
			}

			// highlight header of binary (probably not going to be too noticeable but just in case)
			if(auto binary_info = edb::v1::get_binary_info(region)) {
				painter.fillRect(0, 0, binary_info->header_size() * byte_width, height(), QBrush(Qt::darkBlue));
			}

			if(functions.isEmpty()) {
				const QString s(tr("No Analysis Found For This Region"));
				painter.setPen(QPen(Qt::white));
				painter.drawText((width() - fm.width(s)) / 2, height() - 4, s);
			} else {
				if(auto scroll_area = qobject_cast<QAbstractScrollArea*>(edb::v1::disassembly_widget())) {
					if(QScrollBar *scrollbar = scroll_area->verticalScrollBar()) {
						const int offset = (scrollbar->value()) * byte_width;

						const QString triangle(QChar(0x25b4));

	#ifdef Q_OS_WIN32
						const QFont f = painter.font();
						painter.setFont(QFont("Lucida Sans Unicode", 16));
	#endif
						painter.setPen(QPen(Qt::yellow));
						painter.drawText(offset - fm.width(QChar(0x25b4)) / 2, height(), triangle);
	#ifdef Q_OS_WIN32
						painter.setFont(f);
	#endif
					}
				}
			}
		} else {
			const QString s(tr("No Analysis Found For This Region"));
			painter.setPen(QPen(Qt::white));
			painter.drawText((width() - fm.width(s)) / 2, height() - 4, s);
		}
	} else {
			const QString s(tr("No Analysis Found For This Region"));
			painter.setPen(QPen(Qt::white));
			painter.drawText((width() - fm.width(s)) / 2, height() - 4, s);
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void AnalyzerWidget::mousePressEvent(QMouseEvent *event) {

	mouse_pressed_ = true;

	if(const IRegion::pointer region = edb::v1::current_cpu_view_region()) {
		const IAnalyzer::FunctionMap functions = edb::v1::analyzer()->functions(region);
		if(region->size() != 0 && !functions.empty()) {
			const auto byte_width = static_cast<float>(width()) / region->size();
			const edb::address_t address = qBound<edb::address_t>(region->start(), region->start() + edb::address_t(event->x() / byte_width), region->end() - 1);
			edb::v1::jump_to_address(address);
		}
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void AnalyzerWidget::mouseReleaseEvent(QMouseEvent *event) {
	Q_UNUSED(event);
	mouse_pressed_ = false;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void AnalyzerWidget::mouseMoveEvent(QMouseEvent *event) {
	if(mouse_pressed_) {
		mousePressEvent(event);
	}
}

}
