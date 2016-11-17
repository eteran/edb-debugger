#include "edb.h"
#include "ISymbolManager.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPalette>

class ExpressionDialog : public QDialog {
	Q_OBJECT
	private:
		QVBoxLayout layout_;
		QLabel label_text_;
		QLabel label_error_;
		QLineEdit expression_;
		QDialogButtonBox button_box_;
		QPalette palette_error_;
		edb::address_t last_address_;
	public:
		ExpressionDialog(const QString &title, const QString prompt);

		edb::address_t getAddress();
	private slots:
		void on_text_changed(const QString& text);
};
