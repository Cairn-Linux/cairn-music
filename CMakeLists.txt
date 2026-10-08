cmake_minimum_required(VERSION 3.22)

project(CairnMusicPrototype VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia Test)

include(CTest)

add_library(cairnmusic_core STATIC
    src/AppController.cpp
    src/AppController.h
    src/AudioEngine.cpp
    src/AudioEngine.h
    src/AudioRenderer.cpp
    src/AudioRenderer.h
    src/CompositionModel.cpp
    src/CompositionModel.h
    src/PlaybackProgress.cpp
    src/PlaybackProgress.h
    src/ProjectStore.cpp
    src/ProjectStore.h
)
target_link_libraries(cairnmusic_core PUBLIC Qt6::Core Qt6::Multimedia)
target_include_directories(cairnmusic_core PUBLIC src)

qt_add_executable(cairn-music src/main.cpp)
target_compile_definitions(cairn-music PRIVATE
    CAIRN_MUSIC_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml")
target_link_libraries(cairn-music PRIVATE
    cairnmusic_core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)

if(BUILD_TESTING)
    add_executable(tst_compositionmodel tests/tst_compositionmodel.cpp)
    target_link_libraries(tst_compositionmodel PRIVATE cairnmusic_core Qt6::Test)
    add_test(NAME compositionmodel COMMAND tst_compositionmodel)

    add_executable(tst_projectstore tests/tst_projectstore.cpp)
    target_link_libraries(tst_projectstore PRIVATE cairnmusic_core Qt6::Test)
    add_test(NAME projectstore COMMAND tst_projectstore)

    add_executable(tst_audiorenderer tests/tst_audiorenderer.cpp)
    target_link_libraries(tst_audiorenderer PRIVATE cairnmusic_core Qt6::Test)
    add_test(NAME audiorenderer COMMAND tst_audiorenderer)

    add_executable(tst_appcontroller tests/tst_appcontroller.cpp)
    target_link_libraries(tst_appcontroller PRIVATE cairnmusic_core Qt6::Test)
    add_test(NAME appcontroller COMMAND tst_appcontroller)

    add_executable(tst_playbackprogress tests/tst_playbackprogress.cpp)
    target_link_libraries(tst_playbackprogress PRIVATE cairnmusic_core Qt6::Test)
    add_test(NAME playbackprogress COMMAND tst_playbackprogress)

    add_executable(tst_qmlsmoke tests/tst_qmlsmoke.cpp)
    target_compile_definitions(tst_qmlsmoke PRIVATE
        CAIRN_MUSIC_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml")
    target_link_libraries(tst_qmlsmoke PRIVATE
        cairnmusic_core Qt6::Test Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    add_test(NAME qmlsmoke COMMAND tst_qmlsmoke)
    set_tests_properties(qmlsmoke PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
