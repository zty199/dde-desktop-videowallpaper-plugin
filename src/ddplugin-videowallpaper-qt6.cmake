set(BIN_NAME dd-videowallpaper-plugin)
if(OPT_ENABLE_AUDIO_OUTPUT MATCHES ON)
    add_compile_definitions(ENABLE_AUDIO_OUTPUT)
endif()

set(QT_VERSION_MAJOR 6)
set(DTK_VERSION_MAJOR 6)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Concurrent Widgets)
find_package(Dtk${DTK_VERSION_MAJOR} COMPONENTS Core REQUIRED)

include(ddplugin-videowallpaper.cmake)
