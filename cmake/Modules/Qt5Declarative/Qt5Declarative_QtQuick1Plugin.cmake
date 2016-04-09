
add_library(Qt5::QtQuick1Plugin MODULE IMPORTED)

_populate_Declarative_plugin_properties(QtQuick1Plugin RELEASE "qml1tooling/libqmldbg_inspector.so")

list(APPEND Qt5Declarative_PLUGINS Qt5::QtQuick1Plugin)
