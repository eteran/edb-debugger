greaterThan(QT_MAJOR_VERSION, 4) {
	CONFIG += c++11
} else {
	*-g++* | *-clang* {
		QMAKE_CXXFLAGS += -std=c++11
	}
}
