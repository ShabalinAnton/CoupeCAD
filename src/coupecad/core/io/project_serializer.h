#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coupecad::core {

class Project;

// Интерфейс сериализатора проекта. В v1 — одна реализация
// JsonProjectSerializer. Задел под binary форматы (CBOR/MessagePack/…) в §4.8.
class ProjectSerializer {
public:
    virtual ~ProjectSerializer() = default;

    // Человекочитаемое имя encoding'а (попадает в meta.json content_encoding).
    // Для JsonProjectSerializer вернёт "json".
    virtual const char* content_encoding() const noexcept = 0;

    // Сериализовать Project в байты. Байты — полезная нагрузка для записи
    // в ZIP под именем `project.<ext>` (ext выбирает CcadArchive по encoding).
    virtual std::vector<std::uint8_t> serialize(const Project& project) const = 0;

    // Десериализовать байты в Project. Бросает FileFormatError::InvalidData
    // при несоответствии схеме. Schema-миграции выполняются CcadArchive'ом
    // ДО вызова deserialize — сюда приходит актуальная current-схема.
    virtual Project deserialize(const std::vector<std::uint8_t>& bytes) const = 0;
};

}  // namespace coupecad::core
