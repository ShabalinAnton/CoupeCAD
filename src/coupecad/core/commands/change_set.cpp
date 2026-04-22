#include "coupecad/core/commands/change_set.h"

namespace coupecad::core {

namespace {
template <class V>
void append(V& dst, const V& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}
}  // namespace

void ChangeSet::merge(const ChangeSet& o) {
    append(added_panels, o.added_panels);
    append(removed_panels, o.removed_panels);
    append(updated_panels, o.updated_panels);
    append(added_hardware, o.added_hardware);
    append(removed_hardware, o.removed_hardware);
    append(updated_hardware, o.updated_hardware);
    append(added_materials, o.added_materials);
    append(removed_materials, o.removed_materials);
    append(updated_materials, o.updated_materials);
    if (o.cabinet_changed) cabinet_changed = true;
}

}  // namespace coupecad::core
