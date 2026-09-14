find_package(SQLite3 REQUIRED)

if(MASTERTWEAK_STATIC)
    # TagLib::tag provided by FetchContent in root CMakeLists.txt
    set(_taglib_target TagLib::tag)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
    set(_taglib_target PkgConfig::TAGLIB)
endif()

add_library(preset_builder_core STATIC
    src/services/stats_service.cpp
    src/services/export_service.cpp
    src/services/ingest_service.cpp
    src/services/similarity_service.cpp
    src/adapters/database.cpp
    src/adapters/sqlite_track_repository.cpp
    src/adapters/sqlite_preset_repository.cpp
    src/adapters/embedded_tag_metadata_provider.cpp
)

target_include_directories(preset_builder_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(preset_builder_core
    PUBLIC
        mastertweak_core
        SQLite::SQLite3
    PRIVATE
        picosha2
        ${_taglib_target}
)
