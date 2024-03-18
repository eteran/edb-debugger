
#ifndef REGISTER_VIEW_MODEL_BASE_H_20151206_
#define REGISTER_VIEW_MODEL_BASE_H_20151206_

#include "Register.h"
#include "util/Integer.h"
#include <QAbstractItemModel>
#include <deque>
#include <memory>
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
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
	[[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex &index, const QVariant &data, int role = Qt::EditRole) override;

public:
	void setActiveIndex(const QModelIndex &newActiveIndex);
	[[nodiscard]] QModelIndex activeIndex() const;

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

	[[nodiscard]] virtual RegisterViewItem *parent() const {
		return parentItem;
	}

	[[nodiscard]] QString name() const {
		return name_;
	}

	[[nodiscard]] virtual int row() const {
		Q_ASSERT(row_ != -1);
		return row_;
	}

	[[nodiscard]] virtual bool changed() const {
		return false;
	}

	[[nodiscard]] virtual RegisterViewItem *child(int /*row*/) {
		return nullptr;
	}

	[[nodiscard]] virtual int childCount() const {
		return 0;
	}

	[[nodiscard]] virtual QVariant data(int /*column*/) const {
		return QVariant();
	}

	[[nodiscard]] virtual int valueMaxLength() const {
		return 0;
	}

public:
	[[nodiscard]] virtual QByteArray rawValue() const = 0;
};

class AbstractRegisterItem : public RegisterViewItem {
protected:
	AbstractRegisterItem(const QString &name)
		: RegisterViewItem(name) {
	}

public:
	// check whether it has some valid value (not unknown etc.)
	[[nodiscard]] virtual bool valid() const = 0;
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
	[[nodiscard]] virtual QString valueString() const;

public:
	RegisterItem(const QString &name);

public:
	[[nodiscard]] bool changed() const override;
	[[nodiscard]] bool valid() const override;
	[[nodiscard]] int childCount() const override;
	[[nodiscard]] QByteArray rawValue() const override;
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] RegisterViewItem *child(int) override;
	bool setValue(const QByteArray &value) override;
	bool setValue(const QString &valueStr) override;
	bool setValue(const Register &reg) override;
	void invalidate() override;
	void saveValue() override;
};

template <class StoredType>
class SimpleRegister : public RegisterItem<StoredType> {
public:
	SimpleRegister(const QString &name)
		: RegisterItem<StoredType>(name) {
	}

public:
	virtual void update(const StoredType &newValue, const QString &newComment);
	[[nodiscard]] int valueMaxLength() const override;
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
	virtual ~BitFieldProperties()                 = default;
	[[nodiscard]] virtual unsigned offset() const = 0;
	[[nodiscard]] virtual unsigned length() const = 0;
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
	[[nodiscard]] FlagsRegister<UnderlyingType> *reg() const;
	[[nodiscard]] UnderlyingType lengthToMask() const;
	[[nodiscard]] UnderlyingType calcValue(UnderlyingType regVal) const;
	[[nodiscard]] UnderlyingType value() const;
	[[nodiscard]] UnderlyingType prevValue() const;

public:
	BitFieldItem(const BitFieldDescriptionEx &descr);

public:
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] bool changed() const override;
	[[nodiscard]] int valueMaxLength() const override;
	[[nodiscard]] QByteArray rawValue() const override;

	[[nodiscard]] unsigned offset() const override;
	[[nodiscard]] unsigned length() const override;
};

template <class StoredType>
class FlagsRegister final : public SimpleRegister<StoredType> {
	template <class UnderlyingType>
	friend class BitFieldItem;

public:
	FlagsRegister(const QString &name, const std::vector<BitFieldDescriptionEx> &bitFields);

public:
	[[nodiscard]] RegisterViewItem *child(int) override;
	[[nodiscard]] int childCount() const override;

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
	[[nodiscard]] NumberDisplayMode format() const;
	[[nodiscard]] QByteArray rawValue() const override;
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] bool changed() const override;
	[[nodiscard]] int valueMaxLength() const override;

