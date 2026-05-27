add_executable(mastertweak main.cpp)

target_link_libraries(mastertweak
    PRIVATE
        mastertweak_core
        cli11
)
