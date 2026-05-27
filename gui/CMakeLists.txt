qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    PresetSelector.h
    PresetSelector.cpp
    AnalysisPanel.h
    AnalysisPanel.cpp
    ParameterPanel.h
    ParameterPanel.cpp
    TransportWidget.h
    TransportWidget.cpp
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
