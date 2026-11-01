# XKB observer shared by the QML plugin, the diagnostic tool and the tests.
add_library(plasmakey_core STATIC
    core/modifiermasks.cpp
    core/modifiermasks.h
    core/xkbwatcher.cpp
    core/xkbwatcher.h
)
set_target_properties(plasmakey_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(plasmakey_core PUBLIC core ${XCB_XKB_INCLUDE_DIR})
target_link_libraries(plasmakey_core PUBLIC Qt6::Core ${XCB_XKB_LIBRARY} ${XCB_LIBRARY})
target_compile_options(plasmakey_core PRIVATE -Wall -Wextra)

# QML module "plasmakey.xkb", loaded by the plasmoid inside plasmashell.
qt_add_qml_module(plasmakeyxkb
    URI plasmakey.xkb
    VERSION 1.0
    PLUGIN_TARGET plasmakeyxkb
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/plasmakey/xkb
    SOURCES qml/modifierstate.cpp qml/modifierstate.h
)
target_include_directories(plasmakeyxkb PRIVATE qml)
target_link_libraries(plasmakeyxkb PRIVATE plasmakey_core Qt6::Gui Qt6::Qml)
target_compile_options(plasmakeyxkb PRIVATE -Wall -Wextra)

set(PLASMAKEY_QML_MODULE_DEST ${PLASMAKEY_QMLDIR}/plasmakey/xkb)
install(TARGETS plasmakeyxkb LIBRARY DESTINATION ${PLASMAKEY_QML_MODULE_DEST})
install(FILES
    ${CMAKE_BINARY_DIR}/qml/plasmakey/xkb/qmldir
    ${CMAKE_BINARY_DIR}/qml/plasmakey/xkb/plasmakeyxkb.qmltypes
    DESTINATION ${PLASMAKEY_QML_MODULE_DEST}
)

# Diagnostic tool: prints masks, mapping and live state changes.
add_executable(plasmakey-diag tools/plasmakey-diag.cpp)
target_link_libraries(plasmakey-diag PRIVATE plasmakey_core)
target_compile_options(plasmakey-diag PRIVATE -Wall -Wextra)
install(TARGETS plasmakey-diag RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
