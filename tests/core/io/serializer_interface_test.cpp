#include "coupecad/core/errors.h"
#include "coupecad/core/io/project_serializer.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(FileFormatError, SubclassesExist) {
    EXPECT_STREQ((CorruptedArchive{"cf.corrupt", "x"}.what()), "x");
    EXPECT_STREQ((MissingManifest{"cf.missing", "y"}.what()), "y");
    EXPECT_STREQ((ChecksumMismatch{"cf.checksum", "z"}.what()), "z");
    EXPECT_STREQ((UnsupportedVersion{"cf.ver", "w"}.what()), "w");
    EXPECT_STREQ((UnsupportedEncoding{"cf.enc", "u"}.what()), "u");
    EXPECT_STREQ((InvalidData{"cf.data", "v"}.what()), "v");
}

TEST(FileFormatError, InheritsFileFormatError) {
    try {
        throw CorruptedArchive{"cf.corrupt", "x"};
    } catch (const FileFormatError&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "CorruptedArchive not a FileFormatError";
    }
}

TEST(ProjectSerializer, InterfaceIsAbstract) {
    static_assert(!std::is_default_constructible_v<ProjectSerializer>,
                  "ProjectSerializer must be abstract");
}
