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
#include "RegisterViewModelBase.h"
#include "FloatX.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IThread.h"
#include "State.h"
#include "Types.h"
#include "edb.h"
#include "util/Container.h"

#include <QBrush>
#include <QDebug>
#include <QSettings>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>

#include <boost/range/adaptor/reversed.hpp>

namespace RegisterViewModelBase {

namespace {

template <class T1, class T2>
T1 *checked_cast(T2 object) {
	Q_ASSERT(dynamic_cast<T1 *>(object));
	return static_cast<T1 *>(object);
}

RegisterViewItem *get_item(const QModelIndex &index) {
	if (!index.isValid()) {
		return nullptr;
	}

	return static_cast<RegisterViewItem *>(index.internalPointer());
}

QString toString(const edb::value80 &value, NumberDisplayMode format) {
	switch (format) {
	case NumberDisplayMode::Float:
		return format_float(value);
	case NumberDisplayMode::Hex:
		return value.toHexString();
	default:
		return QString("bug: format=%1").arg(static_cast<int>(format));
	}
}

// Sets register with name `name` to value `value`
// Returns whether it succeeded
// If succeeded, `resultingValue` is set to what the function got back after setting
// `resultingValue` can differ from `value` if e.g. the kernel doesn't allow to flip some
// bits of the register, like EFLAGS on x86.
template <typename T>
bool set_debugee_register(const QString &name, const T &value, T &resultingValue) {

	if (IDebugger *core = edb::v1::debugger_core) {

		IProcess *process = core->process();
		Q_ASSERT(process);

		State state;
		// read
		process->currentThread()->getState(&state);
		Register reg = state[name];

		if (!reg) {
			qWarning() << qPrintable(QString("Warning: failed to get register %1 (in function `%2`)").arg(name).arg(Q_FUNC_INFO));
			return false;
		}

		Q_ASSERT(reg.bitSize() == 8 * sizeof(T));

		const auto origValue = reg.value<T>();
		if (origValue == value) {
			return true; // do nothing if it's not different, it'll help us check result
		}

		// modify
		reg.setValueFrom(value);

		// write
		state.setRegister(reg);
		process->currentThread()->setState(state);

		// check
		process->currentThread()->getState(&state);
		const Register resultReg = state[name];

		if (!resultReg) {
			return false;
		}

		resultingValue = resultReg.value<T>();

		return resultingValue != origValue; // success if we changed it
	}

	return false;
}

}

// ----------------- RegisterViewItem impl ---------------------------

void RegisterViewItem::init(RegisterViewItem *parent, int row) {
	parentItem = parent;
	row_       = row;
}

// ---------------- CategoriesHolder impl ------------------------------

CategoriesHolder::CategoriesHolder()
	: RegisterViewItem("") {
}

int CategoriesHolder::childCount() const {

	return std::accumulate(categories.cbegin(), categories.cend(), 0, [](const int &psum, const std::unique_ptr<RegisterViewModelBase::Category> &cat) {
		return psum + cat->visible();
	});
}

QVariant CategoriesHolder::data(int /*column*/) const {
	return {};
}

QByteArray CategoriesHolder::rawValue() const {
	return {};
}

RegisterViewItem *CategoriesHolder::child(int visibleRow) {

	// return visible row #visibleRow
	for (std::size_t row = 0; row < categories.size(); ++row) {
		if (categories[row]->visible()) {
			--visibleRow;
		}

		if (visibleRow == -1) {
			return categories[row].get();
		}
	}

	return nullptr;
}

template <typename CategoryType>
CategoryType *CategoriesHolder::insert(const QString &name) {

	categories.emplace_back(std::make_unique<CategoryType>(name, categories.size()));
	return static_cast<CategoryType *>(categories.back().get());
}

SIMDCategory *CategoriesHolder::insertSimd(const QString &name, const std::vector<NumberDisplayMode> &validFormats) {
	auto cat = std::make_unique<SIMDCategory>(name, categories.size(), validFormats);
	auto ret = cat.get();
	categories.emplace_back(std::move(cat));
	return ret;
}

// ---------------- Model impl -------------------

Model::Model(QObject *parent)
	: QAbstractItemModel(parent), rootItem(new CategoriesHolder) {
}

void Model::setActiveIndex(const QModelIndex &newActiveIndex) {
	activeIndex_ = newActiveIndex;
}

QModelIndex Model::activeIndex() const {
	return activeIndex_;
}

QModelIndex Model::index(int row, int column, const QModelIndex &parent) const {

	if (!hasIndex(row, column, parent)) {
		return QModelIndex();
	}

	RegisterViewItem *parentItem;
	if (!parent.isValid()) {
		parentItem = rootItem.get();
	} else {
		parentItem = static_cast<RegisterViewItem *>(parent.internalPointer());
	}

	if (auto childItem = parentItem->child(row)) {
		return createIndex(row, column, childItem);
	} else {
		return QModelIndex();
	}
}

int Model::rowCount(const QModelIndex &parent) const {
	const RegisterViewItem *item = parent.isValid() ? get_item(parent) : rootItem.get();
	return item->childCount();
}

int Model::columnCount(const QModelIndex &) const {
	return 3; // all items reserve 3 columns: regName, value, comment
}

QModelIndex Model::parent(const QModelIndex &index) const {

	if (!index.isValid()) {
		return QModelIndex();
	}

	RegisterViewItem *parent = get_item(index)->parent();
	if (!parent || parent == rootItem.get()) {
		return QModelIndex();
	}

	return createIndex(parent->row(), 0, parent);
}

Qt::ItemFlags Model::flags(const QModelIndex &index) const {

	if (!index.isValid()) {
		return Qt::NoItemFlags;
	}

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | (index.column() == 1 ? Qt::ItemIsEditable : Qt::NoItemFlags);
}

QVariant Model::data(const QModelIndex &index, int role) const {

	const RegisterViewItem *item = get_item(index);
	if (!item) {
		return {};
	}

	switch (role) {
	case Qt::DisplayRole:
		return item->data(index.column());

	case Qt::ForegroundRole:
		if (index.column() != VALUE_COLUMN || !item->changed()) {
			return {}; // default color for unchanged register and for non-value column
		}
		return QBrush(Qt::red); // TODO: use user palette

	case RegisterChangedRole:
		return item->changed();

	case FixedLengthRole:
		if (index.column() == NAME_COLUMN) {
			return item->name().size();
		} else if (index.column() == VALUE_COLUMN) {
			return item->valueMaxLength();
		} else {
			return {};
		}

	case RawValueRole: {
		if (index.column() != VALUE_COLUMN) {
			return {};
		}

		const QByteArray ret = item->rawValue();
		if (ret.size()) {
			return ret;
		}

		return {};
	}
	case ChosenSIMDSizeRole:
		if (auto simdCat = dynamic_cast<const SIMDCategory *>(item)) {
			return static_cast<int>(simdCat->chosenSize());
		}

		return {};

	case ChosenSIMDFormatRole:
		if (auto simdCat = dynamic_cast<const SIMDCategory *>(item)) {
			return static_cast<int>(simdCat->chosenFormat());
		}

		return {};

	case ChosenSIMDSizeRowRole:
		if (auto simdCat = dynamic_cast<const SIMDCategory *>(item)) {
			switch (simdCat->chosenSize()) {
			case ElementSize::BYTE:
				return BYTES_ROW;
			case ElementSize::WORD:
				return WORDS_ROW;
			case ElementSize::DWORD:
				return DWORDS_ROW;
			case ElementSize::QWORD:
				return QWORDS_ROW;
			}
		}

		return {};

	case ChosenFPUFormatRole:
		if (auto fpuCat = dynamic_cast<const FPUCategory *>(item)) {
			return static_cast<int>(fpuCat->chosenFormat());
		}
		return {};

	case ChosenFPUFormatRowRole:
		if (auto fpuCat = dynamic_cast<const FPUCategory *>(item)) {
			switch (fpuCat->chosenFormat()) {
			case NumberDisplayMode::Float:
				return FPU_FLOAT_ROW;
			case NumberDisplayMode::Hex:
				return FPU_HEX_ROW;
			default:
				return {};
			}
		}
		return {};

	case ChosenSIMDFormatRowRole:
		if (auto simdCat = dynamic_cast<const SIMDCategory *>(item)) {
			switch (simdCat->chosenFormat()) {
			case NumberDisplayMode::Hex:
				return SIMD_HEX_ROW;
			case NumberDisplayMode::Signed:
				return SIMD_SIGNED_ROW;
			case NumberDisplayMode::Unsigned:
				return SIMD_UNSIGNED_ROW;
			case NumberDisplayMode::Float:
				return SIMD_FLOAT_ROW;
			}
		}
		return {};

	case ValidSIMDFormatsRole:
		if (auto simdCat = dynamic_cast<const SIMDCategory *>(item)) {
			return QVariant::fromValue(simdCat->validFormats());
		}
		return {};
	case IsNormalRegisterRole: {
		// Normal register is defined by its hierarchy level:
		// root(invalid index)->category->register
		// If level is not like this, it's not a normal register

		// parent should be category
		if (!index.parent().isValid()) {
			return false;
		}

		// parent of category is root, i.e. invalid index
		if (index.parent().parent().isValid()) {
			return false;
		}

		return true;
	}
	case IsFPURegisterRole:
		return !!dynamic_cast<const GenericFPURegister *>(item);

	case IsBitFieldRole:
		return !!dynamic_cast<const BitFieldProperties *>(item);

	case IsSIMDElementRole:
		return !!dynamic_cast<const SIMDElement *>(item);

	case BitFieldOffsetRole:
		if (auto bitField = dynamic_cast<const BitFieldProperties *>(item)) {
			return bitField->offset();
		}
		return {};

	case BitFieldLengthRole:
		if (auto bitField = dynamic_cast<const BitFieldProperties *>(item)) {
			return bitField->length();
		}
		return {};

	case ValueAsRegisterRole:
		if (IDebugger *core = edb::v1::debugger_core) {
			const auto name = index.sibling(index.row(), NAME_COLUMN).data().toString();
			if (name.isEmpty()) {
				return {};
			}

			State state;

			// read
			if (IProcess *process = core->process()) {
				process->currentThread()->getState(&state);
			}
			return QVariant::fromValue(state[name]);
		}
		break;

	default:
		return {};
	}
	return {};
}

bool Model::setData(const QModelIndex &index, const QVariant &data, int role) {
	RegisterViewItem *item       = get_item(index);
	const QModelIndex valueIndex = index.sibling(index.row(), VALUE_COLUMN);
	bool ok                      = false;

	switch (role) {
	case Qt::EditRole:
	case RawValueRole:
		if (this->data(index, IsNormalRegisterRole).toBool()) {
			auto reg = checked_cast<AbstractRegisterItem>(item);
			if (role == Qt::EditRole && data.type() == QVariant::String) {
				ok = reg->setValue(data.toString());
			} else if (data.type() == QVariant::ByteArray) {
				ok = reg->setValue(data.toByteArray());
			}
		}
		break;
	case ValueAsRegisterRole: {
		auto regItem = checked_cast<AbstractRegisterItem>(item);
		assert(data.isValid());
		assert(index.isValid());

		const auto name = index.sibling(index.row(), NAME_COLUMN).data().toString();
		Q_UNUSED(name)

		const auto regVal = data.value<Register>();
		assert(name.toLower() == regVal.name().toLower());

		ok = regItem->setValue(regVal);
		break;
	}
	}

	if (ok) {
		Q_EMIT dataChanged(valueIndex, valueIndex);
		if (rowCount(valueIndex)) {
			Q_EMIT dataChanged(this->index(0, VALUE_COLUMN, valueIndex),
							   this->index(rowCount(valueIndex), COMMENT_COLUMN, valueIndex));
		}
		return true;
	}

	return false;
}

void Model::setChosenSIMDSize(const QModelIndex &index, ElementSize const newSize) {
	const auto cat = get_item(index);
	Q_ASSERT(cat);

	const auto simdCat = dynamic_cast<SIMDCategory *>(cat);
	Q_ASSERT(simdCat);

	// Not very crash-worthy problem, just don't do anything in release mode
	if (!simdCat) {
		return;
	}

	simdCat->setChosenSize(newSize);
	Q_EMIT SIMDDisplayFormatChanged();

	// Make treeviews update root register items with default view
	const auto valueIndex = index.sibling(index.row(), VALUE_COLUMN);
	Q_EMIT dataChanged(valueIndex, valueIndex);
}

void Model::setChosenSIMDFormat(const QModelIndex &index, NumberDisplayMode const newFormat) {
	const auto cat = get_item(index);
	Q_ASSERT(cat);

	const auto simdCat = dynamic_cast<SIMDCategory *>(cat);
	Q_ASSERT(simdCat);

	// Not very crash-worthy problem, just don't do anything in release mode
	if (!simdCat) {
		return;
	}

	simdCat->setChosenFormat(newFormat);
	Q_EMIT SIMDDisplayFormatChanged();

	// Make treeviews update root register items with default view
	const auto valueIndex = index.sibling(index.row(), VALUE_COLUMN);
	Q_EMIT dataChanged(valueIndex, valueIndex);
}

void Model::setChosenFPUFormat(const QModelIndex &index, NumberDisplayMode const newFormat) {
	const auto cat = get_item(index);
	Q_ASSERT(cat);

	const auto fpuCat = dynamic_cast<FPUCategory *>(cat);
	Q_ASSERT(fpuCat);

	// Not very crash-worthy problem, just don't do anything in release mode
	if (!fpuCat) {
		return;
	}

	fpuCat->setChosenFormat(newFormat);
	Q_EMIT FPUDisplayFormatChanged();

	// Make treeviews update root register items with default view
	const auto valueIndex = index.sibling(index.row(), VALUE_COLUMN);
	Q_EMIT dataChanged(valueIndex, valueIndex);
}

void Model::hideAll() {
	for (const auto &cat : rootItem->categories) {
		cat->hide();
	}
}

Category *Model::addCategory(const QString &name) {
	return rootItem->insert(name);
}

SIMDCategory *Model::addSIMDCategory(const QString &name, const std::vector<NumberDisplayMode> &validFormats) {
	return rootItem->insertSimd(name, validFormats);
}

FPUCategory *Model::addFPUCategory(const QString &name) {
	return rootItem->insert<FPUCategory>(name);
}

void Model::hide(Category *cat) {
	cat->hide();
}

void Model::show(Category *cat) {
	cat->show();
}

void Model::saveValues() {
	for (const auto &cat : rootItem->categories) {
		cat->saveValues();
	}
}

// -------------------- Category impl --------------------

Category::Category(const QString &name, int row)
	: RegisterViewItem(name) {
	init(nullptr, row);
}

Category::Category(Category &&other) noexcept
	: RegisterViewItem(std::move(other.name_)) {
	parentItem = other.parentItem;
	row_       = other.row_;
	registers  = std::move(other.registers);
}

int Category::childCount() const {
	return registers.size();
}

RegisterViewItem *Category::child(int row) {
	if (row < 0 || row >= childCount()) {
		return nullptr;
	}

	return registers[row].get();
}

QVariant Category::data(int column) const {
	if (column == 0) {
		return name_;
	}

	return {};
}

QByteArray Category::rawValue() const {
	return {};
}

void Category::hide() {
	visible_ = false;
	for (const auto &reg : registers) {
		reg->invalidate();
	}
}

void Category::show() {
	visible_ = true;
}

bool Category::visible() const {
	return visible_;
}

void Category::addRegister(std::unique_ptr<AbstractRegisterItem> reg) {
	registers.emplace_back(std::move(reg));
	registers.back()->init(this, registers.size() - 1);
}

AbstractRegisterItem *Category::getRegister(std::size_t i) const {
	return registers[i].get();
}

void Category::saveValues() {
	for (auto &reg : registers)
		reg->saveValue();
}

// -------------------- RegisterItem impl ------------------------
template <typename T>
RegisterItem<T>::RegisterItem(const QString &name)
	: AbstractRegisterItem(name) {
	invalidate();
}

template <typename T>
void RegisterItem<T>::invalidate() {

	util::mark_memory(&value_, sizeof(value_));
	util::mark_memory(&prevValue_, sizeof(prevValue_));

	comment_.clear();
	valueKnown_     = false;
	prevValueKnown_ = false;
}

template <typename T>
bool RegisterItem<T>::valid() const {
	return valueKnown_;
}

template <typename T>
bool RegisterItem<T>::changed() const {
	return !valueKnown_ || !prevValueKnown_ || prevValue_ != value_;
}

template <typename T>
void RegisterItem<T>::saveValue() {
	prevValue_      = value_;
	prevValueKnown_ = valueKnown_;
}

template <typename T>
int RegisterItem<T>::childCount() const {
	return 0;
}

template <typename T>
RegisterViewItem *RegisterItem<T>::child(int) {
	return nullptr; // simple register item has no children
}

template <typename T>
QString RegisterItem<T>::valueString() const {
	if (!this->valueKnown_) {
		return "???";
	}

	return this->value_.toHexString();
}

template <typename T>
QVariant RegisterItem<T>::data(int column) const {
	switch (column) {
	case Model::NAME_COLUMN:
		return this->name_;
	case Model::VALUE_COLUMN:
		return valueString();
	case Model::COMMENT_COLUMN:
		return this->comment_;
	}
	return {};
}

template <typename T>
QByteArray RegisterItem<T>::rawValue() const {
	if (!this->valueKnown_) {
		return {};
	}
	return QByteArray(reinterpret_cast<const char *>(&this->value_), sizeof(this->value_));
}

template <typename T>
bool RegisterItem<T>::setValue(const Register &reg) {
	assert(reg.bitSize() == 8 * sizeof(T));
	return set_debugee_register<T>(reg.name(), reg.value<T>(), value_);
}

template <typename T>
bool RegisterItem<T>::setValue(const QByteArray &newValue) {
	T value;
	std::memcpy(&value, newValue.constData(), newValue.size());
	return set_debugee_register<T>(name(), value, value_);
}

template <typename T>
typename std::enable_if<(sizeof(T) > sizeof(std::uint64_t)), bool>::type setValue(T & /*valueToSet*/, const QString & /*name*/, const QString & /*valueStr*/) {
	qWarning() << "FIXME: unimplemented" << Q_FUNC_INFO;
	return false; // TODO: maybe do set?.. would be arch-dependent then due to endianness
}

template <typename T>
typename std::enable_if<sizeof(T) <= sizeof(std::uint64_t), bool>::type setValue(T &valueToSet, const QString &name, const QString &valueStr) {
	bool ok          = false;
	const auto value = T::fromHexString(valueStr, &ok);

	if (!ok) {
		return false;
	}

	return set_debugee_register(name, value, valueToSet);
}

template <typename T>
bool RegisterItem<T>::setValue(const QString &valueStr) {
	// TODO: ask ArchProcessor to actually set it and return true only if done
	return RegisterViewModelBase::setValue(value_, name(), valueStr);
}

template class RegisterItem<edb::value16>;
template class RegisterItem<edb::value32>;
template class RegisterItem<edb::value64>;
template class RegisterItem<edb::value80>;
template class RegisterItem<edb::value128>;
template class RegisterItem<edb::value256>;

// -------------------- SimpleRegister impl -----------------------

template <typename T>
void SimpleRegister<T>::update(const T &value, const QString &comment) {
	this->value_      = value;
	this->comment_    = comment;
	this->valueKnown_ = true;
}

template <typename T>
int SimpleRegister<T>::valueMaxLength() const {
	return 2 * sizeof(T);
}

template class SimpleRegister<edb::value16>;
template class SimpleRegister<edb::value32>;
template class SimpleRegister<edb::value64>;
template class SimpleRegister<edb::value80>;
template class SimpleRegister<edb::value128>;
template class SimpleRegister<edb::value256>;

// ---------------- BitFieldItem impl -----------------------

template <typename UnderlyingType>
BitFieldItem<UnderlyingType>::BitFieldItem(const BitFieldDescriptionEx &descr)
	: RegisterViewItem(descr.name), offset_(descr.offset), length_(descr.length), explanations(descr.explanations) {
	Q_ASSERT(8 * sizeof(UnderlyingType) >= length_);
	Q_ASSERT(explanations.size() == 0 || explanations.size() == 2u << (length_ - 1));
}

template <typename UnderlyingType>
FlagsRegister<UnderlyingType> *BitFieldItem<UnderlyingType>::reg() const {
	return checked_cast<FlagsRegister<UnderlyingType>>(this->parent());
}

template <typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::lengthToMask() const {
	return 2 * (1ull << (length_ - 1)) - 1;
}

template <typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::calcValue(UnderlyingType regVal) const {
	return (regVal >> offset_) & lengthToMask();
}

template <typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::value() const {
	return calcValue(reg()->value_);
}

template <typename UnderlyingType>
UnderlyingType BitFieldItem<UnderlyingType>::prevValue() const {
	return calcValue(reg()->prevValue_);
}

template <typename UnderlyingType>
int BitFieldItem<UnderlyingType>::valueMaxLength() const {
	return std::ceil(length_ / 4.); // number of nibbles
}

template <typename UnderlyingType>
bool BitFieldItem<UnderlyingType>::changed() const {
	return !reg()->valueKnown_ || !reg()->prevValueKnown_ || value() != prevValue();
}

template <typename UnderlyingType>
QVariant BitFieldItem<UnderlyingType>::data(int column) const {
	const auto str = reg()->valid() ? value().toHexString() : QString(sizeof(UnderlyingType) * 2, '?');
	switch (column) {
	case Model::NAME_COLUMN:
		return name();
	case Model::VALUE_COLUMN:
		Q_ASSERT(str.size() > 0);
		return str.right(std::ceil(length_ / 4.));
	case Model::COMMENT_COLUMN:
		if (explanations.empty()) {
			return {};
		}
		return reg()->valid() ? explanations[value()] : QString();
	}
	return {};
}

template <typename UnderlyingType>
QByteArray BitFieldItem<UnderlyingType>::rawValue() const {
	const auto val = value();
	return QByteArray(reinterpret_cast<const char *>(&val), sizeof(val));
}

template <typename UnderlyingType>
unsigned BitFieldItem<UnderlyingType>::length() const {
	return length_;
}

template <typename UnderlyingType>
unsigned BitFieldItem<UnderlyingType>::offset() const {
	return offset_;
}

// ---------------- FlagsRegister impl ------------------------

template <typename StoredType>
FlagsRegister<StoredType>::FlagsRegister(const QString &name, const std::vector<BitFieldDescriptionEx> &bitFields)
	: SimpleRegister<StoredType>(name) {

	for (auto &field : bitFields) {
		fields.emplace_back(field);
		fields.back().init(this, fields.size() - 1);
	}
}

template <typename StoredType>
int FlagsRegister<StoredType>::childCount() const {
	return fields.size();
}

template <typename StoredType>
RegisterViewItem *FlagsRegister<StoredType>::child(int row) {
	return &fields[row];
}

template class FlagsRegister<edb::value16>;
template class FlagsRegister<edb::value32>;
template class FlagsRegister<edb::value64>;

// --------------------- SIMDFormatItem impl -----------------------

template <class StoredType, class SizingType>
QString SIMDFormatItem<StoredType, SizingType>::name(NumberDisplayMode format) const {

	switch (format) {
	case NumberDisplayMode::Hex:
		return "hex";
	case NumberDisplayMode::Signed:
		return "signed";
	case NumberDisplayMode::Unsigned:
		return "unsigned";
	case NumberDisplayMode::Float:
		return "float";
	}
	Q_UNREACHABLE();
}

template <class StoredType, class SizingType>
SIMDFormatItem<StoredType, SizingType>::SIMDFormatItem(NumberDisplayMode format)
	: RegisterViewItem(name(format)), format_(format) {
}

template <class StoredType, class SizingType>
NumberDisplayMode SIMDFormatItem<StoredType, SizingType>::format() const {
	return format_;
}

template <class StoredType, class SizingType>
bool SIMDFormatItem<StoredType, SizingType>::changed() const {
	return parent()->changed();
}

template <class SizingType>
typename std::enable_if<(sizeof(SizingType) >= sizeof(float) && sizeof(SizingType) != sizeof(edb::value80)), QString>::type toString(SizingType value, NumberDisplayMode format) {
	return format == NumberDisplayMode::Float ? format_float(value) : util::format_int(value, format);
}

template <class SizingType>
typename std::enable_if<sizeof(SizingType) < sizeof(float), QString>::type toString(SizingType value, NumberDisplayMode format) {
	return format == NumberDisplayMode::Float ? "(too small element width for float)" : util::format_int(value, format);
}

template <class StoredType, class SizingType>
QVariant SIMDFormatItem<StoredType, SizingType>::data(int column) const {

	switch (column) {
	case Model::NAME_COLUMN:
		return this->name();

	case Model::VALUE_COLUMN:
		if (const auto parent = dynamic_cast<SIMDSizedElement<StoredType, SizingType> *>(this->parent())) {
			return parent->valid() ? toString(parent->value(), format_) : "???";
		}

		if (const auto parent = dynamic_cast<FPURegister<SizingType> *>(this->parent())) {
			return parent->valid() ? toString(parent->value(), format_) : "???";
		}

		EDB_PRINT_AND_DIE("failed to detect parent type");

	case Model::COMMENT_COLUMN:
		return {};
	}

	return {};
}

template <class StoredType, class SizingType>
QByteArray SIMDFormatItem<StoredType, SizingType>::rawValue() const {
	return this->parent()->rawValue();
}

template <>
int SIMDFormatItem<edb::value80, edb::value80>::valueMaxLength() const {

	// TODO(eteran): we need a sensible implementation for non-80-bit long double support
#ifndef _MSC_VER
	Q_ASSERT(sizeof(edb::value80) <= sizeof(long double));
#endif
	switch (format_) {
	case NumberDisplayMode::Hex:
		return 2 * sizeof(edb::value80);
	case NumberDisplayMode::Float:
		return max_printed_length<long double>();
	default:
		EDB_PRINT_AND_DIE("Unexpected format: ", static_cast<long>(format_));
	}
}

template <class StoredType, class SizingType>
int SIMDFormatItem<StoredType, SizingType>::valueMaxLength() const {

	using Unsigned = typename SizingType::InnerValueType;
	using Signed   = typename std::make_signed<Unsigned>::type;

	switch (format_) {
	case NumberDisplayMode::Hex:
		return 2 * sizeof(SizingType);
	case NumberDisplayMode::Signed:
		return max_printed_length<Signed>();
	case NumberDisplayMode::Unsigned:
		return max_printed_length<Unsigned>();
	case NumberDisplayMode::Float:
		switch (sizeof(SizingType)) {
		case sizeof(float):
			return max_printed_length<float>();
		case sizeof(double):
			return max_printed_length<double>();
		default:
			if (sizeof(SizingType) < sizeof(float)) {
				return 0;
			}
			EDB_PRINT_AND_DIE("Unexpected sizing type's size for format float: ", sizeof(SizingType));
		}
	}

	Q_UNREACHABLE();
}

// --------------------- SIMDSizedElement  impl -------------------------

template <class StoredType, class SizingType>
SIMDSizedElement<StoredType, SizingType>::SIMDSizedElement(const QString &name, const std::vector<NumberDisplayMode> &validFormats)
	: RegisterViewItem(name) {

	for (const auto format : validFormats) {
		if (format != NumberDisplayMode::Float || sizeof(SizingType) >= sizeof(float)) {
			// The order must be as expected by other functions
			Q_ASSERT(format != NumberDisplayMode::Float || formats.size() == Model::SIMD_FLOAT_ROW);
			Q_ASSERT(format != NumberDisplayMode::Hex || formats.size() == Model::SIMD_HEX_ROW);
			Q_ASSERT(format != NumberDisplayMode::Signed || formats.size() == Model::SIMD_SIGNED_ROW);
			Q_ASSERT(format != NumberDisplayMode::Unsigned || formats.size() == Model::SIMD_UNSIGNED_ROW);

			formats.emplace_back(format);
			formats.back().init(this, formats.size() - 1);
		}
	}
}

template <class StoredType, class SizingType>
RegisterViewItem *SIMDSizedElement<StoredType, SizingType>::child(int row) {
	return &formats[row];
}

template <class StoredType, class SizingType>
int SIMDSizedElement<StoredType, SizingType>::childCount() const {
	return formats.size();
}

template <class StoredType, class SizingType>
SIMDRegister<StoredType> *SIMDSizedElement<StoredType, SizingType>::reg() const {
	return checked_cast<SIMDRegister<StoredType>>(this->parent()->parent());
}

template <class StoredType, class SizingType>
bool SIMDSizedElement<StoredType, SizingType>::valid() const {
	return reg()->valueKnown_;
}

template <class StoredType, class SizingType>
QString SIMDSizedElement<StoredType, SizingType>::valueString() const {
	if (!valid()) {
		return "??";
	}
	return toString(value(), reg()->category()->chosenFormat());
}

template <class StoredType, class SizingType>
int SIMDSizedElement<StoredType, SizingType>::valueMaxLength() const {
	for (const auto &format : formats) {
		if (format.format() == reg()->category()->chosenFormat()) {
			return format.valueMaxLength();
		}
	}
	return 0;
}

template <class StoredType, class SizingType>
SizingType SIMDSizedElement<StoredType, SizingType>::value() const {
	std::size_t offset = this->row() * sizeof(SizingType);
	return SizingType(reg()->value_, offset);
}

template <class StoredType, class SizingType>
QVariant SIMDSizedElement<StoredType, SizingType>::data(int column) const {
	switch (column) {
	case Model::NAME_COLUMN:
		return this->name_;
	case Model::VALUE_COLUMN:
		return valueString();
	case Model::COMMENT_COLUMN:
		return {};
	}
	return {};
}

template <class StoredType, class SizingType>
QByteArray SIMDSizedElement<StoredType, SizingType>::rawValue() const {
	const auto value = this->value();
	return QByteArray(reinterpret_cast<const char *>(&value), sizeof(value));
}

template <class StoredType, class SizingType>
bool SIMDSizedElement<StoredType, SizingType>::changed() const {
	const auto reg           = this->reg();
	const std::size_t offset = this->row() * sizeof(SizingType);

	return !reg->valueKnown_ || !reg->prevValueKnown_ || SizingType(reg->prevValue_, offset) != SizingType(reg->value_, offset);
}

// --------------------- SIMDSizedElementsContainer impl -------------------------

template <class StoredType>
template <class SizeType, class... Args>
void SIMDSizedElementsContainer<StoredType>::addElement(Args &&... args) {
	using Element = SIMDSizedElement<StoredType, SizeType>;

	elements.emplace_back(std::make_unique<Element>(std::forward<Args>(args)...));
}

template <class StoredType>
SIMDSizedElementsContainer<StoredType>::SIMDSizedElementsContainer(const QString &name, std::size_t size, const std::vector<NumberDisplayMode> &validFormats)
	: RegisterViewItem(name) {

	for (unsigned elemN = 0; elemN < sizeof(StoredType) / size; ++elemN) {
		const auto name = QString("#%1").arg(elemN);
		switch (size) {
		case sizeof(edb::value8):
			addElement<edb::value8>(name, validFormats);
			break;
		case sizeof(edb::value16):
			addElement<edb::value16>(name, validFormats);
			break;
		case sizeof(edb::value32):
			addElement<edb::value32>(name, validFormats);
			break;
		case sizeof(edb::value64):
			addElement<edb::value64>(name, validFormats);
			break;
		default:
			EDB_PRINT_AND_DIE("Unexpected element size ", static_cast<long>(size));
		}
		elements.back()->init(this, elemN);
	}
}

template <class StoredType>
SIMDSizedElementsContainer<StoredType>::SIMDSizedElementsContainer(SIMDSizedElementsContainer &&other) noexcept
	: RegisterViewItem(other), elements(std::move(other.elements)) {
}

template <class StoredType>
RegisterViewItem *SIMDSizedElementsContainer<StoredType>::child(int row) {
	Q_ASSERT(unsigned(row) < elements.size());
	return elements[row].get();
}

template <class StoredType>
int SIMDSizedElementsContainer<StoredType>::childCount() const {
	return elements.size();
}

template <class StoredType>
QVariant SIMDSizedElementsContainer<StoredType>::data(int column) const {
	switch (column) {
	case Model::NAME_COLUMN:
		return this->name_;
	case Model::VALUE_COLUMN: {
		const auto width = elements[0]->valueMaxLength();
		QString str;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		for (auto &elem : boost::adaptors::reverse(elements)) {
			str += elem->data(column).toString().rightJustified(width + 1);
		}
#else
#error "This piece of code relies on little endian byte order"
#endif
		return str;
	}
	case Model::COMMENT_COLUMN:
		return {};
	default:
		return {};
	}
}

template <class StoredType>
QByteArray SIMDSizedElementsContainer<StoredType>::rawValue() const {
	return this->parent()->rawValue();
}

template <class StoredType>
bool SIMDSizedElementsContainer<StoredType>::changed() const {
	for (auto &elem : elements) {
		if (elem->changed()) {
			return true;
		}
	}

	return false;
}

// --------------------- SIMDRegister impl -------------------------

template <class StoredType>
SIMDRegister<StoredType>::SIMDRegister(const QString &name, const std::vector<NumberDisplayMode> &validFormats)
	: SimpleRegister<StoredType>(name) {

	static const auto sizeNames = util::make_array(
		QObject::tr("bytes"),
		QObject::tr("words"),
		QObject::tr("dwords"),
		QObject::tr("qwords"));

	// NOTE: If you change order, don't forget about enum SizesOrder and places where it's used
	for (unsigned shift = 0; (1u << shift) <= sizeof(std::uint64_t); ++shift) {
		const auto size = 1u << shift;
		sizedElementContainers.emplace_back(sizeNames[shift], size, validFormats);
		sizedElementContainers.back().init(this, shift);
	}
}

template <class StoredType>
int SIMDRegister<StoredType>::childCount() const {
	return sizedElementContainers.size();
}

template <class StoredType>
RegisterViewItem *SIMDRegister<StoredType>::child(int row) {
	Q_ASSERT(static_cast<size_t>(row) < sizedElementContainers.size());
	return &sizedElementContainers[row];
}

template <class StoredType>
SIMDCategory *SIMDRegister<StoredType>::category() const {
	const auto cat = this->parent();
	return checked_cast<SIMDCategory>(cat);
}

template <class StoredType>
QVariant SIMDRegister<StoredType>::data(int column) const {

	if (column != Model::VALUE_COLUMN) {
		return RegisterItem<StoredType>::data(column);
	}

	const auto chosenSize = category()->chosenSize();
	switch (chosenSize) {
	case Model::ElementSize::BYTE:
		return sizedElementContainers[Model::BYTES_ROW].data(column);
	case Model::ElementSize::WORD:
		return sizedElementContainers[Model::WORDS_ROW].data(column);
	case Model::ElementSize::DWORD:
		return sizedElementContainers[Model::DWORDS_ROW].data(column);
	case Model::ElementSize::QWORD:
		return sizedElementContainers[Model::QWORDS_ROW].data(column);
	}
	Q_UNREACHABLE();
}

template class SIMDRegister<edb::value64>;
template class SIMDRegister<edb::value128>;
template class SIMDRegister<edb::value256>;

// ----------------------------- FPURegister impl ---------------------------

template <class FloatType>
FPURegister<FloatType>::FPURegister(const QString &name)
	: SimpleRegister<FloatType>(name) {
	formats.emplace_back(NumberDisplayMode::Hex);
	formats.back().init(this, Model::FPU_HEX_ROW);
	formats.emplace_back(NumberDisplayMode::Float);
	formats.back().init(this, Model::FPU_FLOAT_ROW);
}

template <class FloatType>
void FPURegister<FloatType>::update(const FloatType &newValue, const QString &newComment) {
	SimpleRegister<FloatType>::update(newValue, newComment);
}

template <class FloatType>
void FPURegister<FloatType>::saveValue() {
	SimpleRegister<FloatType>::saveValue();
}

template <class FloatType>
int FPURegister<FloatType>::childCount() const {
	return formats.size();
}

template <class FloatType>
RegisterViewItem *FPURegister<FloatType>::child(int row) {
	Q_ASSERT(static_cast<size_t>(row) < formats.size());
	return &formats[row];
}

template <class FloatType>
FloatType FPURegister<FloatType>::value() const {
	return this->value_;
}

template <class FloatType>
QString FPURegister<FloatType>::valueString() const {
	if (!this->valid()) {
		return "??";
	}
	return toString(value(), category()->chosenFormat());
}

template <class FloatType>
FPUCategory *FPURegister<FloatType>::category() const {
	const auto cat = this->parent();
	return checked_cast<FPUCategory>(cat);
}

template <class FloatType>
int FPURegister<FloatType>::valueMaxLength() const {
	for (const auto &format : formats) {
		if (format.format() == category()->chosenFormat()) {
			return format.valueMaxLength();
		}
	}
	return 0;
}

template class FPURegister<edb::value80>;

// ---------------------------- SIMDCategory impl ------------------------------
const auto settingsMainKey     = QLatin1String("RegisterViewModelBase");
const auto settingsFormatKey   = QLatin1String("format");
const auto settingsSIMDSizeKey = QLatin1String("size");

SIMDCategory::SIMDCategory(const QString &name, int row, const std::vector<NumberDisplayMode> &validFormats)
	: Category(name, row), validFormats_(validFormats) {

	QSettings settings;
	settings.beginGroup(settingsMainKey + "/" + name);

	const auto defaultFormat = NumberDisplayMode::Hex;
	chosenFormat_            = static_cast<NumberDisplayMode>(settings.value(settingsFormatKey, static_cast<int>(defaultFormat)).toInt());

	if (!util::contains(validFormats, chosenFormat_)) {
		chosenFormat_ = defaultFormat;
	}

	chosenSize_ = static_cast<Model::ElementSize>(settings.value(settingsSIMDSizeKey, static_cast<int>(Model::ElementSize::WORD)).toInt());
}

SIMDCategory::~SIMDCategory() {

	QSettings settings;
	settings.beginGroup(settingsMainKey + "/" + name());

	// Simple guard against rewriting settings which didn't change between
	// categories with the same name (but e.g. different bitness)
	// FIXME: this won't work correctly in general if multiple categories changed settings
	if (formatChanged_) {
		settings.setValue(settingsFormatKey, static_cast<int>(chosenFormat_));
	}

	if (sizeChanged_) {
		settings.setValue(settingsSIMDSizeKey, static_cast<int>(chosenSize_));
	}
}

NumberDisplayMode SIMDCategory::chosenFormat() const {
	return chosenFormat_;
}

Model::ElementSize SIMDCategory::chosenSize() const {
	return chosenSize_;
}

void SIMDCategory::setChosenFormat(NumberDisplayMode newFormat) {
	formatChanged_ = true;
	chosenFormat_  = newFormat;
}

void SIMDCategory::setChosenSize(Model::ElementSize newSize) {
	sizeChanged_ = true;
	chosenSize_  = newSize;
}

const std::vector<NumberDisplayMode> &SIMDCategory::validFormats() const {
	return validFormats_;
}

// ----------------------------- FPUCategory impl ---------------------------

FPUCategory::FPUCategory(const QString &name, int row)
	: Category(name, row) {

	QSettings settings;
	settings.beginGroup(settingsMainKey + "/" + name);
	const auto defaultFormat = NumberDisplayMode::Float;

	chosenFormat_ = static_cast<NumberDisplayMode>(settings.value(settingsFormatKey, static_cast<int>(defaultFormat)).toInt());

	if (chosenFormat_ != NumberDisplayMode::Float && chosenFormat_ != NumberDisplayMode::Hex) {
		chosenFormat_ = defaultFormat;
	}
}

FPUCategory::~FPUCategory() {
	QSettings settings;
	settings.beginGroup(settingsMainKey + "/" + name());
	if (formatChanged_) {
		settings.setValue(settingsFormatKey, static_cast<int>(chosenFormat_));
	}
}

void FPUCategory::setChosenFormat(NumberDisplayMode newFormat) {
	formatChanged_ = true;
	chosenFormat_  = newFormat;
}

NumberDisplayMode FPUCategory::chosenFormat() const {
	return chosenFormat_;
}

// -----------------------------------------------

void Model::dataUpdateFinished() {
	Q_EMIT dataChanged(
		index(0, 1 /*names don't change*/, QModelIndex()),
		index(rowCount() - 1, NUM_COLS - 1, QModelIndex()));
}

}
