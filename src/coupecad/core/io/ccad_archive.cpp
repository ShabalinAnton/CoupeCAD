#include "coupecad/core/io/ccad_archive.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <nlohmann/json.hpp>
#include <zip.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <sstream>

namespace coupecad::core {

namespace {

class SystemClock : public IClock {
public:
    std::chrono::system_clock::time_point now() const override {
        return std::chrono::system_clock::now();
    }
};

std::string iso8601_utc(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

std::vector<std::uint8_t> read_entry(zip_t* z, const char* name) {
    struct zip_stat st;
    if (zip_stat(z, name, 0, &st) < 0) {
        throw MissingManifest{"ccad.missing_entry",
                              std::string{"Archive missing entry: "} + name};
    }
    zip_file_t* f = zip_fopen(z, name, 0);
    if (!f) {
        throw CorruptedArchive{"ccad.entry_open_failed",
                                std::string{"Cannot open entry: "} + name};
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(st.size));
    zip_int64_t got = zip_fread(f, out.data(), out.size());
    zip_fclose(f);
    if (got != static_cast<zip_int64_t>(out.size())) {
        throw CorruptedArchive{"ccad.short_read", name};
    }
    return out;
}

void add_entry(zip_t* z, const char* name,
                const std::vector<std::uint8_t>& bytes) {
    zip_source_t* src = zip_source_buffer(z, bytes.data(), bytes.size(), 0);
    if (!src) {
        throw CorruptedArchive{"ccad.add_source_failed", name};
    }
    if (zip_file_add(z, name, src, ZIP_FL_OVERWRITE) < 0) {
        zip_source_free(src);
        throw CorruptedArchive{"ccad.add_entry_failed", name};
    }
}

}  // namespace

std::unique_ptr<IClock> make_system_clock() {
    return std::make_unique<SystemClock>();
}

std::string CcadArchive::data_file_name(const std::string& encoding) {
    if (encoding == "json") return "project.json";
    throw UnsupportedEncoding{"ccad.unknown_encoding",
                               "Unknown content_encoding: " + encoding};
}

void CcadArchive::save(const Project& project,
                        const std::filesystem::path& path,
                        const ProjectSerializer& serializer,
                        const IClock& clock) {
    auto payload = serializer.serialize(project);
    std::string encoding = serializer.content_encoding();
    std::string data_name = data_file_name(encoding);

    nlohmann::json meta{
        {"schema_version", 1},
        {"content_encoding", encoding},
        {"app_version", "0.1.0"},
        {"created_at", iso8601_utc(clock.now())},
        {"modified_at", iso8601_utc(clock.now())},
    };
    std::string meta_str = meta.dump(2);
    std::vector<std::uint8_t> meta_bytes(meta_str.begin(), meta_str.end());

    int err = 0;
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    zip_t* z = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_EXCL, &err);
    if (!z) {
        throw CorruptedArchive{"ccad.open_for_write_failed",
                                "Cannot open archive for writing: " + path.string()};
    }
    try {
        add_entry(z, "meta.json", meta_bytes);
        add_entry(z, data_name.c_str(), payload);
    } catch (...) {
        zip_discard(z);
        throw;
    }
    if (zip_close(z) < 0) {
        throw CorruptedArchive{"ccad.close_failed",
                                "zip_close failed for: " + path.string()};
    }
}

Project CcadArchive::load(const std::filesystem::path& path,
                            const ProjectSerializer& serializer,
                            const MigrationChain& chain) {
    if (!std::filesystem::exists(path)) {
        throw CorruptedArchive{"ccad.file_not_found", path.string()};
    }
    int err = 0;
    zip_t* z = zip_open(path.string().c_str(), ZIP_RDONLY, &err);
    if (!z) {
        throw CorruptedArchive{"ccad.open_failed", path.string()};
    }
    try {
        auto meta_bytes = read_entry(z, "meta.json");
        nlohmann::json meta;
        try {
            meta = nlohmann::json::parse(meta_bytes.begin(), meta_bytes.end());
        } catch (const nlohmann::json::parse_error& e) {
            throw InvalidData{"ccad.meta_invalid_json", e.what()};
        }
        if (!meta.contains("schema_version") || !meta.contains("content_encoding")) {
            throw MissingManifest{"ccad.meta_fields_missing",
                                   "meta.json missing required fields"};
        }
        std::string encoding = meta.at("content_encoding").get<std::string>();
        if (encoding != serializer.content_encoding()) {
            throw UnsupportedEncoding{"ccad.encoding_mismatch",
                                        "Archive encoding '" + encoding +
                                        "' does not match serializer '" +
                                        serializer.content_encoding() + "'"};
        }
        std::string data_name = data_file_name(encoding);
        auto payload = read_entry(z, data_name.c_str());

        // Парсим payload как JSON, прогоняем миграции, затем передаём
        // serializer'у как bytes после migration.
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(payload.begin(), payload.end());
        } catch (const nlohmann::json::parse_error& e) {
            throw InvalidData{"ccad.payload_parse_failed", e.what()};
        }
        root = chain.run(std::move(root), 1);
        std::string migrated = root.dump(2);
        std::vector<std::uint8_t> migrated_bytes(migrated.begin(), migrated.end());

        zip_close(z);
        return serializer.deserialize(migrated_bytes);
    } catch (...) {
        zip_close(z);
        throw;
    }
}

}  // namespace coupecad::core
