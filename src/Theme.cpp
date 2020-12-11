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
	QVariant value = settings.value(name).toString();
	if (value.isValid()) {
		return QColor(value.toString());
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
	theme.palette[Theme::Palette::Window]                  = readColor(settings, "theme.palette.window");
	theme.palette[Theme::Palette::WindowDisabled]          = readColor(settings, "theme.palette.window.disabled");
	theme.palette[Theme::Palette::WindowText]              = readColor(settings, "theme.palette.windowtext");
	theme.palette[Theme::Palette::WindowTextDisabled]      = readColor(settings, "theme.palette.windowtext.disabled");
	theme.palette[Theme::Palette::Base]                    = readColor(settings, "theme.palette.base");
	theme.palette[Theme::Palette::BaseDisabled]            = readColor(settings, "theme.palette.base.disabled");
	theme.palette[Theme::Palette::AlternateBase]           = readColor(settings, "theme.palette.alternatebase");
	theme.palette[Theme::Palette::AlternateBaseDisabled]   = readColor(settings, "theme.palette.alternatebase.disabled");
	theme.palette[Theme::Palette::ToolTipBase]             = readColor(settings, "theme.palette.tooltipbase");
	theme.palette[Theme::Palette::ToolTipBaseDisabled]     = readColor(settings, "theme.palette.tooltipbase.disabled");
	theme.palette[Theme::Palette::ToolTipText]             = readColor(settings, "theme.palette.tooltiptext");
	theme.palette[Theme::Palette::ToolTipTextDisabled]     = readColor(settings, "theme.palette.tooltiptext.disabled");
	theme.palette[Theme::Palette::Text]                    = readColor(settings, "theme.palette.text");
	theme.palette[Theme::Palette::TextDisabled]            = readColor(settings, "theme.palette.text.disabled");
	theme.palette[Theme::Palette::Button]                  = readColor(settings, "theme.palette.button");
	theme.palette[Theme::Palette::ButtonDisabled]          = readColor(settings, "theme.palette.button.disabled");
	theme.palette[Theme::Palette::ButtonText]              = readColor(settings, "theme.palette.buttontext");
	theme.palette[Theme::Palette::ButtonTextDisabled]      = readColor(settings, "theme.palette.buttontext.disabled");
	theme.palette[Theme::Palette::BrightText]              = readColor(settings, "theme.palette.brighttext");
	theme.palette[Theme::Palette::BrightTextDisabled]      = readColor(settings, "theme.palette.brighttext.disabled");
	theme.palette[Theme::Palette::Highlight]               = readColor(settings, "theme.palette.highlight");
	theme.palette[Theme::Palette::HighlightDisabled]       = readColor(settings, "theme.palette.highlight.disabled");
	theme.palette[Theme::Palette::HighlightedText]         = readColor(settings, "theme.palette.highlightedtext");
	theme.palette[Theme::Palette::HighlightedTextDisabled] = readColor(settings, "theme.palette.highlightedtext.disabled");
	theme.palette[Theme::Palette::Link]                    = readColor(settings, "theme.palette.link");
	theme.palette[Theme::Palette::LinkDisabled]            = readColor(settings, "theme.palette.link.disabled");
	theme.palette[Theme::Palette::LinkVisited]             = readColor(settings, "theme.palette.linkvisited");
	theme.palette[Theme::Palette::LinkVisitedDisabled]     = readColor(settings, "theme.palette.linkvisited.disabled");
	theme.palette[Theme::Palette::Light]                   = readColor(settings, "theme.palette.light");
	theme.palette[Theme::Palette::LightDisabled]           = readColor(settings, "theme.palette.light.disabled");
	theme.palette[Theme::Palette::Midlight]                = readColor(settings, "theme.palette.midlight");
	theme.palette[Theme::Palette::MidlightDisabled]        = readColor(settings, "theme.palette.midlight.disabled");
	theme.palette[Theme::Palette::Dark]                    = readColor(settings, "theme.palette.dark");
	theme.palette[Theme::Palette::DarkDisabled]            = readColor(settings, "theme.palette.dark.disabled");
	theme.palette[Theme::Palette::Mid]                     = readColor(settings, "theme.palette.mid");
	theme.palette[Theme::Palette::MidDisabled]             = readColor(settings, "theme.palette.mid.disabled");
	theme.palette[Theme::Palette::Shadow]                  = readColor(settings, "theme.palette.shadow");
	theme.palette[Theme::Palette::ShadowDisabled]          = readColor(settings, "theme.palette.shadow.disabled");

	// various text/syntax settings
	theme.text[Theme::Text::Address] = readFormat(settings, "theme.address");

	theme.text[Theme::Text::AlternatingByte]      = readFormat(settings, "theme.alternating_byte");
	theme.text[Theme::Text::Arithmetic]           = readFormat(settings, "theme.arithmetic");
	theme.text[Theme::Text::Brackets]             = readFormat(settings, "theme.brackets");
	theme.text[Theme::Text::Comma]                = readFormat(settings, "theme.comma");
	theme.text[Theme::Text::Comparison]           = readFormat(settings, "theme.comparison");
	theme.text[Theme::Text::Constant]             = readFormat(settings, "theme.constant");
	theme.text[Theme::Text::DataXfer]             = readFormat(settings, "theme.data_xfer");
	theme.text[Theme::Text::Data]                 = readFormat(settings, "theme.data");
	theme.text[Theme::Text::Filling]              = readFormat(settings, "theme.filling");
	theme.text[Theme::Text::FlowCtrl]             = readFormat(settings, "theme.flow_ctrl");
	theme.text[Theme::Text::Function]             = readFormat(settings, "theme.function");
	theme.text[Theme::Text::Logic]                = readFormat(settings, "theme.logic");
	theme.text[Theme::Text::NonPrintingCharacter] = readFormat(settings, "theme.non_printing_character");
	theme.text[Theme::Text::Operator]             = readFormat(settings, "theme.operator");
	theme.text[Theme::Text::Prefix]               = readFormat(settings, "theme.prefix");
	theme.text[Theme::Text::Ptr]                  = readFormat(settings, "theme.ptr");
	theme.text[Theme::Text::Register]             = readFormat(settings, "theme.register");
	theme.text[Theme::Text::Shift]                = readFormat(settings, "theme.shift");
	theme.text[Theme::Text::Stack]                = readFormat(settings, "theme.stack");
	theme.text[Theme::Text::System]               = readFormat(settings, "theme.system");
	theme.text[Theme::Text::TakenJump]            = readFormat(settings, "theme.taken_jump");

	// misc settings
	theme.misc[Theme::Misc::Badge] = readFormat(settings, "theme.badge");

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
