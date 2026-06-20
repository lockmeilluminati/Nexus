
import struct, json, io, sys, os
from PIL import Image, ImageFile
ImageFile.LOAD_TRUNCATED_IMAGES = True

def diagnose_glb(path):
    print(f"\nDIAGNOSTIC: {path}")
    print("="*60)
    with open(path, "rb") as f:
        data = f.read()

    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != 0x46546C67:
        print("ERROR: Not a GLB file")
        return

    offset = 12
    json_chunk = None
    bin_chunk  = None

    while offset < len(data) - 8:
        clen, ctype = struct.unpack_from("<II", data, offset)
        cbytes = data[offset+8 : offset+8+clen]
        if ctype == 0x4E4F534A: json_chunk = cbytes.decode("utf-8")
        elif ctype == 0x004E4942: bin_chunk = cbytes
        offset += 8 + clen
        if offset % 4: offset += 4 - (offset % 4)

    if not json_chunk:
        print("ERROR: No JSON chunk")
        return

    gltf   = json.loads(json_chunk)
    images = gltf.get("images", [])
    bvs    = gltf.get("bufferViews", [])

    print(f"Images in GLB: {len(images)}")
    print()

    broken = []
    for i, img in enumerate(images):
        bv_idx = img.get("bufferView")
        mime   = img.get("mimeType", "unknown")
        if bv_idx is None or not bin_chunk:
            print(f"  [{i}] uri-based image (external) — skipped")
            continue

        bv     = bvs[bv_idx]
        start  = bv.get("byteOffset", 0)
        length = bv.get("byteLength", 0)
        raw    = bin_chunk[start:start+length]

        try:
            pil = Image.open(io.BytesIO(raw))
            mode = pil.mode
            fmt  = pil.format
            prog = pil.info.get("progressive", False)
            status = "OK" if (mode == "RGB" and not prog) else f"BROKEN ({mode}{'+ progressive' if prog else ''})"
            if "BROKEN" in status:
                broken.append(i)
            print(f"  [{i}] bv={bv_idx} mime={mime} size={length}b -> {fmt} {mode} {'progressive' if prog else ''} [{status}]")
        except Exception as e:
            print(f"  [{i}] bv={bv_idx} mime={mime} size={length}b -> UNREADABLE: {e}")
            broken.append(i)

    print()
    if broken:
        print(f"BROKEN IMAGES (will be white in engine): indices {broken}")
        print(f"Run: py nexus_glb_repacker.py \"{path}\"")
    else:
        print("All images are clean — if still white, issue is in material assignment not textures.")

if len(sys.argv) < 2:
    # Auto-scan Envs/ and Chars/ folders
    import glob
    found = glob.glob("Envs/**/*.glb", recursive=True) + glob.glob("Chars/**/*.glb", recursive=True)
    if not found:
        print("Usage: py nexus_glb_diagnostic.py <file.glb>")
        print("Or run from your project folder to auto-scan Envs/ and Chars/")
    for f in found:
        diagnose_glb(f)
else:
    diagnose_glb(sys.argv[1])
