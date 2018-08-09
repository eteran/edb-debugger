
#ifndef DIALOG_XREFS_H_
#define DIALOG_XREFS_H_

#include <QDialog>
#include <utility>
#include "edb.h"

class QListWidgetItem;

namespace AnalyzerPlugin {

namespace Ui { class DialogXRefs; }

class DialogXRefs : public QDialog {
	Q_OBJECT

public:
    explicit DialogXRefs(QWidget *parent = nullptr);
	~DialogXRefs() override;

public Q_SLOTS:
	void on_listReferences_itemDoubleClicked(QListWidgetItem *item);
	
public:
	void addReference(const std::pair<edb::address_t, edb::address_t> &reference);

private:
	 Ui::DialogXRefs *const ui;
};

}

#endif
