/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
length_disasm_back
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef QDISASSEMBLYVIEW_20061101_H_
#define QDISASSEMBLYVIEW_20061101_H_

#include "IRegion.h"
#include "NavigationHistory.h"
#include "Types.h"
#include "Status.h"
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QCache>
#include <QPixmap>
#include <QSvgRenderer>
#include <QSet>

class IAnalyzer;
class QPainter;
class QTextDocument;
class SyntaxHighlighter;

class QDisassemblyView : public QAbstractScrollArea {
	Q_OBJECT

public:
	QDisassemblyView(QWidget *parent = 0);
	~QDisassemblyView();

protected:
	virtual bool event(QEvent *event);
	virtual void mouseDoubleClickEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void paintEvent(QPaintEvent *event);
	virtual void wheelEvent(QWheelEvent *e);
	virtual void keyPressEvent(QKeyEvent *event);

public:
	IRegion::pointer region() const;
	bool addressShown(edb::address_t address) const;
	edb::address_t addressFromPoint(const QPoint &pos) const;
	edb::address_t selectedAddress() const;
	int selectedSize() const;
	void add_comment(edb::address_t address, QString comment);
	int remove_comment(edb::address_t address);
	QString get_comment(edb::address_t address);
	void clear_comments();
	void setSelectedAddress(edb::address_t address);
	QByteArray saveState() const;
	void restoreState(const QByteArray &stateBuffer);

Q_SIGNALS:
	void signal_updated();

public Q_SLOTS:
	void setFont(const QFont &f);
	void resizeEvent(QResizeEvent *event);
	void scrollTo(edb::address_t address);
	void setAddressOffset(edb::address_t address);
	void setRegion(const IRegion::pointer &r);
	void setCurrentAddress(edb::address_t address);
	void clear();
	void update();
	void setShowAddressSeparator(bool value);
	void resetColumns();

private Q_SLOTS:
	void scrollbar_action_triggered(int action);

signals:
	void breakPointToggled(edb::address_t address);
	void regionChanged();

private:
	QString formatAddress(edb::address_t address) const;
	QString format_invalid_instruction_bytes(const edb::Instruction &inst, QPainter &painter) const;
	edb::address_t address_from_coord(int x, int y) const;
	edb::address_t previous_instructions(edb::address_t current_address, int count);
	edb::address_t following_instructions(edb::address_t current_address, int count);
	int address_length() const;
	int auto_line1() const;
	int draw_instruction(QPainter &painter, const edb::Instruction &inst, int y, int line_height, int l2, int l3) const;
	Result<int> get_instruction_size(edb::address_t address) const;
	Result<int> get_instruction_size(edb::address_t address, quint8 *buf, int *size) const;
	int line1() const;
	int line2() const;
	int line3() const;
	int line_height() const;
	void updateScrollbars();
	void updateSelectedAddress(QMouseEvent *event);
	void paint_line_bg(QPainter &painter, QBrush brush, int line, int num_lines = 1);

private:
	IRegion::pointer                  region_;
	QVector<edb::address_t>           show_addresses_;
	SyntaxHighlighter *const          highlighter_;
	edb::address_t                    address_offset_;
	edb::address_t                    selected_instruction_address_;
	edb::address_t                    current_address_;
	qreal                             font_height_; // height of a character in this font
	qreal                             font_width_;  // width of a character in this font
	qreal                             icon_width_;
	qreal                             icon_height_;
	int                               line1_;
	int                               line2_;
	int                               line3_;
	int                               selected_instruction_size_;
	bool                              moving_line1_;
	bool                              moving_line2_;
	bool                              moving_line3_;
	bool                              selecting_address_;
	bool                              show_address_separator_;
	QHash<edb::address_t, QString>    comments_;
	NavigationHistory                 history_;
	QSvgRenderer                      breakpoint_renderer_;
	QSvgRenderer                      current_renderer_;
	QSvgRenderer                      current_bp_renderer_;
	QVector<quint8>                   instruction_buffer_;
};

#endif
