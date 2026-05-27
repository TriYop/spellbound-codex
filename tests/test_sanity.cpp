#include "mastertweak/version.hpp"

#include <doctest.h>

TEST_CASE("version string is non-empty") {
    CHECK_FALSE(mastertweak::version().empty());
}
