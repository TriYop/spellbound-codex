add_executable(mastertweak main.cpp)

target_link_libraries(mastertweak
    PRIVATE
        mastertweak_core
        cli11
)

if(MASTERTWEAK_STATIC)
    # The CLI has no Qt or ALSA deps, so a fully static binary is viable.
    # Only libc remains dynamic (glibc ABI is stable across distros).
    target_link_options(mastertweak PRIVATE -static)
endif()
