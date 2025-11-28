/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
