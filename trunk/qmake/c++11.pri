greaterThan(QT_MAJOR_VERSION, 4) {
	CONFIG += c++11
} else {
	linux-g++ {
    	system(g++ -dumpversion | grep -e "^4\\.[7-9]\\.[0-9]$" > /dev/null) {
			QMAKE_CXXFLAGS += -std=c++0x
		}
	}
}
