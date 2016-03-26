#ifndef REGISTER_VIEW_MODEL_H_20151206
#define REGISTER_VIEW_MODEL_H_20151206

#include <QAbstractItemModel>
#include <memory>
#include <deque>
#include <vector>
#include "Util.h"
#include "Register.h"

Q_DECLARE_METATYPE(std::vector<NumberDisplayMode>)
Q_DECLARE_METATYPE(Register)

namespace RegisterViewModelBase
{

class RegisterViewItem;
class AbstractRegisterItem;
class CategoriesHolder;
class Category;
class SIMDCategory;
class FPUCategory;

// Sets register with name `name` to value `value`
// Returns whether it succeeded
// If succeeded, `resultingValue` is set to what the function got back after setting
// `resultingValue` can differ from `value` if e.g. the kernel doesn't allow to flip some
// bits of the register, like EFLAGS on x86.
template<typename T>
bool setDebuggeeRegister(QString const& name, T const& value, T& resultingValue);

class Model : public QAbstractItemModel
{
	Q_OBJECT
public:
	enum Column
	{
		NAME_COLUMN,
		VALUE_COLUMN,
		COMMENT_COLUMN,
		NUM_COLS
	};
	struct ElementSize
	{
		enum T
		{
			BYTE=1,
			WORD=2,
			DWORD=4,
			QWORD=8
		} value;
		ElementSize()=default;
		ElementSize(T v):value(v){}
		explicit ElementSize(int v):value(static_cast<T>(v)){}
		operator T() const {return value;}
	};
	enum SizesOrder
	{
		BYTES_ROW,
		WORDS_ROW,
		DWORDS_ROW,
		QWORDS_ROW
	};
	enum FormatsOrder
	{
		SIMD_HEX_ROW,
		SIMD_SIGNED_ROW,
		SIMD_UNSIGNED_ROW,
		SIMD_FLOAT_ROW
	};
	enum FPUFormatsOrder
	{
		FPU_HEX_ROW,
		FPU_FLOAT_ROW
	};
	enum Role
	{
		// true if changed, false otherwise
		// Property of: register's value column
		RegisterChangedRole=Qt::UserRole,
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

		FirstConcreteRole=Qt::UserRole+10000 // first role available for use in derived models
	};
	Model(QObject* parent=nullptr);
	int rowCount(QModelIndex const& parent=QModelIndex()) const override;
	int columnCount(QModelIndex const& parent=QModelIndex()) const override;
	QModelIndex parent(QModelIndex const& index) const override;
	QModelIndex index(int row, int column, QModelIndex const& parent=QModelIndex()) const override;
	QVariant data(QModelIndex const& index, int role=Qt::DisplayRole) const override;
	bool setData(QModelIndex const& index, QVariant const& data, int role=Qt::EditRole) override;
	Qt::ItemFlags flags(QModelIndex const& index) const override;
	~Model();

	void setActiveIndex(QModelIndex const& newActiveIndex);
	QModelIndex activeIndex() const;

	virtual void setChosenSIMDSize(QModelIndex const& index, ElementSize newSize);
	virtual void setChosenSIMDFormat(QModelIndex const& index, NumberDisplayMode newFormat);
	virtual void setChosenFPUFormat(QModelIndex const& index, NumberDisplayMode newFormat);

	// Should be called after updating all the data
	virtual void dataUpdateFinished();
	// should be called when the debugger is about to resume, to save current register values to previous
	virtual void saveValues();
protected:
	// All categories are there to stay after they've been inserted
	Category* addCategory(QString const& name);
	FPUCategory* addFPUCategory(QString const& name);
	SIMDCategory* addSIMDCategory(QString const& name,
								  std::vector<NumberDisplayMode> const& validFormats);
	void hide(Category* cat);
	void show(Category* cat);
	void hide(AbstractRegisterItem* reg);
	void show(AbstractRegisterItem* reg);
	void hideAll();
private:
	std::unique_ptr<CategoriesHolder> rootItem;
	QPersistentModelIndex activeIndex_;
Q_SIGNALS:
	void SIMDDisplayFormatChanged();
	void FPUDisplayFormatChanged();
};

class RegisterViewItem
{
protected:
	RegisterViewItem* parentItem=0;
	int row_=-1;
	QString name_;
public:
	RegisterViewItem(QString const& name) : name_(name) {}
	void init(RegisterViewItem* parent, int row);

