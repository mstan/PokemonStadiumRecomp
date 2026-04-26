/*
 * pokestadium_render.h — RT64-backed RendererContext for PokemonStadiumRecomp.
 *
 * Adapted from Zelda64Recomp's zelda_render.h. Strips the texture-pack
 * machinery (which depends on the mod loader subsystem we don't ship)
 * and the recompui dependency. Otherwise the RT64 setup mirrors
 * Zelda64Recomp's proven configuration.
 */

#ifndef POKESTADIUM_RENDER_H
#define POKESTADIUM_RENDER_H

#include <memory>

#include "ultramodern/renderer_context.hpp"

namespace RT64 {
    struct Application;
}

namespace pokestadium {
namespace renderer {

class RT64Context final : public ultramodern::renderer::RendererContext {
public:
    RT64Context(uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool debug);
    ~RT64Context() override;

    bool valid() override { return app != nullptr; }

    bool update_config(const ultramodern::renderer::GraphicsConfig& old_config,
                       const ultramodern::renderer::GraphicsConfig& new_config) override;
    void enable_instant_present() override;
    void send_dl(const OSTask* task) override;
    void update_screen() override;
    void shutdown() override;
    uint32_t get_display_framerate() const override;
    float get_resolution_scale() const override;

private:
    std::unique_ptr<RT64::Application> app;
};

std::unique_ptr<ultramodern::renderer::RendererContext>
create_render_context(uint8_t* rdram,
                      ultramodern::renderer::WindowHandle window_handle,
                      bool developer_mode);

} // namespace renderer
} // namespace pokestadium

#endif
