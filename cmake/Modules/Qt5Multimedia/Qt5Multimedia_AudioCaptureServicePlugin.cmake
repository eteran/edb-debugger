
add_library(Qt5::AudioCaptureServicePlugin MODULE IMPORTED)

_populate_Multimedia_plugin_properties(AudioCaptureServicePlugin RELEASE "mediaservice/libqtmedia_audioengine.so")

list(APPEND Qt5Multimedia_PLUGINS Qt5::AudioCaptureServicePlugin)
