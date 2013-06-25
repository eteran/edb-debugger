
#ifndef HEX_STRING_VALIDATOR_20130625_H_
#define HEX_STRING_VALIDATOR_20130625_H_

#include <QValidator>

class HexStringValidator : public QValidator {
public:
	HexStringValidator(QObject * parent);

public:
	virtual void fixup(QString &input) const;
	virtual State validate(QString &input, int &pos) const;
};

#endif
