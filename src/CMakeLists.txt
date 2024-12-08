# Qt6
if(OPT_ENABLE_QT6 MATCHES ON)
    set(DFM_BUILD_WITH_QT6 TRUE)
endif()
if(DFM_BUILD_WITH_QT6)
    include(ddplugin-videowallpaper-qt6.cmake)
else()
    include(ddplugin-videowallpaper-qt5.cmake)
endif()
