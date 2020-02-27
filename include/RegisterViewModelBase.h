
#ifndef REGISTER_VIEW_MODEL_BASE_H_20151206_
#define REGISTER_VIEW_MODEL_BASE_H_20151206_

#include "Register.h"
#include "util/Integer.h"
#include <QAbstractItemModel>
#include <deque>
#include <vector>

Q_DECLARE_METATYPE(std::vector<NumberDisplayMode>)
Q_DECLARE_METATYPE(Register)

namespace RegisterViewModelBase {

class RegisterViewItem;
class AbstractRegisterItem;
class CategoriesHolder;
class Category;
class SIMDCategory;
class FPUCategory;

class EDB_EXPORT Model : public QAbstractItemModel {
	Q_OBJECT

public:
	enum Column {
		NAME_COLUMN,
		VALUE_COLUMN,
		COMMENT_COLUMN,
		NUM_COLS
	};

	enum class ElementSize {
		BYTE  = 1,
		WORD  = 2,
		DWORD = 4,
		QWORD = 8,
	};

	enum SizesOrder {
		BYTES_ROW,
		WORDS_ROW,
		DWORDS_ROW,
		QWORDS_ROW
	};

	enum FormatsOrder {
		SIMD_HEX_ROW,
		SIMD_SIGNED_ROW,
		SIMD_UNSIGNED_ROW,
		SIMD_FLOAT_ROW
	};

	enum FPUFormatsOrder {
		FPU_HEX_ROW,
		FPU_FLOAT_ROW
	};

	enum Role {
		// true if changed, false otherwise
		// Property of: register's value column
		RegisterChangedRole = Qt::UserRole,
		// fixed length of text (name, value) or undefined (0) if it's not fixed (comment)
		// Type: int
		FixedLengthRole,
		// QByteArray with raw data
		// Property of: register's value column
		RawValueRole,
		// What user chose to be current size of SIMD element.
		// Type: ElementSize
		// Property of: Category
		ChosenSIMDSizeRole,
		// What user chose to be current format of SIMD element.
		// Type: NumberDisplayMode
		// Property of: Category
		ChosenSIMDFormatRole,
		// What row to take in given register index to get chosen-sized elements root
		// Type: int
		// Property of: Category
		ChosenSIMDSizeRowRole,
		// What row to take in given sized element to get chosen-formatted element
		// Type: int
		// Property of: Category
		ChosenSIMDFormatRowRole,
		// Which SIMD formats are valid to be set.
		// Type: std::vector<NumberDisplayMode>
		// Property of: Category
		ValidSIMDFormatsRole,
		// Whether the index corresponds to a normal register, i.e. not a bit field nor a SIMD element
		// Type: bool
		IsNormalRegisterRole,
		// Whether the index corresponds to a bit field of a normal register
		// Type: bool
		IsBitFieldRole,
		// Whether the index corresponds to an element of a SIMD register
		// Type: bool
		IsSIMDElementRole,
		// Whether the index corresponds to any-sized FPU register
		// Type: bool
		IsFPURegisterRole,
		// Offset of bit field in the register, starting from least significant bit
		// Type: int
		BitFieldOffsetRole,
		// Length of bit field in bits
		// Type: int
		BitFieldLengthRole,
		// Value as EDB's Register
		// Type: Register
		ValueAsRegisterRole,

		// What user chose to be current format of FPU registers
		// Type: NumberDisplayMode
		// Property of: Category
		ChosenFPUFormatRole,
		// What row to take in given register to get chosen-formatted value
		// Type: int
		// Property of: Category
		ChosenFPUFormatRowRole,

		FirstConcreteRole = Qt::UserRole + 10000 // first role available for use in derived models
	};

	Model(QObject *parent = nullptr);
	~Model() override = default;

public:
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex &index, const QVariant &data, int role = Qt::EditRole) override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

public:
	void setActiveIndex(const QModelIndex &newActiveIndex);
	QModelIndex activeIndex() const;

public:
	void setChosenSIMDSize(const QModelIndex &index, ElementSize newSize);
	void setChosenSIMDFormat(const QModelIndex &index, NumberDisplayMode newFormat);
	void setChosenFPUFormat(const QModelIndex &index, NumberDisplayMode newFormat);

	// Should be called after updating all the data
	void dataUpdateFinished();
	// should be called when the debugger is about to resume, to save current register values to previous
	void saveValues();

protected:
	// All categories are there to stay after they've been inserted
	Category *addCategory(const QString &name);
	FPUCategory *addFPUCategory(const QString &name);
	SIMDCategory *addSIMDCategory(const QString &name, const std::vector<NumberDisplayMode> &validFormats);
	void hide(Category *cat);
	void show(Category *cat);
	void hide(AbstractRegisterItem *reg);
	void show(AbstractRegisterItem *reg);
	void hideAll();

private:
	std::unique_ptr<CategoriesHolder> rootItem;
	QPersistentModelIndex activeIndex_;

Q_SIGNALS:
	void SIMDDisplayFormatChanged();
	void FPUDisplayFormatChanged();
};

