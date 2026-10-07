
#ifndef COMMENT_H_
#define COMMENT_H_

#include "Module.h"
#include "Types.h"

#include <QString>

#include <optional>

struct Comment {
	QString comment;
	edb::address_t address;
	std::optional<Module> module;
};

#endif
