/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYNTAX_HIGHLIGHTER_H_20191119_
#define SYNTAX_HIGHLIGHTER_H_20191119_

#include <QRegExp>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QVector>
#include <vector>

class SyntaxHighlighter : public QObject {
	Q_OBJECT

public:
	explicit SyntaxHighlighter(QObject *parent = nullptr);

private:
	void createRules();

public:
	QVector<QTextLayout::FormatRange> highlightBlock(const QString &text);

private:
	struct HighlightingRule {
		HighlightingRule() = default;
		HighlightingRule(const QString &regex, const QTextCharFormat &fmt);

		QRegExp pattern;
		QTextCharFormat format;
	};

	std::vector<HighlightingRule> rules_;
};

#endif
