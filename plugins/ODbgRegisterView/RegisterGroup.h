
#ifndef REGISTER_GROUP_H_
#define REGISTER_GROUP_H_

#include <QWidget>

namespace ODbgRegisterView {

class SIMDValueManager;
class FieldWidget;
class ValueField;
class ODBRegView;

class RegisterGroup : public QWidget {
	Q_OBJECT
	friend SIMDValueManager;

private:
	QList<QAction *> menuItems;
	QString name;

	int lineAfterLastField() const;
	ODBRegView *regView() const;

public:
	explicit RegisterGroup(const QString &name, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	QList<FieldWidget *> fields() const;
	QList<ValueField *> valueFields() const;
	void setIndices(const QList<QModelIndex> &indices);
	void insert(int line, int column, FieldWidget *widget);
	// Insert, but without moving to its place
	void insert(FieldWidget *widget);
	void setupPositionAndSize(int line, int column, FieldWidget *widget);
	void appendNameValueComment(const QModelIndex &nameIndex, const QString &tooltip = "", bool insertComment = true);
	void showMenu(const QPoint &position, const QList<QAction *> &additionalItems = {}) const;
	QMargins getFieldMargins() const;

protected:
	void mousePressEvent(QMouseEvent *event) override;

public Q_SLOTS:
	void adjustWidth();
};

}

#endif
