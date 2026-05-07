#include "coupecad/renderer/occt/view_driver.h"

#include <gtest/gtest.h>

namespace {

using coupecad::renderer::occt::ViewDriver;

}  // namespace

TEST(ViewDriverTest, ConstructDoesNotThrow) {
    EXPECT_NO_THROW({
        ViewDriver driver;
    });
}

TEST(ViewDriverTest, ViewerAndViewAccessible) {
    ViewDriver driver;
    EXPECT_FALSE(driver.viewer().IsNull());
    EXPECT_FALSE(driver.view().IsNull());
}

TEST(ViewDriverTest, GlAvailableFlagIsConsistent) {
    ViewDriver driver;
    const bool gl1 = driver.gl_available();
    const bool gl2 = driver.gl_available();
    EXPECT_EQ(gl1, gl2);
}

TEST(ViewDriverTest, SetViewportSizeUpdatesNeutralWindow) {
    ViewDriver driver;
    driver.set_viewport_size(800, 600);
    EXPECT_EQ(driver.viewport_width(), 800);
    EXPECT_EQ(driver.viewport_height(), 600);
}

TEST(ViewDriverTest, NeutralWindowAccessibleAfterConstruction) {
    ViewDriver driver;
    EXPECT_FALSE(driver.window().IsNull());
}
