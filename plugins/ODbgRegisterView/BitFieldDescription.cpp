
#include "BitFieldDescription.h"
#include <QString>

namespace ODbgRegisterView {

BitFieldDescription::BitFieldDescription(int textWidth, const std::vector<QString> &valueNames, const std::vector<QString> &setValueTexts, const std::function<bool(unsigned, unsigned)> &valueEqualComparator)
	: textWidth(textWidth), valueNames(valueNames), setValueTexts(setValueTexts), valueEqualComparator(valueEqualComparator) {
}

}
