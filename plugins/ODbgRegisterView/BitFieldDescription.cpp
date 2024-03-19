
#include "BitFieldDescription.h"
#include <QString>

namespace ODbgRegisterView {

BitFieldDescription::BitFieldDescription(int textWidth, std::vector<QString> valueNames, std::vector<QString> setValueTexts, std::function<bool(unsigned, unsigned)> valueEqualComparator)
	: textWidth(textWidth), valueNames(std::move(valueNames)), setValueTexts(std::move(setValueTexts)), valueEqualComparator(std::move(valueEqualComparator)) {
}

}
