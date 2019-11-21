
#ifndef SIMD_VALUE_MANAGER_H_20191119_
#define SIMD_VALUE_MANAGER_H_20191119_

#include "RegisterGroup.h"
#include "RegisterViewModelBase.h"
#include "Util.h"

#include <QAction>
#include <QModelIndex>
#include <QObject>

namespace ODbgRegisterView {

class SimdValueManager : public QObject {
	Q_OBJECT
private:
	QPersistentModelIndex regIndex_;
	int lineInGroup_;
	QList<ValueField *> elements_;
	QList<QAction *> menuItems_;
	NumberDisplayMode intMode_ = NumberDisplayMode::Hex;

	enum MenuItemNumbers {
		VIEW_AS_BYTES,
		VIEW_AS_WORDS,
		VIEW_AS_DWORDS,
		VIEW_AS_QWORDS,

		VIEW_AS_FLOAT32,
		VIEW_AS_FLOAT64,

		VIEW_INT_AS_HEX,
		VIEW_INT_AS_SIGNED,
		VIEW_INT_AS_UNSIGNED,

		MENU_ITEMS_COUNT
	};

	using Model = RegisterViewModelBase::Model;
	Model *model() const;
	RegisterGroup *group() const;
	Model::ElementSize currentSize() const;
	NumberDisplayMode currentFormat() const;
	void setupMenu();
	void updateMenu();
	void fillGroupMenu();

public:
	SimdValueManager(int lineInGroup, const QModelIndex &nameIndex, RegisterGroup *parent = nullptr);

public Q_SLOTS:
	void displayFormatChanged();

private:
	void showAsInt(Model::ElementSize size);
	void showAsFloat(Model::ElementSize size);
	void setIntFormat(NumberDisplayMode format);
};

}

#endif