inline std::ostream &operator<<(std::ostream &os, Model::ElementSize size) {
	os << static_cast<std::underlying_type<Model::ElementSize>::type>(size);
	return os;
}

class RegisterViewItem {
protected:
	RegisterViewItem *parentItem = nullptr;
	int row_                     = -1;
	QString name_;

public:
	RegisterViewItem(const QString &name)
		: name_(name) {
	}

	virtual ~RegisterViewItem() = default;

public:
	void init(RegisterViewItem *parent, int row);

	virtual RegisterViewItem *parent() const {
		return parentItem;
	}

	QString name() const {
		return name_;
	}

	virtual int row() const {
		Q_ASSERT(row_ != -1);
		return row_;
	}

	virtual bool changed() const {
		return false;
	}

	virtual RegisterViewItem *child(int /*row*/) {
		return nullptr;
	}

	virtual int childCount() const {
		return 0;
	}

	virtual QVariant data(int /*column*/) const {
		return QVariant();
	}

	virtual int valueMaxLength() const {
		return 0;
	}

public:
	virtual QByteArray rawValue() const = 0;
};

class AbstractRegisterItem : public RegisterViewItem {
protected:
	AbstractRegisterItem(const QString &name)
		: RegisterViewItem(name) {
	}

public:
	// check whether it has some valid value (not unknown etc.)
	virtual bool valid() const = 0;
	// Should be used when EDB is about to resume execution of debuggee —
	// so that it's possible to check whether it changed on next stop
	virtual void saveValue() = 0;
	// clear all data, mark them unknown, both for current and previous states
	virtual void invalidate()                      = 0;
	virtual bool setValue(const QString &valueStr) = 0;
	virtual bool setValue(const QByteArray &value) = 0;
	virtual bool setValue(const Register &reg)     = 0;
};

template <class StoredType>
class RegisterItem : public AbstractRegisterItem {
protected:
	QString comment_;
	StoredType value_;
	StoredType prevValue_;
	bool valueKnown_     = false;
	bool prevValueKnown_ = false;

protected:
	virtual QString valueString() const;

public:
	RegisterItem(const QString &name);
	bool valid() const override;
	void saveValue() override;
	bool changed() const override;
	void invalidate() override;
	int childCount() const override;
	RegisterViewItem *child(int) override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;
	bool setValue(const QString &valueStr) override;
	bool setValue(const QByteArray &value) override;
	bool setValue(const Register &reg) override;
};

template <class StoredType>
class SimpleRegister : public RegisterItem<StoredType> {
public:
	SimpleRegister(const QString &name)
		: RegisterItem<StoredType>(name) {
	}

public:
	virtual void update(const StoredType &newValue, const QString &newComment);
	int valueMaxLength() const override;
};

struct BitFieldDescriptionEx {
	QString name;
	unsigned offset;
	unsigned length;
	std::vector<QString> explanations;

	// Prevent compiler warnings about missing initializer: make default argument explicitly default
	BitFieldDescriptionEx(QString name, unsigned offset, unsigned length, std::vector<QString> explanations = std::vector<QString>())
		: name(name), offset(offset), length(length), explanations(explanations) {
	}
};

class BitFieldProperties {
public:
	virtual ~BitFieldProperties()   = default;
	virtual unsigned offset() const = 0;
	virtual unsigned length() const = 0;
};

template <class UnderlyingType>
class FlagsRegister;

template <class UnderlyingType>
class BitFieldItem final : public RegisterViewItem, public BitFieldProperties {
protected:
	unsigned offset_;
	unsigned length_;
	std::vector<QString> explanations;

protected:
	FlagsRegister<UnderlyingType> *reg() const;
	UnderlyingType lengthToMask() const;
	UnderlyingType calcValue(UnderlyingType regVal) const;
	UnderlyingType value() const;
	UnderlyingType prevValue() const;

public:
	BitFieldItem(const BitFieldDescriptionEx &descr);

public:
	QVariant data(int column) const override;
	bool changed() const override;
	int valueMaxLength() const override;
	QByteArray rawValue() const override;

	unsigned offset() const override;
	unsigned length() const override;
};

template <class StoredType>
class FlagsRegister final : public SimpleRegister<StoredType> {
	template <class UnderlyingType>
	friend class BitFieldItem;

public:
	FlagsRegister(const QString &name, const std::vector<BitFieldDescriptionEx> &bitFields);

public:
	RegisterViewItem *child(int) override;
	int childCount() const override;

protected:
	std::vector<BitFieldItem<StoredType>> fields;
};

template <class StoredType>
class SIMDRegister;

