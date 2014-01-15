
include(../plugins.pri)

# Input
HEADERS += symbols.h BinaryInfo.h ELF32.h ELF64.h PE32.h elf_binary.h pe_binary.h DialogHeader.h
FORMS += DialogHeader.ui
SOURCES += symbols.cpp BinaryInfo.cpp ELF32.cpp ELF64.cpp PE32.cpp DialogHeader.cpp

