#include "RegisterViewModelBase.h"
#include "edb.h"
#include "IDebugger.h"
#include "State.h"
#include "Types.h"
#include "Util.h"

#include <algorithm>
#include <numeric>
#include <boost/range/adaptor/reversed.hpp>
#include <cstdint>
#include <QBrush>
#include <QDebug>
#include <QSettings>
#include <QString>
#include <QtGlobal>


#define CHECKED_CAST(TYPE,OBJECT) checked_cast<TYPE>(OBJECT)

namespace {

template <class T1, class T2>
T1* checked_cast(T2 object) {
	Q_ASSERT(dynamic_cast<T1*>(object));
	return static_cast<T1*>(object);
}

}

namespace RegisterViewModelBase
{

template<typename T>
bool setDebuggeeRegister(QString const& name, T const& value, T& resultingValue)
{
	if(auto*const dcore=edb::v1::debugger_core)
	{
		State state;
		// read
		dcore->get_state(&state);
		auto reg=state[name];
		if(!reg)
		{
			qWarning() << qPrintable(QString("Warning: failed to get register %1 (in function `%2`)").arg(name).arg(Q_FUNC_INFO));
			return false;
		}
		Q_ASSERT(reg.bitSize()==8*sizeof(T));
		const auto origValue=reg.value<T>();
		if(origValue==value) return true; // do nothing if it's not different, it'll help us check result
		// modify
		reg.setValueFrom(value);
		// write
		state.set_register(reg);
		dcore->set_state(state);
		// check
		dcore->get_state(&state);
		const auto resultReg=state[name];
		if(!resultReg) return false;
		resultingValue=resultReg.value<T>();
		bool success=resultingValue!=origValue; // success if we changed it
		return success;
	}
	return false;
}
template bool setDebuggeeRegister<edb::value16 >(QString const& name, edb::value16  const& value, edb::value16 & resultingValue);
template bool setDebuggeeRegister<edb::value32 >(QString const& name, edb::value32  const& value, edb::value32 & resultingValue);
template bool setDebuggeeRegister<edb::value64 >(QString const& name, edb::value64  const& value, edb::value64 & resultingValue);
template bool setDebuggeeRegister<edb::value80 >(QString const& name, edb::value80  const& value, edb::value80 & resultingValue);
template bool setDebuggeeRegister<edb::value128>(QString const& name, edb::value128 const& value, edb::value128& resultingValue);
template bool setDebuggeeRegister<edb::value256>(QString const& name, edb::value256 const& value, edb::value256& resultingValue);

RegisterViewItem* getItem(QModelIndex const& index)
{
	if(!index.isValid()) return nullptr;
	return static_cast<RegisterViewItem*>(index.internalPointer());
}

// ----------------- RegisterViewItem impl ---------------------------

void RegisterViewItem::init(RegisterViewItem* parent, int row)
{
	parentItem=parent;
	row_=row;
}

// ---------------- CategoriesHolder impl ------------------------------

CategoriesHolder::CategoriesHolder()
	: RegisterViewItem("")
{}

int CategoriesHolder::childCount() const
{
	return std::accumulate(categories.cbegin(),categories.cend(),0,
			[](const int& psum,const std::unique_ptr<RegisterViewModelBase::Category>& cat)
				{ return psum+cat->visible(); });
}

QVariant CategoriesHolder::data(int /*column*/) const
{
	return {};
}

QByteArray CategoriesHolder::rawValue() const
{
	return {};
}

RegisterViewItem* CategoriesHolder::child(int visibleRow)
{
	// return visible row #visibleRow
	for(std::size_t row=0;row<categories.size(); ++row)
	{
		if(categories[row]->visible()) --visibleRow;
		if(visibleRow==-1) return categories[row].get();
	}
	return nullptr;
}

template<typename CategoryType>
CategoryType* CategoriesHolder::insert(QString const& name)
{
	categories.emplace_back(util::make_unique<CategoryType>(name,categories.size()));
	return static_cast<CategoryType*>(categories.back().get());
}

SIMDCategory* CategoriesHolder::insertSIMD(QString const& name,
										   std::vector<NumberDisplayMode> const& validFormats)
{
	auto cat=util::make_unique<SIMDCategory>(name,categories.size(),validFormats);
	auto ret=cat.get();
	categories.emplace_back(std::move(cat));
	return ret;
}

// ---------------- Model impl -------------------

Model::Model(QObject* parent)
	: QAbstractItemModel(parent),
	  rootItem(new CategoriesHolder)
{
}

Model::~Model()
{
}

void Model::setActiveIndex(QModelIndex const& newActiveIndex)
{
	activeIndex_=newActiveIndex;
}

QModelIndex Model::activeIndex() const
{
	return activeIndex_;
}

QModelIndex Model::index(int row, int column, QModelIndex const& parent) const
{
	if(!hasIndex(row,column,parent)) return QModelIndex();

	RegisterViewItem* parentItem;
	if(!parent.isValid()) parentItem = rootItem.get();
	else parentItem = static_cast<RegisterViewItem*>(parent.internalPointer());

	auto childItem = parentItem->child(row);

	if(childItem) return createIndex(row, column, childItem);
	else return QModelIndex();
}

int Model::rowCount(QModelIndex const& parent) const
{
	const auto item = parent.isValid() ? getItem(parent) : rootItem.get();
	return item->childCount();
}

int Model::columnCount(QModelIndex const&) const
{
	return 3; // all items reserve 3 columns: regName, value, comment
}

QModelIndex Model::parent(QModelIndex const& index) const
{
	if(!index.isValid()) return QModelIndex();

	const auto parent=getItem(index)->parent();
	if(!parent || parent==rootItem.get()) return QModelIndex();

	return createIndex(parent->row(),0,parent);
}

Qt::ItemFlags Model::flags(QModelIndex const& index) const
{
	if(!index.isValid()) return 0;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | (index.column()==1 ? Qt::ItemIsEditable : Qt::NoItemFlags);
}

QVariant Model::data(QModelIndex const& index, int role) const
{
	const auto*const item=getItem(index);
	if(!item) return {};

	switch(role)
	{
	case Qt::DisplayRole:
		return item->data(index.column());

	case Qt::ForegroundRole:
		if(index.column()!=VALUE_COLUMN || !item->changed())
			return {}; // default color for unchanged register and for non-value column
		return QBrush(Qt::red); // TODO: use user palette

	case RegisterChangedRole:
		return item->changed();

	case FixedLengthRole:
		if(index.column()==NAME_COLUMN)
			return item->name().size();
		else if(index.column()==VALUE_COLUMN)
			return item->valueMaxLength();
		else return {};

	case RawValueRole:
	{
		if(index.column()!=VALUE_COLUMN)
			return {};
		const auto ret=item->rawValue();
		if(ret.size()) return ret;
	}
	case ChosenSIMDSizeRole:
	{
		const auto simdCat=dynamic_cast<SIMDCategory const*>(item);
		if(!simdCat) return {};
		return static_cast<int>(simdCat->chosenSize());
	}
	case ChosenSIMDFormatRole:
	{
		const auto simdCat=dynamic_cast<SIMDCategory const*>(item);
		if(!simdCat) return {};
		return static_cast<int>(simdCat->chosenFormat());
	}
	case ChosenSIMDSizeRowRole:
	{
		const auto simdCat=dynamic_cast<SIMDCategory const*>(item);
		if(!simdCat) return {};
		switch(simdCat->chosenSize())
		{
		case ElementSize::BYTE:  return BYTES_ROW;
		case ElementSize::WORD:  return WORDS_ROW;
		case ElementSize::DWORD: return DWORDS_ROW;
		case ElementSize::QWORD: return QWORDS_ROW;
		}
		return {};
	}
	case ChosenFPUFormatRole:
		if(const auto fpuCat=dynamic_cast<FPUCategory const*>(item))
			return static_cast<int>(fpuCat->chosenFormat());
		else return {};
	case ChosenFPUFormatRowRole:
		if(const auto fpuCat=dynamic_cast<FPUCategory const*>(item))
		{
			switch(fpuCat->chosenFormat())
			{
			case NumberDisplayMode::Float: return FPU_FLOAT_ROW;
			case NumberDisplayMode::Hex: return FPU_HEX_ROW;
			default: return {};
			}
		}
		return {};

	case ChosenSIMDFormatRowRole:
	{
		const auto simdCat=dynamic_cast<SIMDCategory const*>(item);
		if(!simdCat) return {};
		switch(simdCat->chosenFormat())
		{
		case NumberDisplayMode::Hex: return SIMD_HEX_ROW;
		case NumberDisplayMode::Signed: return SIMD_SIGNED_ROW;
		case NumberDisplayMode::Unsigned: return SIMD_UNSIGNED_ROW;
		case NumberDisplayMode::Float: return SIMD_FLOAT_ROW;
		}
		return {};
	}
	case ValidSIMDFormatsRole:
	{
		const auto simdCat=dynamic_cast<SIMDCategory const*>(item);
		if(!simdCat) return {};
		return QVariant::fromValue(simdCat->validFormats());
	}
	case IsNormalRegisterRole:
	{
		// Normal register is defined by its hierarchy level:
		// root(invalid index)->category->register
		// If level is not like this, it's not a normal register

		// parent should be category
		if(!index.parent().isValid()) return false;
		// parent of category is root, i.e. invalid index
		if(index.parent().parent().isValid()) return false;
		return true;
	}
	case IsFPURegisterRole:
	{
		return !!dynamic_cast<GenericFPURegister const*>(item);
	}
	case IsBitFieldRole:
		return !!dynamic_cast<BitFieldProperties const*>(item);
	case IsSIMDElementRole:
		return !!dynamic_cast<SIMDElement const*>(item);
	case BitFieldOffsetRole:
	{
		const auto bitField=dynamic_cast<BitFieldProperties const*>(item);
		if(!bitField) return {};
		return bitField->offset();
	}
	case BitFieldLengthRole:
	{
		const auto bitField=dynamic_cast<BitFieldProperties const*>(item);
		if(!bitField) return {};
		return bitField->length();
	}
	case ValueAsRegisterRole:
		if(auto*const dcore=edb::v1::debugger_core)
		{
			const auto name=index.sibling(index.row(),NAME_COLUMN).data().toString();
			if(name.isEmpty()) return {};
			State state;
			// read
			dcore->get_state(&state);
			return QVariant::fromValue(state[name]);
		}
	break;

	default:
		return {};
	}
	return {};
}

bool Model::setData(QModelIndex const& index, QVariant const& data, int role)
{
	auto*const item=getItem(index);
	const auto valueIndex=index.sibling(index.row(),VALUE_COLUMN);
	bool ok=false;

	switch(role)
	{
	case Qt::EditRole:
	case RawValueRole:
		if(this->data(index,IsNormalRegisterRole).toBool())
		{
			auto*const reg=checked_cast<AbstractRegisterItem>(item);
			if(role==Qt::EditRole && data.type()==QVariant::String)
				ok=reg->setValue(data.toString());
			else if(data.type()==QVariant::ByteArray)
				ok=reg->setValue(data.toByteArray());
		}
		break;
	case ValueAsRegisterRole:
	{
		auto*const regItem=checked_cast<AbstractRegisterItem>(item);
		assert(data.isValid());
		assert(index.isValid());
		const auto name=index.sibling(index.row(),NAME_COLUMN).data().toString();
		const auto regVal=data.value<Register>();
		assert(name.toLower()==regVal.name().toLower());
		ok=regItem->setValue(regVal);
		break;
	}
	}
	if(ok)
	{
		Q_EMIT dataChanged(valueIndex,valueIndex);
		if(rowCount(valueIndex))
		{
			Q_EMIT dataChanged(this->index(0,VALUE_COLUMN,valueIndex),
							   this->index(rowCount(valueIndex),COMMENT_COLUMN,valueIndex));
		}
		return true;
	}
	return false;
}

void Model::setChosenSIMDSize(QModelIndex const& index, ElementSize const newSize)
{
	const auto cat=getItem(index);
	Q_ASSERT(cat);

	const auto simdCat=dynamic_cast<SIMDCategory*>(cat);
	Q_ASSERT(simdCat);
	// Not very crash-worthy problem, just don't do anything in release mode
	if(!simdCat) return;

	simdCat->setChosenSize(newSize);
	Q_EMIT SIMDDisplayFormatChanged();
	// Make treeviews update root register items with default view
	const auto valueIndex=index.sibling(index.row(),VALUE_COLUMN);
	Q_EMIT dataChanged(valueIndex, valueIndex);
}

void Model::setChosenSIMDFormat(QModelIndex const& index, NumberDisplayMode const newFormat)
{
	const auto cat=getItem(index);
	Q_ASSERT(cat);

	const auto simdCat=dynamic_cast<SIMDCategory*>(cat);
	Q_ASSERT(simdCat);
	// Not very crash-worthy problem, just don't do anything in release mode
	if(!simdCat) return;


	simdCat->setChosenFormat(newFormat);
	Q_EMIT SIMDDisplayFormatChanged();
	// Make treeviews update root register items with default view
	const auto valueIndex=index.sibling(index.row(),VALUE_COLUMN);
	Q_EMIT dataChanged(valueIndex, valueIndex);
}

void Model::setChosenFPUFormat(QModelIndex const& index, NumberDisplayMode const newFormat)
{
	const auto cat=getItem(index);
	Q_ASSERT(cat);

	const auto fpuCat=dynamic_cast<FPUCategory*>(cat);
	Q_ASSERT(fpuCat);
	// Not very crash-worthy problem, just don't do anything in release mode
	if(!fpuCat) return;

	fpuCat->setChosenFormat(newFormat);
	Q_EMIT FPUDisplayFormatChanged();
	// Make treeviews update root register items with default view
	const auto valueIndex=index.sibling(index.row(),VALUE_COLUMN);
	Q_EMIT dataChanged(valueIndex, valueIndex);
}

void Model::hideAll()
{
	for(const auto& cat : rootItem->categories)
		cat->hide();
}

Category* Model::addCategory(QString const& name)
{
	return rootItem->insert(name);
}

SIMDCategory* Model::addSIMDCategory(QString const& name,
									 std::vector<NumberDisplayMode> const& validFormats)
{
	return rootItem->insertSIMD(name,validFormats);
}

FPUCategory* Model::addFPUCategory(QString const& name)
{
	return rootItem->insert<FPUCategory>(name);
}

void Model::hide(Category* cat)
{
	cat->hide();
}

void Model::show(Category* cat)
{
	cat->show();
}

void Model::saveValues()
{
	for(const auto& cat : rootItem->categories)
		cat->saveValues();
}

// -------------------- Category impl --------------------

Category::Category(QString const& name, int row)
	: RegisterViewItem(name)
{
	init(nullptr,row);
}

Category::Category(Category&& other)
	: RegisterViewItem(other.name())
{
	parentItem=other.parentItem;
	row_=other.row_;
	registers=std::move(other.registers);
}

int Category::childCount() const
{
	return registers.size();
}

RegisterViewItem* Category::child(int row)
{
	if(row < 0 || row >= childCount())
		return nullptr;
	return registers[row].get();
}

QVariant Category::data(int column) const
{
	if(column==0) return name_;
	return {};
}

QByteArray Category::rawValue() const
{
	return {};
}

void Category::hide()
{
	visible_=false;
	for(const auto& reg : registers)
		reg->invalidate();
}

void Category::show()
{
	visible_=true;
}

bool Category::visible() const
{
	return visible_;
}

void Category::addRegister(std::unique_ptr<AbstractRegisterItem> reg)
{
	registers.emplace_back(std::move(reg));
	registers.back()->init(this,registers.size()-1);
}

AbstractRegisterItem* Category::getRegister(std::size_t i) const
{
	return registers[i].get();
}

void Category::saveValues()
{
	for(auto& reg : registers)
		reg->saveValue();
}

// -------------------- RegisterItem impl ------------------------
template<typename T>
RegisterItem<T>::RegisterItem(QString const& name)
	: AbstractRegisterItem(name)
{
	invalidate();
}

template<typename T>
void RegisterItem<T>::invalidate()
{
	util::markMemory(&value_,sizeof value_);
	util::markMemory(&prevValue_,sizeof prevValue_);
	comment_.clear();
	valueKnown_=false;
	prevValueKnown_=false;
}

template<typename T>
bool RegisterItem<T>::valid() const
{
	return valueKnown_;
}

template<typename T>
bool RegisterItem<T>::changed() const
{
	return !valueKnown_ || !prevValueKnown_ || prevValue_!=value_;
}

template<typename T>
void RegisterItem<T>::saveValue()
{
	prevValue_=value_;
	prevValueKnown_=valueKnown_;
}

template<typename T>
int RegisterItem<T>::childCount() const
{
	return 0;
}

template<typename T>
RegisterViewItem* RegisterItem<T>::child(int)
{
	return nullptr; // simple register item has no children
}

template<typename T>
QString RegisterItem<T>::valueString() const
{
	if(!this->valueKnown_) return "???";
	return this->value_.toHexString();
}

template<typename T>
QVariant RegisterItem<T>::data(int column) const
{
	switch(column)
	{
	case Model::NAME_COLUMN:    return this->name_;
	case Model::VALUE_COLUMN:   return valueString();
	case Model::COMMENT_COLUMN: return this->comment_;
	}
	return {};
}

template<typename T>
QByteArray RegisterItem<T>::rawValue() const
{
	if(!this->valueKnown_) return {};
	return QByteArray(reinterpret_cast<const char*>(&this->value_),
					  sizeof this->value_);
}

template<typename T>
bool RegisterItem<T>::setValue(Register const& reg)
{
	assert(reg.bitSize()==8*sizeof(T));
	return setDebuggeeRegister<T>(reg.name(),reg.value<T>(),value_);
}
	
template<typename T>
bool RegisterItem<T>::setValue(QByteArray const& newValue)
{
	T value;
	std::memcpy(&value,newValue.constData(),newValue.size());
	return setDebuggeeRegister<T>(name(),value,value_);
}

template<typename T> typename std::enable_if<(sizeof(T)>sizeof(std::uint64_t)),
bool>::type setValue(T& /*valueToSet*/, QString const& /*name*/, QString const& /*valueStr*/)
{
	qWarning() << "FIXME: unimplemented" << Q_FUNC_INFO;
	return false; // TODO: maybe do set?.. would be arch-dependent then due to endianness
}

template<typename T> typename std::enable_if<sizeof(T)<=sizeof(std::uint64_t),
bool>::type setValue(T& valueToSet, QString const& name, QString const& valueStr)
{
	bool ok=false;
	const auto value=T::fromHexString(valueStr,&ok);
	if(!ok) return false;
	return setDebuggeeRegister(name,value,valueToSet);
}

template<typename T>
bool RegisterItem<T>::setValue(QString const& valueStr)
{
	// TODO: ask ArchProcessor to actually set it and return true only if done
	return RegisterViewModelBase::setValue(value_,name(),valueStr);
}

template class RegisterItem<edb::value16>;
template class RegisterItem<edb::value32>;
template class RegisterItem<edb::value64>;
template class RegisterItem<edb::value80>;
template class RegisterItem<edb::value128>;
template class RegisterItem<edb::value256>;

// -------------------- SimpleRegister impl -----------------------

template<typename T>
void SimpleRegister<T>::update(T const& value, QString const& comment)
{
	this->value_=value;
	this->comment_=comment;
	this->valueKnown_=true;
}

template<typename T>
int SimpleRegister<T>::valueMaxLength() const
{
	return 2*sizeof(T);
}

template class SimpleRegister<edb::value16>;
template class SimpleRegister<edb::value32>;
template class SimpleRegister<edb::value64>;
template class SimpleRegister<edb::value80>;
template class SimpleRegister<edb::value128>;
template class SimpleRegister<edb::value256>;

// ---------------- BitFieldItem impl -----------------------

template<typename UnderlyingType>
BitFieldItem<UnderlyingType>::BitFieldItem(BitFieldDescription const& descr)
	: RegisterViewItem(descr.name),
	  offset_(descr.offset),
	  length_(descr.length),
	  explanations(descr.explanations)
{
	Q_ASSERT(8*sizeof(UnderlyingType)>=length_);
	Q_ASSERT(explanations.size()==0 || explanations.size()==2u<<(length_-1));
}

template<typename UnderlyingType>
FlagsRegister<UnderlyingType>* BitFieldItem<UnderlyingType>::reg() const
{
	return checked_cast<FlagsRegister<UnderlyingType>>(this->parent());
}

template<typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::lengthToMask() const
{
	return 2*(1ull << (length_-1))-1;
}

template<typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::calcValue(UnderlyingType regVal) const
{
	return (regVal>>offset_)&lengthToMask();
}

template<typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::value() const
{
	return calcValue(reg()->value_);
}

template<typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::prevValue() const
{
	return calcValue(reg()->prevValue_);
}

template<typename UnderlyingType>
int BitFieldItem<UnderlyingType>::valueMaxLength() const
{
	return std::ceil(length_/4.); // number of nibbles
}

template<typename UnderlyingType>
bool BitFieldItem<UnderlyingType>::changed() const
{
	return !reg()->valueKnown_ || !reg()->prevValueKnown_ || value()!=prevValue();
}

template<typename UnderlyingType>
QVariant BitFieldItem<UnderlyingType>::data(int column) const
{
	const auto str=reg()->valid() ? value().toHexString() : QString(sizeof(UnderlyingType)*2,'?');
	switch(column)
	{
	case Model::NAME_COLUMN:
		return name();
	case Model::VALUE_COLUMN:
		Q_ASSERT(str.size()>0);
		return str.right(std::ceil(length_/4.));
	case Model::COMMENT_COLUMN:
		if(explanations.empty()) return {};
		return reg()->valid() ? explanations[value()] : QString();
	}
	return {};
}

template<typename UnderlyingType>
QByteArray BitFieldItem<UnderlyingType>::rawValue() const
{
	const auto val=value();
	return QByteArray(reinterpret_cast<const char*>(&val),sizeof val);
}

template<typename UnderlyingType>
unsigned BitFieldItem<UnderlyingType>::length() const
{
	return length_;
}

template<typename UnderlyingType>
unsigned BitFieldItem<UnderlyingType>::offset() const
{
	return offset_;
}

// ---------------- FlagsRegister impl ------------------------

template<typename StoredType>
FlagsRegister<StoredType>::FlagsRegister(QString const& name,
										 std::vector<BitFieldDescription> const& bitFields)
			   : SimpleRegister<StoredType>(name)
{
	for(auto field : bitFields)
	{
		fields.emplace_back(field);
		fields.back().init(this,fields.size()-1);
	}
}

template<typename StoredType>
int FlagsRegister<StoredType>::childCount() const
{
	return fields.size();
}

template<typename StoredType>
RegisterViewItem* FlagsRegister<StoredType>::child(int row)
{
	return &fields[row];
}

template class FlagsRegister<edb::value16>;
template class FlagsRegister<edb::value32>;
template class FlagsRegister<edb::value64>;

// --------------------- SIMDFormatItem impl -----------------------

template<class StoredType, class SizingType>
QString SIMDFormatItem<StoredType,SizingType>::name(NumberDisplayMode format) const
{
	switch(format)
	{
	case NumberDisplayMode::Hex: return "hex";
	case NumberDisplayMode::Signed: return "signed";
	case NumberDisplayMode::Unsigned: return "unsigned";
	case NumberDisplayMode::Float: return "float";
	}
	EDB_PRINT_AND_DIE("Unexpected format ",(long)format);
}

template<class StoredType, class SizingType>
SIMDFormatItem<StoredType,SizingType>::SIMDFormatItem(NumberDisplayMode format)
	: RegisterViewItem(name(format)),
	  format_(format)
{
}

template<class StoredType, class SizingType>
NumberDisplayMode SIMDFormatItem<StoredType,SizingType>::format() const
{
	return format_;
}

template<class StoredType, class SizingType>
bool SIMDFormatItem<StoredType,SizingType>::changed() const
{
	return parent()->changed();
}

template<class SizingType> typename std::enable_if<(sizeof(SizingType)>=sizeof(float) &&
													sizeof(SizingType)!=sizeof(edb::value80)),
QString>::type toString(SizingType value,NumberDisplayMode format)
{
	return format==NumberDisplayMode::Float ? formatFloat(value) : util::formatInt(value,format);
}

template<class SizingType> typename std::enable_if<sizeof(SizingType)<sizeof(float),
QString>::type toString(SizingType value,NumberDisplayMode format)
{
	return format==NumberDisplayMode::Float ? "(too small element width for float)" : util::formatInt(value,format);
}

QString toString(edb::value80 const& value, NumberDisplayMode format)
{
	switch(format)
	{
	case NumberDisplayMode::Float: return formatFloat(value);
	case NumberDisplayMode::Hex: return value.toHexString();
	default: return QString("bug: format=%1").arg(static_cast<int>(format));
	}
}

template<class StoredType, class SizingType>
QVariant SIMDFormatItem<StoredType,SizingType>::data(int column) const
{
	switch(column)
	{
	case Model::NAME_COLUMN: return this->name();
	case Model::VALUE_COLUMN:
		{
			if(const auto parent=dynamic_cast<SIMDSizedElement<StoredType,SizingType>*>(this->parent()))
				return parent->valid() ? toString(parent->value(),format_) : "???";
			if(const auto parent=dynamic_cast<FPURegister<SizingType>*>(this->parent()))
				return parent->valid() ? toString(parent->value(),format_) : "???";
			EDB_PRINT_AND_DIE("failed to detect parent type");
		}
	case Model::COMMENT_COLUMN: return {};
	}
	return {};
}

template<class StoredType, class SizingType>
QByteArray SIMDFormatItem<StoredType,SizingType>::rawValue() const
{
	return static_cast<RegisterViewItem*>(this->parent())->rawValue();
}

template<>
int SIMDFormatItem<edb::value80,edb::value80>::valueMaxLength() const
{
	Q_ASSERT(sizeof(edb::value80)<=sizeof(long double));
	switch(format_)
	{
	case NumberDisplayMode::Hex: return 2*sizeof(edb::value80);
	case NumberDisplayMode::Float: return maxPrintedLength<long double>();
	default: EDB_PRINT_AND_DIE("Unexpected format: ",(long)format_);
	}
}

template<class StoredType, class SizingType>
int SIMDFormatItem<StoredType,SizingType>::valueMaxLength() const
{
	using Unsigned=typename SizingType::InnerValueType;
	using Signed=typename std::make_signed<Unsigned>::type;
	switch(format_)
	{
	case NumberDisplayMode::Hex: return 2*sizeof(SizingType);
	case NumberDisplayMode::Signed: return maxPrintedLength<Signed>();
	case NumberDisplayMode::Unsigned: return maxPrintedLength<Unsigned>();
	case NumberDisplayMode::Float:
		switch(sizeof(SizingType))
		{
		case sizeof(float): return maxPrintedLength<float>();
		case sizeof(double): return maxPrintedLength<double>();
		default:
			if(sizeof(SizingType)<sizeof(float)) return 0;
			EDB_PRINT_AND_DIE("Unexpected sizing type's size for format float: ",sizeof(SizingType));
		}
	}
	EDB_PRINT_AND_DIE("Unexpected format: ",(long)format_);
}

// --------------------- SIMDSizedElement  impl -------------------------

template<class StoredType, class SizingType>
SIMDSizedElement<StoredType,SizingType>::SIMDSizedElement(
							QString const& name,
							std::vector<NumberDisplayMode> const& validFormats)
	: RegisterViewItem(name)
{
	for(const auto format : validFormats)
	{
		if(format!=NumberDisplayMode::Float || sizeof(SizingType)>=sizeof(float))
		{
			// The order must be as expected by other functions
			Q_ASSERT(format!=NumberDisplayMode::Float || formats.size()==Model::SIMD_FLOAT_ROW);
			Q_ASSERT(format!=NumberDisplayMode::Hex || formats.size()==Model::SIMD_HEX_ROW);
			Q_ASSERT(format!=NumberDisplayMode::Signed || formats.size()==Model::SIMD_SIGNED_ROW);
			Q_ASSERT(format!=NumberDisplayMode::Unsigned || formats.size()==Model::SIMD_UNSIGNED_ROW);

			formats.emplace_back(format);
			formats.back().init(this,formats.size()-1);
		}
	}
}

template<class StoredType, class SizingType>
RegisterViewItem* SIMDSizedElement<StoredType,SizingType>::child(int row)
{
	return &formats[row];
}

template<class StoredType, class SizingType>
int SIMDSizedElement<StoredType,SizingType>::childCount() const
{
	return formats.size();
}

template<class StoredType, class SizingType>
SIMDRegister<StoredType>* SIMDSizedElement<StoredType,SizingType>::reg() const
{
	return checked_cast<SIMDRegister<StoredType>>(this->parent()->parent());
}

template<class StoredType, class SizingType>
bool SIMDSizedElement<StoredType,SizingType>::valid() const
{
	return reg()->valueKnown_;
}

template<class StoredType, class SizingType>
QString SIMDSizedElement<StoredType,SizingType>::valueString() const
{
	if(!valid()) return "??";
	return toString(value(),reg()->category()->chosenFormat());
}

template<class StoredType, class SizingType>
int SIMDSizedElement<StoredType,SizingType>::valueMaxLength() const
{
	for(const auto& format : formats)
		if(format.format()==reg()->category()->chosenFormat())
			return format.valueMaxLength();
	return 0;
}

template<class StoredType, class SizingType>
SizingType SIMDSizedElement<StoredType,SizingType>::value() const
{
	std::size_t offset=this->row()*sizeof(SizingType);
	return SizingType(reg()->value_,offset);
}

template<class StoredType, class SizingType>
QVariant SIMDSizedElement<StoredType,SizingType>::data(int column) const
{
	switch(column)
	{
	case Model::NAME_COLUMN: return this->name_;
	case Model::VALUE_COLUMN: return valueString();
	case Model::COMMENT_COLUMN: return {};
	}
	return {};
}

template<class StoredType, class SizingType>
QByteArray SIMDSizedElement<StoredType,SizingType>::rawValue() const
{
	const auto value=this->value();
	return QByteArray(reinterpret_cast<const char*>(&value),sizeof value);
}

template<class StoredType, class SizingType>
bool SIMDSizedElement<StoredType,SizingType>::changed() const
{
	const auto reg=this->reg();
	const std::size_t offset=this->row()*sizeof(SizingType);
	return !reg->valueKnown_ || !reg->prevValueKnown_ ||
		SizingType(reg->prevValue_,offset) != SizingType(reg->value_,offset);
}

// --------------------- SIMDSizedElementsContainer impl -------------------------

template<class StoredType>
template<class SizeType, class... Args>
void SIMDSizedElementsContainer<StoredType>::addElement(Args&&... args)
{
	using Element=SIMDSizedElement<StoredType,SizeType>;

	elements.emplace_back(util::make_unique<Element>(std::forward<Args>(args)...));
}

template<class StoredType>
SIMDSizedElementsContainer<StoredType>::SIMDSizedElementsContainer(
							QString const& name,
							std::size_t size,
							std::vector<NumberDisplayMode> const& validFormats)
	: RegisterViewItem(name)
{
	for(unsigned elemN=0;elemN<sizeof(StoredType)/size;++elemN)
	{
		const auto name=QString("#%1").arg(elemN);
		switch(size)
		{
		case sizeof(edb::value8 ): addElement<edb::value8 >(name,validFormats); break;
		case sizeof(edb::value16): addElement<edb::value16>(name,validFormats); break;
		case sizeof(edb::value32): addElement<edb::value32>(name,validFormats); break;
		case sizeof(edb::value64): addElement<edb::value64>(name,validFormats); break;
		default: EDB_PRINT_AND_DIE("Unexpected element size ",(long)size);
		}
		elements.back()->init(this,elemN);
	}
}

template<class StoredType>
SIMDSizedElementsContainer<StoredType>::SIMDSizedElementsContainer(SIMDSizedElementsContainer&& other)
	: RegisterViewItem(other)
{
	elements=std::move(other.elements);
}

template<class StoredType>
RegisterViewItem* SIMDSizedElementsContainer<StoredType>::child(int row)
{
	Q_ASSERT(unsigned(row)<elements.size());
	return elements[row].get();
}

template<class StoredType>
int SIMDSizedElementsContainer<StoredType>::childCount() const
{
	return elements.size();
}

template<class StoredType>
QVariant SIMDSizedElementsContainer<StoredType>::data(int column) const
{
	switch(column)
	{
	case Model::NAME_COLUMN: return this->name_;
	case Model::VALUE_COLUMN:
	{
		const auto width=elements[0]->valueMaxLength();
		QString str;
#if Q_BYTE_ORDER==Q_LITTLE_ENDIAN
		for(auto& elem : boost::adaptors::reverse(elements))
			str+=elem->data(column).toString().rightJustified(width+1);
#else
#error "This piece of code relies on little endian byte order"
#endif
		return str;
	}
	case Model::COMMENT_COLUMN: return {};
	default: return {};
	}
}

template<class StoredType>
QByteArray SIMDSizedElementsContainer<StoredType>::rawValue() const
{
	return static_cast<RegisterViewItem*>(this->parent())->rawValue();
}

template<class StoredType>
bool SIMDSizedElementsContainer<StoredType>::changed() const
{
	for(auto& elem : elements)
		if(elem->changed()) return true;
	return false;
}

// --------------------- SIMDRegister impl -------------------------

template<class StoredType>
SIMDRegister<StoredType>::SIMDRegister(
									QString const& name,
									std::vector<NumberDisplayMode> const& validFormats)
	: SimpleRegister<StoredType>(name)
{
	static const auto sizeNames=util::make_array(QObject::tr("bytes"),
												 QObject::tr("words"),
												 QObject::tr("dwords"),
												 QObject::tr("qwords"));
	// NOTE: If you change order, don't forget about enum SizesOrder and places where it's used
	for(unsigned shift=0;(1u<<shift)<=sizeof(std::uint64_t);++shift)
	{
		const auto size=1u<<shift;
		sizedElementContainers.emplace_back(sizeNames[shift],size,validFormats);
		sizedElementContainers.back().init(this,shift);
	}
}

template<class StoredType>
int SIMDRegister<StoredType>::childCount() const
{
	return sizedElementContainers.size();
}

template<class StoredType>
RegisterViewItem* SIMDRegister<StoredType>::child(int row)
{
	Q_ASSERT(unsigned(row)<sizedElementContainers.size());
	return &sizedElementContainers[row];
}


template<class StoredType>
SIMDCategory* SIMDRegister<StoredType>::category() const
{
	const auto cat=this->parent();
	return checked_cast<SIMDCategory>(cat);
}

template<class StoredType>
QVariant SIMDRegister<StoredType>::data(int column) const
{
	if(column!=Model::VALUE_COLUMN) return RegisterItem<StoredType>::data(column);
	const auto chosenSize=category()->chosenSize();
	switch(chosenSize)
	{
	case Model::ElementSize::BYTE:  return sizedElementContainers[Model::BYTES_ROW].data(column);
	case Model::ElementSize::WORD:  return sizedElementContainers[Model::WORDS_ROW].data(column);
	case Model::ElementSize::DWORD: return sizedElementContainers[Model::DWORDS_ROW].data(column);
	case Model::ElementSize::QWORD: return sizedElementContainers[Model::QWORDS_ROW].data(column);
	default: EDB_PRINT_AND_DIE("Unexpected chosenSize: ",chosenSize);
	}
}

template class SIMDRegister<edb::value64>;
template class SIMDRegister<edb::value128>;
template class SIMDRegister<edb::value256>;

// ----------------------------- FPURegister impl ---------------------------

template<class FloatType>
FPURegister<FloatType>::FPURegister(QString const& name)
	: SimpleRegister<FloatType>(name)
{
	formats.emplace_back(NumberDisplayMode::Hex);
	formats.back().init(this,Model::FPU_HEX_ROW);
	formats.emplace_back(NumberDisplayMode::Float);
	formats.back().init(this,Model::FPU_FLOAT_ROW);
}

template<class FloatType>
void FPURegister<FloatType>::update(FloatType const& newValue, QString const& newComment)
{
	SimpleRegister<FloatType>::update(newValue,newComment);
}

template<class FloatType>
void FPURegister<FloatType>::saveValue()
{
	SimpleRegister<FloatType>::saveValue();
}

template<class FloatType>
int FPURegister<FloatType>::childCount() const
{
	return formats.size();
}

template<class FloatType>
RegisterViewItem* FPURegister<FloatType>::child(int row)
{
	Q_ASSERT(unsigned(row)<formats.size());
	return &formats[row];
}

template<class FloatType>
FloatType FPURegister<FloatType>::value() const
{
	return this->value_;
}

template<class FloatType>
QString FPURegister<FloatType>::valueString() const
{
	if(!this->valid()) return "??";
	return toString(value(),category()->chosenFormat());
}

template<class FloatType>
FPUCategory* FPURegister<FloatType>::category() const
{
	const auto cat=this->parent();
	return checked_cast<FPUCategory>(cat);
}

template<class FloatType>
int FPURegister<FloatType>::valueMaxLength() const
{
	for(const auto& format : formats)
		if(format.format()==category()->chosenFormat())
			return format.valueMaxLength();
	return 0;
}


template class FPURegister<edb::value80>;

// ---------------------------- SIMDCategory impl ------------------------------
const QString settingsMainKey="RegisterViewModelBase";
const QString settingsFormatKey="format";
const QString settingsSIMDSizeKey="size";

SIMDCategory::SIMDCategory(QString const& name, int row,
							std::vector<NumberDisplayMode> const& validFormats)
	: Category(name,row),
	  validFormats_(validFormats)
{
	QSettings settings;
	settings.beginGroup(settingsMainKey+"/"+name);
	const auto defaultFormat=NumberDisplayMode::Hex;
	chosenFormat_=static_cast<NumberDisplayMode>(settings.value(settingsFormatKey,
												 static_cast<int>(defaultFormat)).toInt());
	if(!util::contains(validFormats,chosenFormat_))
		chosenFormat_=defaultFormat;

	chosenSize_=static_cast<Model::ElementSize>(settings.value(settingsSIMDSizeKey,
												static_cast<int>(Model::ElementSize::WORD)).toInt());
}

SIMDCategory::~SIMDCategory()
{
	QSettings settings;
	settings.beginGroup(settingsMainKey+"/"+name());
	// Simple guard against rewriting settings which didn't change between
	// categories with the same name (but e.g. different bitness)
	// FIXME: this won't work correctly in general if multiple categories changed settings
	if(formatChanged_)
		settings.setValue(settingsFormatKey,static_cast<int>(chosenFormat_));
	if(sizeChanged_)
		settings.setValue(settingsSIMDSizeKey,static_cast<int>(chosenSize_));
}

NumberDisplayMode SIMDCategory::chosenFormat() const
{
	return chosenFormat_;
}

Model::ElementSize SIMDCategory::chosenSize() const
{
	return chosenSize_;
}

void SIMDCategory::setChosenFormat(NumberDisplayMode newFormat)
{
	formatChanged_=true;
	chosenFormat_=newFormat;
}

void SIMDCategory::setChosenSize(Model::ElementSize newSize)
{
	sizeChanged_=true;
	chosenSize_=newSize;
}

std::vector<NumberDisplayMode>const& SIMDCategory::validFormats() const
{
	return validFormats_;
}

// ----------------------------- FPUCategory impl ---------------------------

FPUCategory::FPUCategory(QString const& name, int row)
	: Category(name,row)
{
	QSettings settings;
	settings.beginGroup(settingsMainKey+"/"+name);
	const auto defaultFormat=NumberDisplayMode::Float;
	chosenFormat_=static_cast<NumberDisplayMode>(settings.value(settingsFormatKey,
												 static_cast<int>(defaultFormat)).toInt());
	if(chosenFormat_!=NumberDisplayMode::Float && chosenFormat_!=NumberDisplayMode::Hex)
		chosenFormat_=defaultFormat;
}

FPUCategory::~FPUCategory()
{
	QSettings settings;
	settings.beginGroup(settingsMainKey+"/"+name());
	if(formatChanged_)
		settings.setValue(settingsFormatKey,static_cast<int>(chosenFormat_));
}

void FPUCategory::setChosenFormat(NumberDisplayMode newFormat)
{
	formatChanged_=true;
	chosenFormat_=newFormat;
}

NumberDisplayMode FPUCategory::chosenFormat() const
{
	return chosenFormat_;
}

// -----------------------------------------------

void Model::dataUpdateFinished()
{
	Q_EMIT dataChanged(index(0,1/*names don't change*/,QModelIndex()), index(rowCount()-1,NUM_COLS-1,QModelIndex()));
}

}
