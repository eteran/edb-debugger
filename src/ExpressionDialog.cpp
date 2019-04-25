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

#include "ExpressionDialog.h"
#include "Expression.h"
#include "ISymbolManager.h"	
#include "Symbol.h"
#include "edb.h"

#include <QCompleter>
#include <QPushButton>

ExpressionDialog::ExpressionDialog(const QString &title, const QString &prompt, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {

	setWindowTitle(title);

	layout_      = new QVBoxLayout(this);
	label_text_  = new QLabel(prompt, this);
	label_error_ = new QLabel(this);
	expression_  = new QLineEdit(this);
	button_box_  = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

	connect(button_box_, &QDialogButtonBox::accepted, this, &ExpressionDialog::accept);
	connect(button_box_, &QDialogButtonBox::rejected, this, &ExpressionDialog::reject);

	layout_->addWidget(label_text_);
	layout_->addWidget(expression_);
	layout_->addWidget(label_error_);
	layout_->addWidget(button_box_);

	palette_error_.setColor(QPalette::WindowText, Qt::red);
	label_error_->setPalette(palette_error_);

	button_box_->button(QDialogButtonBox::Ok)->setEnabled(false);

	setLayout(layout_);

	connect(expression_, &QLineEdit::textChanged, this, &ExpressionDialog::on_text_changed);
	expression_->selectAll();

	QList<std::shared_ptr<Symbol>> symbols = edb::v1::symbol_manager().symbols();
	QStringList allLabels;

	for(const std::shared_ptr<Symbol> &sym: symbols) {
		allLabels.append(sym->name_no_prefix);
	}

	allLabels.append(edb::v1::symbol_manager().labels().values());

	QCompleter *completer = new QCompleter(allLabels, this);
	expression_->setCompleter(completer);
	allLabels.clear();
}

void ExpressionDialog::on_text_changed(const QString& text) {
	QHash<edb::address_t, QString> labels = edb::v1::symbol_manager().labels();
	edb::address_t resAddr = labels.key(text);

	bool retval = false;

	if (resAddr) {
		last_address_ = resAddr;
		retval = true;
	} else {
		Expression<edb::address_t> expr(text, edb::v1::get_variable, edb::v1::get_value);

		Result<edb::address_t, ExpressionError> address = expr.evaluate_expression();
		if(address) {
			label_error_->clear();
			retval = true;
			last_address_ = *address;
		} else {
			label_error_->setText(address.error().what());
			retval = false;
		}
	}

	button_box_->button(QDialogButtonBox::Ok)->setEnabled(retval);
}

edb::address_t ExpressionDialog::getAddress() {
	return last_address_;
}
