
include(../plugins.pri)

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += concurrent
}

# Input
HEADERS += HeapAnalyzer.h DialogHeap.h ResultViewModel.h
FORMS += DialogHeap.ui
SOURCES += HeapAnalyzer.cpp DialogHeap.cpp ResultViewModel.cpp

graph {
	DEFINES += ENABLE_GRAPH
}
