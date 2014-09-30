/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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
#include "IDebuggerCore.h"
#include "edb.h"
#include "string_hash.h"

#include <QMessageBox>
#include <QDebug>
#include <QProcess>
#include <QRegExp>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

#include "ui_DialogAssembler.h"

namespace Assembler {

//------------------------------------------------------------------------------
// Name: DialogAssembler
// Desc: constructor
//------------------------------------------------------------------------------
DialogAssembler::DialogAssembler(QWidget *parent) : QDialog(parent), ui(new Ui::DialogAssembler), address_(0), instruction_size_(0) {
	ui->setupUi(this);
}

//------------------------------------------------------------------------------
// Name: ~DialogAssembler
// Desc:
//------------------------------------------------------------------------------
DialogAssembler::~DialogAssembler() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: set_address
// Desc:
//------------------------------------------------------------------------------
void DialogAssembler::set_address(edb::address_t address) {
	address_ = address;
	ui->address->setText(edb::v1::format_pointer(address_));

	quint8 buffer[edb::Instruction::MAX_SIZE];
	if(const int size = edb::v1::get_instruction_bytes(address, buffer)) {
		edb::Instruction inst(buffer, buffer + size, address, std::nothrow);
		if(inst) {
			ui->assembly->setEditText(QString::fromStdString(to_string(inst)));
			instruction_size_ = inst.size();
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_accepted
// Desc:
//------------------------------------------------------------------------------
void DialogAssembler::on_buttonBox_accepted() {

	static const QString mnemonic_regex   = "([a-z][a-z0-9]*)";
	static const QString register_regex   = "((?:(?:e|r)?(?:ax|bx|cx|dx|bp|sp|si|di|ip))|(?:[abcd](?:l|h))|(?:sp|bp|si|di)l|(?:[cdefgs]s)|(?:x?mm[0-7])|r(?:8|9|(?:1[0-5]))[dwb]?)";
	static const QString constant_regex   = "((?:0[0-7]*)|(?:0x[0-9a-f]+)|(?:[1-9][0-9]*))";

	static const QString pointer_regex    = "(?:(t?byte|(?:xmm|[qdf]?)word)(?:\\s+ptr)?)?";
	static const QString segment_regex    = "([csdefg]s)";
	static const QString expression_regex = QString("(%1\\s*(?:\\s+%2\\s*:\\s*)?\\[(\\s*(?:(?:%3(?:\\s*\\+\\s*%3(?:\\s*\\*\\s*%4)?)?(?:\\s*\\+\\s*%4)?)|(?:(?:%3(?:\\s*\\*\\s*%4)?)(?:\\s*\\+\\s*%4)?)|(?:%4)\\s*))\\])").arg(pointer_regex, segment_regex, register_regex, constant_regex);

	static const QString operand_regex    = QString("((?:%1)|(?:%2)|(?:%3))").arg(register_regex, constant_regex, expression_regex);

	static const QString assembly_regex   = QString("%1(?:\\s+%2\\s*(?:\\s*,\\s*%2\\s*(?:\\s*,\\s*%2\\s*)?)?)?").arg(mnemonic_regex, operand_regex);

// [                 OFFSET]
// [     INDEX             ]
// [     INDEX      +OFFSET]
// [     INDEX*SCALE       ]
// [     INDEX*SCALE+OFFSET]
// [BASE                   ]
// [BASE            +OFFSET]
// [BASE+INDEX             ]
// [BASE+INDEX      +OFFSET]
// [BASE+INDEX*SCALE       ]
// [BASE+INDEX*SCALE+OFFSET]
// -------------------------
// [((BASE(\+INDEX(\*SCALE)?)?(\+OFFSET)?)|((INDEX(\*SCALE)?)(\+OFFSET)?)|(OFFSET))]


	const QString assembly = ui->assembly->currentText().trimmed();
	QRegExp regex(assembly_regex, Qt::CaseInsensitive, QRegExp::RegExp2);

	if(regex.exactMatch(assembly)) {
		const QStringList list = regex.capturedTexts();


/*
[0]  -> whole match
[1]  -> mnemonic

[2]  -> whole operand 1
[3]  -> operand 1 (REGISTER)
[4]  -> operand 1 (IMMEDIATE)
[5]  -> operand 1 (EXPRESSION)
[6]  -> operand 1 pointer (EXPRESSION)
[7]  -> operand 1 segment (EXPRESSION)
[8]  -> operand 1 internal expression (EXPRESSION)
[9]  -> operand 1 base (EXPRESSION)
[10] -> operand 1 index (EXPRESSION)
[11] -> operand 1 scale (EXPRESSION)
[12] -> operand 1 displacement (EXPRESSION)
[13] -> operand 1 index (EXPRESSION) (version 2)
[14] -> operand 1 scale (EXPRESSION) (version 2)
[15] -> operand 1 displacement (EXPRESSION) (version 2)
[16] -> operand 1 displacement (EXPRESSION) (version 3)

[17] -> whole operand 2
[18] -> operand 2 (REGISTER)
[19] -> operand 2 (IMMEDIATE)
[20] -> operand 2 (EXPRESSION)
[21] -> operand 2 pointer (EXPRESSION)
[22] -> operand 2 segment (EXPRESSION)
[23] -> operand 2 internal expression (EXPRESSION)
[24] -> operand 2 base (EXPRESSION)
[25] -> operand 2 index (EXPRESSION)
[26] -> operand 2 scale (EXPRESSION)
[27] -> operand 2 displacement (EXPRESSION)
[28] -> operand 2 index (EXPRESSION) (version 2)
[29] -> operand 2 scale (EXPRESSION) (version 2)
[30] -> operand 2 displacement (EXPRESSION) (version 2)
[31] -> operand 2 displacement (EXPRESSION) (version 3)

[32] -> whole operand 3
[33] -> operand 3 (REGISTER)
[34] -> operand 3 (IMMEDIATE)
[35] -> operand 3 (EXPRESSION)
[36] -> operand 3 pointer (EXPRESSION)
[37] -> operand 3 segment (EXPRESSION)
[38] -> operand 3 internal expression (EXPRESSION)
[39] -> operand 3 base (EXPRESSION)
[40] -> operand 3 index (EXPRESSION)
[41] -> operand 3 scale (EXPRESSION)
[42] -> operand 3 displacement (EXPRESSION)
[43] -> operand 3 index (EXPRESSION) (version 2)
[44] -> operand 3 scale (EXPRESSION) (version 2)
[45] -> operand 3 displacement (EXPRESSION) (version 2)
[46] -> operand 3 displacement (EXPRESSION) (version 3)
*/

		int operand_count = 0;
		if(!list[2].isEmpty()) {
			++operand_count;
		}

		if(!list[17].isEmpty()) {
			++operand_count;
		}

		if(!list[32].isEmpty()) {
			++operand_count;
		}

		QStringList operands;

		for(int i = 0; i < operand_count; ++i) {

			int offset = 15 * i;

			if(!list[3 + offset].isEmpty()) {
				operands << list[3 + offset];
			} else if(!list[4 + offset].isEmpty()) {
				operands << list[4 + offset];
			} else if(!list[5 + offset].isEmpty()) {
				if(!list[7 + offset].isEmpty()) {
					operands << QString("%1 [%2:%3]").arg(list[6 + offset], list[7 + offset], list[8 + offset]);
				} else {
					operands << QString("%1 [%2]").arg(list[6 + offset], list[8 + offset]);
				}
			}
		}

		const QString nasm_syntax = list[1] + ' ' + operands.join(",");

		QTemporaryFile source_file(QString("%1/edb_asm_temp_%2_XXXXXX.asm").arg(QDir::tempPath()).arg(getpid()));
		if(!source_file.open()) {
			QMessageBox::critical(this, tr("Error Creating File"), tr("Failed to create temporary source file."));
			return;
		}

		QTemporaryFile output_file(QString("%1/edb_asm_temp_%2_XXXXXX.bin").arg(QDir::tempPath()).arg(getpid()));
		if(!output_file.open()) {
			QMessageBox::critical(this, tr("Error Creating File"), tr("Failed to create temporary object file."));
			return;
		}

		QSettings settings;
		const QString assembler = settings.value("Assembler/helper_application", "/usr/bin/yasm").toString();
		const QFile file(assembler);
		if(assembler.isEmpty() || !file.exists()) {
			QMessageBox::warning(this, tr("Couldn't Find Assembler"), tr("Failed to locate your assembler, please specify one in the options."));
			return;
		}

		const QFileInfo info(assembler);

		QProcess process;
		QStringList arguments;
		QString program(assembler);

		if(info.fileName() == "yasm") {

			switch(edb::v1::debugger_core->cpu_type()) {
			case edb::string_hash<'x', '8', '6'>::value:
				source_file.write("[BITS 32]\n");
				break;
			case edb::string_hash<'x', '8', '6', '-', '6', '4'>::value:
				source_file.write("[BITS 64]\n");
				break;
			default:
				Q_ASSERT(0);
			}

			source_file.write(QString("[SECTION .text vstart=0x%1 valign=1]\n\n").arg(edb::v1::format_pointer(address_)).toLatin1());
			source_file.write(nasm_syntax.toLatin1());
			source_file.write("\n");
			source_file.close();

			arguments << "-o" << output_file.fileName();
			arguments << "-f" << "bin";
			arguments << source_file.fileName();
		} else if(info.fileName() == "nasm") {

			switch(edb::v1::debugger_core->cpu_type()) {
			case edb::string_hash<'x', '8', '6'>::value:
				source_file.write("[BITS 32]\n");
				break;
			case edb::string_hash<'x', '8', '6', '-', '6', '4'>::value:
				source_file.write("[BITS 64]\n");
				break;
			default:
				Q_ASSERT(0);
			}

			source_file.write(QString("ORG 0x%1\n\n").arg(edb::v1::format_pointer(address_)).toLatin1());
			source_file.write(nasm_syntax.toLatin1());
			source_file.write("\n");
			source_file.close();


			arguments << "-o" << output_file.fileName();
			arguments << "-f" << "bin";
			arguments << source_file.fileName();
		}

		process.start(program, arguments);

		if(process.waitForFinished()) {

			const int exit_code = process.exitCode();

			if(exit_code != 0) {
				QMessageBox::warning(this, tr("Error In Code"), process.readAllStandardError());
			} else {
				QByteArray bytes = output_file.readAll();

				if(bytes.size() <= instruction_size_) {
					if(ui->fillWithNOPs->isChecked()) {
						// TODO: get system independent nop-code
						edb::v1::modify_bytes(address_, instruction_size_, bytes, 0x90);
					} else {
						edb::v1::modify_bytes(address_, instruction_size_, bytes, 0x00);
					}
				} else {
					if(ui->keepSize->isChecked()) {
						QMessageBox::warning(this, tr("Error In Code"), tr("New instruction is too big to fit."));
					} else {
						edb::v1::modify_bytes(address_, bytes.size(), bytes, 0x00);
					}
				}
			}
		}
	} else {
		QMessageBox::warning(this, tr("Error In Code"), tr("Failed to assembly the given assemble code."));
	}

}

}
