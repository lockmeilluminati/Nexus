import sys
import io
import os
import struct
import json
from PIL import Image, ImageFile

# Prevents crashes on slightly corrupted image headers
ImageFile.LOAD_TRUNCATED_IMAGES = True

def extract_textures(input_path: str):
    print(f"NEXUS SMART EXTRACTOR: Reading {os.path.basename(input_path)}")

    with open(input_path, 'rb') as f: 
        data = f.read()
        
    if len(data) < 12: return False
    magic, version, total_length = struct.unpack_from('<III', data, 0)
    if magic != 0x46546C67: return False

    offset = 12
    json_data = None
    bin_data  = bytearray()

    while offset < len(data):
        if offset + 8 > len(data): break
        chunk_length, chunk_type = struct.unpack_from('<II', data, offset)
        chunk_start = offset + 8
        chunk_end   = chunk_start + chunk_length
        
        if chunk_type == 0x4E4F534A: 
            json_data = data[chunk_start:chunk_end].decode('utf-8')
        elif chunk_type == 0x004E4942: 
            bin_data = data[chunk_start:chunk_end]

        offset = chunk_end
        if offset % 4 != 0: offset += 4 - (offset % 4)

    if json_data is None: return False

    gltf = json.loads(json_data)
    images = gltf.get('images', [])
    buffer_views = gltf.get('bufferViews', [])
    materials = gltf.get('materials', [])
    textures = gltf.get('textures', [])

    if not images: 
        print("No embedded images found.")
        return True

    # Map textures to image indices
    tex_to_img = {}
    for i, t in enumerate(textures):
        if 'source' in t: tex_to_img[i] = t['source']

    # Build smart names based on Material IDs
    img_to_name = {}
    for mat_idx, mat in enumerate(materials):
        raw_name = mat.get('name', f'Mat{mat_idx}')
        safe_mat = "".join([c for c in raw_name if c.isalnum() or c in ('_', '-')]).rstrip()
        if not safe_mat: safe_mat = f'Mat{mat_idx}'

        pbr = mat.get('pbrMetallicRoughness', {})
        
        base_tex = pbr.get('baseColorTexture')
        if base_tex and base_tex.get('index') is not None:
            img_idx = tex_to_img.get(base_tex['index'])
            if img_idx is not None and img_idx not in img_to_name: 
                img_to_name[img_idx] = f"{mat_idx}_Albedo_{safe_mat}"
            
        norm_tex = mat.get('normalTexture')
        if norm_tex and norm_tex.get('index') is not None:
            img_idx = tex_to_img.get(norm_tex['index'])
            if img_idx is not None and img_idx not in img_to_name: 
                img_to_name[img_idx] = f"{mat_idx}_Normal_{safe_mat}"

        mr_tex = pbr.get('metallicRoughnessTexture')
        if mr_tex and mr_tex.get('index') is not None:
            img_idx = tex_to_img.get(mr_tex['index'])
            if img_idx is not None and img_idx not in img_to_name: 
                img_to_name[img_idx] = f"{mat_idx}_MetalRough_{safe_mat}"

    asset_dir = os.path.dirname(os.path.abspath(input_path))

    # Extract, convert to pure PNG, and save
    count = 0
    for img_idx, img_obj in enumerate(images):
        bv_idx = img_obj.get('bufferView')
        if bv_idx is None: continue

        bv = buffer_views[bv_idx]
        bv_offset = bv.get('byteOffset', 0)
        bv_length = bv.get('byteLength', 0)
        raw_bytes = bytes(bin_data[bv_offset : bv_offset + bv_length])

        try:
            pil_img = Image.open(io.BytesIO(raw_bytes))
            mode = pil_img.mode
            
            safe_name = img_to_name.get(img_idx, f"Unassigned_{img_idx}")
            dump_path = os.path.join(asset_dir, f"{safe_name}.png")

            # CRITICAL FIX: Convert all WebP, CMYK, etc to Standard RGB/RGBA PNGs
            if mode in ('RGBA', 'LA', 'P') or (pil_img.format == 'WEBP' and 'A' in mode):
                dump_img = pil_img.convert('RGBA')
            else:
                dump_img = pil_img.convert('RGB')

            dump_img.save(dump_path, 'PNG')
            count += 1
            print(f"  EXTRACTED -> {safe_name}.png")
        except Exception as e:
            print(f"  WARNING: Failed to extract image {img_idx}: {e}")

    print(f"Done. Extracted {count} textures safely.")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 2: sys.exit(1)
    inp = sys.argv[1]
    if not os.path.exists(inp): sys.exit(1)
    extract_textures(inp)
    sys.exit(0)