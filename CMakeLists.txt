cmake_minimum_required(VERSION 3.16)
project(WinBridge VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(winbridge_core STATIC
    src/shared_space.cpp
    src/shortcuts.cpp
    src/launcher.cpp
    src/manager_backend.cpp
)
target_link_libraries(winbridge_core PUBLIC Qt6::Core Qt6::Gui)

add_executable(winbridge src/winbridge_main.cpp)
target_link_libraries(winbridge PRIVATE winbridge_core Qt6::Core)

add_executable(winbridge-backend src/backend_main.cpp)
target_link_libraries(winbridge-backend PRIVATE winbridge_core Qt6::Core)

add_subdirectory(manager)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(core-tests tests/test_core.cpp)
    target_link_libraries(core-tests PRIVATE winbridge_core Qt6::Core Qt6::Test)
    add_test(NAME core-tests COMMAND core-tests)
endif()

install(TARGETS winbridge winbridge-backend RUNTIME DESTINATION bin)
install(FILES packaging/winbridge.desktop packaging/winbridge-manager.desktop DESTINATION share/applications)
install(FILES assets/winbridge.png DESTINATION share/pixmaps)
install(FILES README.md DESTINATION share/doc/winbridge)
