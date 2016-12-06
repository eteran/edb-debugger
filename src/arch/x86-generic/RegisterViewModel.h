#ifndef REGISTER_VIEW_MODEL_X86_20151213
#define REGISTER_VIEW_MODEL_X86_20151213

#include "RegisterViewModelBase.h"
#include "Types.h"

class RegisterViewModel : public RegisterViewModelBase::Model
{
	// Relying on categories being there for the whole lifetime of the model
	RegisterViewModelBase::Category* gprs32;
	RegisterViewModelBase::Category* gprs64;
	RegisterViewModelBase::Category* genStatusRegs32;
	RegisterViewModelBase::Category* genStatusRegs64;
	RegisterViewModelBase::Category* segRegs;
	RegisterViewModelBase::Category* dbgRegs32;
	RegisterViewModelBase::Category* dbgRegs64;
	RegisterViewModelBase::FPUCategory* fpuRegs32;
	RegisterViewModelBase::FPUCategory* fpuRegs64;
	RegisterViewModelBase::SIMDCategory* mmxRegs;
	RegisterViewModelBase::SIMDCategory* sseRegs32;
	RegisterViewModelBase::SIMDCategory* sseRegs64;
	RegisterViewModelBase::SIMDCategory* avxRegs32;
	RegisterViewModelBase::SIMDCategory* avxRegs64;
public:
	enum class CPUMode
	{
		UNKNOWN,
		IA32,
		AMD64
	};
	struct CPUFeatureBits // scoped enum, but allowing to implicitly covert to int
	{
		enum Value
		{
			MMX=1,
			SSE=2,
			AVX=4
		};
	};
	RegisterViewModel(int CPUFeaturesPresent, QObject* parent=nullptr);
	QVariant data(QModelIndex const& index, int role=Qt::DisplayRole) const override;
	void setCPUMode(CPUMode mode);
	// NOTE: all these functions only change data, they don't emit dataChanged!
	// Use dataUpdateFinished() to have dataChanged emitted.
	void updateGPR(std::size_t i, edb::value32 val, QString const& comment=QString());
	void updateGPR(std::size_t i, edb::value64 val, QString const& comment=QString());
	void updateIP(edb::value32,QString const& comment=QString());
	void updateIP(edb::value64,QString const& comment=QString());
	void updateFlags(edb::value32, QString const& comment=QString());
	void updateFlags(edb::value64, QString const& comment=QString());
	void updateSegReg(std::size_t i, edb::value16, QString const& comment=QString());
	void updateFPUReg(std::size_t i, edb::value80, QString const& comment=QString());
	void updateFCR(edb::value16, QString const& comment=QString());
	void updateFSR(edb::value16, QString const& comment=QString());
	void updateFTR(edb::value16, QString const& comment=QString());
	void invalidateFIP();
	void invalidateFDP();
	void invalidateFIS();
	void invalidateFDS();
	void invalidateFOP();
	void updateFIP(edb::value32, QString const& comment=QString());
	void updateFIP(edb::value64, QString const& comment=QString());
	void updateFDP(edb::value32, QString const& comment=QString());
	void updateFDP(edb::value64, QString const& comment=QString());
	void updateFIS(edb::value16, QString const& comment=QString());
	void updateFDS(edb::value16, QString const& comment=QString());
	void updateFOP(edb::value16, QString const& comment=QString());
	void updateDR(std::size_t i, edb::value32, QString const& comment=QString());
	void updateDR(std::size_t i, edb::value64, QString const& comment=QString());
	void updateMMXReg(std::size_t i, edb::value64, QString const& comment=QString());
	void invalidateMMXReg(std::size_t i);
	void updateSSEReg(std::size_t i, edb::value128, QString const& comment=QString());
	void invalidateSSEReg(std::size_t i);
	void updateAVXReg(std::size_t i, edb::value256, QString const& comment=QString());
	void invalidateAVXReg(std::size_t i);
	void updateMXCSR(edb::value32, QString const& comment=QString());
	void invalidateMXCSR();
private:
	void hide64BitModeCategories();
	void hide32BitModeCategories();
	void show64BitModeCategories();
	void show32BitModeCategories();
	void hideGenericCategories();
	void showGenericCategories();
	std::tuple<RegisterViewModelBase::Category* /*sse*/,
			   RegisterViewModelBase::Category* /*avx*/,
			   unsigned/*maxRegs*/> getSSEparams() const;
	RegisterViewModelBase::FPUCategory* getFPUcat() const;
	CPUMode mode=static_cast<CPUMode>(-1);
};

#endif
