cmake_minimum_required(VERSION 3.22)
project(MasterTweak
    VERSION 0.1.0
    DESCRIPTION "Offline auto-mastering app driven by MixAdvice profile presets"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MASTERTWEAK_BUILD_GUI    "Build the Qt6 GUI"    ON)
option(MASTERTWEAK_BUILD_TESTS  "Build unit tests"     ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

if(MASTERTWEAK_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
    qt_standard_project_setup()
endif()

if(MASTERTWEAK_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(third_party)
add_subdirectory(core)
add_subdirectory(preset_builder)
add_subdirectory(cli)

if(MASTERTWEAK_BUILD_GUI)
    add_subdirectory(gui)
endif()

if(MASTERTWEAK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ── Install rules ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS mastertweak
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(MASTERTWEAK_BUILD_GUI)
    install(TARGETS MasterTweak
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/presets)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/presets/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mastertweak/presets
        FILES_MATCHING PATTERN "*.xml"
    )
endif()

# ── CPack ─────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME        "MasterTweak")
set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR      "MasterTweak")
set(CPACK_PACKAGE_CONTACT     "yvan.janet@gmail.com")
set(CPACK_GENERATOR           "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/\\.git/;/\\.cache/")
include(CPack)
