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

#include "DialogAssembler.h"
#include "IDebugger.h"
#include "edb.h"
#include "string_hash.h"

#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QRegExp>
#include <QSettings>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QXmlQuery>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

namespace AssemblerPlugin {

namespace {

/**
 * @brief escape_html
 * @param str
 * @return
 */
QString escape_html(const QString &str) {
	return str.toHtmlEscaped();
}

/**
 * @brief assembler_description
 * @return
 */
QDomDocument assembler_description() {

	const QString assembler = QSettings().value("Assembler/helper", "yasm").toString();

	QFile file(":/debugger/Assembler/xml/assemblers.xml");
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {

		QXmlQuery query;
		QString assembler_xml;
		query.setFocus(&file);
		query.setQuery(QString("assemblers/assembler[@name='%1']").arg(escape_html(assembler)));
		if (query.isValid()) {
			query.evaluateTo(&assembler_xml);
		}

		QDomDocument xml;
		xml.setContent(assembler_xml);
		return xml;
	}
	return {};
}

/**
 * @brief fixupSyntax
 * @param insn
 * @return
 */
QString fixup_syntax(QString insn) {

	const QDomElement asmRoot = assembler_description().documentElement();
	if (asmRoot.isNull()) {
		return insn;
	}

	const QDomElement opSizes = asmRoot.firstChildElement("operand_sizes");
	if (opSizes.isNull()) {
		return insn;
	}

	static const QString sizes[] = {
		"byte",
		"word",
		"dword",
		"qword",
		"tbyte",
		"xmmword",
		"ymmword",
		"zmmword"};

	for (const QString &size : sizes) {
		const QString replacement = opSizes.attribute(size);
		if (!replacement.isEmpty()) {
			insn.replace(QRegExp("\\b" + size + "\\b"), replacement);
		}
	}

	return insn;
}

}

/**
 * @brief DialogAssembler::DialogAssembler
 * @param parent
 * @param f
 */
DialogAssembler::DialogAssembler(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	// Disable click focus: we don't want to unnecessarily defocus instruction entry without need
	ui.fillWithNOPs->setFocusPolicy(Qt::TabFocus);
	ui.keepSize->setFocusPolicy(Qt::TabFocus);
}

/**
 * @brief DialogAssembler::setAddress
 * @param address
 */
void DialogAssembler::setAddress(edb::address_t address) {
	address_ = address;
	ui.address->setText(edb::v1::format_pointer(address_));

	uint8_t buffer[edb::Instruction::MaxSize];
	if (const int size = edb::v1::get_instruction_bytes(address, buffer)) {
		edb::Instruction inst(buffer, buffer + size, address);
		if (inst) {
			ui.assembly->setEditText(fixup_syntax(edb::v1::formatter().toString(inst).c_str()).simplified());
			instructionSize_ = inst.byteSize();
		}
	}
}

/**
 * @brief DialogAssembler::on_buttonBox_accepted
 */
void DialogAssembler::on_buttonBox_accepted() {

	if (IDebugger *core = edb::v1::debugger_core) {
		const QString nasm_syntax = ui.assembly->currentText().trimmed();

		const QDomElement asm_root = assembler_description().documentElement();
		if (!asm_root.isNull()) {
			QDomElement asm_executable = asm_root.firstChildElement("executable");
			QDomElement asm_template   = asm_root.firstChildElement("template");
#if defined(EDB_ARM32)
			const auto mode = core->cpuMode();
			while (mode == IDebugger::CpuMode::ARM32 && asm_template.attribute("mode") != "arm" ||
				   mode == IDebugger::CpuMode::Thumb && asm_template.attribute("mode") != "thumb") {

				asm_template = asm_template.nextSiblingElement("template");
				if (asm_template.isNull()) {
					QMessageBox::critical(
						this,
						tr("Error running assembler"),
						tr("Failed to locate source file template for current CPU mode"));
					return;
				}
			}
#endif

			const QString asm_name = asm_root.attribute("name");
			const QString asm_cmd  = asm_executable.attribute("command_line");
			const QString asm_ext  = asm_executable.attribute("extension");
			Q_UNUSED(asm_name)

			QString asm_code = asm_template.text();

			QStringList command_line = edb::v1::parse_command_line(asm_cmd);
			if (command_line.isEmpty()) {
				QMessageBox::warning(this, tr("Couldn't Find Assembler"), tr("Failed to locate your assembler."));
				return;
			}

			QTemporaryFile source_file(QString("%1/edb_asm_temp_%2_XXXXXX.%3").arg(QDir::tempPath()).arg(QCoreApplication::applicationPid()).arg(asm_ext));
			if (!source_file.open()) {
				QMessageBox::critical(this, tr("Error Creating File"), tr("Failed to create temporary source file."));
				return;
			}

			QTemporaryFile output_file(QString("%1/edb_asm_temp_%2_XXXXXX.bin").arg(QDir::tempPath()).arg(QCoreApplication::applicationPid()));
			if (!output_file.open()) {
				QMessageBox::critical(this, tr("Error Creating File"), tr("Failed to create temporary object file."));
				return;
			}

			const QString bitsStr = std::to_string(core->pointerSize() * 8).c_str();
			const QString addrStr = edb::v1::format_pointer(address_);

			static const char *bitsTag = "%BITS%";
			static const char *addrTag = "%ADDRESS%";
			static const char *insnTag = "%INSTRUCTION%";

			asm_code.replace(bitsTag, bitsStr);
			asm_code.replace(addrTag, addrStr);
			asm_code.replace(insnTag, nasm_syntax);

			source_file.write(asm_code.toLatin1());
			source_file.close();

			QProcess process;
			QString program(command_line[0]);

			command_line.pop_front();

			QStringList arguments = command_line;
			for (QString &arg : arguments) {
				arg.replace("%OUT%", output_file.fileName());
				arg.replace("%IN%", source_file.fileName());
				arg.replace(bitsTag, bitsStr);
				arg.replace(addrTag, addrStr);
				arg.replace(insnTag, nasm_syntax);
			}

			qDebug() << "RUNNING ASM TOOL: " << program << arguments;

			process.start(program, arguments);

			if (process.waitForFinished()) {

				const int exit_code = process.exitCode();

				if (exit_code != 0) {
					QMessageBox::warning(this, tr("Error In Code"), process.readAllStandardError());
				} else {
					QByteArray bytes              = output_file.readAll();
					const size_t replacement_size = bytes.size();

					if (replacement_size != 0 && replacement_size <= instructionSize_) {
						if (ui.fillWithNOPs->isChecked()) {
							if (!edb::v1::modify_bytes(address_, instructionSize_, bytes, core->nopFillByte())) {
								return;
							}
						} else {
							if (!edb::v1::modify_bytes(address_, instructionSize_, bytes, 0x00)) {
								return;
							}
						}
					} else if (replacement_size == 0) {
						const QString stdError = process.readAllStandardError();
						QMessageBox::warning(this, tr("Error In Code"), tr("Got zero bytes from the assembler") + (stdError.isEmpty() ? "" : tr(", here's what it has to say:\n\n") + stdError));
						return;
					} else {
						if (ui.keepSize->isChecked()) {
							QMessageBox::warning(this, tr("Error In Code"), tr("New instruction is too big to fit."));
							return;
						} else {
							if (!edb::v1::modify_bytes(address_, replacement_size, bytes, 0x00)) {
								return;
							}
						}
					}

					const edb::address_t new_address = address_ + replacement_size;
					setAddress(new_address);
					edb::v1::set_cpu_selected_address(new_address);
				}
				ui.assembly->setFocus(Qt::OtherFocusReason);
				ui.assembly->lineEdit()->selectAll();
			} else if (process.error() == QProcess::FailedToStart) {
				QMessageBox::warning(this, tr("Couldn't Launch Assembler"), tr("Failed to start your assembler."));
				return;
			}
		}
	}
}

/**
 * @brief DialogAssembler::showEvent
 * @param event
 */
void DialogAssembler::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	QSettings settings;
	const QString assembler = settings.value("Assembler/helper", "yasm").toString();

	ui.label->setText(tr("Assembler: %1").arg(assembler));

	ui.assembly->setFocus(Qt::OtherFocusReason);
}

}
