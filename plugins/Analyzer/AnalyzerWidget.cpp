
#include "AnalyzerWidget.h"
#include "AnalyzerInterface.h"
#include "MemoryRegions.h"
#include "Debugger.h"
#include "DebuggerCoreInterface.h"
#include "State.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QAbstractScrollArea>
#include <QScrollBar>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
AnalyzerWidget::AnalyzerWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
	setMinimumHeight(20);
	setMaximumHeight(20);
	
	connect(edb::v1::disassembly_widget(), SIGNAL(regionChanged()), this, SLOT(repaint()));


	if(QAbstractScrollArea *scroll_area = qobject_cast<QAbstractScrollArea*>(edb::v1::disassembly_widget())) {
		if(QScrollBar *scrollbar = scroll_area->verticalScrollBar()) {
			connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(repaint()));
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
	
	const MemRegion region = edb::v1::current_cpu_view_region();
	if(region.size() != 0) {	
		const float byte_width = static_cast<float>(width()) / region.size();
		
		const QSet<edb::address_t> specified_functions = edb::v1::analyzer()->specified_functions();
		
		const AnalyzerInterface::FunctionMap functions = edb::v1::analyzer()->functions(region);
		for(AnalyzerInterface::FunctionMap::const_iterator it = functions.begin(); it != functions.end(); ++it) {
			const AnalyzerInterface::Function &f = it.value();
			
			const int first_offset = (f.entry_address - region.start) * byte_width;
			const int last_offset  = (f.end_address - region.start) * byte_width;
			
			if(!specified_functions.contains(f.entry_address)) {
				painter.fillRect(first_offset, 0, last_offset - first_offset, height(), QBrush(Qt::darkGreen));
			} else {
				painter.fillRect(first_offset, 0, last_offset - first_offset, height(), QBrush(Qt::darkRed));
			}
		}
		
		if(QAbstractScrollArea *scroll_area = qobject_cast<QAbstractScrollArea*>(edb::v1::disassembly_widget())) {
			if(QScrollBar *scrollbar = scroll_area->verticalScrollBar()) {
				const int offset = (scrollbar->value()) * byte_width;
				
				const QString triangle(QChar(0x25b4));
				QFontMetrics fm(font());
				painter.setPen(QPen(Qt::yellow));
				painter.drawText(offset - fm.width(QChar(0x25b4)) / 2, height(), triangle);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void AnalyzerWidget::mousePressEvent(QMouseEvent *event) {
	
	const MemRegion region = edb::v1::current_cpu_view_region();
	if(region.size() != 0) {
		
		const float byte_width = static_cast<float>(width()) / region.size();
		const edb::address_t address = region.start + static_cast<edb::address_t>(event->x() / byte_width);		
		edb::v1::jump_to_address(address);
	}
}
