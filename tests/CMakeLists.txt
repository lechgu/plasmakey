add_executable(test_modifiermasks test_modifiermasks.cpp)
target_link_libraries(test_modifiermasks PRIVATE plasmakey_core Qt6::Test)
add_test(NAME modifiermasks COMMAND test_modifiermasks)
set_tests_properties(modifiermasks PROPERTIES LABELS unit)

# Live test against the running X server (changes keyboard state briefly).
# Needs Xlib's XKB API (libx11-dev) to set latches, which XCB cannot do.
find_package(X11)
if(X11_FOUND AND X11_Xkb_FOUND)
    add_executable(plasmakey-livetest livetest.cpp)
    target_link_libraries(plasmakey-livetest PRIVATE plasmakey_core X11::X11)
    add_test(NAME live COMMAND plasmakey-livetest)
    set_tests_properties(live PROPERTIES LABELS live SKIP_RETURN_CODE 77 RUN_SERIAL TRUE)
else()
    message(STATUS "libx11-dev not found: plasmakey-livetest will not be built")
endif()

# Offscreen renderer for the plasmoid's QML (`make preview`): icon in every
# state at several sizes, light and dark, plus the popup. Touches nothing live.
find_package(Qt6 REQUIRED COMPONENTS Quick)
add_executable(plasmakey-preview preview/preview.cpp)
target_compile_definitions(plasmakey-preview PRIVATE
    PLASMAKEY_BUILD_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    PLASMAKEY_PREVIEW_QML="${CMAKE_CURRENT_SOURCE_DIR}/preview/Preview.qml"
)
target_link_libraries(plasmakey-preview PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui)
add_dependencies(plasmakey-preview plasmakeyxkb)
