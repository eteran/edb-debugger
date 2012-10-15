
include(../plugins.pri)

# Input
HEADERS += ProcessProperties.h DialogProcessProperties.h DialogStrings.h
FORMS += dialogprocess.ui dialogstrings.ui
SOURCES += ProcessProperties.cpp DialogProcessProperties.cpp DialogStrings.cpp

QT += network
