#include "coupecad/core/project.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

Project Project::create_empty(std::string name,
                              std::unique_ptr<UuidGenerator> uuid_gen) {
    Project p;
    p.uuid_gen_ = std::move(uuid_gen);
    p.meta_.name = std::move(name);

    // Один дефолтный материал — чтобы Cabinet прошёл validate().
    Material default_material{
        .id = p.uuid_gen_->next_id<MaterialIdTag>(),
        .name = "Default ChipboardLaminated 16mm",
        .kind = MaterialKind::ChipboardLaminated,
        .default_thickness = Millimeters{16},
        .color_hint = RGBA{220, 220, 220, 255},
    };
    auto material_id = default_material.id;
    p.materials_[material_id] = std::move(default_material);

    // Один пустой Cabinet с разумными дефолтами.
    p.cabinet_.id = p.uuid_gen_->next_id<CabinetIdTag>();
    p.cabinet_.name = "Cabinet";
    p.cabinet_.dimensions = Dimensions{
        .width = Millimeters{2400},
        .depth = Millimeters{600},
        .height = Millimeters{2400},
    };
    p.cabinet_.default_panel_material = material_id;
    p.cabinet_.default_panel_thickness = Millimeters{16};
    p.cabinet_.default_back_thickness = Millimeters{4};

    return p;
}

void Project::validate() const {
    cabinet_.validate();
    for (const auto& [key, m] : materials_) {
        if (key != m.id) {
            throw DomainError{"project.material_key_mismatch",
                              "materials map key != material.id"};
        }
        m.validate();
    }
    for (const auto& [key, spec] : hardware_catalog_) {
        if (key != spec.ref) {
            throw DomainError{"project.hardware_spec_key_mismatch",
                              "hardware_catalog key != spec.ref"};
        }
        spec.validate();
    }
    // Cross-aggregate проверка: cabinet.default_panel_material должен
    // существовать в materials_.
    if (materials_.find(cabinet_.default_panel_material) == materials_.end()) {
        throw DomainError{"project.default_material_missing",
                          "cabinet.default_panel_material is not in materials"};
    }
    // Каждый material_override и edge_banding в панелях должен ссылаться
    // на существующий материал.
    for (const auto& [_, p] : cabinet_.panels) {
        if (p.material_override &&
            materials_.find(*p.material_override) == materials_.end()) {
            throw DomainError{"project.panel_material_missing",
                              "panel.material_override references unknown material"};
        }
        auto check_eb = [&](const std::optional<EdgeBanding>& eb) {
            if (eb && materials_.find(eb->material_id) == materials_.end()) {
                throw DomainError{"project.edge_banding_material_missing",
                                  "panel.edge_banding references unknown material"};
            }
        };
        check_eb(p.edge_banding.front);
        check_eb(p.edge_banding.back);
        check_eb(p.edge_banding.left);
        check_eb(p.edge_banding.right);
    }
    // Каждый HardwareItem.ref должен быть в hardware_catalog.
    for (const auto& [_, h] : cabinet_.hardware) {
        if (hardware_catalog_.find(h.ref) == hardware_catalog_.end()) {
            throw DomainError{"project.hardware_ref_missing",
                              "HardwareItem.ref not found in hardware_catalog"};
        }
    }
}

}  // namespace coupecad::core
