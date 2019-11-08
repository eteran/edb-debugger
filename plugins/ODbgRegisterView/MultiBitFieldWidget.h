
#ifndef MULTI_BITFIELD_WIDGET_H_
#define MULTI_BITFIELD_WIDGET_H_

#include "ValueField.h"

namespace ODbgRegisterView {

struct BitFieldDescription;

class MultiBitFieldWidget final : public ValueField {
	Q_OBJECT

public:
	MultiBitFieldWidget(const QModelIndex &index, const BitFieldDescription &bfd, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public Q_SLOTS:
	void setValue(int value);
	void adjustToData() override;

private:
	QList<QAction *> valueActions_;
	std::function<bool(unsigned, unsigned)> equal_;
};

}

#endif
