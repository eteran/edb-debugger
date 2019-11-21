
#ifndef REGISTER_GROUP_H_20191119_
#define REGISTER_GROUP_H_20191119_

#include <QWidget>

namespace ODbgRegisterView {

class SimdValueManager;
class FieldWidget;
class ValueField;
class ODBRegView;

class RegisterGroup : public QWidget {
	Q_OBJECT
	friend SimdValueManager;

public:
	explicit RegisterGroup(const QString &name_, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
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

public Q_SLOTS:
	void adjustWidth();

protected:
	void mousePressEvent(QMouseEvent *event) override;

private:
	int lineAfterLastField() const;
	ODBRegView *regView() const;

private:
	QList<QAction *> menuItems_;
	QString name_;
};

}

#endif
