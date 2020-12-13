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
	theme.palette[Theme::Window]                  = readColor(settings, "theme.palette.window", baseTheme.palette[Theme::Window]);
	theme.palette[Theme::WindowDisabled]          = readColor(settings, "theme.palette.window.disabled", baseTheme.palette[Theme::WindowDisabled]);
	theme.palette[Theme::WindowText]              = readColor(settings, "theme.palette.windowtext", baseTheme.palette[Theme::WindowText]);
	theme.palette[Theme::WindowTextDisabled]      = readColor(settings, "theme.palette.windowtext.disabled", baseTheme.palette[Theme::WindowTextDisabled]);
	theme.palette[Theme::Base]                    = readColor(settings, "theme.palette.base", baseTheme.palette[Theme::Base]);
	theme.palette[Theme::BaseDisabled]            = readColor(settings, "theme.palette.base.disabled", baseTheme.palette[Theme::BaseDisabled]);
	theme.palette[Theme::AlternateBase]           = readColor(settings, "theme.palette.alternatebase", baseTheme.palette[Theme::AlternateBase]);
	theme.palette[Theme::AlternateBaseDisabled]   = readColor(settings, "theme.palette.alternatebase.disabled", baseTheme.palette[Theme::AlternateBaseDisabled]);
	theme.palette[Theme::ToolTipBase]             = readColor(settings, "theme.palette.tooltipbase", baseTheme.palette[Theme::ToolTipBase]);
	theme.palette[Theme::ToolTipBaseDisabled]     = readColor(settings, "theme.palette.tooltipbase.disabled", baseTheme.palette[Theme::ToolTipBaseDisabled]);
	theme.palette[Theme::ToolTipText]             = readColor(settings, "theme.palette.tooltiptext", baseTheme.palette[Theme::ToolTipText]);
	theme.palette[Theme::ToolTipTextDisabled]     = readColor(settings, "theme.palette.tooltiptext.disabled", baseTheme.palette[Theme::ToolTipTextDisabled]);
	theme.palette[Theme::Text]                    = readColor(settings, "theme.palette.text", baseTheme.palette[Theme::Text]);
	theme.palette[Theme::TextDisabled]            = readColor(settings, "theme.palette.text.disabled", baseTheme.palette[Theme::TextDisabled]);
	theme.palette[Theme::Button]                  = readColor(settings, "theme.palette.button", baseTheme.palette[Theme::Button]);
	theme.palette[Theme::ButtonDisabled]          = readColor(settings, "theme.palette.button.disabled", baseTheme.palette[Theme::ButtonDisabled]);
	theme.palette[Theme::ButtonText]              = readColor(settings, "theme.palette.buttontext", baseTheme.palette[Theme::ButtonText]);
	theme.palette[Theme::ButtonTextDisabled]      = readColor(settings, "theme.palette.buttontext.disabled", baseTheme.palette[Theme::ButtonTextDisabled]);
	theme.palette[Theme::BrightText]              = readColor(settings, "theme.palette.brighttext", baseTheme.palette[Theme::BrightText]);
	theme.palette[Theme::BrightTextDisabled]      = readColor(settings, "theme.palette.brighttext.disabled", baseTheme.palette[Theme::BrightTextDisabled]);
	theme.palette[Theme::Highlight]               = readColor(settings, "theme.palette.highlight", baseTheme.palette[Theme::Highlight]);
	theme.palette[Theme::HighlightDisabled]       = readColor(settings, "theme.palette.highlight.disabled", baseTheme.palette[Theme::HighlightDisabled]);
	theme.palette[Theme::HighlightedText]         = readColor(settings, "theme.palette.highlightedtext", baseTheme.palette[Theme::HighlightedText]);
	theme.palette[Theme::HighlightedTextDisabled] = readColor(settings, "theme.palette.highlightedtext.disabled", baseTheme.palette[Theme::HighlightedTextDisabled]);
	theme.palette[Theme::Link]                    = readColor(settings, "theme.palette.link", baseTheme.palette[Theme::Link]);
	theme.palette[Theme::LinkDisabled]            = readColor(settings, "theme.palette.link.disabled", baseTheme.palette[Theme::LinkDisabled]);
	theme.palette[Theme::LinkVisited]             = readColor(settings, "theme.palette.linkvisited", baseTheme.palette[Theme::LinkVisited]);
	theme.palette[Theme::LinkVisitedDisabled]     = readColor(settings, "theme.palette.linkvisited.disabled", baseTheme.palette[Theme::LinkVisitedDisabled]);
	theme.palette[Theme::Light]                   = readColor(settings, "theme.palette.light", baseTheme.palette[Theme::Light]);
	theme.palette[Theme::LightDisabled]           = readColor(settings, "theme.palette.light.disabled", baseTheme.palette[Theme::LightDisabled]);
	theme.palette[Theme::Midlight]                = readColor(settings, "theme.palette.midlight", baseTheme.palette[Theme::Midlight]);
	theme.palette[Theme::MidlightDisabled]        = readColor(settings, "theme.palette.midlight.disabled", baseTheme.palette[Theme::MidlightDisabled]);
	theme.palette[Theme::Dark]                    = readColor(settings, "theme.palette.dark", baseTheme.palette[Theme::Dark]);
	theme.palette[Theme::DarkDisabled]            = readColor(settings, "theme.palette.dark.disabled", baseTheme.palette[Theme::DarkDisabled]);
	theme.palette[Theme::Mid]                     = readColor(settings, "theme.palette.mid", baseTheme.palette[Theme::Mid]);
	theme.palette[Theme::MidDisabled]             = readColor(settings, "theme.palette.mid.disabled", baseTheme.palette[Theme::MidDisabled]);
	theme.palette[Theme::Shadow]                  = readColor(settings, "theme.palette.shadow", baseTheme.palette[Theme::Shadow]);
	theme.palette[Theme::ShadowDisabled]          = readColor(settings, "theme.palette.shadow.disabled", baseTheme.palette[Theme::ShadowDisabled]);

	// various text/syntax settings
	theme.text[Theme::Address]              = readFormat(settings, "theme.address", baseTheme.text[Theme::Address]);
	theme.text[Theme::AlternatingByte]      = readFormat(settings, "theme.alternating_byte", baseTheme.text[Theme::AlternatingByte]);
	theme.text[Theme::Arithmetic]           = readFormat(settings, "theme.arithmetic", baseTheme.text[Theme::Arithmetic]);
	theme.text[Theme::Brackets]             = readFormat(settings, "theme.brackets", baseTheme.text[Theme::Brackets]);
	theme.text[Theme::Comma]                = readFormat(settings, "theme.comma", baseTheme.text[Theme::Comma]);
	theme.text[Theme::Comparison]           = readFormat(settings, "theme.comparison", baseTheme.text[Theme::Comparison]);
	theme.text[Theme::Constant]             = readFormat(settings, "theme.constant", baseTheme.text[Theme::Constant]);
	theme.text[Theme::DataXfer]             = readFormat(settings, "theme.data_xfer", baseTheme.text[Theme::DataXfer]);
	theme.text[Theme::Data]                 = readFormat(settings, "theme.data", baseTheme.text[Theme::Data]);
	theme.text[Theme::Filling]              = readFormat(settings, "theme.filling", baseTheme.text[Theme::Filling]);
	theme.text[Theme::FlowCtrl]             = readFormat(settings, "theme.flow_ctrl", baseTheme.text[Theme::FlowCtrl]);
	theme.text[Theme::Function]             = readFormat(settings, "theme.function", baseTheme.text[Theme::Function]);
	theme.text[Theme::Logic]                = readFormat(settings, "theme.logic", baseTheme.text[Theme::Logic]);
	theme.text[Theme::NonPrintingCharacter] = readFormat(settings, "theme.non_printing_character", baseTheme.text[Theme::NonPrintingCharacter]);
	theme.text[Theme::Operator]             = readFormat(settings, "theme.operator", baseTheme.text[Theme::Operator]);
	theme.text[Theme::Prefix]               = readFormat(settings, "theme.prefix", baseTheme.text[Theme::Prefix]);
	theme.text[Theme::Ptr]                  = readFormat(settings, "theme.ptr", baseTheme.text[Theme::Ptr]);
	theme.text[Theme::Register]             = readFormat(settings, "theme.register", baseTheme.text[Theme::Register]);
	theme.text[Theme::Shift]                = readFormat(settings, "theme.shift", baseTheme.text[Theme::Shift]);
	theme.text[Theme::Stack]                = readFormat(settings, "theme.stack", baseTheme.text[Theme::Stack]);
	theme.text[Theme::System]               = readFormat(settings, "theme.system", baseTheme.text[Theme::System]);
	theme.text[Theme::TakenJump]            = readFormat(settings, "theme.taken_jump", baseTheme.text[Theme::TakenJump]);

	// misc settings
	theme.misc[Theme::Badge] = readFormat(settings, "theme.badge", baseTheme.misc[Theme::Badge]);

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

QString Theme::themeName(const QString &theme_file) {
	QString themeFile = themeDirectory() + QDir::separator() + theme_file;
	QSettings settings(themeFile, QSettings::IniFormat);

	settings.beginGroup("Meta");
	QString name = settings.value("name", theme_file).toString();
	settings.endGroup();
	return name;

}
