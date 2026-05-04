#include "coupecad/geometry/hardware_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <gtest/gtest.h>

namespace {

using namespace coupecad::core;
using coupecad::geometry::build_hardware_compound;

// Фикстура: проект с шкафом 800×500×2000, одной SideLeft панелью,
// одной hardware-spec "hinge.blum.110" (50×30×20 мм) и одним HardwareItem
// с одним attachment к этой панели в локальной точке (10, 20, 30).
struct Fixture {
    Project project = Project::create_empty("test", make_seeded_uuid_generator(11));
    PanelId side_left_id;
    HardwareRef ref;
    HardwareItemId item_id;

    Fixture() {
        auto& cab = project.mutable_cabinet();
        cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
        cab.default_panel_material = project.materials().begin()->first;

        Panel side;
        side.id = PanelId{project.uuid_gen().next()};
        side.role = PanelRole::SideLeft;
        side_left_id = side.id;
        cab.panels.emplace(side.id, std::move(side));

        ref = HardwareRef{"hinge.blum.110"};
        HardwareSpec spec;
        spec.ref = ref;
        spec.kind = HardwareKind::Hinge;
        spec.name = "Blum 110";
        spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
        project.mutable_hardware_catalog().emplace(ref, std::move(spec));

        HardwareItem item;
        item.id = HardwareItemId{project.uuid_gen().next()};
        item.ref = ref;
        item.attachments.push_back(
            PanelAttachment{side_left_id,
                            Vec3{Millimeters{10}, Millimeters{20}, Millimeters{30}},
                            Quat::identity()});
        item_id = item.id;
        cab.hardware.emplace(item.id, std::move(item));
    }
};

std::size_t count_solids(const TopoDS_Compound& c) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(c, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

}  // namespace

TEST(HardwareShapeTest, SingleAttachment_OneSolidInCompound) {
    Fixture fx;
    const auto& item = fx.project.cabinet().hardware.at(fx.item_id);
    const TopoDS_Compound compound = build_hardware_compound(fx.project, item);

    EXPECT_EQ(compound.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_solids(compound), 1u);
}

TEST(HardwareShapeTest, SingleAttachment_BBoxAtWorldPosition) {
    Fixture fx;
    const auto& item = fx.project.cabinet().hardware.at(fx.item_id);
    const TopoDS_Compound compound = build_hardware_compound(fx.project, item);

    Bnd_Box bbox;
    BRepBndLib::Add(compound, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // SideLeft panel origin = (0,0,0), orientation identity.
    // Attachment local (10, 20, 30), bbox (50, 30, 20).
    // World box: from (10, 20, 30) to (60, 50, 50).
    EXPECT_NEAR(xmin, 10.0, 1e-6);
    EXPECT_NEAR(ymin, 20.0, 1e-6);
    EXPECT_NEAR(zmin, 30.0, 1e-6);
    EXPECT_NEAR(xmax, 60.0, 1e-6);
    EXPECT_NEAR(ymax, 50.0, 1e-6);
    EXPECT_NEAR(zmax, 50.0, 1e-6);
}

TEST(HardwareShapeTest, MultipleAttachments_OneSolidPerAttachment) {
    Fixture fx;
    auto& item = fx.project.mutable_cabinet().hardware.at(fx.item_id);
    item.attachments.push_back(
        PanelAttachment{fx.side_left_id,
                        Vec3{Millimeters{200}, Millimeters{20}, Millimeters{30}},
                        Quat::identity()});

    const TopoDS_Compound compound = build_hardware_compound(fx.project, item);
    EXPECT_EQ(count_solids(compound), 2u);
}

TEST(HardwareShapeTest, UnknownSpecRef_ThrowsDomainError) {
    Fixture fx;
    auto& item = fx.project.mutable_cabinet().hardware.at(fx.item_id);
    item.ref = HardwareRef{"does.not.exist"};

    try {
        (void)build_hardware_compound(fx.project, item);
        FAIL() << "Expected DomainError";
    } catch (const DomainError& ex) {
        EXPECT_EQ(ex.code(), "project.hardware_ref_missing");
    }
}

TEST(HardwareShapeTest, UnknownAttachmentPanel_ThrowsDomainError) {
    Fixture fx;
    auto& item = fx.project.mutable_cabinet().hardware.at(fx.item_id);
    auto orphan = make_seeded_uuid_generator(99);
    item.attachments[0].panel_id = PanelId{orphan->next()};

    try {
        (void)build_hardware_compound(fx.project, item);
        FAIL() << "Expected DomainError";
    } catch (const DomainError& ex) {
        EXPECT_EQ(ex.code(), "cabinet.hardware_unknown_panel");
        EXPECT_NE(std::string{ex.what()}.find(item.attachments[0].panel_id.to_string()),
                  std::string::npos);
    }
}
