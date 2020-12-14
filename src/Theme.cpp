/*
Copyright (C) 2020 - 2020 Evan Teran
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
 * @brief Theme::userThemes
 * @return
 */
QString themeDirectory() {
	static const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	return QString("%1/%2/%3").arg(configDir, QApplication::organizationName(), QLatin1String("themes"));
}


/**
 * @brief readColor
 * @param settings
 * @param name
 * @param defaultValue
 * @return
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
 * @brief readFormat
 * @param settings
 * @param name
 * @param defaultValue
 * @return
 */
QTextCharFormat readFormat(QSettings &settings, const QString &name, const QTextCharFormat &defaultValue = QTextCharFormat()) {

	QTextCharFormat format;
	format.setForeground(readColor(settings, QString("%1.foreground").arg(name), defaultValue.foreground().color()));
	format.setBackground(readColor(settings, QString("%1.background").arg(name), defaultValue.background().color()));
	format.setFontWeight(settings.value(QString("%1.weight").arg(name), defaultValue.fontWeight()).toInt());
	format.setFontItalic(settings.value(QString("%1.italic").arg(name), defaultValue.fontItalic()).toBool());
	format.setFontUnderline(settings.value(QString("%1.underline").arg(name), defaultValue.fontUnderline()).toBool());
	return format;
}

