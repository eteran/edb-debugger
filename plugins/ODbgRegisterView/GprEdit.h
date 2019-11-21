/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

	QSize minimumSizeHint() const override {
		return sizeHint();
	}

	QSize sizeHint() const override;

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
