/*
Copyright (C) 2017 - 2017 Evan Teran
                          evan.teran@gmail.com
Copyright (C) 2017 - 2017 Ruslan Kabatsayev
                          b7.10110111@gmail.com

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

#include "RegisterViewModel.h"
#include <QDebug>

using util::make_unique;
using GPR=RegisterViewModelBase::SimpleRegister<edb::value32>;

QVariant RegisterViewModel::data(QModelIndex const& index, int role) const {
	if(role==FixedLengthRole)
	{
		using namespace RegisterViewModelBase;
		const auto reg=static_cast<RegisterViewItem*>(index.internalPointer());
		const auto name=reg->data(NAME_COLUMN).toString();
		if(index.column()==NAME_COLUMN && name.length()>=2)
		{
			if(name[0]=='r' && '0'<name[1] && name[1]<'9')
				return 3;
		}
	}
	return Model::data(index,role);
}

void addGPRs(RegisterViewModelBase::Category* gprs)
{
	gprs->addRegister(make_unique<GPR>("R0"));
	gprs->addRegister(make_unique<GPR>("R1"));
	gprs->addRegister(make_unique<GPR>("R2"));
	gprs->addRegister(make_unique<GPR>("R3"));
	gprs->addRegister(make_unique<GPR>("R4"));
	gprs->addRegister(make_unique<GPR>("R5"));
	gprs->addRegister(make_unique<GPR>("R6"));
	gprs->addRegister(make_unique<GPR>("R7"));
	gprs->addRegister(make_unique<GPR>("R8"));
	gprs->addRegister(make_unique<GPR>("R9"));
	gprs->addRegister(make_unique<GPR>("R10"));
	gprs->addRegister(make_unique<GPR>("R11"));
	gprs->addRegister(make_unique<GPR>("R12"));
	gprs->addRegister(make_unique<GPR>("SP"));
	gprs->addRegister(make_unique<GPR>("LR"));
	gprs->addRegister(make_unique<GPR>("PC"));
}

RegisterViewModel::RegisterViewModel(int cpuSuppFlags, QObject* parent)
	: RegisterViewModelBase::Model(parent),
	  gprs(addCategory(tr("General Purpose")))
{
	addGPRs(gprs);
}

void invalidate(RegisterViewModelBase::Category* cat, int row, const char* nameToCheck)
{
	if(!cat) return;
	Q_ASSERT(row<cat->childCount());
	const auto reg=cat->getRegister(row);
	Q_ASSERT(!nameToCheck || reg->name()==nameToCheck); Q_UNUSED(nameToCheck);
	reg->invalidate();
}

template<typename RegType, typename ValueType>
void updateRegister(RegisterViewModelBase::Category* cat, int row, ValueType value, QString const& comment, const char* nameToCheck=0)
{
	const auto reg=cat->getRegister(row);
	if(!dynamic_cast<RegType*>(reg))
	{
		qWarning() << "Failed to update register " << reg->name() << ": failed to convert register passed to expected type " << typeid(RegType).name();
		invalidate(cat,row,nameToCheck);
		return;
	}
	Q_ASSERT(!nameToCheck || reg->name()==nameToCheck); Q_UNUSED(nameToCheck);
	static_cast<RegType*>(reg)->update(value,comment);
}

void RegisterViewModel::updateGPR(std::size_t i, edb::value32 val, QString const& comment)
{
	Q_ASSERT(int(i)<gprs->childCount());
	updateRegister<GPR>(gprs,i,val,comment);
}
