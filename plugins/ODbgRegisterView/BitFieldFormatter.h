/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BIT_FIELD_FORMATTER_H_20191119_
#define BIT_FIELD_FORMATTER_H_20191119_

#include <QString>
#include <vector>

namespace ODbgRegisterView {

struct BitFieldDescription;

class BitFieldFormatter {
public:
	explicit BitFieldFormatter(const BitFieldDescription &bfd);
	QString operator()(const QString &str) const;

private:
	std::vector<QString> valueNames;
};

}

#endif
