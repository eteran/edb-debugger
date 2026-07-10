/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryFile>
#include <QTextDocument>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

namespace AssemblerPlugin {

namespace {

/**
 * @brief Loads and returns the XML description for the currently configured assembler helper.
 *
 * @return The assembler description, or an empty document if the description could not be loaded.
 */
QDomDocument assembler_description() {
	const QString assembler = QSettings().value(QStringLiteral("Assembler/helper"), QStringLiteral("yasm")).toString();

	QFile file(QStringLiteral(":/debugger/Assembler/xml/assemblers.xml"));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return {};
	}

	QDomDocument doc;
	if (!doc.setContent(&file)) {
		return {};
	}

	// Find: assemblers/assembler[@name="..."]
	const QDomNodeList assemblers = doc.elementsByTagName(QStringLiteral("assembler"));
	for (int i = 0; i < assemblers.size(); ++i) {
		const QDomElement el = assemblers.at(i).toElement();
		if (el.attribute(QStringLiteral("name")) == assembler) {
			// Wrap the matched element in a new QDomDocument
			QDomDocument out;
			out.appendChild(out.importNode(el, true));
			return out;
		}
	}

	return {};
}

/**
 * @brief Translates operand size keywords in an instruction string to the syntax expected by the active assembler.
 *
 * @param insn The instruction string to fix up.
 * @return The instruction with fixed syntax, or the original if no changes were made.
 */
QString fixup_syntax(QString insn) {

	const QDomElement asmRoot = assembler_description().documentElement();
	if (asmRoot.isNull()) {
		return insn;
	}

	const QDomElement opSizes = asmRoot.firstChildElement(QStringLiteral("operand_sizes"));
	if (opSizes.isNull()) {
		return insn;
	}

	static const QString sizes[] = {
		QStringLiteral("byte"),
		QStringLiteral("word"),
		QStringLiteral("dword"),
		QStringLiteral("qword"),
		QStringLiteral("tbyte"),
		QStringLiteral("xmmword"),
		QStringLiteral("ymmword"),
		QStringLiteral("zmmword"),
	};

	for (const QString &size : sizes) {
		const QString replacement = opSizes.attribute(size);
		if (!replacement.isEmpty()) {
			const QRegularExpression re(QStringLiteral("\\b") + size + QStringLiteral("\\b"));
			insn.replace(re, replacement);
		}
	}

	return insn;
}

}

/**
 * @brief Constructs the assembler dialog and configures focus policies on its button widgets.
 *
 * @param parent
 * @param f
 */
DialogAssembler::DialogAssembler(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	// Disable click focus: we don't want to unnecessarily de-focus instruction entry without need
	ui.fillWithNOPs->setFocusPolicy(Qt::TabFocus);
	ui.keepSize->setFocusPolicy(Qt::TabFocus);
}

/**
 * @brief Sets the target address and pre-fills the assembly input with the current instruction's text.
 *
 * @param address
 */
void DialogAssembler::setAddress(edb::address_t address) {
	address_ = address;
	ui.address->setText(edb::v1::format_pointer(address_));

	uint8_t buffer[edb::Instruction::MaxSize];
	if (const int size = edb::v1::get_instruction_bytes(address, buffer)) {
		edb::Instruction inst(buffer, buffer + size, address);
		if (inst) {
			ui.assembly->setEditText(fixup_syntax(QString::fromStdString(edb::v1::formatter().toString(inst))).simplified());
			instructionSize_ = inst.byteSize();
		}
	}
}

/**
 * @brief Assembles the entered instruction and patches it into the debugged process at the target address.
 */
