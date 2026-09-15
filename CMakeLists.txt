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
# Build with static dependencies for cross-distro portability.
# The CLI binary becomes fully self-contained (only libc remains dynamic).
# The GUI binary gets static C++ runtime and non-Qt deps; Qt itself stays
# dynamic unless you supply a static Qt6 build via CMAKE_PREFIX_PATH.
option(MASTERTWEAK_STATIC "Link dependencies statically for cross-distro portability" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

if(MASTERTWEAK_STATIC)
    # Prefer .a over .so for every find_library() call, including those made
    # inside FetchContent subprojects (same CMake invocation).
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")

    # Remove libstdc++ and libgcc_s runtime dependencies from every target.
    # libc stays dynamic — its ABI is stable across all Linux distributions.
    add_link_options(-static-libstdc++ -static-libgcc)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    include(FetchContent)

    # libsndfile — Ubuntu ships no .a; build from source.
    # ENABLE_MPEG=OFF: skip mp3lame/mpg123 (no .a available; not needed for WAV/FLAC/AIFF).
    # FLAC/Vorbis/Ogg/Opus .a files are present in libsndfile1-dev dependencies.
    FetchContent_Declare(sndfile
        GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
        GIT_TAG        1.2.2
        GIT_SHALLOW    TRUE
    )
    set(ENABLE_MPEG            OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXTERNAL_LIBS   ON  CACHE BOOL "" FORCE)
    set(SNDFILE_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(SNDFILE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(sndfile)

    # taglib — Ubuntu ships no .a; build from source.
    FetchContent_Declare(taglib
        GIT_REPOSITORY https://github.com/taglib/taglib.git
        GIT_TAG        v2.0.2
        GIT_SHALLOW    TRUE
    )
    set(TAGLIB_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(TAGLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(taglib)
    # FetchContent builds the raw target named 'tag'; add a namespace alias so
    # downstream code can use TagLib::tag unconditionally.
    add_library(TagLib::tag ALIAS tag)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
endif()

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
add_subdirectory(tools)

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