// we do this immediately invoked lambda being assigned to a static stuff
// to make it so the theme is read once even if this function is called several times
Theme readTheme(QSettings &settings, const Theme &baseTheme = Theme()) {
	Theme theme;

	settings.beginGroup("Theme");
	// General application palette
	theme.palette[Theme::Window]                  = readColor(settings, "palette.window", baseTheme.palette[Theme::Window]);
	theme.palette[Theme::WindowDisabled]          = readColor(settings, "palette.window.disabled", baseTheme.palette[Theme::WindowDisabled]);
	theme.palette[Theme::WindowText]              = readColor(settings, "palette.windowtext", baseTheme.palette[Theme::WindowText]);
	theme.palette[Theme::WindowTextDisabled]      = readColor(settings, "palette.windowtext.disabled", baseTheme.palette[Theme::WindowTextDisabled]);
	theme.palette[Theme::Base]                    = readColor(settings, "palette.base", baseTheme.palette[Theme::Base]);
	theme.palette[Theme::BaseDisabled]            = readColor(settings, "palette.base.disabled", baseTheme.palette[Theme::BaseDisabled]);
	theme.palette[Theme::AlternateBase]           = readColor(settings, "palette.alternatebase", baseTheme.palette[Theme::AlternateBase]);
	theme.palette[Theme::AlternateBaseDisabled]   = readColor(settings, "palette.alternatebase.disabled", baseTheme.palette[Theme::AlternateBaseDisabled]);
	theme.palette[Theme::ToolTipBase]             = readColor(settings, "palette.tooltipbase", baseTheme.palette[Theme::ToolTipBase]);
	theme.palette[Theme::ToolTipBaseDisabled]     = readColor(settings, "palette.tooltipbase.disabled", baseTheme.palette[Theme::ToolTipBaseDisabled]);
	theme.palette[Theme::ToolTipText]             = readColor(settings, "palette.tooltiptext", baseTheme.palette[Theme::ToolTipText]);
	theme.palette[Theme::ToolTipTextDisabled]     = readColor(settings, "palette.tooltiptext.disabled", baseTheme.palette[Theme::ToolTipTextDisabled]);
	theme.palette[Theme::Text]                    = readColor(settings, "palette.text", baseTheme.palette[Theme::Text]);
	theme.palette[Theme::TextDisabled]            = readColor(settings, "palette.text.disabled", baseTheme.palette[Theme::TextDisabled]);
	theme.palette[Theme::Button]                  = readColor(settings, "palette.button", baseTheme.palette[Theme::Button]);
	theme.palette[Theme::ButtonDisabled]          = readColor(settings, "palette.button.disabled", baseTheme.palette[Theme::ButtonDisabled]);
	theme.palette[Theme::ButtonText]              = readColor(settings, "palette.buttontext", baseTheme.palette[Theme::ButtonText]);
	theme.palette[Theme::ButtonTextDisabled]      = readColor(settings, "palette.buttontext.disabled", baseTheme.palette[Theme::ButtonTextDisabled]);
	theme.palette[Theme::BrightText]              = readColor(settings, "palette.brighttext", baseTheme.palette[Theme::BrightText]);
	theme.palette[Theme::BrightTextDisabled]      = readColor(settings, "palette.brighttext.disabled", baseTheme.palette[Theme::BrightTextDisabled]);
	theme.palette[Theme::Highlight]               = readColor(settings, "palette.highlight", baseTheme.palette[Theme::Highlight]);
	theme.palette[Theme::HighlightDisabled]       = readColor(settings, "palette.highlight.disabled", baseTheme.palette[Theme::HighlightDisabled]);
	theme.palette[Theme::HighlightedText]         = readColor(settings, "palette.highlightedtext", baseTheme.palette[Theme::HighlightedText]);
	theme.palette[Theme::HighlightedTextDisabled] = readColor(settings, "palette.highlightedtext.disabled", baseTheme.palette[Theme::HighlightedTextDisabled]);
	theme.palette[Theme::Link]                    = readColor(settings, "palette.link", baseTheme.palette[Theme::Link]);
	theme.palette[Theme::LinkDisabled]            = readColor(settings, "palette.link.disabled", baseTheme.palette[Theme::LinkDisabled]);
	theme.palette[Theme::LinkVisited]             = readColor(settings, "palette.linkvisited", baseTheme.palette[Theme::LinkVisited]);
	theme.palette[Theme::LinkVisitedDisabled]     = readColor(settings, "palette.linkvisited.disabled", baseTheme.palette[Theme::LinkVisitedDisabled]);
	theme.palette[Theme::Light]                   = readColor(settings, "palette.light", baseTheme.palette[Theme::Light]);
	theme.palette[Theme::LightDisabled]           = readColor(settings, "palette.light.disabled", baseTheme.palette[Theme::LightDisabled]);
	theme.palette[Theme::Midlight]                = readColor(settings, "palette.midlight", baseTheme.palette[Theme::Midlight]);
	theme.palette[Theme::MidlightDisabled]        = readColor(settings, "palette.midlight.disabled", baseTheme.palette[Theme::MidlightDisabled]);
	theme.palette[Theme::Dark]                    = readColor(settings, "palette.dark", baseTheme.palette[Theme::Dark]);
	theme.palette[Theme::DarkDisabled]            = readColor(settings, "palette.dark.disabled", baseTheme.palette[Theme::DarkDisabled]);
	theme.palette[Theme::Mid]                     = readColor(settings, "palette.mid", baseTheme.palette[Theme::Mid]);
	theme.palette[Theme::MidDisabled]             = readColor(settings, "palette.mid.disabled", baseTheme.palette[Theme::MidDisabled]);
	theme.palette[Theme::Shadow]                  = readColor(settings, "palette.shadow", baseTheme.palette[Theme::Shadow]);
	theme.palette[Theme::ShadowDisabled]          = readColor(settings, "palette.shadow.disabled", baseTheme.palette[Theme::ShadowDisabled]);

	// various text/syntax settings
	theme.text[Theme::Address]              = readFormat(settings, "address", baseTheme.text[Theme::Address]);
	theme.text[Theme::AlternatingByte]      = readFormat(settings, "alternating_byte", baseTheme.text[Theme::AlternatingByte]);
	theme.text[Theme::Arithmetic]           = readFormat(settings, "arithmetic", baseTheme.text[Theme::Arithmetic]);
	theme.text[Theme::Brackets]             = readFormat(settings, "brackets", baseTheme.text[Theme::Brackets]);
	theme.text[Theme::Comma]                = readFormat(settings, "comma", baseTheme.text[Theme::Comma]);
	theme.text[Theme::Comparison]           = readFormat(settings, "comparison", baseTheme.text[Theme::Comparison]);
	theme.text[Theme::Constant]             = readFormat(settings, "constant", baseTheme.text[Theme::Constant]);
	theme.text[Theme::DataXfer]             = readFormat(settings, "data_xfer", baseTheme.text[Theme::DataXfer]);
	theme.text[Theme::Data]                 = readFormat(settings, "data", baseTheme.text[Theme::Data]);
	theme.text[Theme::Filling]              = readFormat(settings, "filling", baseTheme.text[Theme::Filling]);
	theme.text[Theme::FlowCtrl]             = readFormat(settings, "flow_ctrl", baseTheme.text[Theme::FlowCtrl]);
	theme.text[Theme::Function]             = readFormat(settings, "function", baseTheme.text[Theme::Function]);
	theme.text[Theme::Logic]                = readFormat(settings, "logic", baseTheme.text[Theme::Logic]);
	theme.text[Theme::NonPrintingCharacter] = readFormat(settings, "non_printing_character", baseTheme.text[Theme::NonPrintingCharacter]);
	theme.text[Theme::Operator]             = readFormat(settings, "operator", baseTheme.text[Theme::Operator]);
	theme.text[Theme::Prefix]               = readFormat(settings, "prefix", baseTheme.text[Theme::Prefix]);
	theme.text[Theme::Ptr]                  = readFormat(settings, "ptr", baseTheme.text[Theme::Ptr]);
	theme.text[Theme::Register]             = readFormat(settings, "register", baseTheme.text[Theme::Register]);
	theme.text[Theme::Shift]                = readFormat(settings, "shift", baseTheme.text[Theme::Shift]);
	theme.text[Theme::Stack]                = readFormat(settings, "stack", baseTheme.text[Theme::Stack]);
	theme.text[Theme::System]               = readFormat(settings, "system", baseTheme.text[Theme::System]);
	theme.text[Theme::TakenJump]            = readFormat(settings, "taken_jump", baseTheme.text[Theme::TakenJump]);

	// misc settings
	theme.misc[Theme::Badge] = readFormat(settings, "badge", baseTheme.misc[Theme::Badge]);

	settings.endGroup();

	return theme;
}

