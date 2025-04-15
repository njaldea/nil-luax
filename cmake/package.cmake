include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

configure_package_config_file(
    cmake/config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
    NO_SET_AND_CHECK_MACRO
)
    
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config-version.cmake
    VERSION 0.0.1
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config-version.cmake"
    DESTINATION "share/${CMAKE_PROJECT_NAME}"
)

function(nil_install_targets TARGET)
    install(TARGETS ${TARGET} EXPORT nil-${TARGET}-targets)
    install(
        EXPORT nil-${TARGET}-targets
        NAMESPACE nil::
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
    )
endfunction()

function(nil_install_headers)
    install(DIRECTORY publish/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endfunction()
