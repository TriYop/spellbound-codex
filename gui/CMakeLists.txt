qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    utils.h
    AudioControl.h
    AudioControl.cpp
    RotaryKnob.h
    RotaryKnob.cpp
    VerticalFader.h
    VerticalFader.cpp
    ChainPanel.h
    ChainPanel.cpp
    PresetSelector.h
    PresetSelector.cpp
    TransportWidget.h
    TransportWidget.cpp
    TargetLevelCombo.h
    TargetLevelCombo.cpp
    VUMeterWidget.h
    VUMeterWidget.cpp
    AcoustIdMetadataProvider.h
    AcoustIdMetadataProvider.cpp
    PresetBuilderCtx.h
    PresetBuilderDialog.h
    PresetBuilderDialog.cpp
    IngestTab.h
    IngestTab.cpp
    BrowseTab.h
    BrowseTab.cpp
    CreatePresetTab.h
    CreatePresetTab.cpp
)

target_link_libraries(MasterTweak
    PRIVATE
        mastertweak_core
        preset_builder_core
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        miniaudio
        rtmidi
)

set_target_properties(MasterTweak PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
