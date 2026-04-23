from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class CoupeCADConan(ConanFile):
    name = "coupecad"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    # Stage 0: только GoogleTest. Qt подключается извне (aqtinstall/install-qt-action).
    # Stage 1a: добавляются spdlog (логирование), fmt (форматирование),
    # stduuid (UUID для id.h).
    # Stage 1c: nlohmann_json + libzip для .ccad I/O.
    # Stage 2: OpenCASCADE для геометрического слоя.
    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("fmt/10.2.1")
        self.requires("stduuid/1.2.3")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libzip/1.10.1")
        self.requires("opencascade/7.9.1")
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
        self.folders.build = "build/default"
        self.folders.generators = "build/default"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()
