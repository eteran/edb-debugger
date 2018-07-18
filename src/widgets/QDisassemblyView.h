/*
Copyright (C) 2006 - 2017 Evan Teran
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

#ifndef QDISASSEMBLYVIEW_20061101_H_
#define QDISASSEMBLYVIEW_20061101_H_

#include "NavigationHistory.h"
#include "Types.h"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QCache>
#include <QPixmap>
#include <QSvgRenderer>

#include <memory>
#include <vector>

template <class T>
class Result;

class IRegion;
class IAnalyzer;
class QPainter;
class QTextDocument;
class SyntaxHighlighter;

class QDisassemblyView : public QAbstractScrollArea {
	Q_OBJECT

public:
    explicit QDisassemblyView(QWidget *parent = nullptr);
    ~QDisassemblyView() override;

protected:
    bool event(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;

public:
	std::shared_ptr<IRegion> region() const;
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
	void restoreComments(QVariantList &);

Q_SIGNALS:
	void signal_updated();

public Q_SLOTS:
	void setFont(const QFont &f);
    void resizeEvent(QResizeEvent *event) override;
	void scrollTo(edb::address_t address);
	void setAddressOffset(edb::address_t address);
	void setRegion(const std::shared_ptr<IRegion> &r);
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
	edb::address_t address_from_coord(int x, int y) const;
	edb::address_t previous_instructions(edb::address_t current_address, int count);
	edb::address_t following_instructions(edb::address_t current_address, int count);
	int address_length() const;
	int auto_line1() const;
	int draw_instruction(QPainter &painter, const edb::Instruction &inst, int y, int line_height, int l2, int l3, bool selected);
    QString instructionString(const edb::Instruction &inst) const;
	Result<int> get_instruction_size(edb::address_t address) const;
	Result<int> get_instruction_size(edb::address_t address, quint8 *buf, int *size) const;
	int line1() const;
	int line2() const;
	int line3() const;
	int line_height() const;
	void updateScrollbars();
	void updateSelectedAddress(QMouseEvent *event);
	void paint_line_bg(QPainter &painter, QBrush brush, int line, int num_lines = 1);
	bool get_line_of_address(edb::address_t addr, unsigned int& line) const;
	unsigned updateDisassembly(unsigned lines_to_render);
	unsigned getSelectedLineNumber() const;

private:
	std::shared_ptr<IRegion>          region_;
	QVector<edb::address_t>           show_addresses_;
	std::vector<CapstoneEDB::Instruction> instructions_;
	SyntaxHighlighter *const          highlighter_;
	edb::address_t                    address_offset_;
	edb::address_t                    selected_instruction_address_;
	edb::address_t                    current_address_;
	int                               font_height_; // height of a character in this font
	int                               font_width_;  // width of a character in this font
	int                               icon_width_;
	int                               icon_height_;
	int                               line0_;
	int                               line1_;
	int                               line2_;
	int                               line3_;
	int                               selected_instruction_size_;
	bool                              moving_line1_;
	bool                              moving_line2_;
	bool                              moving_line3_;
	bool                              selecting_address_;
	bool                              show_address_separator_;
	bool                              partial_last_line_;
	QHash<edb::address_t, QString>    comments_;
	NavigationHistory                 history_;
	QSvgRenderer                      breakpoint_renderer_;
	QSvgRenderer                      current_renderer_;
	QSvgRenderer                      current_bp_renderer_;
	QVector<quint8>                   instruction_buffer_;
	QCache<QString, QPixmap>          syntax_cache_;
};

#endif
