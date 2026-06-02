import re, sys
p = sys.argv[1] if len(sys.argv) > 1 else "build/PokemonStadiumRecomp.exe"
with open(p, "rb") as f:
    data = f.read()
seen = set()
for m in re.finditer(rb"([A-Za-z0-9_\-]+\.dll)\x00", data):
    name = m.group(1).decode().lower()
    if name not in seen:
        seen.add(name)
for n in sorted(seen):
    print(n)
