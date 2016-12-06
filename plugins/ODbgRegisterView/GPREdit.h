#include <QLineEdit>

class GPREdit : public QLineEdit
{
public:
	enum class Format
	{
		Hex,
		Signed,
		Unsigned,
		Character
	};
public:
	GPREdit(std::size_t offsetInInteger, std::size_t integerSize, Format format, QWidget* parent=nullptr);
	void setGPRValue(std::uint64_t gprValue);
	void updateGPRValue(std::uint64_t& gpr) const;

	QSize minimumSizeHint() const override
	{ return sizeHint(); }
	QSize sizeHint() const override;
private:
	void setupFormat(Format newFormat);
	int naturalWidthInChars;
	std::size_t integerSize;
	std::size_t offsetInInteger;
	Format format;
	std::uint64_t signBit;
};