	virtual RegisterViewItem* parent() const { return parentItem; }
	QString name() const { return name_; }
	virtual int row() const { Q_ASSERT(row_!=-1); return row_; }
	virtual bool changed() const { return false; };
	virtual ~RegisterViewItem(){}
	virtual RegisterViewItem* child(int /*row*/) { return nullptr; }
	virtual int childCount() const { return 0; }
	virtual QVariant data(int /*column*/) const { return QVariant(); }
	virtual int valueMaxLength() const { return 0; }
	virtual QByteArray rawValue() const = 0;
};

class AbstractRegisterItem : public RegisterViewItem
{
protected:
	AbstractRegisterItem(QString const& name) : RegisterViewItem(name) {}
public:
	// check whether it has some valid value (not unknown etc.)
	virtual bool valid() const = 0;
	// Should be used when EDB is about to resume execution of debuggee —
	// so that it's possible to check whether it changed on next stop
	virtual void saveValue() = 0;
	// clear all data, mark them unknown, both for current and previous states
	virtual void invalidate() = 0;
	virtual bool setValue(QString const& valueStr) = 0;
	virtual bool setValue(QByteArray const& value) = 0;
	virtual bool setValue(Register const& reg) = 0;
};

template<class StoredType>
class RegisterItem : public AbstractRegisterItem
{
protected:
	QString comment_;
	bool valueKnown_=false;
	bool prevValueKnown_=false;
	StoredType value_;
	StoredType prevValue_;

	virtual QString valueString() const;
public:
	RegisterItem(QString const& name);
	bool valid() const override;
	void saveValue() override;
	bool changed() const override;
	void invalidate() override;
	int childCount() const override;
	RegisterViewItem* child(int) override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;
	bool setValue(QString const& valueStr) override;
	bool setValue(QByteArray const& value) override;
	bool setValue(Register const& reg) override;
};

template<class StoredType>
class SimpleRegister : public RegisterItem<StoredType>
{
public:
	SimpleRegister(QString const& name) : RegisterItem<StoredType>(name) {}
	virtual void update(StoredType const& newValue, QString const& newComment);
	virtual int valueMaxLength() const override;
};

struct BitFieldDescription
{
	QString name;
	unsigned offset;
	unsigned length;
	std::vector<QString> explanations;
	// Prevent compiler warnings about missing initializer: make default argument explicitly default
	BitFieldDescription(QString name,unsigned offset,unsigned length,
						std::vector<QString> explanations=std::vector<QString>())
		: name(name),offset(offset),length(length),explanations(explanations) {}
};

class BitFieldProperties
{
public:
	virtual unsigned offset() const = 0;
	virtual unsigned length() const = 0;
};

template<class UnderlyingType>
class FlagsRegister;

template<class UnderlyingType>
class BitFieldItem : public RegisterViewItem, public BitFieldProperties
{
protected:
	unsigned offset_;
	unsigned length_;
	std::vector<QString> explanations;

	FlagsRegister<UnderlyingType>* reg() const;
	UnderlyingType lengthToMask() const;
	UnderlyingType calcValue(UnderlyingType regVal) const;
	UnderlyingType value() const;
	UnderlyingType prevValue() const;
public:
	BitFieldItem(BitFieldDescription const& descr);
	QVariant data(int column) const override;
	bool changed() const override;
	int valueMaxLength() const override;
	QByteArray rawValue() const override;

	unsigned offset() const override;
	unsigned length() const override;
};

template<class StoredType>
class FlagsRegister : public SimpleRegister<StoredType>
{
	void addField(std::unique_ptr<BitFieldItem<StoredType>> item);
public:
	FlagsRegister(QString const& name, std::vector<BitFieldDescription> const& bitFields);
	int childCount() const override;
	RegisterViewItem* child(int) override;
protected:
	std::vector<BitFieldItem<StoredType>> fields;

	template<class UnderlyingType>
	friend class BitFieldItem;
};

template<class StoredType>
class SIMDRegister;

template<class StoredType, class SizingType>
class SIMDFormatItem : public RegisterViewItem
{
	NumberDisplayMode format_;

	QString name(NumberDisplayMode format) const;
public:
	SIMDFormatItem(NumberDisplayMode format);
	NumberDisplayMode format() const;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;
	bool changed() const override;
	int valueMaxLength() const override;
	using RegisterViewItem::name;
};

class SIMDElement {}; // generic non-templated class to dynamic_cast to

template<class StoredType, class SizingType>
class SIMDSizedElement : public RegisterViewItem, public SIMDElement
{
	friend class SIMDFormatItem<StoredType,SizingType>;
	QString valueString() const;
	SizingType value() const;
	bool valid() const;
	SIMDRegister<StoredType>* reg() const;

