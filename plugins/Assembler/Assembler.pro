
include(../plugins.pri)

QT += xml xmlpatterns

# Input
HEADERS += Assembler.h DialogAssembler.h OptionsPage.h
FORMS += DialogAssembler.ui OptionsPage.ui
SOURCES += Assembler.cpp DialogAssembler.cpp OptionsPage.cpp
RESOURCES   = Assembler.qrc
