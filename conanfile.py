from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class CoupeCADConan(ConanFile):
    name = "coupecad"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    # Stage 0: только GoogleTest. Qt подключается извне (aqtinstall/install-qt-action).
    # OpenCASCADE добавится в Stage 2 (Geometry layer).
    def requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
        # Принудительно используем build/default/ как на Linux/macOS,
        # так и на Windows, чтобы CMakePresets.json совпадал с conan layout.
        self.folders.build = "build/default"
        self.folders.generators = "build/default"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()
