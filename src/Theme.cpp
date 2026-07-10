/*
 * Copyright (C) 2020 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Theme.h"
#include "Configuration.h"
#include "edb.h"
#include <QApplication>
#include <QDir>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>

namespace {

/**
 * @brief Returns the directory where themes are stored.
 *
 * @return The path to the theme directory.
 */
QString themeDirectory() {
	static const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	return QStringLiteral("%1/%2/%3").arg(configDir, QApplication::organizationName(), QLatin1String("themes"));
}

/**
 * @brief Reads a color from the settings.
 *
 * @param settings The settings object to read from.
 * @param name The name of the color to read.
 * @param defaultValue The default value to return if the color is not found.
 * @return The color read from the settings or the default value if not found.
 */
QColor readColor(QSettings &settings, const QString &name, const QColor &defaultValue = QColor()) {

	QVariant variant = settings.value(name);

	auto color = variant.value<QColor>();
	if (color.isValid()) {
		return color;
	}

	return defaultValue;
}

/**
 * @brief Reads a text character format from the settings.
 *
 * @param settings The settings object to read from.
 * @param name The name of the format to read.
 * @param defaultValue The default value to return if the format is not found.
 * @return The format read from the settings or the default value if not found.
 */
QTextCharFormat readFormat(QSettings &settings, const QString &name, const QTextCharFormat &defaultValue = QTextCharFormat()) {

	QTextCharFormat format;
	format.setForeground(readColor(settings, QStringLiteral("%1.foreground").arg(name), defaultValue.foreground().color()));
	format.setBackground(readColor(settings, QStringLiteral("%1.background").arg(name), defaultValue.background().color()));
	format.setFontWeight(settings.value(QStringLiteral("%1.weight").arg(name), defaultValue.fontWeight()).toInt());
	format.setFontItalic(settings.value(QStringLiteral("%1.italic").arg(name), defaultValue.fontItalic()).toBool());
	format.setFontUnderline(settings.value(QStringLiteral("%1.underline").arg(name), defaultValue.fontUnderline()).toBool());
	return format;
}

/**
 * @brief Reads the theme from the settings.
 *
 * @param settings The settings object to read from.
 * @param baseTheme The base theme to use as a fallback.
 * @return The theme read from the settings or the base theme if not found.
 */
