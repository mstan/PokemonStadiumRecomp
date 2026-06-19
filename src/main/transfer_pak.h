#pragma once

// App-side Transfer Pak glue. The cart model + accessory HLE now live in the
// shared engine (librecomp::gbcart / gbpak.cpp / si_joybus.cpp); this only reads
// the app config and registers carts with the engine.

namespace pkmnstadium::transfer_pak {
    // Load the per-port cart config (launcher.cfg + PSR_TRANSFER_PAK_P*_ROM/_SAVE)
    // and register each cart with librecomp::gbcart. Idempotent.
    void initialize();

    // True iff a cart is mounted in the Transfer Pak on this port.
    bool has_transfer_pak(int port);
}
