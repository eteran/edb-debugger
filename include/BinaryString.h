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

#ifndef BINARYSTRING_20060821_H_
#define BINARYSTRING_20060821_H_

#include "API.h"

#include <QWidget>

namespace Ui { class BinaryStringWidget; }

class QString;
class QByteArray;

class EDB_EXPORT BinaryString : public QWidget {
	Q_OBJECT

public:
	BinaryString(QWidget *parent = 0);
	virtual ~BinaryString();

private Q_SLOTS:
	void on_txtAscii_textEdited(const QString &);
	void on_txtHex_textEdited(const QString &);
	void on_txtUTF16_textEdited(const QString &);
	void on_keepSize_stateChanged();

public:
	void setMaxLength(int n);
	QByteArray value() const;
	void setValue(const QByteArray &);

private:
	void setEntriesMaxLength(int n);

	Ui::BinaryStringWidget *const ui;
	enum class Mode
	{
	    LengthLimited, // obeys setMaxLength()
	    MemoryEditing  // obeys user's choice in keepSize checkbox
	} mode;
	int requestedMaxLength;
	int valueOriginalLength;
};

#endif

