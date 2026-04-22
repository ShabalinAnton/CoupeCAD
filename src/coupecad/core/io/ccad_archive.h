#pragma once

#include "coupecad/core/io/project_serializer.h"
#include "coupecad/core/io/schema_migration.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace coupecad::core {

class Project;

// Клок для тестов — позволяет подменить «сейчас» на детерминированное
// значение. В production — std::chrono::system_clock::now().
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::system_clock::time_point now() const = 0;
};

std::unique_ptr<IClock> make_system_clock();

// Чтение и запись `.ccad` файлов.
class CcadArchive {
public:
    // Записать Project в указанный путь. encoding выбирается
    // serializer.content_encoding() — влияет на имя файла данных
    // внутри ZIP (`project.json` для "json"). Перезаписывает если файл уже есть.
    static void save(const Project& project,
                      const std::filesystem::path& path,
                      const ProjectSerializer& serializer,
                      const IClock& clock = *make_system_clock());

    // Прочитать Project из пути. Бросает FileFormatError subclasses при
    // ошибках формата. Migration chain прогоняется перед вызовом
    // serializer.deserialize.
    static Project load(const std::filesystem::path& path,
                         const ProjectSerializer& serializer,
                         const MigrationChain& chain = default_migration_chain());

    // Константа имени data-файла внутри архива для data encoding.
    // В v1 content_encoding всегда "json", так что имя — "project.json".
    static std::string data_file_name(const std::string& encoding);
};

}  // namespace coupecad::core
