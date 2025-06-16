cmake_minimum_required(VERSION 3.16)

project(TestQRhi VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_REQUIRED_VERSION "6.9")

find_package(Qt6 ${QT_REQUIRED_VERSION} CONFIG REQUIRED COMPONENTS Quick Gui GuiPrivate ShaderTools)
qt_standard_project_setup(REQUIRES ${QT_REQUIRED_VERSION})

qt_add_executable(appTestQRhi
    main.cpp
)

qt_add_qml_module(appTestQRhi
    URI TestQRhi
    VERSION 1.0
    QML_FILES
        Main.qml

    SOURCES
        rhiitem.h
        rhiitem.cpp

    RESOURCES
        README.md
)

qt_add_resources(appTestQRhi Images
    PREFIX "/images"
    FILES
        dizzy_4.png
)

qt_add_shaders(appTestQRhi "spectacol_shaders"
    GLSL "310es,450"
    PRECOMPILE
    OPTIMIZED
    PREFIX
        /
    FILES
        simple.frag
        simple.vert
)

target_link_libraries(appTestQRhi
    PRIVATE Qt::Quick Qt::GuiPrivate Qt::ShaderToolsPrivate
)
