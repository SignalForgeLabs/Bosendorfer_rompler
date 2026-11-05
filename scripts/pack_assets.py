#!/usr/bin/env python3
import os
import sys
import wave

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
    ("BOSENDRF.SF2", "Vienna_290_Full.bin"),
    ("BOSENBRI.SF2", "Vienna_290_Brite.bin"),
    ("IRs/venue/Dry.wav", "Venue_Dry.bin"),
    ("IRs/venue/ConradPrebysConcertHallSeatF111.wav", "ConradPrebysConcertHallSeatF111.bin"),
    ("IRs/venue/MillsGreekTheater.wav", "MillsGreekTheater.bin"),
    ("IRs/venue/NaumburgBandshell.wav", "NaumburgBandshell.bin"),
    ("IRs/venue/SteinmanHall.wav", "SteinmanHall.bin"),
    ("IRs/venue/Space4ArtGallery.wav", "Space4ArtGallery.bin"),
]


def ensure_dry_ir(dry_wav_path):
    if os.path.exists(dry_wav_path):
        return
    os.makedirs(os.path.dirname(dry_wav_path), exist_ok=True)
    with wave.open(dry_wav_path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(44100)
        wf.writeframes(b"\x00\x00\x00\x00" * 64)


def scramble_file(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Error: Missing input asset {input_path}")
        return False

    with open(input_path, "rb") as f_in:
        raw_bytes = bytearray(f_in.read())

    if input_path.lower().endswith(".sf2"):
        # Replace any internal 11-byte or 8-byte ASCII SF2 preset/instrument brand strings
        # with exact-length Vienna 290 equivalents so chunk lengths remain identical.
        for old_bytes, new_bytes in [
            (b"Bosendorfer", b"Vienna 290 "),
            (b"BOSENDORFER", b"VIENNA 290 "),
            (b"bosendorfer", b"vienna 290 "),
            (b"BOSENDRF",    b"VIENNA29"),
            (b"BOSENBRI",    b"VIENNABR"),
        ]:
            raw_bytes = bytearray(bytes(raw_bytes).replace(old_bytes, new_bytes))

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

    ensure_dry_ir(os.path.join(in_dir, "IRs", "venue", "Dry.wav"))

    os.makedirs(out_dir, exist_ok=True)
    valid_outputs = {rel_out for _, rel_out in PRODUCTION_ASSETS}
    for existing in os.listdir(out_dir):
        if existing.endswith(".bin") and existing not in valid_outputs:
            os.remove(os.path.join(out_dir, existing))

    for rel_in, rel_out in PRODUCTION_ASSETS:
        in_file = os.path.join(in_dir, rel_in)
        out_file = os.path.join(out_dir, rel_out)
        if not scramble_file(in_file, out_file):
            sys.exit(1)
