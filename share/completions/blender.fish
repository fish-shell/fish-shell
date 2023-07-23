function __blender_list_scenes -a file
    stdbuf --output=0 blender --background test.blend --python-expr 'import bpy

for name in [scene.name for scene in list(bpy.data.scenes)]:
    print(name)' | head --lines=-2 | tail --lines=+6
end

function __blender_list_addons
    ls --format single-column /usr/share/blender/scripts/addons --color=never |
        string replace -r '.py$' ''
end

function __blender_list_engines
    stdbuf --output=0 blender --background --engine help |
        tail --lines=+6 | string replace -r '^\s+' ''
end

function __blender_echo_input_file_name
    echo $argv |
        string split -n ' ' |
        string match -r -v '^-' |
        head --lines=1
end

complete -c blender -s h -l help -d 'show help'
complete -c blender -s v -l version -d 'show version'

complete -c blender -s b -l background -d 'hide UI'
complete -c blender -s a -l render-anim -d 'render frames' -r
complete -c blender -s S -l scene -a '(__blender_list_scenes (commandline -poc))' -n 'test -n (__blender_echo_input_file_name (commandline -poc))' -d 'active scene' -x
complete -c blender -s s -l frame-start -d 'start frame' -x
complete -c blender -s e -l end-start -d 'end frame' -x
complete -c blender -s j -l frame-jump -d 'skipped frame count' -x
complete -c blender -s o -l render-output -d 'render output' -r
complete -c blender -s E -l engine -a '(__blender_list_engines)' -d 'render engine' -x
complete -c blender -s t -l threads -d 'thread count'

complete -c blender -s F -l render-format -a 'TGA RAWTGA JPEG IRIS IRIZ AVIRAW AVIJPEG PNG BMP' -d 'render format' -x
complete -c blender -s x -l use-extension -a 'true false' -d 'whether add a file extension to an end of a file' -x
