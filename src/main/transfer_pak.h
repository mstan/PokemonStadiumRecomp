#pragma once

namespace pkmnstadium::transfer_pak {
    // PSR-specific Transfer Pak config glue. Reads launcher.cfg (per-port GB ROM
    // + save) and registers the carts with the shared engine cart model
    // (librecomp::gbcart). The cart model + libultra accessory HLE now live in
    // librecomp (gbcart.cpp / gbpak.cpp) — this is just the launcher integration.
    void initialize();
    bool has_transfer_pak(int port);
}
