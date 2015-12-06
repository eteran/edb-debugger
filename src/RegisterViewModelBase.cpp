#include "RegisterViewModelBase.h"
#include <QString>
#include <cstdint>
#include <algorithm>
#include "Types.h"
#include <QBrush>
#include "Util.h"
#include <QDebug>
#include <boost/range/adaptor/reversed.hpp>

namespace RegisterViewModelBase
{

RegisterViewItem* getItem(QModelIndex const& index)
{
	if(!index.isValid()) return nullptr;
	return static_cast<RegisterViewItem*>(index.internalPointer());
}

bool AbstractRegisterItem::valid()
{ return valueKnown_; }

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
	return QVariant();
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

Category* CategoriesHolder::insert(QString const& name)
{
	categories.emplace_back(util::make_unique<Category>(name,categories.size()));
	return categories.back().get();
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
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant Model::data(QModelIndex const& index, int role) const
{
	if(!index.isValid()) return QVariant();

	switch(role)
	{
	case Qt::DisplayRole:
		return getItem(index)->data(index.column());
	case Qt::ForegroundRole:
		{
			const auto reg=dynamic_cast<RegisterViewItem*>(getItem(index));
			if(!reg) return QVariant();
			if(!reg->changed()) return QVariant(); // default color for unchanged register
			switch(index.column())
			{
			case VALUE_COLUMN:
				return QVariant(QBrush(Qt::red)); // TODO: use user palette
			default:
				return QVariant(); // default color for other columns
			}
		}
	default:
		return QVariant();
	}
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
	return QVariant();
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
void RegisterItem<T>::invalidate()
{
	util::markMemory(&this->value_,sizeof this->value_);
	util::markMemory(&this->prevValue_,sizeof this->prevValue_);
	this->comment_.clear();
	this->valueKnown_=false;
	this->prevValueKnown_=false;
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
	return QVariant();
}

// -------------------- SimpleRegister impl -----------------------

template<typename T>
void SimpleRegister<T>::update(T const& value, QString const& comment)
{
	this->value_=value;
	this->comment_=comment;
	this->valueKnown_=true;
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
	: RegisterItem<UnderlyingType>(descr.name),
	  offset(descr.offset),
	  length(descr.length),
	  explanations(descr.explanations)
{
	Q_ASSERT(8*sizeof(UnderlyingType)>=length);
	Q_ASSERT(explanations.size()==0 || explanations.size()==2u<<(length-1));
}

template<typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::lengthToMask() const
{
	return 2*(1ull << (length-1))-1;
}

template<typename UnderlyingType>
void BitFieldItem<UnderlyingType>::update(UnderlyingType newValue)
{
	this->valueKnown_=true;
	this->value_=(newValue>>offset)&lengthToMask();
}

template<typename UnderlyingType>
QVariant BitFieldItem<UnderlyingType>::data(int column) const
{
	const auto datum=RegisterItem<UnderlyingType>::data(column);
	switch(column)
	{
	case Model::VALUE_COLUMN:
		{
		Q_ASSERT(datum.isValid());
		const auto str=datum.toString();
		Q_ASSERT(str.size()>0);
		return str.right(std::ceil(length/4.));
		}
	case Model::COMMENT_COLUMN:
		if(explanations.empty()) return datum;
		return this->valueKnown_ ? explanations[this->value_] : QString();
	}
	return datum;
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
void FlagsRegister<StoredType>::update(StoredType const& newValue, QString const& newComment)
{
	SimpleRegister<StoredType>::update(newValue,newComment);
	for(auto& field : fields)
		field.update(newValue);
}

template<typename StoredType>
void FlagsRegister<StoredType>::saveValue()
{
	SimpleRegister<StoredType>::saveValue();
	for(auto& field : fields)
		field.saveValue();
}

template<typename StoredType>
void FlagsRegister<StoredType>::invalidate()
{
	SimpleRegister<StoredType>::invalidate();
	for(auto& field : fields)
		field.invalidate();
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
QString SIMDFormatItem<StoredType,SizingType>::name() const
{
	switch(format)
	{
	case NumberDisplayMode::Hex: return "Hex";
	case NumberDisplayMode::Signed: return "Signed";
	case NumberDisplayMode::Unsigned: return "Unsigned";
	case NumberDisplayMode::Float: return "Float";
	}
	EDB_PRINT_AND_DIE("Unexpected format ",(long)format);
}

template<class StoredType, class SizingType>
SIMDFormatItem<StoredType,SizingType>::SIMDFormatItem(NumberDisplayMode format)
	: RegisterViewItem(name()),
	  format(format)
{
}

template<class StoredType, class SizingType>
bool SIMDFormatItem<StoredType,SizingType>::changed() const
{
	return parent()->changed();
}

template<class SizingType> typename std::enable_if<(sizeof(SizingType)>=sizeof(float)),
QString>::type toString(SizingType value,NumberDisplayMode format)
{
	return format==NumberDisplayMode::Float ? formatFloat(value) : util::formatInt(value,format);
}

template<class SizingType> typename std::enable_if<sizeof(SizingType)<sizeof(float),
QString>::type toString(SizingType value,NumberDisplayMode format)
{
	return format==NumberDisplayMode::Float ? "(too small element width for float)" : util::formatInt(value,format);
}

template<class StoredType, class SizingType>
QVariant SIMDFormatItem<StoredType,SizingType>::data(int column) const
{
	switch(column)
	{
	case Model::NAME_COLUMN: return this->name();
	case Model::VALUE_COLUMN:
		{
			const auto value=static_cast<SIMDSizedElement<StoredType,SizingType>*>(parent())->value();
			return toString(value,format);
		}
	case Model::COMMENT_COLUMN: return QVariant();
	}
	return QVariant();
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
	Q_ASSERT(dynamic_cast<SIMDRegister<StoredType>*>(this->parent()->parent()));
	return static_cast<SIMDRegister<StoredType>*>(this->parent()->parent());
}

template<class StoredType, class SizingType>
bool SIMDSizedElement<StoredType,SizingType>::valid() const
{
	return reg()->valueKnown_;
}

template<class StoredType, class SizingType>
QString SIMDSizedElement<StoredType,SizingType>::valueString() const
{
	return valid() ? value().toHexString() : "???";
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
	case Model::COMMENT_COLUMN: return QVariant();
	}
	return QVariant();
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
		QString str;
#if Q_BYTE_ORDER==Q_LITTLE_ENDIAN
		for(auto& elem : boost::adaptors::reverse(elements))
			str+=elem->data(column).toString()+" ";
#else
#error "This piece of code relies on little endian byte order"
#endif
		return str.trimmed();
	}
	case Model::COMMENT_COLUMN: return QVariant();
	default: return QVariant();
	}
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
									std::vector<NumberDisplayMode> validFormats)
	: SimpleRegister<StoredType>(name)
{
	static const auto sizeNames=util::make_array(QObject::tr("bytes"),
												 QObject::tr("words"),
												 QObject::tr("dwords"),
												 QObject::tr("qwords"));
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

template class SIMDRegister<edb::value64>;
template class SIMDRegister<edb::value128>;
template class SIMDRegister<edb::value256>;

// -----------------------------------------------

void Model::dataUpdateFinished()
{
	Q_EMIT dataChanged(index(0,1/*names don't change*/,QModelIndex()), index(rowCount(),NUM_COLS-1,QModelIndex()));
}

}