void DialogAssembler::on_buttonBox_accepted() {

	if (IDebugger *core = edb::v1::debugger_core) {
		const QString nasm_syntax = ui.assembly->currentText().trimmed();

		const QDomElement asm_root = assembler_description().documentElement();
		if (!asm_root.isNull()) {
			QDomElement asm_executable = asm_root.firstChildElement(QStringLiteral("executable"));
			QDomElement asm_template   = asm_root.firstChildElement(QStringLiteral("template"));

			qDebug() << "ASM ROOT: " << asm_root.attribute(QStringLiteral("name")) << asm_executable.attribute(QStringLiteral("command_line")) << asm_template.text();

#if defined(EDB_ARM32)
			const auto mode = core->cpuMode();
			while (mode == IDebugger::CpuMode::ARM32 && asm_template.attribute(QStringLiteral("mode")) != QStringLiteral("arm") ||
				   mode == IDebugger::CpuMode::Thumb && asm_template.attribute(QStringLiteral("mode")) != QStringLiteral("thumb")) {

				asm_template = asm_template.nextSiblingElement(QStringLiteral("template"));
				if (asm_template.isNull()) {
					QMessageBox::critical(
						this,
						tr("Error running assembler"),
						tr("Failed to locate source file template for current CPU mode"));
					return;
				}
			}
#endif

			const QString asm_name = asm_root.attribute(QStringLiteral("name"));
			const QString asm_cmd  = asm_executable.attribute(QStringLiteral("command_line"));
			const QString asm_ext  = asm_executable.attribute(QStringLiteral("extension"));
			Q_UNUSED(asm_name)

			QString asm_code = asm_template.text();

			QStringList command_line = edb::v1::parse_command_line(asm_cmd);
			if (command_line.isEmpty()) {
				QMessageBox::warning(this, tr("Couldn't Find Assembler"), tr("Failed to locate your assembler."));
				return;
			}

			QTemporaryFile source_file(QStringLiteral("%1/edb_asm_temp_%2_XXXXXX.%3").arg(QDir::tempPath()).arg(QCoreApplication::applicationPid()).arg(asm_ext));
			if (!source_file.open()) {
				QMessageBox::critical(this, tr("Error Creating File"), tr("Failed to create temporary source file."));
				return;
			}

			QTemporaryFile output_file(QStringLiteral("%1/edb_asm_temp_%2_XXXXXX.bin").arg(QDir::tempPath()).arg(QCoreApplication::applicationPid()));
			if (!output_file.open()) {
				QMessageBox::critical(this, tr("Error Creating File"), tr("Failed to create temporary object file."));
				return;
			}

			const QString bitsStr = QString::number(core->pointerSize() * 8);
			const QString addrStr = edb::v1::format_pointer(address_);

			static const QString bitsTag = QStringLiteral("%BITS%");
			static const QString addrTag = QStringLiteral("%ADDRESS%");
			static const QString insnTag = QStringLiteral("%INSTRUCTION%");

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
						arg.replace(QLatin1String("%OUT%"), output_file.fileName());
						arg.replace(QLatin1String("%IN%"), source_file.fileName());
				arg.replace(bitsTag, bitsStr);
				arg.replace(addrTag, addrStr);
				arg.replace(insnTag, nasm_syntax);
			}

			qDebug() << "RUNNING ASM TOOL: " << program << arguments;

			process.start(program, arguments);

			if (process.waitForFinished()) {

				const int exit_code = process.exitCode();

				if (exit_code != 0) {
					QMessageBox::warning(this, tr("Error In Code"), QString::fromLocal8Bit(process.readAllStandardError()));
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
						const QString stdError = QString::fromLocal8Bit(process.readAllStandardError());
						QMessageBox::warning(this, tr("Error In Code"), tr("Got zero bytes from the assembler") + (stdError.isEmpty() ? QString() : tr(", here's what it has to say:\n\n") + stdError));
						return;
					} else {
						if (ui.keepSize->isChecked()) {
							QMessageBox::warning(this, tr("Error In Code"), tr("New instruction is too big to fit."));
							return;
						}

						if (!edb::v1::modify_bytes(address_, replacement_size, bytes, 0x00)) {
							return;
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
 * @brief Updates the assembler label and focuses the assembly input when the dialog becomes visible.
 *
 * @param event
 */
void DialogAssembler::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	QSettings settings;
	const QString assembler = settings.value(QStringLiteral("Assembler/helper"), QStringLiteral("yasm")).toString();

	ui.label->setText(tr("Assembler: %1").arg(assembler));

	ui.assembly->setFocus(Qt::OtherFocusReason);
}

}
