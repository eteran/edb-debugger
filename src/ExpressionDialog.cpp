#include "ExpressionDialog.h"
#include "Expression.h"

#include <QPushButton>
#include <QCompleter>

ExpressionDialog::ExpressionDialog(const QString &title, const QString prompt) :
	QDialog(edb::v1::debugger_ui),
	layout_(this),
	label_text_("Replace me"),
	button_box_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal)
{
	setWindowTitle(title);
	label_text_.setText(prompt);
	connect(&button_box_, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&button_box_, SIGNAL(rejected()), this, SLOT(reject()));

	layout_.addWidget(&label_text_);
	layout_.addWidget(&expression_);
	layout_.addWidget(&label_error_);
	layout_.addWidget(&button_box_);

	palette_error_.setColor(QPalette::WindowText, Qt::red);
	label_error_.setPalette(palette_error_);

	button_box_.button(QDialogButtonBox::Ok)->setEnabled(false);

	setLayout(&layout_);

	connect(&expression_, SIGNAL(textChanged(const QString&)), this, SLOT(on_text_changed(const QString&)));
	expression_.selectAll();

	QList<Symbol::pointer> symbols = edb::v1::symbol_manager().symbols();
	QList<QString> allLabels;

	for(const Symbol::pointer &sym: symbols)
	{
		allLabels.append(sym->name_no_prefix);
	}
	allLabels.append(edb::v1::symbol_manager().labels().values());

	QCompleter *completer = new QCompleter(allLabels);
	expression_.setCompleter(completer);
	allLabels.clear();
}

void ExpressionDialog::on_text_changed(const QString& text) {
	QHash<edb::address_t, QString> labels = edb::v1::symbol_manager().labels();
	edb::address_t resAddr = labels.key(text);

	bool retval = false;

	if (resAddr)
	{
		last_address_ = resAddr;
		retval = true;
	}
	else
	{
		Expression<edb::address_t> expr(text, edb::v1::get_variable, edb::v1::get_value);
		ExpressionError err;

		bool ok;
		last_address_ = expr.evaluate_expression(&ok, &err);
		if(ok) {
			retval = true;
		} else {
			label_error_.setText(err.what());
			retval = false;
		}
	}

	button_box_.button(QDialogButtonBox::Ok)->setEnabled(retval);
	if (retval) {
		label_error_.clear();
	}
}

edb::address_t ExpressionDialog::getAddress() {
	return last_address_;
}
