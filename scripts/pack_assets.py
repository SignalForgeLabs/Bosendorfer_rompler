#!/usr/bin/env python3
import os
import sys

# Rolling 32-byte compile key (keep identical in your C++ header)
CIPHER_KEY = b"SFALabs_AudioShield_2026_AJ_Key"

PRODUCTION_ASSETS = [
    ("stage_grand.png", "stage_grand.bin"),
    ("piano_harp.png", "piano_harp.bin"),
    ("Prototype_look_and_feel.png", "Prototype_look_and_feel.bin"),
    ("Neumann_KM184_L.png", "Neumann_KM184_L.bin"),
    ("Neuman_KM184_R.png", "Neuman_KM184_R.bin"),
    ("Mic_Neumann_KM184.wav", "Mic_Neumann_KM184.bin"),
    ("Neuman_TLM_L.png", "Neuman_TLM_L.bin"),
    ("Neuman_TLM_R.png", "Neuman_TLM_R.bin"),
    ("Mic_Neumann_TLM103.wav", "Mic_Neumann_TLM103.bin"),
    ("Neuman_U87-L.png", "Neuman_U87-L.bin"),
    ("Neuman_U87-R.png", "Neuman_U87-R.bin"),
    ("Mic_Neumann_U87.wav", "Mic_Neumann_U87.bin"),
    ("RCA_Ribbon_L.png", "RCA_Ribbon_L.bin"),
    ("RCA_Ribbon_R.png", "RCA_Ribbon_R.bin"),
    ("RCA 44BX NEVE.wav", "RCA 44BX NEVE.bin"),
    ("BOSENDRF.SF2", "BOSENDRF.bin"),
    ("BOSENBRI.SF2", "BOSENBRI.bin"),
    ("IRs/Room_ViennaKonzerthaus.wav", "Room_ViennaKonzerthaus.bin"),
    ("IRs/Room_MapleStage40ft.wav", "Room_MapleStage40ft.bin"),
    ("IRs/Room_ScoringStudioHall.wav", "Room_ScoringStudioHall.bin"),
]


def scramble_file(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Error: Missing input asset {input_path}")
        return False

    with open(input_path, "rb") as f_in:
        raw_bytes = bytearray(f_in.read())

    key_len = len(CIPHER_KEY)
    for i in range(len(raw_bytes)):
        raw_bytes[i] ^= CIPHER_KEY[i % key_len]

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "wb") as f_out:
        f_out.write(raw_bytes)

    print(f"[Shielded] {os.path.basename(input_path)} -> {os.path.basename(output_path)}")
    return True


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: pack_assets.py <input_dir> <output_dir>")
        sys.exit(1)

    in_dir = sys.argv[1]
    out_dir = sys.argv[2]

    for rel_in, rel_out in PRODUCTION_ASSETS:
        in_file = os.path.join(in_dir, rel_in)
        out_file = os.path.join(out_dir, rel_out)
        if not scramble_file(in_file, out_file):
            sys.exit(1)

