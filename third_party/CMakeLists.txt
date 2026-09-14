add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)

add_library(cli11 INTERFACE)
target_include_directories(cli11 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/CLI11)

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/doctest)

add_library(picosha2 INTERFACE)
target_include_directories(picosha2 SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# RtMidi — ALSA backend
add_library(rtmidi STATIC rtmidi/RtMidi.cpp)
target_include_directories(rtmidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rtmidi)
target_compile_definitions(rtmidi PUBLIC __LINUX_ALSA__)
target_compile_options(rtmidi PRIVATE -w)
target_link_libraries(rtmidi PUBLIC asound pthread)