private:
	[[nodiscard]] QString name(NumberDisplayMode format) const;

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
	[[nodiscard]] QByteArray rawValue() const override;
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] RegisterViewItem *child(int row) override;
	[[nodiscard]] bool changed() const override;
	[[nodiscard]] int childCount() const override;
	[[nodiscard]] int valueMaxLength() const override;

private:
	[[nodiscard]] QString valueString() const;
	[[nodiscard]] SIMDRegister<StoredType> *reg() const;
	[[nodiscard]] SizingType value() const;
	[[nodiscard]] bool valid() const;

private:
	std::vector<SIMDFormatItem<StoredType, SizingType>> formats;
};

template <class StoredType>
class SIMDSizedElementsContainer final : public RegisterViewItem {
	template <class SizeType, class... Args>
	void addElement(Args &&...args);

protected:
	std::vector<std::unique_ptr<RegisterViewItem>> elements;

public:
	SIMDSizedElementsContainer(const QString &name, std::size_t size, const std::vector<NumberDisplayMode> &validFormats);
	SIMDSizedElementsContainer(SIMDSizedElementsContainer &&other) noexcept;

public:
	[[nodiscard]] RegisterViewItem *child(int row) override;
	[[nodiscard]] int childCount() const override;
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] QByteArray rawValue() const override;
	[[nodiscard]] bool changed() const override;
};

template <class StoredType>
class SIMDRegister final : public SimpleRegister<StoredType> {
	template <class U, class V>
	friend class SIMDSizedElement;

protected:
	std::deque<SIMDSizedElementsContainer<StoredType>> sizedElementContainers;

	[[nodiscard]] SIMDCategory *category() const;

public:
	SIMDRegister(const QString &name, const std::vector<NumberDisplayMode> &validFormats);

public:
	[[nodiscard]] int childCount() const override;
	[[nodiscard]] RegisterViewItem *child(int) override;
	[[nodiscard]] QVariant data(int column) const override;
};

class GenericFPURegister {}; // generic non-templated class to dynamic_cast to

template <class FloatType>
class FPURegister final : public SimpleRegister<FloatType>, public GenericFPURegister {
	template <class U, class V>
	friend class SIMDFormatItem;

public:
	FPURegister(const QString &name);

public:
	[[nodiscard]] QString valueString() const override;
	[[nodiscard]] RegisterViewItem *child(int) override;
	[[nodiscard]] int childCount() const override;
	[[nodiscard]] int valueMaxLength() const override;
	void saveValue() override;
	void update(const FloatType &newValue, const QString &newComment) override;

protected:
	[[nodiscard]] FPUCategory *category() const;

private:
	[[nodiscard]] FloatType value() const;

protected:
	std::vector<SIMDFormatItem<FloatType, FloatType>> formats;
};

class Category : public RegisterViewItem {
public:
	Category(const QString &name, int row);
	Category(Category &&other) noexcept;

public:
	[[nodiscard]] AbstractRegisterItem *getRegister(std::size_t i) const;
	[[nodiscard]] QByteArray rawValue() const override;
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] RegisterViewItem *child(int row) override;
	[[nodiscard]] bool visible() const;
	[[nodiscard]] int childCount() const override;
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
	~SIMDCategory() override;

public:
	[[nodiscard]] virtual Model::ElementSize chosenSize() const;
	[[nodiscard]] virtual NumberDisplayMode chosenFormat() const;
	virtual void setChosenSize(Model::ElementSize newSize);
	virtual void setChosenFormat(NumberDisplayMode newFormat);

public:
	[[nodiscard]] const std::vector<NumberDisplayMode> &validFormats() const;

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
	~FPUCategory() override;

public:
	[[nodiscard]] NumberDisplayMode chosenFormat() const;
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

	[[nodiscard]] SIMDCategory *insertSimd(const QString &name, const std::vector<NumberDisplayMode> &validFormats);
	[[nodiscard]] int childCount() const override;
	[[nodiscard]] RegisterViewItem *child(int row) override;
	[[nodiscard]] QVariant data(int column) const override;
	[[nodiscard]] QByteArray rawValue() const override;

private:
	std::vector<std::unique_ptr<Category>> categories;
};

}

#endif
