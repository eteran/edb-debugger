
include(../plugins.pri)

# Input
HEADERS += symbols.h demangle.h BinaryInfo.h ELF32.h ELF64.h PE32.h elf_binary.h pe_binary.h DialogHeader.h OptionsPage.h
FORMS += DialogHeader.ui OptionsPage.ui
SOURCES += symbols.cpp BinaryInfo.cpp ELF32.cpp ELF64.cpp PE32.cpp DialogHeader.cpp OptionsPage.cpp

