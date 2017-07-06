/*
Copyright (C) 2017 - 2017 Evan Teran
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

#ifndef REGISTER_VIEW_MODEL_X86_20151213
#define REGISTER_VIEW_MODEL_X86_20151213

#include "RegisterViewModelBase.h"
#include "Types.h"

class RegisterViewModel : public RegisterViewModelBase::Model
{
	Q_OBJECT

public:
	RegisterViewModel(int CPUFeaturesPresent, QObject* parent = nullptr);
	QVariant data(QModelIndex const& index, int role=Qt::DisplayRole) const override;
};

#endif
