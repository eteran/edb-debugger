/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FunctionDBPlugin.h"
#include "FunctionDB.h"
#include "Debugger.h"
#include <QtDebug>

namespace {
	const Function funcs[] = {

		// ctype functions
		{ "isalnum", FunctionInfo('i') },
		{ "isalpha", FunctionInfo('i') },
		{ "iscntrl", FunctionInfo('i') },
		{ "isdigit", FunctionInfo('i') },
		{ "isgraph", FunctionInfo('i') },
		{ "islower", FunctionInfo('i') },
		{ "isprint", FunctionInfo('i') },
		{ "ispunct", FunctionInfo('i') },
		{ "isspace", FunctionInfo('i') },
		{ "isupper", FunctionInfo('i') },
		{ "isxdigit", FunctionInfo('i') },
		{ "tolower", FunctionInfo('i') },
		{ "toupper", FunctionInfo('i') },

		// errno functions
		{ "__errno_location", FunctionInfo() },

		// setjmp functions
		{ "setjmp", FunctionInfo('p') },
		{ "longjmp", FunctionInfo('p', 'i') },

		// signal functions
		{ "signal", FunctionInfo('i', 'p') },
		{ "raise", FunctionInfo('i') },

		// stdio functions
		{ "fputc", FunctionInfo('c', 'p') },
		{ "putchar", FunctionInfo('c') },
		{ "printf", FunctionInfo('f') },
		{ "fputs", FunctionInfo('s', 'p') },
		{ "puts", FunctionInfo('s') },
		{ "fopen", FunctionInfo('s', 'p') },
		{ "fclose", FunctionInfo('p') },
		{ "fwrite", FunctionInfo('p', 'u', 'u', 'p') },
		{ "fread", FunctionInfo('p', 'u', 'u', 'p') },
		{ "remove", FunctionInfo('s') },
		{ "sprintf", FunctionInfo('p', 's') },
		{ "gets", FunctionInfo('p') },
		{ "getchar", FunctionInfo() },
		{ "putc", FunctionInfo('i', 'p') },
		{ "clearerr", FunctionInfo('p') },
		{ "feof", FunctionInfo('p') },
		{ "ferror", FunctionInfo('p') },
		// vsprintf
		// vfprintf
		// fprintf
		// setbuf
		// setvbuf
		// setlinebuf
		// setbuffer
		// perror
		// fflush
		// fseek
		// rewind
		// fgets
		// ftell
		// tmpnam
		// tmpfile
		// snprintf
		// scanf
		// fscanf
		// sscanf
		// vscanf
		// vfscanf
		// vsscanf
		// fsetpos
		// fgetpos
		// fileno
		// freopen
		// ungetc
		// rename

		// stdlib functions
		{ "malloc", FunctionInfo('u') },
		{ "free", FunctionInfo('p') },
		{ "realloc", FunctionInfo('p', 'u') },
		{ "calloc", FunctionInfo('u', 'u') },
		{ "getenv", FunctionInfo('s') },
		{ "exit", FunctionInfo('i') },
		{ "system", FunctionInfo('s') },
		{ "abort", FunctionInfo() },
		{ "abs", FunctionInfo('i') },
		{ "labs", FunctionInfo('i') },
		{ "qsort", FunctionInfo('p', 'u', 'u', 'p') },
		{ "bsearch", FunctionInfo('p', 'p', 'u', 'u', 'p') },
		{ "srand", FunctionInfo('u') },
		{ "rand", FunctionInfo() },
		{ "div", FunctionInfo('i', 'i') },
		{ "ldiv", FunctionInfo('i', 'i') },
		{ "atoi", FunctionInfo('s') },
		{ "atol", FunctionInfo('s') },
		{ "atof", FunctionInfo('s') },
		{ "atexit", FunctionInfo('p') },
		// mbtowc
		// mblen
		// strtoul
		// strtol
		// strtod
		// strtof
		// mbstowcs
		// wcstombs
		// wctomb

		// string functions
		{ "memchr", FunctionInfo('p', 'c', 'u') },
		{ "memcmp", FunctionInfo('p', 'p', 'u') },
		{ "memcpy", FunctionInfo('p', 'p', 'u') },
		{ "memmove", FunctionInfo('p', 'p', 'u') },
		{ "memset", FunctionInfo('p', 'c', 'u') },
		{ "strcat", FunctionInfo('s', 's') },
		{ "strchr", FunctionInfo('s', 'c') },
		{ "strcmp", FunctionInfo('s', 's') },
		{ "strcoll", FunctionInfo('s', 's') },
		{ "strcpy", FunctionInfo('p', 's') },
		{ "strcspn", FunctionInfo('s', 's') },
		{ "strerror", FunctionInfo('i') },
		{ "strlen", FunctionInfo('s') },
		{ "strncat", FunctionInfo('s', 's', 'u') },
		{ "strncmp", FunctionInfo('s', 's', 'u') },
		{ "strncpy", FunctionInfo('p', 's', 'u') },
		{ "strpbrk", FunctionInfo('s', 's') },
		{ "strrchr", FunctionInfo('s', 'c') },
		{ "strspn", FunctionInfo('s', 's') },
		{ "strstr", FunctionInfo('s', 's') },
		{ "strtok", FunctionInfo('s', 's') },
		{ "strxfrm", FunctionInfo('p', 's', 'u') },
		{ "strdup", FunctionInfo('s') },
		{ "strndup", FunctionInfo('s', 'u') },

		// locale functions
		{ "setlocale", FunctionInfo('i', 's') },

		// misc other functions
		{ "__cxa_atexit", FunctionInfo('p', 'p', 'p') },
		{ "__libc_start_main", FunctionInfo('p', 'i', 'p', 'p', 'p', 'p', 'p') },
		{ "opendir", FunctionInfo('s') },
		{ "sbrk", FunctionInfo('p') },
		{ "getpagesize", FunctionInfo() },
		{ "ioctl", FunctionInfo('i', 'i', 'p') },
		{ "isatty", FunctionInfo('i') },
		{ "poll", FunctionInfo('p', 'u', 'i') },
		{ "setenv", FunctionInfo('s', 's', 'i') },
		{ "bindtextdomain", FunctionInfo('s', 's') },
		{ "textdomain", FunctionInfo('s') },
		{ "getopt", FunctionInfo('i', 'p', 's') },
		{ "getopt_long", FunctionInfo('i', 'p', 's', 'p', 'p') },
		{ "dcgettext", FunctionInfo('s', 's', 'i') },

		{ "mmap", FunctionInfo('p', 'u', 'i', 'i', 'i', 'u') },
		{ "munmap", FunctionInfo('p', 'u') },
		{ "mprotect", FunctionInfo('p', 'u', 'i') },

		{ "dlopen", FunctionInfo('s', 'i') },
		{ "dlclose", FunctionInfo('p') },
		{ "dlsym", FunctionInfo('p', 's') },

		{ "pthread_create", FunctionInfo('p', 'p', 'p', 'p') },
		{ "pthread_join", FunctionInfo('p', 'p') },
		{ "pthread_exit", FunctionInfo('p') },
	};

	const Function *funcs_begin = funcs;
	const Function *funcs_end   = funcs + (sizeof(funcs) / sizeof(funcs[0]));
}

//------------------------------------------------------------------------------
// Name: FunctionDBPlugin()
// Desc:
//------------------------------------------------------------------------------
const FunctionInfo *FunctionDBPlugin::find(const QString &name) const {
	const Function *const it = qFind(funcs_begin, funcs_end, name);
	if(it != funcs_end) {
		if(const FunctionInfo *const info = &it->f) {
			return info;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: private_init()
// Desc:
//------------------------------------------------------------------------------
void FunctionDBPlugin::private_init() {
	edb::v1::set_function_db(this);
	qDebug() << "[Function Database] loaded with" << (sizeof(funcs) / sizeof(funcs[0])) << "function definitions.";
}


//------------------------------------------------------------------------------
// Name: menu(QWidget *parent)
// Desc:
//------------------------------------------------------------------------------
QMenu *FunctionDBPlugin::menu(QWidget *parent) {
	Q_UNUSED(parent);
	return 0;
}

Q_EXPORT_PLUGIN2(FunctionDBPlugin, FunctionDBPlugin)
