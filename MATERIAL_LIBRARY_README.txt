NEXUS 3D — CHARACTER MATERIAL LIBRARY
======================================

HOW IT WORKS
------------
The Quaternius Universal Animation Library characters are intentionally
untextured skeleton rigs. This is by design — you apply your own skin.

Drop any .png or .jpg texture into:

    Materials/Characters/

The filename becomes the preset name in the panel.
Example:  light_skin.png  →  shows as "light skin" in the library.

The engine auto-creates this folder on first launch.


RECOMMENDED FREE TEXTURE SOURCES
---------------------------------

1. AmbientCG  (ambientcg.com)
   - 100% CC0, no attribution required
   - Search: "skin" — grab the 1K PNG versions
   - These are tileable but work fine as flat character tones

2. 3DTextures.me  (3dtextures.me)
   - Free human skin atlases
   - Download the Diffuse/Albedo map only (ignore normal/roughness,
     your engine uses Raylib's default shader)

3. textures.com
   - Free tier: 15 downloads/day
   - Search "human skin" under the Organic category

4. Make your own in 5 seconds (Photoshop / GIMP / Paint.NET):
   - New image 1024x1024
   - Fill with a skin tone hex:
       Light:   #F1C27D
       Medium:  #C68642
       Dark:    #8D5524
       Fantasy: #8FBC8F  (orc green)
       Undead:  #B0C4A0  (zombie pale)
   - Export as PNG
   - This is honestly the best option for a stylized game engine!

5. Blender users:
   - Any baked diffuse texture from your character works perfectly
   - Export at 1024x1024 PNG


TIPS
----
- 1024x1024 is the ideal size — matches the engine's 16-bit limits
- The albedo is applied to ALL material slots on the character mesh
- You can have as many presets as you want — they show as a thumbnail grid
- Click a thumbnail to instantly preview on the selected character
- This works on ANY GLB/GLTF character, not just Quaternius rigs


WIRING INTO main.cpp
--------------------

  // 1. Include
  #include "nexus_material_library.h"

  // 2. Global instance (near your other globals)
  NexusMaterialLibrary matLibrary;
  bool matLibraryOpen = false;

  // 3. After InitWindow()
  matLibrary.ScanAndLoad();

  // 4. In your ImGui menu bar
  if (ImGui::MenuItem("Material Library")) matLibraryOpen = true;

  // 5. In your ImGui draw section (pass your currently selected SceneObject)
  matLibrary.DrawPanel(scene[selectedIndex], matLibraryOpen);

  // 6. On shutdown before CloseWindow()
  matLibrary.Unload();
