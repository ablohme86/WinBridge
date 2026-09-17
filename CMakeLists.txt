cmake_minimum_required(VERSION 3.16)
project(WinBridge VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(KF6WindowSystem QUIET)
find_package(X11 QUIET)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(winbridge_core STATIC
    src/executable_icon.cpp
    src/shared_space.cpp
    src/shortcuts.cpp
    src/launcher.cpp
    src/manager_backend.cpp
)
target_link_libraries(winbridge_core PUBLIC Qt6::Core Qt6::Gui)

add_executable(winbridge src/winbridge_main.cpp src/opener.cpp src/single_instance.cpp)
target_link_libraries(winbridge PRIVATE winbridge_core Qt6::Core Qt6::Widgets Qt6::Network)
if(TARGET KF6::WindowSystem)
    target_link_libraries(winbridge PRIVATE KF6::WindowSystem)
    target_compile_definitions(winbridge PRIVATE WINBRIDGE_KWINDOWSYSTEM)
endif()
if(X11_FOUND)
    target_link_libraries(winbridge PRIVATE ${X11_LIBRARIES})
    target_include_directories(winbridge PRIVATE ${X11_INCLUDE_DIR})
    target_compile_definitions(winbridge PRIVATE WINBRIDGE_HAVE_X11)
endif()

add_executable(winbridge-backend src/backend_main.cpp)
target_link_libraries(winbridge-backend PRIVATE winbridge_core Qt6::Core)

add_subdirectory(manager)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(core-tests tests/test_core.cpp)
    target_link_libraries(core-tests PRIVATE winbridge_core Qt6::Core Qt6::Test)
    add_test(NAME core-tests COMMAND core-tests -platform offscreen)
    add_executable(opener-tests tests/test_opener.cpp src/opener.cpp src/single_instance.cpp)
    target_link_libraries(opener-tests PRIVATE winbridge_core Qt6::Core Qt6::Widgets Qt6::Network Qt6::Test)
    if(TARGET KF6::WindowSystem)
        target_link_libraries(opener-tests PRIVATE KF6::WindowSystem)
        target_compile_definitions(opener-tests PRIVATE WINBRIDGE_KWINDOWSYSTEM)
    endif()
    if(X11_FOUND)
        target_link_libraries(opener-tests PRIVATE ${X11_LIBRARIES})
        target_include_directories(opener-tests PRIVATE ${X11_INCLUDE_DIR})
        target_compile_definitions(opener-tests PRIVATE WINBRIDGE_HAVE_X11)
    endif()
    add_test(NAME opener-tests COMMAND opener-tests -platform offscreen)
    find_program(UPDATE_MIME_DATABASE update-mime-database)
    if(NOT UPDATE_MIME_DATABASE)
        message(FATAL_ERROR "MIME tests require shared-mime-info (update-mime-database)")
    endif()
    add_executable(mime-tests tests/test_mime.cpp)
    target_link_libraries(mime-tests PRIVATE Qt6::Core Qt6::Test)
    target_compile_definitions(mime-tests PRIVATE
        WINBRIDGE_MIME_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/packaging/winbridge-exe.xml"
        UPDATE_MIME_DATABASE="${UPDATE_MIME_DATABASE}")
    add_test(NAME mime-tests COMMAND mime-tests)
endif()

install(TARGETS winbridge winbridge-backend RUNTIME DESTINATION bin)
install(FILES packaging/winbridge.desktop packaging/winbridge-manager.desktop DESTINATION share/applications)
install(FILES packaging/winbridge-exe.xml DESTINATION share/mime/packages)
install(FILES assets/winbridge-app-launcher.png DESTINATION share/pixmaps RENAME winbridge.png)
install(FILES assets/winbridge-manager-icon.png DESTINATION share/pixmaps RENAME winbridge-manager.png)
foreach(ICON_SIZE 16 22 24 32 48 64 128 256 512)
    install(FILES assets/icons/${ICON_SIZE}x${ICON_SIZE}/winbridge.png
        DESTINATION share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps)
    install(FILES assets/icons/${ICON_SIZE}x${ICON_SIZE}/winbridge-manager.png
        DESTINATION share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps)
endforeach()
install(FILES README.md INSTALL.md DESTINATION share/doc/winbridge)
