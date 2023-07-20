function __blender_list_scenes -a file
    blender -b $file --python-expr 'import bpy

    for name in [scene.name for scene in list(bpy.data.scenes)]:
        print(name)'
end

function __blender_list_addons
    ls --format single-column /usr/share/blender/scripts/addons --color=never |
        string replace --regex '.py$' ''
end