/**
 * @brief readSystemTheme
 * @return
 */
Theme readSystemTheme() {
	if (QApplication::palette().window().color().lightnessF() >= 0.5) {
		QSettings settings(":/themes/system-light.ini", QSettings::IniFormat);
		return readTheme(settings);
	} else {
		QSettings settings(":/themes/system-dark.ini", QSettings::IniFormat);
		return readTheme(settings);
	}
}

/**
 * @brief readTheme
 * @return
 */
Theme readTheme() {

	// Ensure that the directory exists, this will help users know where to put the theme files
	QDir().mkpath(themeDirectory());

	Theme system = readSystemTheme();

	QString theme_name = edb::v1::config().theme_name;

	// Handle the built-in themese
	if(theme_name == "System") {
		return system;
	} else if(theme_name == "Dark [Built-in]") {
		QSettings settings(":/themes/dark.ini", QSettings::IniFormat);
		return readTheme(settings, system);
	} else if (theme_name == "Light [Built-in]") {
		QSettings settings(":/themes/light.ini", QSettings::IniFormat);
		return readTheme(settings, system);
	}

	QString themeFile = themeDirectory() + QDir::separator() + theme_name;
	QSettings settings(themeFile, QSettings::IniFormat);
	return readTheme(settings, system);
}

}

/**
 * @brief Theme::load
 * @return
 */
Theme Theme::load() {

	// we do this function indirection to make it so the theme is
	// read once even if this function is called several times
	static Theme theme = readTheme();
	return theme;
}


/**
 * @brief Theme::userThemes
 * @return
 */
QStringList Theme::userThemes() {
	QDir directory(themeDirectory());
	return directory.entryList(QStringList() << "*.ini", QDir::Files);
}

/**
 * @brief Theme::themeName
 * @param theme_file
 * @return
 */
QString Theme::themeName(const QString &theme_file) {
	QString themeFile = themeDirectory() + QDir::separator() + theme_file;
	QSettings settings(themeFile, QSettings::IniFormat);

	settings.beginGroup("Meta");
	QString name = settings.value("name", theme_file).toString();
	settings.endGroup();
	return name;

}
