import json

with open('F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json', 'r') as f:
    r = json.load(f)

for k in ('audio_afx_in', 'audio_afx_out', 'audio_mbp_pre', 'audio_mbp_disp',
         'audio_mbp_chain', 'interp_probe', 'gdl_submit', 'gdl_walk'):
    print(f'=== {k} ===')
    print(json.dumps(r.get(k), indent=2)[:2000])
    print()
