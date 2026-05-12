#include "demo_project.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

namespace coupecad::app {

namespace {

void add_panel(core::Cabinet& cab, core::UuidGenerator& gen,
               core::PanelRole role) {
    core::Panel p;
    p.id = gen.next_id<core::PanelIdTag>();
    p.role = role;
    cab.panels.emplace(p.id, p);
}

void add_shelf(core::Cabinet& cab, core::UuidGenerator& gen,
               core::Millimeters height_from_bottom) {
    core::Panel p;
    p.id = gen.next_id<core::PanelIdTag>();
    p.role = core::PanelRole::Shelf;
    p.role_params = core::ShelfParams{
        .height_from_bottom = height_from_bottom,
        .extent = core::ShelfFullWidth{}};
    cab.panels.emplace(p.id, p);
}

}  // namespace

core::Project make_demo_project() {
    auto project = core::Project::create_empty("Demo cabinet");
    auto& cab = project.mutable_cabinet();
    cab.dimensions = {core::Millimeters{1200},
                      core::Millimeters{600},
                      core::Millimeters{2000}};
    auto& gen = project.uuid_gen();
    add_panel(cab, gen, core::PanelRole::Bottom);
    add_panel(cab, gen, core::PanelRole::Top);
    add_panel(cab, gen, core::PanelRole::SideLeft);
    add_panel(cab, gen, core::PanelRole::SideRight);
    add_panel(cab, gen, core::PanelRole::Back);
    add_shelf(cab, gen, core::Millimeters{700});
    add_shelf(cab, gen, core::Millimeters{1400});
    return project;
}

}  // namespace coupecad::app
