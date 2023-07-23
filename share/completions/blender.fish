function __blender_list_scenes -a file
    stdbuf --output=0 blender --background test.blend --python-expr 'import bpy

for name in [scene.name for scene in list(bpy.data.scenes)]:
    print(name)' | head --lines=-2 | tail --lines=+6
end

function __blender_list_addons
    ls --format single-column /usr/share/blender/scripts/addons --color=never |
        string replace -r '.py$' ''
end
