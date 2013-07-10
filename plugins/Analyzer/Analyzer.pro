
include(../plugins.pri)

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += concurrent
}

HEADERS += \
	Analyzer.h \
	AnalyzerWidget.h \
	AnalyzerOptionsPage.h \
	DialogSpecifiedFunctions.h
	
SOURCES += \
	Analyzer.cpp \
	AnalyzerWidget.cpp \
	AnalyzerOptionsPage.cpp \
	DialogSpecifiedFunctions.cpp
	
FORMS += \
	analyzer_options_page.ui\
	dialogspecified.ui

