qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ChainPanel.h
    ChainPanel.cpp
    PresetSelector.h
    PresetSelector.cpp
    TransportWidget.h
    TransportWidget.cpp
    TargetLevelCombo.h
    TargetLevelCombo.cpp
)

target_link_libraries(MasterTweak
    PRIVATE
        mastertweak_core
        Qt6::Core
        Qt6::Widgets
        miniaudio
)

set_target_properties(MasterTweak PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
