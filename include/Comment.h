
#ifndef COMMENT_H_
#define COMMENT_H_

#include "Types.h"
#include "Module.h"

#include <QString>

#include <optional>

struct Comment {
	QString comment;
	edb::address_t address;
	std::optional<Module> module;
};

#endif
