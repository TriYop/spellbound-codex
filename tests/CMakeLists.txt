add_executable(mastertweak_tests
    test_main.cpp
    test_sanity.cpp
    test_io.cpp
    test_analysis.cpp
    test_preset.cpp
    test_advice.cpp
    test_dsp.cpp
    test_pipeline.cpp
    test_target_level.cpp
    test_gain_stager.cpp
    test_preset_builder.cpp
)

target_link_libraries(mastertweak_tests
    PRIVATE
        mastertweak_core
        preset_builder_core
        doctest
)

add_test(NAME mastertweak_tests COMMAND mastertweak_tests)
