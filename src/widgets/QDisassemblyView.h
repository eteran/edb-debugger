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

#ifndef QDISASSEMBLY_VIEW_H_20061101_
#define QDISASSEMBLY_VIEW_H_20061101_

#include "NavigationHistory.h"
#include "Types.h"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QCache>
#include <QPainterPath>
#include <QPixmap>
#include <QSvgRenderer>

#include <memory>
#include <vector>

#include <boost/optional.hpp>

template <class T, class E>
class Result;

class IRegion;
class IAnalyzer;
class QPainter;
class QTextDocument;
class SyntaxHighlighter;

class QDisassemblyView final : public QAbstractScrollArea {
	Q_OBJECT

private:
	struct DrawingContext {
		int l1;
		int l2;
		int l3;
		int l4;
		int linesToRender;
		int selectedLines;
		int lineHeight;
		QPalette::ColorGroup group;
		std::map<int, int> lineBadgeWidth; // for jmp drawing
	};

public:
	explicit QDisassemblyView(QWidget *parent = nullptr);
	~QDisassemblyView() override = default;

protected:
	bool event(QEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void wheelEvent(QWheelEvent *e) override;
	void keyPressEvent(QKeyEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

public:
	QByteArray saveState() const;
	QString getComment(edb::address_t address);
	bool addressShown(edb::address_t address) const;
	edb::address_t addressFromPoint(const QPoint &pos) const;
	edb::address_t selectedAddress() const;
	int removeComment(edb::address_t address);
	int selectedSize() const;
	std::shared_ptr<IRegion> region() const;
	void addComment(edb::address_t address, QString comment);
	void clearComments();
	void restoreComments(QVariantList &);
	void restoreState(const QByteArray &stateBuffer);
	void setSelectedAddress(edb::address_t address);

Q_SIGNALS:
	void signalUpdated();
	void breakPointToggled(edb::address_t address);
	void regionChanged();

public Q_SLOTS:
	void setFont(const QFont &f);
	void scrollTo(edb::address_t address);
	void setRegion(const std::shared_ptr<IRegion> &r);
	void setCurrentAddress(edb::address_t address);
	void clear();
	void update();
	void setShowAddressSeparator(bool value);
	void resetColumns();

private:
	void scrollbarActionTriggered(int action);
	QString formatAddress(edb::address_t address) const;
	QString instructionString(const edb::Instruction &inst) const;
	Result<int, QString> getInstructionSize(edb::address_t address) const;
	Result<int, QString> getInstructionSize(edb::address_t address, uint8_t *buf, int *size) const;
	boost::optional<unsigned int> getLineOfAddress(edb::address_t addr) const;
	edb::address_t addressFromCoord(int x, int y) const;
	int addressLength() const;
	int autoLine2() const;
	int line0() const;
	int line1() const;
	int line2() const;
	int line3() const;
	int line4() const;
	int lineHeight() const;
	int previousInstructions(int current_address, int count);
	int previousInstruction(IAnalyzer *analyzer, int current_address);
	int followingInstructions(int current_address, int count);
	int followingInstruction(int current_address);
	int updateDisassembly(int lines_to_render);
	int getSelectedLineNumber() const;
	void paintLineBg(QPainter &painter, QBrush brush, int line, int num_lines = 1);
	void setAddressOffset(edb::address_t address);
	void updateScrollbars();
	void updateSelectedAddress(QMouseEvent *event);

	void drawInstruction(QPainter &painter, const edb::Instruction &inst, const DrawingContext *ctx, int y, bool selected);
	void drawHeaderAndBackground(QPainter &painter, const DrawingContext *ctx, const std::unique_ptr<IBinary> &binary_info);
	void drawRegiserBadges(QPainter &painter, DrawingContext *ctx);
	void drawSymbolNames(QPainter &painter, const DrawingContext *ctx);
	void drawSidebarElements(QPainter &painter, const DrawingContext *ctx);
	void drawInstructionBytes(QPainter &painter, const DrawingContext *ctx);
	void drawFunctionMarkers(QPainter &painter, const DrawingContext *ctx);
	void drawComments(QPainter &painter, const DrawingContext *ctx);
	void drawJumpArrows(QPainter &painter, const DrawingContext *ctx);
	void drawDisassembly(QPainter &painter, const DrawingContext *ctx);
	void drawDividers(QPainter &painter, const DrawingContext *ctx);

private:
	edb::address_t addressOffset_{0};
	edb::address_t selectedInstructionAddress_{0};
	edb::address_t currentAddress_{0};
	int fontHeight_                = 0; // height of a character in this font
	int fontWidth_                 = 0; // width of a character in this font
	int iconWidth_                 = 0;
	int iconHeight_                = 0;
	int line0_                     = 0;
	int line1_                     = 0;
	int line2_                     = 0;
	int line3_                     = 0;
	int line4_                     = 0;
	int selectedInstructionSize_   = 0;
	bool movingLine1_              = false;
	bool movingLine2_              = false;
	bool movingLine3_              = false;
	bool movingLine4_              = false;
	bool selectingAddress_         = false;
	bool partialLastLine_          = false;
	QColor addressForegroundColor_ = Qt::red;
	QColor fillingBytesColor_      = Qt::gray;
	QColor badgeBackgroundColor_   = Qt::blue;
	QColor badgeForegroundColor_   = Qt::white;
	QColor takenJumpColor_         = Qt::red;

private:
	std::shared_ptr<IRegion> region_;
	QVector<edb::address_t> showAddresses_;
	std::vector<CapstoneEDB::Instruction> instructions_;
	SyntaxHighlighter *highlighter_;
	bool showAddressSeparator_;
	QHash<edb::address_t, QString> comments_;
	NavigationHistory history_;
	QSvgRenderer breakpointRenderer_;
	QSvgRenderer currentRenderer_;
	QSvgRenderer currentBpRenderer_;
	std::vector<uint8_t> instructionBuffer_;
	QCache<QString, QPixmap> syntaxCache_;

private:
	struct JumpArrow {

		int sourceLine;

		edb::address_t target;

		// if target is visible in viewport
		bool destInViewport;

		// only valid is dst_in_viewport is true
		bool destInMiddleOfInstruction;

		// if dst_in_viewport is false, then this param is ignored
		int destLine;

		// if dst_in_viewport is false, then the value here should be near INT_MAX
		size_t distance;

		// length of arrow horizontal
		int horizontalLength;
	};
};

#endif
