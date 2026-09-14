add_library(mastertweak_core STATIC
    src/version.cpp
    src/io.cpp
    src/analysis.cpp
    src/preset.cpp
    src/advice.cpp
    src/pipeline.cpp
    src/target_level.cpp
    src/codec_correction.cpp
    src/report.cpp
    src/resonance_detector.cpp
    src/dsp/parametric_eq.cpp
    src/dsp/compressor.cpp
    src/dsp/multiband_comp.cpp
    src/dsp/mixbus_comp.cpp
    src/dsp/saturator.cpp
    src/dsp/stereo_width.cpp
    src/dsp/limiter.cpp
    src/dsp/dither.cpp
    src/dsp/gain_stager.cpp
    src/dsp/resonance_eq.cpp
)

target_include_directories(mastertweak_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mastertweak_core
    PUBLIC
        $<IF:$<BOOL:${MASTERTWEAK_STATIC}>,SndFile::sndfile,PkgConfig::SNDFILE>
        AudioPluginsCommon::dsp
        AudioPluginsCommon::analysis
    PRIVATE
        pugixml
)

target_compile_definitions(mastertweak_core PUBLIC
    MASTERTWEAK_VERSION_STRING="${PROJECT_VERSION}"
)
