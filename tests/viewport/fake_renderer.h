#pragma once

#include "coupecad/renderer/i_renderer.h"

#include <optional>
#include <vector>

namespace coupecad::viewport::testing {

class FakeRenderer : public coupecad::renderer::IRenderer {
public:
    std::vector<core::ChangeSet>            sync_calls;
    std::vector<renderer::CameraState>      set_camera_calls;
    std::vector<renderer::EntityId>         select_calls;
    std::vector<renderer::EntityId>         deselect_calls;
    int                                     clear_selection_count = 0;
    int                                     fit_all_count = 0;
    int                                     rebuild_all_count = 0;
    std::optional<renderer::EntityId>       next_pick_result;
    renderer::ViewportSize                  last_set_viewport{1, 1};

    void sync(const core::ChangeSet& cs) override { sync_calls.push_back(cs); }
    void rebuild_all() override                    { ++rebuild_all_count; }
    void set_camera(const renderer::CameraState& s) override {
        set_camera_calls.push_back(s);
        current_ = s;
    }
    renderer::CameraState camera() const override  { return current_; }
    void fit_all() override                        { ++fit_all_count; }
    std::optional<renderer::EntityId> pick(int, int) override {
        return next_pick_result;
    }
    void select(const renderer::EntityId& id) override {
        select_calls.push_back(id);
        selection_.push_back(id);
    }
    void deselect(const renderer::EntityId& id) override {
        deselect_calls.push_back(id);
    }
    void clear_selection() override {
        ++clear_selection_count;
        selection_.clear();
    }
    std::vector<renderer::EntityId> selection() const override {
        return selection_;
    }
    void set_viewport_size(renderer::ViewportSize s) override {
        last_set_viewport = s;
    }
    renderer::ViewportSize viewport_size() const override {
        return last_set_viewport;
    }
    std::vector<std::uint8_t> render_to_image() override { return {}; }

private:
    renderer::CameraState current_{};
    std::vector<renderer::EntityId> selection_;
};

}  // namespace coupecad::viewport::testing
