
unix {
	CONFIG(debug, debug|release) {
		DEFINES    += QT_SHAREDPOINTER_TRACK_POINTERS
		OBJECTS_DIR = $${OUT_PWD}/.debug-shared/obj
		MOC_DIR     = $${OUT_PWD}/.debug-shared/moc
		RCC_DIR     = $${OUT_PWD}/.debug-shared/rcc
		UI_DIR      = $${OUT_PWD}/.debug-shared/uic
	}
	
	CONFIG(release, debug|release) {
		OBJECTS_DIR = $${OUT_PWD}/.release-shared/obj
		MOC_DIR     = $${OUT_PWD}/.release-shared/moc
		RCC_DIR     = $${OUT_PWD}/.release-shared/rcc
		UI_DIR      = $${OUT_PWD}/.release-shared/uic
	}
}

unix {
	QMAKE_DISTCLEAN += -r              \
		$${OUT_PWD}/.debug-shared/     \
		$${OUT_PWD}/.release-shared/
}


