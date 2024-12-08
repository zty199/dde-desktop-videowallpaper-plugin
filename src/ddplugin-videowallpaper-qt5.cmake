set(BIN_NAME ddplugin-videowallpaper)

set(QT_VERSION_MAJOR 5)
set(DTK_VERSION_MAJOR "")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Concurrent DBus Widgets)
find_package(Dtk${DTK_VERSION_MAJOR} COMPONENTS Widget REQUIRED)

include(ddplugin-videowallpaper.cmake)
