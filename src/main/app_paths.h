#pragma once

// Runtime file locations for the PokemonStadiumRecomp executable.
//
// The shipped build is portable: unzip and run, no install step. Every file the
// program writes at runtime — the rom.cfg / launcher.cfg configs and the crash /
// diagnostic logs — lives next to the executable rather than at any absolute or
// build-tree path. These helpers centralize that so a released binary never
// depends on a developer's directory layout.
//
// This header is included from both C++ and C (extras.c) translation units.

#ifdef __cplusplus

#include <filesystem>
#include <string>

namespace pkmnstadium {

// Directory containing the running executable.
std::filesystem::path exe_dir();

// Absolute path for a runtime-written file (config, log, dump), placed next to
// the executable.
std::filesystem::path app_file(const std::string& name);

} // namespace pkmnstadium

extern "C" {
#endif // __cplusplus

// C-callable variant for C translation units (extras.c): writes the absolute
// path of a runtime file located next to the executable into out[0 .. out_size)
// and returns out. On error returns an empty string. Typical use:
//   char buf[1024];
//   FILE *f = fopen(psr_app_file_c("last_error.log", buf, sizeof buf), "a");
const char *psr_app_file_c(const char *name, char *out, unsigned long out_size);

#ifdef __cplusplus
}
#endif
