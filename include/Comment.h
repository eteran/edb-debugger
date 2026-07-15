
#ifndef COMMENT_H_
#define COMMENT_H_

#include "Types.h"
#include "Module.h"

#include <QString>

#include <optional>

struct Comment {
	edb::address_t address;
	QString comment;
	std::optional<Module> module;
};

#endif