	std::vector<SIMDFormatItem<StoredType,SizingType>> formats;
public:
	SIMDSizedElement(
			QString const& name,
			std::vector<NumberDisplayMode> const& validFormats);
	RegisterViewItem* child(int row) override;
	int childCount() const override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;
	bool changed() const override;
	int valueMaxLength() const override;
};

template<class StoredType>
class SIMDSizedElementsContainer : public RegisterViewItem
{
	template<class SizeType, class... Args>
	void addElement(Args&&... args);
protected:
	std::vector<std::unique_ptr<RegisterViewItem>> elements;
public:
	SIMDSizedElementsContainer(
			QString const& name,
			std::size_t size,
			std::vector<NumberDisplayMode> const& validFormats);
	SIMDSizedElementsContainer(SIMDSizedElementsContainer&& other);
	RegisterViewItem* child(int row) override;
	int childCount() const override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;
	bool changed() const override;
};

template<class StoredType>
class SIMDRegister : public SimpleRegister<StoredType>
{
	template<class U, class V>
	friend class SIMDSizedElement;
protected:
	std::deque<SIMDSizedElementsContainer<StoredType>> sizedElementContainers;

	SIMDCategory* category() const;
public:
	SIMDRegister(QString const& name, std::vector<NumberDisplayMode> const& validFormats);
	int childCount() const override;
	RegisterViewItem* child(int) override;
	QVariant data(int column) const override;

};

class GenericFPURegister {}; // generic non-templated class to dynamic_cast to

template<class FloatType>
class FPURegister : public SimpleRegister<FloatType>, public GenericFPURegister
{
	template<class U,class V>
	friend class SIMDFormatItem;
	FloatType value() const;
protected:
	std::vector<SIMDFormatItem<FloatType,FloatType>> formats;
	FPUCategory* category() const;

public:
	FPURegister(QString const& name);
	void saveValue() override;
	void update(FloatType const& newValue, QString const& newComment) override;
	int childCount() const override;
	RegisterViewItem* child(int) override;
	QString valueString() const override;
	int valueMaxLength() const override;
};

class Category : public RegisterViewItem
{
	std::vector<std::unique_ptr<AbstractRegisterItem>> registers;
	bool visible_=true;
public:
	Category(QString const& name, int row);
	Category(Category&& other);

	int childCount() const override;
	RegisterViewItem* child(int row) override;
	QVariant data(int column) const override;
	QByteArray rawValue() const override;

	void addRegister(std::unique_ptr<AbstractRegisterItem> reg);
	AbstractRegisterItem* getRegister(std::size_t i) const;

	void saveValues();

	void hide();
	void show();
	bool visible() const;
};

class SIMDCategory : public Category
{
	bool sizeChanged_=false;
	bool formatChanged_=false;
	Model::ElementSize chosenSize_;
	NumberDisplayMode chosenFormat_;
	std::vector<NumberDisplayMode> const validFormats_;
public:
	SIMDCategory(QString const& name, int row,
				 std::vector<NumberDisplayMode> const& validFormats);
	~SIMDCategory();
	virtual Model::ElementSize chosenSize() const;
	virtual NumberDisplayMode chosenFormat() const;
	virtual void setChosenSize(Model::ElementSize newSize);
	virtual void setChosenFormat(NumberDisplayMode newFormat);
	std::vector<NumberDisplayMode>const& validFormats() const;
};

class FPUCategory : public Category
{
	bool formatChanged_=false;
	NumberDisplayMode chosenFormat_;
public:
	FPUCategory(QString const& name, int row);
	~FPUCategory();
	NumberDisplayMode chosenFormat() const;
	void setChosenFormat(NumberDisplayMode newFormat);
};

class CategoriesHolder : public RegisterViewItem
{
	std::vector<std::unique_ptr<Category>> categories;
public:
	CategoriesHolder();
	template<typename CategoryType=Category>
	CategoryType* insert(QString const& name);
	SIMDCategory* insertSIMD(QString const& name,
				 			 std::vector<NumberDisplayMode> const& validFormats);
	int childCount() const override;
	RegisterViewItem* child(int row) override;
	QVariant data(int column) const;
	QByteArray rawValue() const override;

	friend class Model;
};

}

#endif
