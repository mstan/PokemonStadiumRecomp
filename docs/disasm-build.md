# Building pret/pokestadium on Windows

The disasm is the upstream of our recomp pipeline — it produces
`disasm/build/pokestadium-us.elf`, which N64Recomp and Ghidra both
consume. Without this ELF, neither pipeline has section addresses
or symbols.

pret's Makefile is Debian/Ubuntu-shaped: it expects
`mips-linux-gnu-as`, `mips-linux-gnu-ld`, `make`, `python3`, and
the Python packages in `disasm/requirements.txt`. Native Windows
doesn't ship a MIPS GNU toolchain in any of the usual package
managers, so you have three options.

## Option 1 — WSL2 (recommended)

Cleanest path. The disasm builds out of the box in Ubuntu under
WSL2; only the Python packages and the MIPS toolchain need
installing.

```bash
# In a WSL Ubuntu shell:
sudo apt update
sudo apt install -y \
    make git build-essential \
    binutils-mips-linux-gnu \
    python3 python3-pip python3-venv

# WSL can mount Windows drives at /mnt/f/...
cd /mnt/f/Projects/PokemonStadiumRecomp/disasm

# pret recommends a venv:
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Stage baserom (already done by setup.sh on Windows side, but
# verify the file is visible from WSL):
ls -la baseroms/us/baserom.z64

# Build:
make init    # extracts assets, generates linker scripts
make         # compiles + links pokestadium-us.z64 + pokestadium.elf

# Verify byte-identical rebuild:
md5sum build/pokestadium-us.z64
# expected: ed1378bc12115f71209a77844965ba50
```

The ELF lands at `disasm/build/pokestadium-us.elf` — visible from
both WSL (`/mnt/f/...`) and the Windows side, so N64Recomp picks it
up directly.

**Performance note:** `make init` does a lot of disassembly + asset
extraction work on the staged ROM. First run takes 5–15 minutes
depending on disk speed. Subsequent `make` runs are incremental.

## Option 2 — Docker

Works fine if you already have Docker Desktop. Less common in N64
decomp circles than WSL, so you'll be on your own a bit more.

```dockerfile
# Dockerfile — disasm-builder
FROM ubuntu:22.04
RUN apt update && apt install -y \
    make git build-essential \
    binutils-mips-linux-gnu \
    python3 python3-pip python3-venv
WORKDIR /work
```

Build and use:
```bash
docker build -t pokestadium-disasm -f Dockerfile .
docker run --rm -v "$(pwd):/work" \
  pokestadium-disasm \
  bash -c "cd disasm && pip install -r requirements.txt && make init && make"
```

## Option 3 — MSYS2 with msys2-cross-mips (advanced)

Native Windows path via MSYS2's MinGW environment. Toolchain
availability fluctuates and pret's Makefile sometimes assumes
GNU/Linux-isms (e.g., specific shell behavior, `/dev/null`). Plan
on patching the Makefile.

Not recommended unless WSL2 is unavailable for policy reasons.

## After a successful build

The build produces:
- `disasm/build/pokestadium-us.z64` — should md5 to
  `ed1378bc12115f71209a77844965ba50`. If it doesn't, the ROM
  staged at `disasm/baseroms/us/baserom.z64` is wrong revision.
- `disasm/build/pokestadium-us.elf` — what we feed N64Recomp.
- `disasm/build/pokestadium-us.map` — linker map, useful for
  Ghidra symbol cross-reference.
- `disasm/build/lib/...` — intermediate libraries; ignore.

Once the ELF exists, point `game.toml`'s `[input].elf` at it
(already done — `disasm/build/pokestadium-us.elf`) and the
N64Recomp pipeline can run.

## Troubleshooting

- **"mips-linux-gnu-as: command not found"** — toolchain not
  installed. Re-run `sudo apt install binutils-mips-linux-gnu`.
- **"baserom.z64 checksum mismatch"** — wrong ROM revision.
  Verify v1.0 with MD5 `ed1378bc12115f71209a77844965ba50`.
- **Python install failures on `splat64[mips]`** — the package
  has a few platform-specific deps. Use `pip install
  --no-build-isolation` if a wheel is unavailable for your Python
  version, or install a pre-built MIPS rabbitizer separately.
- **`make init` halts on yay0 or jpeg asset** — pret occasionally
  bumps asset tooling versions. `cd disasm && git pull` to sync.
- **Slow first build** — normal. Asset extraction is one-time.
  `make clean && make` rebuilds in 1–2 minutes on subsequent runs.

## Why we don't ship a Windows-native build path

The MIPS GNU toolchain isn't packaged for native Windows in any
mainstream way that pret's Makefile reliably consumes. WSL2 is
free, ships with Windows, and matches the environment pret tests
on. The minutes saved fighting MSYS2 don't pay off versus a
one-time WSL setup.
