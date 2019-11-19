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

#ifndef X86_REGISTER_VIEW_MODEL_H_20151213_
#define X86_REGISTER_VIEW_MODEL_H_20151213_

#include "RegisterViewModelBase.h"
#include "Types.h"

class RegisterViewModel : public RegisterViewModelBase::Model {
	Q_OBJECT

private:
	// Relying on categories being there for the whole lifetime of the model
	RegisterViewModelBase::Category *gprs32;
	RegisterViewModelBase::Category *gprs64;
	RegisterViewModelBase::Category *genStatusRegs32;
	RegisterViewModelBase::Category *genStatusRegs64;
	RegisterViewModelBase::Category *segRegs;
	RegisterViewModelBase::Category *dbgRegs32;
	RegisterViewModelBase::Category *dbgRegs64;
	RegisterViewModelBase::FPUCategory *fpuRegs32;
	RegisterViewModelBase::FPUCategory *fpuRegs64;
	RegisterViewModelBase::SIMDCategory *mmxRegs;
	RegisterViewModelBase::SIMDCategory *sseRegs32;
	RegisterViewModelBase::SIMDCategory *sseRegs64;
	RegisterViewModelBase::SIMDCategory *avxRegs32;
	RegisterViewModelBase::SIMDCategory *avxRegs64;

public:
	enum class CpuMode {
		UNKNOWN,
		IA32,
		AMD64
	};

	// scoped enum, but allowing to implicitly covert to int
	struct CPUFeatureBits {
		enum Value {
			MMX = 1,
			SSE = 2,
			AVX = 4
		};
	};

	explicit RegisterViewModel(int CPUFeaturesPresent, QObject *parent = nullptr);
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	void setCpuMode(CpuMode mode);

	// NOTE: all these functions only change data, they don't emit dataChanged!
	// Use dataUpdateFinished() to have dataChanged emitted.
	void updateGPR(std::size_t i, edb::value32 val, const QString &comment = QString());
	void updateGPR(std::size_t i, edb::value64 val, const QString &comment = QString());
	void updateIP(edb::value32, const QString &comment = QString());
	void updateIP(edb::value64, const QString &comment = QString());
	void updateFlags(edb::value32, const QString &comment = QString());
	void updateFlags(edb::value64, const QString &comment = QString());
	void updateSegReg(std::size_t i, edb::value16, const QString &comment = QString());
	void updateFPUReg(std::size_t i, edb::value80, const QString &comment = QString());
	void updateFCR(edb::value16, const QString &comment = QString());
	void updateFSR(edb::value16, const QString &comment = QString());
	void updateFTR(edb::value16, const QString &comment = QString());
	void invalidateFIP();
	void invalidateFDP();
	void invalidateFIS();
	void invalidateFDS();
	void invalidateFOP();
	void updateFIP(edb::value32, const QString &comment = QString());
	void updateFIP(edb::value64, const QString &comment = QString());
	void updateFDP(edb::value32, const QString &comment = QString());
	void updateFDP(edb::value64, const QString &comment = QString());
	void updateFIS(edb::value16, const QString &comment = QString());
	void updateFDS(edb::value16, const QString &comment = QString());
	void updateFOP(edb::value16, const QString &comment = QString());
	void updateDR(std::size_t i, edb::value32, const QString &comment = QString());
	void updateDR(std::size_t i, edb::value64, const QString &comment = QString());
	void updateMMXReg(std::size_t i, edb::value64, const QString &comment = QString());
	void invalidateMMXReg(std::size_t i);
	void updateSSEReg(std::size_t i, edb::value128, const QString &comment = QString());
	void invalidateSSEReg(std::size_t i);
	void updateAVXReg(std::size_t i, edb::value256, const QString &comment = QString());
	void invalidateAVXReg(std::size_t i);
	void updateMXCSR(edb::value32, const QString &comment = QString());
	void invalidateMXCSR();

private:
	void hide64BitModeCategories();
	void hide32BitModeCategories();
	void show64BitModeCategories();
	void show32BitModeCategories();
	void hideGenericCategories();
	void showGenericCategories();
	std::tuple<RegisterViewModelBase::Category * /*sse*/, RegisterViewModelBase::Category * /*avx*/, unsigned /*maxRegs*/> getSSEparams() const;
	RegisterViewModelBase::FPUCategory *getFPUcat() const;
	CpuMode mode = static_cast<CpuMode>(-1); // TODO(eteran): why not CpuMode::UNKNOWN?
};

#endif