template <class StoredType, class SizingType>
class SIMDFormatItem final : public RegisterViewItem {
public:
	SIMDFormatItem(NumberDisplayMode format);

public:
	using RegisterViewItem::name;

public:
	NumberDisplayMode format() const;
	QByteArray rawValue() const override;
	QVariant data(int column) const override;
	bool changed() const override;
	int valueMaxLength() const override;

private:
	QString name(NumberDisplayMode format) const;

private:
	NumberDisplayMode format_;
};

class SIMDElement {}; // generic non-templated class to dynamic_cast to

template <class StoredType, class SizingType>
class SIMDSizedElement final : public RegisterViewItem, public SIMDElement {
	friend class SIMDFormatItem<StoredType, SizingType>;

public:
	SIMDSizedElement(const QString &name, const std::vector<NumberDisplayMode> &validFormats);

public:
	QByteArray rawValue() const override;
	QVariant data(int column) const override;
	RegisterViewItem *child(int row) override;
	bool changed() const override;
	int childCount() const override;
	int valueMaxLength() const override;

private:
	QString valueString() const;
	SIMDRegister<StoredType> *reg() const;
	SizingType value() const;
	bool valid() const;

private:
	std::vector<SIMDFormatItem<StoredType, SizingType>> formats;
};

template <class StoredType>
class SIMDSizedElementsContainer final : public RegisterViewItem {
	template <class SizeType, class... Args>
	void addElement(Args &&... args);

protected:
	std::vector<std::unique_ptr<RegisterViewItem>> elements;

public:
	SIMDSizedElementsContainer(const QString &name, std::size_t size, const std::vector<NumberDisplayMode> &validFormats);
	SIMDSizedElementsContainer(SIMDSizedElementsContainer &&other) noexcept;
	RegisterViewItem *child(int row) override;
	int childCount() const override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;
	bool changed() const override;
};

template <class StoredType>
class SIMDRegister final : public SimpleRegister<StoredType> {
	template <class U, class V>
	friend class SIMDSizedElement;

protected:
	std::deque<SIMDSizedElementsContainer<StoredType>> sizedElementContainers;

	SIMDCategory *category() const;

public:
	SIMDRegister(const QString &name, const std::vector<NumberDisplayMode> &validFormats);
	int childCount() const override;
	RegisterViewItem *child(int) override;
	QVariant data(int column) const override;
};

class GenericFPURegister {}; // generic non-templated class to dynamic_cast to

template <class FloatType>
class FPURegister final : public SimpleRegister<FloatType>, public GenericFPURegister {
	template <class U, class V>
	friend class SIMDFormatItem;

public:
	FPURegister(const QString &name);

public:
	QString valueString() const override;
	RegisterViewItem *child(int) override;
	int childCount() const override;
	int valueMaxLength() const override;
	void saveValue() override;
	void update(const FloatType &newValue, const QString &newComment) override;

protected:
	FPUCategory *category() const;

private:
	FloatType value() const;

protected:
	std::vector<SIMDFormatItem<FloatType, FloatType>> formats;
};

class Category : public RegisterViewItem {
public:
	Category(const QString &name, int row);
	Category(Category &&other) noexcept;

public:
	AbstractRegisterItem *getRegister(std::size_t i) const;
	QByteArray rawValue() const override;
	QVariant data(int column) const override;
	RegisterViewItem *child(int row) override;
	bool visible() const;
	int childCount() const override;
	void addRegister(std::unique_ptr<AbstractRegisterItem> reg);
	void hide();
	void saveValues();
	void show();

private:
	std::vector<std::unique_ptr<AbstractRegisterItem>> registers;
	bool visible_ = true;
};

class SIMDCategory final : public Category {
public:
	SIMDCategory(const QString &name, int row, const std::vector<NumberDisplayMode> &validFormats);
	~SIMDCategory();

public:
	virtual Model::ElementSize chosenSize() const;
	virtual NumberDisplayMode chosenFormat() const;
	virtual void setChosenSize(Model::ElementSize newSize);
	virtual void setChosenFormat(NumberDisplayMode newFormat);

public:
	const std::vector<NumberDisplayMode> &validFormats() const;

private:
	bool sizeChanged_   = false;
	bool formatChanged_ = false;
	Model::ElementSize chosenSize_;
	NumberDisplayMode chosenFormat_;
	std::vector<NumberDisplayMode> const validFormats_;
};

class FPUCategory final : public Category {
public:
	FPUCategory(const QString &name, int row);
	~FPUCategory();

public:
	NumberDisplayMode chosenFormat() const;
	void setChosenFormat(NumberDisplayMode newFormat);

private:
	bool formatChanged_ = false;
	NumberDisplayMode chosenFormat_;
};

class CategoriesHolder final : public RegisterViewItem {
	friend class Model;

public:
	CategoriesHolder();

public:
	template <typename CategoryType = Category>
	CategoryType *insert(const QString &name);

	SIMDCategory *insertSimd(const QString &name, const std::vector<NumberDisplayMode> &validFormats);
	int childCount() const override;
	RegisterViewItem *child(int row) override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;

private:
	std::vector<std::unique_ptr<Category>> categories;
};

}

#endif
