# Code Review

## Context Summary

- `CoupeCAD` is a cross-platform CAD application for cabinet furniture.
- `src/coupecad/core/` is the Qt-free domain layer: `Project -> Cabinet -> Panel / Hardware / Material`, commands + undo/redo, and `.ccad` serialization.
- This PR is Stage 2: `src/coupecad/geometry/` adds OpenCASCADE-backed shape generation via `build_panel_solid`, `build_hardware_compound`, `build_cabinet_compound`, and a cached `GeometryBuilder` driven by `ChangeSet` invalidation.
- The docs and plans are consistent with the staged rollout: Stage 0 bootstrap, Stage 1 core/logging/undo/I/O, and this branch implements the approved Stage 2 geometry layer.

## PR Review Comments

- All 6 Copilot PR review comments were legitimate against the reviewed PR revision.
- Current status on `HEAD`: resolved.

Resolved items:

- `hardware_shape.cpp`: unknown attachment-panel diagnostics now include concrete context.
- `geometry_builder.cpp`: unknown panel and hardware ids now throw typed `DomainError` values instead of leaking `std::out_of_range`.
- `geometry_builder.h`: cache-lifetime comment now matches the implementation.
- `panel_shape.cpp` and `hardware_shape.cpp`: Stage 2 error codes now follow the repository's lowercase, dot-separated convention or reuse existing stable codes.

## Findings

- No active correctness or API-contract findings remain from the previous review pass.
- `compute_panel_geometry()` no longer leaks `std::bad_variant_access` for role/params mismatches; those cases now surface as typed `DomainError` values and are covered by tests.
- `GeometryBuilder` no longer leaks STL container exceptions for unknown public ids; those cases now surface as typed `DomainError` values and are covered by tests.

## Accepted Tradeoffs

- OCCT types remain part of the geometry public API. This is intentional and matches the Stage 2 design doc.
- `GeometryBuilder::apply_changes()` still invalidates the full hardware cache when panels change. This is a documented conservative performance tradeoff from the Stage 2 spec, not a correctness defect.

## Validation

- Re-ran `ctest --preset default --output-on-failure`
- Result: `264/264` tests passed
