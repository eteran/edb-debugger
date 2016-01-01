#ifndef REGISTER_VIEW_MODEL_H_20151206
#define REGISTER_VIEW_MODEL_H_20151206

#include <QAbstractItemModel>
#include <memory>
#include <deque>
#include <vector>
#include "Util.h"

namespace RegisterViewModelBase
{

class RegisterViewItem;
class AbstractRegisterItem;
class CategoriesHolder;
class Category;
class Model : public QAbstractItemModel
{
public:
	enum Column
	{
		NAME_COLUMN,
		VALUE_COLUMN,
		COMMENT_COLUMN,
		NUM_COLS
	};
	enum Role
	{
		RegisterChangedRole=Qt::UserRole,
		FixedLengthRole // fixed length of text (name, value) or undefined (0) if it's not fixed (comment)
	};
	Model(QObject* parent=nullptr);
	int rowCount(QModelIndex const& parent=QModelIndex()) const override;
	int columnCount(QModelIndex const& parent=QModelIndex()) const override;
	QModelIndex parent(QModelIndex const& index) const override;
	QModelIndex index(int row, int column, QModelIndex const& parent=QModelIndex()) const override;
	QVariant data(QModelIndex const& index, int role) const override;
	Qt::ItemFlags flags(QModelIndex const& index) const override;
	~Model();

	// Should be called after updating all the data
	virtual void dataUpdateFinished();
	// should be called when the debugger is about to resume, to save current register values to previous
	virtual void saveValues();
protected:
	// All categories are there to stay after they've been inserted
	Category* addCategory(QString const& name);
	void hide(Category* cat);
	void show(Category* cat);
	void hide(AbstractRegisterItem* reg);
	void show(AbstractRegisterItem* reg);
	void hideAll();
private:
	std::unique_ptr<CategoriesHolder> rootItem;
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
};

class AbstractRegisterItem : public RegisterViewItem
{
protected:
	QString comment_;
	bool valueKnown_=false;
	bool prevValueKnown_=false;
	AbstractRegisterItem(QString const& name) : RegisterViewItem(name) {}
public:
	// check whether it has some valid value (not unknown etc.)
	virtual bool valid();
	// Should be used when EDB is about to resume execution of debuggee —
	// so that it's possible to check whether it changed on next stop
	virtual void saveValue() = 0;
	// clear all data, mark them unknown, both for current and previous states
	virtual void invalidate() = 0;
};

template<class StoredType>
class RegisterItem : public AbstractRegisterItem
{
protected:
	StoredType value_;
	StoredType prevValue_;

	virtual QString valueString() const;
public:
	RegisterItem(QString const& name) : AbstractRegisterItem(name) {}
	void saveValue() override;
	bool changed() const override;
	void invalidate() override;
	int childCount() const override;
	RegisterViewItem* child(int) override;
	QVariant data(int column) const override;
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

template<class UnderlyingType>
class BitFieldItem : public RegisterItem<UnderlyingType>
{
protected:
	unsigned offset;
	unsigned length;
	std::vector<QString> explanations;

	UnderlyingType lengthToMask() const;
public:
	BitFieldItem(BitFieldDescription const& descr);
	QVariant data(int column) const override;
	void update(UnderlyingType newValue);
};

template<class StoredType>
class FlagsRegister : public SimpleRegister<StoredType>
{
	void addField(std::unique_ptr<BitFieldItem<StoredType>> item);
public:
	FlagsRegister(QString const& name, std::vector<BitFieldDescription> const& bitFields);
	void update(StoredType const& newValue, QString const& newComment);
	void invalidate() override;
	void saveValue() override;
	int childCount() const override;
	RegisterViewItem* child(int) override;
protected:
	std::vector<BitFieldItem<StoredType>> fields;
};

template<class StoredType>
class SIMDRegister;

template<class StoredType, class SizingType>
class SIMDFormatItem : public RegisterViewItem
{
	NumberDisplayMode format;

	QString name() const;
	QString name(NumberDisplayMode format) const;
public:
	SIMDFormatItem(NumberDisplayMode format);
	QVariant data(int column) const override;
	bool changed() const override;
};

template<class StoredType, class SizingType>
class SIMDSizedElement : public RegisterViewItem
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
	bool changed() const override;
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
	bool changed() const override;
};

template<class StoredType>
class SIMDRegister : public SimpleRegister<StoredType>
{
	template<class U, class V>
	friend class SIMDSizedElement;
protected:
	std::deque<SIMDSizedElementsContainer<StoredType>> sizedElementContainers;
public:
	SIMDRegister(QString const& name, std::vector<NumberDisplayMode> validFormats);
	int childCount() const override;
	RegisterViewItem* child(int) override;
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

	void addRegister(std::unique_ptr<AbstractRegisterItem> reg);
	AbstractRegisterItem* getRegister(std::size_t i) const;

	void saveValues();

	void hide();
	void show();
	bool visible() const;
};

class CategoriesHolder : public RegisterViewItem
{
	std::vector<std::unique_ptr<Category>> categories;
public:
	CategoriesHolder();
	Category* insert(QString const& name);
	int childCount() const override;
	RegisterViewItem* child(int row) override;
	QVariant data(int column) const;

	friend class Model;
};

}

#endif