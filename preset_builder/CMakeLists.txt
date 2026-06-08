find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)

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
    PRIVATE
        ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(preset_builder_core
    PUBLIC
        mastertweak_core
        SQLite::SQLite3
    PRIVATE
        picosha2
        ${TAGLIB_LIBRARIES}
)
