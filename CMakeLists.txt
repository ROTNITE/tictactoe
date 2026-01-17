cmake_minimum_required(VERSION 3.16)

project(tictactoe2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent LinguistTools)

set(APP_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui

    Board.hpp
    MinimaxAI.hpp
    DynamicArray.hpp
    HashMap.hpp
    GameController.hpp
)

qt_add_executable(tictactoe2
    ${APP_SOURCES}
)

target_link_libraries(tictactoe2 PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
)

set(TS_FILES tictactoe2_ru_RU.ts)
qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

qt_finalize_executable(tictactoe2)

enable_testing()

add_executable(opening_tests
    opening_tests.cpp
)
set_target_properties(opening_tests PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
add_test(NAME opening_tests COMMAND opening_tests)
