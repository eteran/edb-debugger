/*
Copyright (C) 2006 - 2015 Evan Teran
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

#ifndef ELFBINARYINFO_20061122_H_
#define ELFBINARYINFO_20061122_H_

#include "IPlugin.h"
#include "ISymbolGenerator.h"
#include "Types.h"

class QMenu;

namespace BinaryInfo {

class BinaryInfo : public QObject, public IPlugin, public ISymbolGenerator {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
#endif
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	BinaryInfo();

private:
	virtual void private_init();
	virtual QWidget* options_page() override;

public:
	virtual QMenu *menu(QWidget *parent = 0);
	virtual QString extra_arguments() const;
	virtual ArgumentStatus parse_argments(QStringList &args);
	
public:
	virtual bool generate_symbol_file(const QString &filename, const QString &symbol_file);

public Q_SLOTS:
	void explore_header();
	
private:
	QMenu *menu_;
};

}

#endif
