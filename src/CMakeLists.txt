# Qt6
if(OPT_ENABLE_QT6 MATCHES ON)
    include(ddplugin-videowallpaper-qt6.cmake)
else()
    include(ddplugin-videowallpaper-qt5.cmake)
endif()
