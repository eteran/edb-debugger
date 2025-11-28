/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GPR_EDIT_H_20190412_
#define GPR_EDIT_H_20190412_

#include <QLineEdit>

namespace ODbgRegisterView {

class GprEdit final : public QLineEdit {
	Q_OBJECT

public:
	enum class Format {
		Hex,
		Signed,
		Unsigned,
		Character
	};

public:
	GprEdit(std::size_t offsetInInteger, std::size_t integerSize, Format format, QWidget *parent = nullptr);

public:
	void setGPRValue(std::uint64_t gprValue);
	void updateGPRValue(std::uint64_t &gpr) const;

	[[nodiscard]] QSize minimumSizeHint() const override {
		return sizeHint();
	}

	[[nodiscard]] QSize sizeHint() const override;

private:
	void setupFormat(Format newFormat);

private:
	int naturalWidthInChars_;
	std::size_t integerSize_;
	std::size_t offsetInInteger_;
	Format format_;
	std::uint64_t signBit_;
};

}

#endif
