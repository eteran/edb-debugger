/*
Copyright (C) 2006 - 2015 Evan Teran
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

#include "DialogOptions.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "edb.h"
#include "Theme.h"

#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QToolBox>

namespace {

//------------------------------------------------------------------------------
// Name: width_to_index
// Desc:
//------------------------------------------------------------------------------
constexpr int width_to_index(int n) {
	switch (n) {
	case 16:
		return 4;
	case 8:
		return 3;
	case 4:
		return 2;
	case 2:
		return 1;
	default:
		return 0;
	}
}
}

//------------------------------------------------------------------------------
// Name: DialogOptions
// Desc:
//------------------------------------------------------------------------------
DialogOptions::DialogOptions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	ui.comboTheme->addItem(tr("System"), "System");
	ui.comboTheme->addItem(tr("Dark [Built-in]"), "Dark [Built-in]");
	ui.comboTheme->addItem(tr("Light [Built-in]"), "Light [Built-in]");
}

//------------------------------------------------------------------------------
// Name: font_from_dialog
// Desc:
//------------------------------------------------------------------------------
QString DialogOptions::fontFromDialog(const QString &default_font) {
	QFont old_font;
	old_font.fromString(default_font);
	return QFontDialog::getFont(nullptr, old_font, this).toString();
}

//------------------------------------------------------------------------------
// Name: addOptionsPage
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::addOptionsPage(QWidget *page) {

	if (!toolbox_) {
		delete ui.tabWidget->findChild<QLabel *>("label_plugins");
		QWidget *const tab        = ui.tabWidget->findChild<QLabel *>("tab_plugins");
		QGridLayout *const layout = ui.tabWidget->findChild<QGridLayout *>("tab_plugins_layout");
		toolbox_                  = new QToolBox(tab);
		layout->addWidget(toolbox_, 0, 0, 1, 1);
	}

	toolbox_->addItem(page, QIcon::fromTheme("plugin"), page->windowTitle());
}

//------------------------------------------------------------------------------
// Name: directoryFromDialog
// Desc:
//------------------------------------------------------------------------------
QString DialogOptions::directoryFromDialog() {
	return QFileDialog::getExistingDirectory(
		this,
		tr("Choose a directory"),
		QString(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

//------------------------------------------------------------------------------
// Name: on_btnTTY_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::on_btnTTY_clicked() {
	const QString filename = QFileDialog::getOpenFileName(
		this,
		tr("Choose Your Terminal Program"));

	ui.txtTTY->setText(filename);
}

//------------------------------------------------------------------------------
// Name: on_btnSymbolDir_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::on_btnSymbolDir_clicked() {
	const QString s = directoryFromDialog();

	if (!s.isEmpty()) {
		ui.txtSymbolDir->setText(s);
	}
}

//------------------------------------------------------------------------------
// Name: on_btnSessionDir_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::on_btnSessionDir_clicked() {
	const QString s = directoryFromDialog();

	if (!s.isEmpty()) {
		ui.txtSessionDir->setText(s);
	}
}

//------------------------------------------------------------------------------
// Name: on_btnPluginDir_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::on_btnPluginDir_clicked() {
	const QString s = directoryFromDialog();

	if (!s.isEmpty()) {
		ui.txtPluginDir->setText(s);
	}
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::showEvent(QShowEvent *event) {

	QDialog::showEvent(event);

	const Configuration &config = edb::v1::config();

	ui.chkHexOffsets->setChecked(config.function_offsets_in_hex);

	ui.rdoSytntaxATT->setChecked(config.syntax == Configuration::Att);
	ui.rdoSytntaxIntel->setChecked(config.syntax != Configuration::Att);

	ui.rdoDetach->setChecked(config.close_behavior == Configuration::Detach);
	ui.rdoKill->setChecked(config.close_behavior == Configuration::Kill);
	ui.rdoReverseCapture->setChecked(config.close_behavior == Configuration::KillIfLaunchedDetachIfAttached);

	ui.rdoBPEntry->setChecked(config.initial_breakpoint == Configuration::EntryPoint);
	ui.rdoBPMain->setChecked(config.initial_breakpoint != Configuration::EntryPoint);

	ui.chkTTY->setChecked(config.tty_enabled);
	ui.txtTTY->setText(config.tty_command);

	ui.chkDeleteStaleSymbols->setChecked(config.remove_stale_symbols);
	ui.chkDisableASLR->setChecked(config.disableASLR);
	ui.chkDisableLazyBinding->setChecked(config.disableLazyBinding);

	ui.chkBreakOnLibraryLoad->setChecked(config.break_on_library_load);

	ui.chkZerosAreFilling->setChecked(config.zeros_are_filling);
	ui.chkRegisterBadges->setChecked(config.show_register_badges);
	ui.chkUppercase->setChecked(config.uppercase_disassembly);
	ui.chkSyntaxHighlighting->setChecked(config.syntax_highlighting_enabled);

	ui.chkFindMain->setChecked(config.find_main);
	ui.chkWarnDataBreakpoint->setChecked(config.warn_on_no_exec_bp);

	ui.spnMinString->setValue(config.min_string_length);

	ui.stackFont->setCurrentFont(config.stack_font);
	ui.dataFont->setCurrentFont(config.data_font);
	ui.registerFont->setCurrentFont(config.registers_font);
	ui.disassemblyFont->setCurrentFont(config.disassembly_font);

	ui.txtSymbolDir->setText(config.symbol_path);
	ui.txtPluginDir->setText(config.plugin_path);
	ui.txtSessionDir->setText(config.session_path);

	ui.chkDataShowAddress->setChecked(config.data_show_address);
	ui.chkDataShowHex->setChecked(config.data_show_hex);
	ui.chkDataShowAscii->setChecked(config.data_show_ascii);
	ui.chkDataShowComments->setChecked(config.data_show_comments);
	ui.cmbDataWordWidth->setCurrentIndex(width_to_index(config.data_word_width));
	ui.cmbDataRowWidth->setCurrentIndex(width_to_index(config.data_row_width));

	ui.chkAddressColon->setChecked(config.show_address_separator);

	ui.chkShowJumpArrow->setChecked(config.show_jump_arrow);

	ui.signalsMessageBoxEnable->setChecked(config.enable_signals_message_box);

	ui.chkTabBetweenMnemonicAndOperands->setChecked(config.tab_between_mnemonic_and_operands);
	ui.chkShowLocalModuleName->setChecked(config.show_local_module_name_in_jump_targets);
	ui.chkShowSymbolicAddresses->setChecked(config.show_symbolic_addresses);
	ui.chkSimplifyRIPRelativeTargets->setChecked(config.simplify_rip_relative_targets);

	ui.rdoPlaceDefault->setChecked(config.startup_window_location == Configuration::SystemDefault);
	ui.rdoPlaceCentered->setChecked(config.startup_window_location == Configuration::Centered);
	ui.rdoPlaceRestore->setChecked(config.startup_window_location == Configuration::Restore);

	ui.listIgnoredExceptions->clear();
	if (edb::v1::debugger_core) {
		QMap<qlonglong, QString> known_exceptions = edb::v1::debugger_core->exceptions();

		for (auto it = known_exceptions.begin(); it != known_exceptions.end(); ++it) {
			auto item = new QListWidgetItem(*it, ui.listIgnoredExceptions);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

			if (config.ignored_exceptions.contains(it.key())) {
				item->setCheckState(Qt::Checked);
			} else {
				item->setCheckState(Qt::Unchecked);
			}
			item->setData(Qt::UserRole, it.key());
		}
	}

	if (IDebugger *core = edb::v1::debugger_core) {
		const auto &bps  = core->supportedBreakpointTypes();
		const auto combo = ui.cmbDefaultBreakpointType;
		combo->clear();
		const auto chosen = config.default_breakpoint_type;
		for (const auto &type : bps) {
			combo->addItem(type.description, QVariant::fromValue(type.type));
			if (type.type == chosen)
				combo->setCurrentIndex(combo->count() - 1);
		}
	}

	// setup the theme list ONCE
	if(currentThemeName_.isEmpty()) {
		QStringList themes = Theme::userThemes();
		for(QString &theme : themes) {
			QString name = Theme::themeName(theme);
			ui.comboTheme->addItem(name, theme);
		}

		int index = ui.comboTheme->findData(config.theme_name);
		if(index == -1) {
			qDebug("Theme not found, defaulting to System");
			index = 0;
		}
		ui.comboTheme->setCurrentIndex(index);
	}

	currentThemeName_ = ui.comboTheme->currentData().toString();

}

//------------------------------------------------------------------------------
// Name: closeEvent
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::closeEvent(QCloseEvent *event) {

	Configuration &config = edb::v1::config();

	if (ui.rdoSytntaxIntel->isChecked()) {
		config.syntax = Configuration::Intel;
	} else if (ui.rdoSytntaxATT->isChecked()) {
		config.syntax = Configuration::Att;
	}

	config.tab_between_mnemonic_and_operands      = ui.chkTabBetweenMnemonicAndOperands->isChecked();
	config.show_local_module_name_in_jump_targets = ui.chkShowLocalModuleName->isChecked();
	config.show_symbolic_addresses                = ui.chkShowSymbolicAddresses->isChecked();
	config.simplify_rip_relative_targets          = ui.chkSimplifyRIPRelativeTargets->isChecked();

	if (ui.rdoDetach->isChecked()) {
		config.close_behavior = Configuration::Detach;
	} else if (ui.rdoKill->isChecked()) {
		config.close_behavior = Configuration::Kill;
	} else if (ui.rdoReverseCapture->isChecked()) {
		config.close_behavior = Configuration::KillIfLaunchedDetachIfAttached;
	}

	config.stack_font              = ui.stackFont->currentFont().toString();
	config.data_font               = ui.dataFont->currentFont().toString();
	config.registers_font          = ui.registerFont->currentFont().toString();
	config.disassembly_font        = ui.disassemblyFont->currentFont().toString();
	config.tty_command             = ui.txtTTY->text();
	config.tty_enabled             = ui.chkTTY->isChecked();
	config.remove_stale_symbols    = ui.chkDeleteStaleSymbols->isChecked();
	config.disableASLR             = ui.chkDisableASLR->isChecked();
	config.disableLazyBinding      = ui.chkDisableLazyBinding->isChecked();
	config.break_on_library_load   = ui.chkBreakOnLibraryLoad->isChecked();
	config.default_breakpoint_type = ui.cmbDefaultBreakpointType->itemData(ui.cmbDefaultBreakpointType->currentIndex()).value<IBreakpoint::TypeId>();

	config.function_offsets_in_hex = ui.chkHexOffsets->isChecked();
	config.show_jump_arrow         = ui.chkShowJumpArrow->isChecked();

	config.zeros_are_filling           = ui.chkZerosAreFilling->isChecked();
	config.show_register_badges        = ui.chkRegisterBadges->isChecked();
	config.uppercase_disassembly       = ui.chkUppercase->isChecked();
	config.syntax_highlighting_enabled = ui.chkSyntaxHighlighting->isChecked();

	config.symbol_path  = ui.txtSymbolDir->text();
	config.plugin_path  = ui.txtPluginDir->text();
	config.session_path = ui.txtSessionDir->text();

	if (ui.rdoBPMain->isChecked()) {
		config.initial_breakpoint = Configuration::MainSymbol;
	} else if (ui.rdoBPEntry->isChecked()) {
		config.initial_breakpoint = Configuration::EntryPoint;
	}

	config.warn_on_no_exec_bp = ui.chkWarnDataBreakpoint->isChecked();
	config.find_main          = ui.chkFindMain->isChecked();

	config.show_address_separator = ui.chkAddressColon->isChecked();

	config.min_string_length = ui.spnMinString->value();

	config.data_show_address  = ui.chkDataShowAddress->isChecked();
	config.data_show_hex      = ui.chkDataShowHex->isChecked();
	config.data_show_ascii    = ui.chkDataShowAscii->isChecked();
	config.data_show_comments = ui.chkDataShowComments->isChecked();
	config.data_word_width    = 1 << ui.cmbDataWordWidth->currentIndex();
	config.data_row_width     = 1 << ui.cmbDataRowWidth->currentIndex();

	CapstoneEDB::Formatter::FormatOptions options = edb::v1::formatter().options();
	options.capitalization                        = config.uppercase_disassembly ? CapstoneEDB::Formatter::UpperCase : CapstoneEDB::Formatter::LowerCase;
	options.syntax                                = static_cast<CapstoneEDB::Formatter::Syntax>(config.syntax);
	options.tabBetweenMnemonicAndOperands         = config.tab_between_mnemonic_and_operands;
	options.simplifyRIPRelativeTargets            = config.simplify_rip_relative_targets;
	edb::v1::formatter().setOptions(options);

	config.enable_signals_message_box = ui.signalsMessageBoxEnable->isChecked();

	if (ui.rdoPlaceDefault->isChecked()) {
		config.startup_window_location = Configuration::SystemDefault;
	} else if (ui.rdoPlaceCentered->isChecked()) {
		config.startup_window_location = Configuration::Centered;
	} else if (ui.rdoPlaceRestore->isChecked()) {
		config.startup_window_location = Configuration::Restore;
	}

	config.ignored_exceptions.clear();
	for (int i = 0; i < ui.listIgnoredExceptions->count(); ++i) {
		auto item = ui.listIgnoredExceptions->item(i);
		if (item->checkState() == Qt::Checked) {
			config.ignored_exceptions.push_back(item->data(Qt::UserRole).toLongLong());
		}
	}

	if (IDebugger *core = edb::v1::debugger_core) {
		core->setIgnoredExceptions(config.ignored_exceptions);
	}

	QString newThemeName = ui.comboTheme->currentData().toString();
	currentThemeName_ = newThemeName;
	config.theme_name = newThemeName;

	config.sendChangeNotification();
	event->accept();
}

//------------------------------------------------------------------------------
// Name: accept
// Desc:
//------------------------------------------------------------------------------
void DialogOptions::accept() {
	close();
}
