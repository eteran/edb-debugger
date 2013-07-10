unix {
	QMAKE_TARGET.arch = $$QMAKE_HOST.arch
	*-g++-32  :QMAKE_TARGET.arch = x86
	*-g++-64  :QMAKE_TARGET.arch = x86_64
	*-clang-32:QMAKE_TARGET.arch = x86
	*-clang-64:QMAKE_TARGET.arch = x86_64
}