Theme readTheme(QSettings &settings, const Theme &baseTheme = Theme()) {

	Theme theme;

	settings.beginGroup(QStringLiteral("Theme"));
	// General application palette
	theme.palette[Theme::Window]                  = readColor(settings, QStringLiteral("palette.window"), baseTheme.palette[Theme::Window]);
	theme.palette[Theme::WindowDisabled]          = readColor(settings, QStringLiteral("palette.window.disabled"), baseTheme.palette[Theme::WindowDisabled]);
	theme.palette[Theme::WindowText]              = readColor(settings, QStringLiteral("palette.windowtext"), baseTheme.palette[Theme::WindowText]);
	theme.palette[Theme::WindowTextDisabled]      = readColor(settings, QStringLiteral("palette.windowtext.disabled"), baseTheme.palette[Theme::WindowTextDisabled]);
	theme.palette[Theme::Base]                    = readColor(settings, QStringLiteral("palette.base"), baseTheme.palette[Theme::Base]);
	theme.palette[Theme::BaseDisabled]            = readColor(settings, QStringLiteral("palette.base.disabled"), baseTheme.palette[Theme::BaseDisabled]);
	theme.palette[Theme::AlternateBase]           = readColor(settings, QStringLiteral("palette.alternatebase"), baseTheme.palette[Theme::AlternateBase]);
	theme.palette[Theme::AlternateBaseDisabled]   = readColor(settings, QStringLiteral("palette.alternatebase.disabled"), baseTheme.palette[Theme::AlternateBaseDisabled]);
	theme.palette[Theme::ToolTipBase]             = readColor(settings, QStringLiteral("palette.tooltipbase"), baseTheme.palette[Theme::ToolTipBase]);
	theme.palette[Theme::ToolTipBaseDisabled]     = readColor(settings, QStringLiteral("palette.tooltipbase.disabled"), baseTheme.palette[Theme::ToolTipBaseDisabled]);
	theme.palette[Theme::ToolTipText]             = readColor(settings, QStringLiteral("palette.tooltiptext"), baseTheme.palette[Theme::ToolTipText]);
	theme.palette[Theme::ToolTipTextDisabled]     = readColor(settings, QStringLiteral("palette.tooltiptext.disabled"), baseTheme.palette[Theme::ToolTipTextDisabled]);
	theme.palette[Theme::Text]                    = readColor(settings, QStringLiteral("palette.text"), baseTheme.palette[Theme::Text]);
	theme.palette[Theme::TextDisabled]            = readColor(settings, QStringLiteral("palette.text.disabled"), baseTheme.palette[Theme::TextDisabled]);
	theme.palette[Theme::Button]                  = readColor(settings, QStringLiteral("palette.button"), baseTheme.palette[Theme::Button]);
	theme.palette[Theme::ButtonDisabled]          = readColor(settings, QStringLiteral("palette.button.disabled"), baseTheme.palette[Theme::ButtonDisabled]);
	theme.palette[Theme::ButtonText]              = readColor(settings, QStringLiteral("palette.buttontext"), baseTheme.palette[Theme::ButtonText]);
	theme.palette[Theme::ButtonTextDisabled]      = readColor(settings, QStringLiteral("palette.buttontext.disabled"), baseTheme.palette[Theme::ButtonTextDisabled]);
	theme.palette[Theme::BrightText]              = readColor(settings, QStringLiteral("palette.brighttext"), baseTheme.palette[Theme::BrightText]);
	theme.palette[Theme::BrightTextDisabled]      = readColor(settings, QStringLiteral("palette.brighttext.disabled"), baseTheme.palette[Theme::BrightTextDisabled]);
	theme.palette[Theme::Highlight]               = readColor(settings, QStringLiteral("palette.highlight"), baseTheme.palette[Theme::Highlight]);
	theme.palette[Theme::HighlightDisabled]       = readColor(settings, QStringLiteral("palette.highlight.disabled"), baseTheme.palette[Theme::HighlightDisabled]);
	theme.palette[Theme::HighlightedText]         = readColor(settings, QStringLiteral("palette.highlightedtext"), baseTheme.palette[Theme::HighlightedText]);
	theme.palette[Theme::HighlightedTextDisabled] = readColor(settings, QStringLiteral("palette.highlightedtext.disabled"), baseTheme.palette[Theme::HighlightedTextDisabled]);
	theme.palette[Theme::Link]                    = readColor(settings, QStringLiteral("palette.link"), baseTheme.palette[Theme::Link]);
	theme.palette[Theme::LinkDisabled]            = readColor(settings, QStringLiteral("palette.link.disabled"), baseTheme.palette[Theme::LinkDisabled]);
	theme.palette[Theme::LinkVisited]             = readColor(settings, QStringLiteral("palette.linkvisited"), baseTheme.palette[Theme::LinkVisited]);
	theme.palette[Theme::LinkVisitedDisabled]     = readColor(settings, QStringLiteral("palette.linkvisited.disabled"), baseTheme.palette[Theme::LinkVisitedDisabled]);
	theme.palette[Theme::Light]                   = readColor(settings, QStringLiteral("palette.light"), baseTheme.palette[Theme::Light]);
	theme.palette[Theme::LightDisabled]           = readColor(settings, QStringLiteral("palette.light.disabled"), baseTheme.palette[Theme::LightDisabled]);
	theme.palette[Theme::Midlight]                = readColor(settings, QStringLiteral("palette.midlight"), baseTheme.palette[Theme::Midlight]);
	theme.palette[Theme::MidlightDisabled]        = readColor(settings, QStringLiteral("palette.midlight.disabled"), baseTheme.palette[Theme::MidlightDisabled]);
	theme.palette[Theme::Dark]                    = readColor(settings, QStringLiteral("palette.dark"), baseTheme.palette[Theme::Dark]);
	theme.palette[Theme::DarkDisabled]            = readColor(settings, QStringLiteral("palette.dark.disabled"), baseTheme.palette[Theme::DarkDisabled]);
	theme.palette[Theme::Mid]                     = readColor(settings, QStringLiteral("palette.mid"), baseTheme.palette[Theme::Mid]);
	theme.palette[Theme::MidDisabled]             = readColor(settings, QStringLiteral("palette.mid.disabled"), baseTheme.palette[Theme::MidDisabled]);
	theme.palette[Theme::Shadow]                  = readColor(settings, QStringLiteral("palette.shadow"), baseTheme.palette[Theme::Shadow]);
	theme.palette[Theme::ShadowDisabled]          = readColor(settings, QStringLiteral("palette.shadow.disabled"), baseTheme.palette[Theme::ShadowDisabled]);

	// various text/syntax settings
	theme.text[Theme::Address]              = readFormat(settings, QStringLiteral("address"), baseTheme.text[Theme::Address]);
	theme.text[Theme::AlternatingByte]      = readFormat(settings, QStringLiteral("alternating_byte"), baseTheme.text[Theme::AlternatingByte]);
	theme.text[Theme::Arithmetic]           = readFormat(settings, QStringLiteral("arithmetic"), baseTheme.text[Theme::Arithmetic]);
	theme.text[Theme::Brackets]             = readFormat(settings, QStringLiteral("brackets"), baseTheme.text[Theme::Brackets]);
	theme.text[Theme::Comma]                = readFormat(settings, QStringLiteral("comma"), baseTheme.text[Theme::Comma]);
	theme.text[Theme::Comparison]           = readFormat(settings, QStringLiteral("comparison"), baseTheme.text[Theme::Comparison]);
	theme.text[Theme::Constant]             = readFormat(settings, QStringLiteral("constant"), baseTheme.text[Theme::Constant]);
	theme.text[Theme::DataXfer]             = readFormat(settings, QStringLiteral("data_xfer"), baseTheme.text[Theme::DataXfer]);
	theme.text[Theme::Data]                 = readFormat(settings, QStringLiteral("data"), baseTheme.text[Theme::Data]);
	theme.text[Theme::Filling]              = readFormat(settings, QStringLiteral("filling"), baseTheme.text[Theme::Filling]);
	theme.text[Theme::FlowCtrl]             = readFormat(settings, QStringLiteral("flow_ctrl"), baseTheme.text[Theme::FlowCtrl]);
	theme.text[Theme::Function]             = readFormat(settings, QStringLiteral("function"), baseTheme.text[Theme::Function]);
	theme.text[Theme::Logic]                = readFormat(settings, QStringLiteral("logic"), baseTheme.text[Theme::Logic]);
	theme.text[Theme::NonPrintingCharacter] = readFormat(settings, QStringLiteral("non_printing_character"), baseTheme.text[Theme::NonPrintingCharacter]);
	theme.text[Theme::Operator]             = readFormat(settings, QStringLiteral("operator"), baseTheme.text[Theme::Operator]);
	theme.text[Theme::Prefix]               = readFormat(settings, QStringLiteral("prefix"), baseTheme.text[Theme::Prefix]);
	theme.text[Theme::Ptr]                  = readFormat(settings, QStringLiteral("ptr"), baseTheme.text[Theme::Ptr]);
	theme.text[Theme::Register]             = readFormat(settings, QStringLiteral("register"), baseTheme.text[Theme::Register]);
	theme.text[Theme::Shift]                = readFormat(settings, QStringLiteral("shift"), baseTheme.text[Theme::Shift]);
	theme.text[Theme::Stack]                = readFormat(settings, QStringLiteral("stack"), baseTheme.text[Theme::Stack]);
	theme.text[Theme::System]               = readFormat(settings, QStringLiteral("system"), baseTheme.text[Theme::System]);
	theme.text[Theme::TakenJump]            = readFormat(settings, QStringLiteral("taken_jump"), baseTheme.text[Theme::TakenJump]);

	// misc settings
	theme.misc[Theme::Badge] = readFormat(settings, QStringLiteral("badge"), baseTheme.misc[Theme::Badge]);

	settings.endGroup();

	return theme;
}

