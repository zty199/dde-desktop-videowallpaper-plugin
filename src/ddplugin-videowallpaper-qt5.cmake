set(BIN_NAME ddplugin-videowallpaper)
if(OPT_ENABLE_AUDIO_OUTPUT MATCHES ON)
    add_compile_definitions(ENABLE_AUDIO_OUTPUT)
endif()

set(QT_VERSION_MAJOR 5)
set(DTK_VERSION_MAJOR "")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core DBus Widgets)
find_package(Dtk${DTK_VERSION_MAJOR} COMPONENTS Widget REQUIRED)

include(ddplugin-videowallpaper.cmake)
