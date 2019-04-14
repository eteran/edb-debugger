
#ifndef ROPTOOL_DIALOG_RESULTS_H
#define ROPTOOL_DIALOG_RESULTS_H

#include <QDialog>
#include <QSortFilterProxyModel>
#include "ResultsModel.h"
#include "ui_DialogResults.h"

class QSortFilterProxyModel;

namespace ROPToolPlugin {

class ResultsModel;

class ResultFilterProxy : public QSortFilterProxyModel {
	Q_OBJECT
public:
	explicit ResultFilterProxy(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
	}

public:
	void setMask(uint32_t mask, bool value) {
		beginResetModel();
		if(value) {
			mask_ |= mask;
		} else {
			mask_ &= ~mask;
		}
		endResetModel();
	}

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
		QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
		if(index.isValid()) {
			if(auto result = reinterpret_cast<const Result *>(index.internalPointer())) {
				if(result->role & mask_) {
					return true;
				}
			}
		}

		return false;
	}

private:
	uint32_t mask_ = 0xffffffff;
};

class DialogResults : public QDialog {
	Q_OBJECT

public:
	explicit DialogResults(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public:
	void addResult(const Result &result);

private Q_SLOTS:
	void on_tableView_doubleClicked(const QModelIndex &index);

public:
	int resultCount() const;

private:
	Ui::DialogResults      ui;
	ResultsModel          *model_;
	QSortFilterProxyModel *filter_model_;
	ResultFilterProxy     *result_filter_;
};




#endif

}
