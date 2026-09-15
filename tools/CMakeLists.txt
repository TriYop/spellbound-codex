add_executable(mastertweak-backfill-analysis backfill_analysis.cpp)

target_link_libraries(mastertweak-backfill-analysis
    PRIVATE
        mastertweak_core
        preset_builder_core
        cli11
)

if(MASTERTWEAK_STATIC)
    target_link_options(mastertweak-backfill-analysis PRIVATE -static)
endif()
