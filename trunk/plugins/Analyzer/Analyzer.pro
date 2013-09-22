
include(../plugins.pri)

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += concurrent
}

HEADERS += \
	Analyzer.h \
	AnalyzerWidget.h \
	AnalyzerOptionsPage.h \
	SpecifiedFunctions.h
	
SOURCES += \
	Analyzer.cpp \
	AnalyzerWidget.cpp \
	AnalyzerOptionsPage.cpp \
	SpecifiedFunctions.cpp
	
FORMS += \
	AnalyzerOptionsPage.ui\
	SpecifiedFunctions.ui