/**
 * @brief Reads the system theme based on the current application palette.
 *
 * @return The system theme.
 */
Theme readSystemTheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	if (QApplication::palette().window().color().lightnessF() >= 0.5f) {
		QSettings settings(QStringLiteral(":/themes/system-light.ini"), QSettings::IniFormat);
		return readTheme(settings);
	}
#else
	if (QApplication::palette().window().color().lightnessF() >= 0.5) {
		QSettings settings(QStringLiteral(":/themes/system-light.ini"), QSettings::IniFormat);
		return readTheme(settings);
	}
#endif

	QSettings settings(QStringLiteral(":/themes/system-dark.ini"), QSettings::IniFormat);
	return readTheme(settings);
}

/**
 * @brief Reads the theme from the settings.
 *
 * @return The theme read from the settings.
 */
Theme readTheme() {

	// Ensure that the directory exists, this will help users know where to put the theme files
	QDir().mkpath(themeDirectory());

	Theme system = readSystemTheme();

	QString theme_name = edb::v1::config().theme_name;

	// Handle the built-in themes
	if (theme_name == QStringLiteral("System")) {
		return system;
	}

	if (theme_name == QStringLiteral("Dark [Built-in]")) {
		QSettings settings(QStringLiteral(":/themes/dark.ini"), QSettings::IniFormat);
		return readTheme(settings, system);
	}

	if (theme_name == QStringLiteral("Light [Built-in]")) {
		QSettings settings(QStringLiteral(":/themes/light.ini"), QSettings::IniFormat);
		return readTheme(settings, system);
	}

	QString themeFile = themeDirectory() + QDir::separator() + theme_name;
	QSettings settings(themeFile, QSettings::IniFormat);
	return readTheme(settings, system);
}

}

/**
 * @brief Loads the theme from the settings.
 *
 * @return The loaded theme.
 */
Theme Theme::load() {

	// we do this function indirection to make it so the theme is
	// read once even if this function is called several times
	static Theme theme = readTheme();
	return theme;
}

/**
 * @brief Returns a list of all user-defined themes.
 *
 * @return A list of all user-defined themes.
 */
QStringList Theme::userThemes() {
	QDir directory(themeDirectory());
	return directory.entryList(QStringList() << QStringLiteral("*.ini"), QDir::Files);
}

/**
 * @brief Returns the name of a theme file.
 *
 * @param theme_file The theme file to get the name of.
 * @return The name of the theme.
 */
QString Theme::themeName(const QString &theme_file) {
	QString themeFile = themeDirectory() + QDir::separator() + theme_file;
	QSettings settings(themeFile, QSettings::IniFormat);

	settings.beginGroup(QStringLiteral("Meta"));
	QString name = settings.value(QStringLiteral("name"), theme_file).toString();
	settings.endGroup();
	return name;
}
