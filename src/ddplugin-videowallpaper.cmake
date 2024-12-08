find_package(PkgConfig REQUIRED)
pkg_check_modules(DFM${DTK_VERSION_MAJOR} REQUIRED
    IMPORTED_TARGET
    dfm${DTK_VERSION_MAJOR}-base
    dfm${DTK_VERSION_MAJOR}-framework
)

if (OPT_USE_LIBMPV MATCHES ON)
    add_compile_definitions(USE_LIBMPV)
    pkg_check_modules(Mpv REQUIRED IMPORTED_TARGET mpv)
    if(OPT_ENABLE_QT6 MATCHES ON)
        find_package(Qt${QT_VERSION_MAJOR} COMPONENTS OpenGLWidgets REQUIRED)
        set(Media_INCLUDE_DIRS
            Qt${QT_VERSION_MAJOR}::OpenGLWidgets
            PkgConfig::Mpv
        )
        set(Media_LIBRARIES
            Qt${QT_VERSION_MAJOR}::OpenGLWidgets
            PkgConfig::Mpv
        )
    else()
        set(Media_INCLUDE_DIRS
            PkgConfig::Mpv
        )
        set(Media_LIBRARIES
            PkgConfig::Mpv
        )
    endif()
else()
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Multimedia REQUIRED)
    set(Media_INCLUDE_DIRS
        Qt${QT_VERSION_MAJOR}::Multimedia
    )
    set(Media_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Multimedia
    )
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
)

if (OPT_ENABLE_QT6 MATCHES ON)
    file(GLOB_RECURSE RM_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/videosurface.hpp"
    )
endif()
if (OPT_USE_LIBMPV MATCHES OFF)
    file(GLOB_RECURSE RM_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/*"
    )
endif()

list(REMOVE_ITEM SRC_FILES ${RM_SRC})

# 查找匹配 ddplugin-videowallpaper*.ts 的文件列表
file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/ddplugin-videowallpaper*.ts")

# 添加 lrelease 命令，传递 TS_FILES 列表
foreach(TS_FILE ${TS_FILES})
    execute_process(
       COMMAND lrelease ${TS_FILE}
       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endforeach()

qt_add_resources(QRC_RESOURCES ts.qrc)

add_library(${BIN_NAME}
    SHARED
    ${SRC_FILES}
    ${QRC_RESOURCES}
)

set_target_properties(${BIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(${BIN_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Widgets
    ${Qt${QT_VERSION_MAJOR}Widgets_PRIVATE_INCLUDE_DIRS}
    Dtk${DTK_VERSION_MAJOR}::Core
    Dtk${DTK_VERSION_MAJOR}::Widget
    PkgConfig::DFM${DTK_VERSION_MAJOR}
    ${Media_INCLUDE_DIRS}
)

target_link_libraries(${BIN_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Widgets
    Dtk${DTK_VERSION_MAJOR}::Core
    Dtk${DTK_VERSION_MAJOR}::Widget
    PkgConfig::DFM${DTK_VERSION_MAJOR}
    ${Media_LIBRARIES}
)

# install library file
install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${DFM_PLUGIN_DESKTOP_EDGE_DIR})

dtk_add_config_meta_files(
    APPID "org.deepin.dde.file-manager"
    BASE "${CMAKE_SOURCE_DIR}/assets/configs"
    FILES "${CMAKE_SOURCE_DIR}/assets/configs/org.deepin.dde.file-manager.desktop.videowallpaper.json"
)
