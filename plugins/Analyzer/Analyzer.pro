
include(../plugins.pri)

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += concurrent
}

HEADERS += \
	Analyzer.h \
	AnalyzerWidget.h \
	OptionsPage.h \
	SpecifiedFunctions.h \
	BasicBlock.h
	
SOURCES += \
	Analyzer.cpp \
	AnalyzerWidget.cpp \
	OptionsPage.cpp \
	SpecifiedFunctions.cpp \
	BasicBlock.cpp
	
FORMS += \
	OptionsPage.ui\
	SpecifiedFunctions.ui

