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
#include <QApplication>
#include <QPalette>
#include <QSettings>

namespace {

QColor readColor(QSettings &settings, const QString &name, const QColor &defaultValue = QColor()) {

	QVariant variant = settings.value(name);

	auto color = variant.value<QColor>();
	if (color.isValid()) {
		return color;
	}

	return defaultValue;
}

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
Theme readTheme(QSettings &settings) {
	Theme theme;

	settings.beginGroup("Theme");
	// General application palette
	theme.palette[Theme::Window]                  = readColor(settings, "theme.palette.window");
	theme.palette[Theme::WindowDisabled]          = readColor(settings, "theme.palette.window.disabled");
	theme.palette[Theme::WindowText]              = readColor(settings, "theme.palette.windowtext");
	theme.palette[Theme::WindowTextDisabled]      = readColor(settings, "theme.palette.windowtext.disabled");
	theme.palette[Theme::Base]                    = readColor(settings, "theme.palette.base");
	theme.palette[Theme::BaseDisabled]            = readColor(settings, "theme.palette.base.disabled");
	theme.palette[Theme::AlternateBase]           = readColor(settings, "theme.palette.alternatebase");
	theme.palette[Theme::AlternateBaseDisabled]   = readColor(settings, "theme.palette.alternatebase.disabled");
	theme.palette[Theme::ToolTipBase]             = readColor(settings, "theme.palette.tooltipbase");
	theme.palette[Theme::ToolTipBaseDisabled]     = readColor(settings, "theme.palette.tooltipbase.disabled");
	theme.palette[Theme::ToolTipText]             = readColor(settings, "theme.palette.tooltiptext");
	theme.palette[Theme::ToolTipTextDisabled]     = readColor(settings, "theme.palette.tooltiptext.disabled");
	theme.palette[Theme::Text]                    = readColor(settings, "theme.palette.text");
	theme.palette[Theme::TextDisabled]            = readColor(settings, "theme.palette.text.disabled");
	theme.palette[Theme::Button]                  = readColor(settings, "theme.palette.button");
	theme.palette[Theme::ButtonDisabled]          = readColor(settings, "theme.palette.button.disabled");
	theme.palette[Theme::ButtonText]              = readColor(settings, "theme.palette.buttontext");
	theme.palette[Theme::ButtonTextDisabled]      = readColor(settings, "theme.palette.buttontext.disabled");
	theme.palette[Theme::BrightText]              = readColor(settings, "theme.palette.brighttext");
	theme.palette[Theme::BrightTextDisabled]      = readColor(settings, "theme.palette.brighttext.disabled");
	theme.palette[Theme::Highlight]               = readColor(settings, "theme.palette.highlight");
	theme.palette[Theme::HighlightDisabled]       = readColor(settings, "theme.palette.highlight.disabled");
	theme.palette[Theme::HighlightedText]         = readColor(settings, "theme.palette.highlightedtext");
	theme.palette[Theme::HighlightedTextDisabled] = readColor(settings, "theme.palette.highlightedtext.disabled");
	theme.palette[Theme::Link]                    = readColor(settings, "theme.palette.link");
	theme.palette[Theme::LinkDisabled]            = readColor(settings, "theme.palette.link.disabled");
	theme.palette[Theme::LinkVisited]             = readColor(settings, "theme.palette.linkvisited");
	theme.palette[Theme::LinkVisitedDisabled]     = readColor(settings, "theme.palette.linkvisited.disabled");
	theme.palette[Theme::Light]                   = readColor(settings, "theme.palette.light");
	theme.palette[Theme::LightDisabled]           = readColor(settings, "theme.palette.light.disabled");
	theme.palette[Theme::Midlight]                = readColor(settings, "theme.palette.midlight");
	theme.palette[Theme::MidlightDisabled]        = readColor(settings, "theme.palette.midlight.disabled");
	theme.palette[Theme::Dark]                    = readColor(settings, "theme.palette.dark");
	theme.palette[Theme::DarkDisabled]            = readColor(settings, "theme.palette.dark.disabled");
	theme.palette[Theme::Mid]                     = readColor(settings, "theme.palette.mid");
	theme.palette[Theme::MidDisabled]             = readColor(settings, "theme.palette.mid.disabled");
	theme.palette[Theme::Shadow]                  = readColor(settings, "theme.palette.shadow");
	theme.palette[Theme::ShadowDisabled]          = readColor(settings, "theme.palette.shadow.disabled");

	// various text/syntax settings
	theme.text[Theme::Address] = readFormat(settings, "theme.address");

	theme.text[Theme::AlternatingByte]  	= readFormat(settings, "theme.alternating_byte");
	theme.text[Theme::Arithmetic]			= readFormat(settings, "theme.arithmetic");
	theme.text[Theme::Brackets] 			= readFormat(settings, "theme.brackets");
	theme.text[Theme::Comma]				= readFormat(settings, "theme.comma");
	theme.text[Theme::Comparison]			= readFormat(settings, "theme.comparison");
	theme.text[Theme::Constant] 			= readFormat(settings, "theme.constant");
	theme.text[Theme::DataXfer] 			= readFormat(settings, "theme.data_xfer");
	theme.text[Theme::Data] 				= readFormat(settings, "theme.data");
	theme.text[Theme::Filling]  			= readFormat(settings, "theme.filling");
	theme.text[Theme::FlowCtrl] 			= readFormat(settings, "theme.flow_ctrl");
	theme.text[Theme::Function] 			= readFormat(settings, "theme.function");
	theme.text[Theme::Logic]				= readFormat(settings, "theme.logic");
	theme.text[Theme::NonPrintingCharacter] = readFormat(settings, "theme.non_printing_character");
	theme.text[Theme::Operator] 			= readFormat(settings, "theme.operator");
	theme.text[Theme::Prefix]				= readFormat(settings, "theme.prefix");
	theme.text[Theme::Ptr]  				= readFormat(settings, "theme.ptr");
	theme.text[Theme::Register] 			= readFormat(settings, "theme.register");
	theme.text[Theme::Shift]				= readFormat(settings, "theme.shift");
	theme.text[Theme::Stack]				= readFormat(settings, "theme.stack");
	theme.text[Theme::System]				= readFormat(settings, "theme.system");
	theme.text[Theme::TakenJump]			= readFormat(settings, "theme.taken_jump");

	// misc settings
	theme.misc[Theme::Badge] = readFormat(settings, "theme.badge");

	settings.endGroup();

	return theme;
}

Theme readSystemTheme() {
	if (QApplication::palette().window().color().lightnessF() >= 0.5) {
		QSettings settings(":/themes/system-light.ini", QSettings::IniFormat);
		return readTheme(settings);
	} else {
		QSettings settings(":/themes/system-dark.ini", QSettings::IniFormat);
		return readTheme(settings);
	}
}

Theme readTheme() {

	Theme system = readSystemTheme();
	// TODO(eteran): combine with what the user has chosen...

	return system;
}

}

Theme Theme::load() {

	// we do this function indirection to make it so the theme is
	// read once even if this function is called several times
	static Theme theme = readTheme();
	return theme;
}
