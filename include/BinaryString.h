/*
Copyright (C) 2006 - 2023 Evan Teran
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

#ifndef BINARY_STRING_H_20060821_
#define BINARY_STRING_H_20060821_

#include "API.h"
#include <QWidget>

namespace Ui {
class BinaryStringWidget;
}

class QString;
class QByteArray;

class EDB_EXPORT BinaryString : public QWidget {
	Q_OBJECT

private:
	enum class Mode {
		LengthLimited, // obeys setMaxLength()
		MemoryEditing  // obeys user's choice in keepSize checkbox
	};

public:
	BinaryString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~BinaryString() override;

private Q_SLOTS:
	void on_txtAscii_textEdited(const QString &text);
	void on_txtHex_textEdited(const QString &text);
	void on_txtUTF16_textEdited(const QString &text);
	void on_keepSize_stateChanged(int state);

public:
	[[nodiscard]] bool showKeepSize() const;
	[[nodiscard]] QByteArray value() const;
	void setMaxLength(int n);
	void setShowKeepSize(bool visible);
	void setValue(const QByteArray &);

private:
	void setEntriesMaxLength(int n);

private:
	::Ui::BinaryStringWidget *ui = nullptr;
	Mode mode_                   = Mode::MemoryEditing;
	int requestedMaxLength_      = 0;
	int valueOriginalLength_     = 0;
};

#endif
