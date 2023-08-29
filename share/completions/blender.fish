function __blender_list_scenes -a file
    blender --background $file --python-expr 'import bpy

for name in [scene.name for scene in list(bpy.data.scenes)]:
    print(f"\t{name}")' |
        string replace -r -f '^\s+' ''
end

function __blender_list_addons
    path basename /usr/share/blender/scripts/addons/*.py |
        path change-extension ''
end

function __blender_list_engines
    blender --background --engine help | string replace -r -f '^\s+' ''
end

function __blender_echo_input_file_name
    echo $argv |
        string split -n ' ' |
        string match -r -v '^-' |
        head --lines=1
end

function __blender_complete_addon_list
    set -l previous_token (commandline -oc)[-1]
    set -l current_token (commandline -t)

    if test "$previous_token" = --addons
        __blender_list_addons |
            string replace -r '^' $current_token |
            string replace -r '$' ','
    end
end

complete -c blender -s h -l help -d 'show help'
complete -c blender -s v -l version -d 'show version'

complete -c blender -s b -l background -d 'hide UI'
complete -c blender -s a -l render-anim -d 'specify render frames' -r
complete -c blender -s S -l scene -a '(__blender_list_scenes (commandline -poc))' -n 'test -n (__blender_echo_input_file_name (commandline -poc))' -d 'specify scene' -x
complete -c blender -s s -l frame-start -d 'specify start frame' -x
complete -c blender -s e -l end-start -d 'specify end frame' -x
complete -c blender -s j -l frame-jump -d 'skip frame count' -x
complete -c blender -s o -l render-output -d 'specify render output' -r
complete -c blender -s E -l engine -a '(__blender_list_engines)' -d 'render engine' -x
complete -c blender -s t -l threads -d 'specify thread count'

complete -c blender -s F -l render-format -a 'TGA RAWTGA JPEG IRIS IRIZ AVIRAW AVIJPEG PNG BMP' -d 'specify render format' -x
complete -c blender -s x -l use-extension -a 'true false' -d 'whether add a file extension to an end of a file' -x

complete -c blender -s a -d 'animation playback options' -x

complete -c blender -s w -l window-border -d 'show window borders'
complete -c blender -s W -l window-fullscreen -d 'show in fullscreen'
complete -c blender -s p -l window-geometry -d 'specify position and size' -x
complete -c blender -s M -l window-maximized -d 'maximize window'
complete -c blender -o con -l start-console -d 'open console'
complete -c blender -l no-native-pixels -d 'do not use native pixel size'
complete -c blender -l no-native-pixels -d 'open unfocused'

complete -c blender -s y -l enable-autoexec -d 'enable Python scripts automatic execution'
complete -c blender -s Y -l disable-autoexec -d 'disable Python scripts automatic execution'
complete -c blender -s P -l python -d 'specify Python script' -r
complete -c blender -l python-text -d 'specify Python text block' -x
complete -c blender -l python-expr -d 'specify Python expression' -x
complete -c blender -l python-console -d 'open interactive console'
complete -c blender -l python-exit-code -d 'specify Python exit code on exception'
complete -c blender -l addons -a '(__blender_complete_addon_list)' -d 'specify addons' -x

complete -c blender -l log -d 'enable logging categories' -x
complete -c blender -l log-level -d 'specify log level' -x
complete -c blender -l log-show-basename -d 'hide file leading path'
complete -c blender -l log-show-backtrace -d 'show backtrace'
complete -c blender -l log-show-timestamp -d 'show timestamp'
complete -c blender -l log-file -d 'specify log file' -r

complete -c blender -s d -l debug -d 'enable debugging'
complete -c blender -l debug-value -d 'specify debug value'
complete -c blender -l debug-events -d 'enable debug messages'
complete -c blender -l debug-ffmpeg -d 'enable debug messages from FFmpeg library'
complete -c blender -l debug-handlers -d 'enable debug messages for event handling'
complete -c blender -l debug-libmv -d 'enable debug messages for libmv library'
complete -c blender -l debug-cycles -d 'enable debug messages for Cycles'
complete -c blender -l debug-memory -d 'enable fully guarded memory allocation and debugging'
complete -c blender -l debug-jobs -d 'enable time profiling for background jobs'
complete -c blender -l debug-python -d 'enable debug messages for Python'
complete -c blender -l debug-depsgraph -d 'enable all debug messages for dependency graph'
complete -c blender -l debug-depsgraph-evel -d 'enable debug messages for dependency graph related on evalution'
complete -c blender -l debug-depsgraph-build -d 'enable debug messages for dependency graph related on its construction'
complete -c blender -l debug-depsgraph-tag -d 'enable debug messages for dependency graph related on tagging'
complete -c blender -l debug-depsgraph-no-threads -d 'enable single treaded evaluation for dependency graph'
complete -c blender -l debug-depsgraph-time -d 'enable debug messages for dependency graph related on timing'
complete -c blender -l debug-depsgraph-time -d 'enable colors for dependency graph debug messages'
complete -c blender -l debug-depsgraph-uuid -d 'enable virefication for dependency graph session-wide identifiers'
complete -c blender -l debug-ghost -d 'enable debug messages for Ghost'
complete -c blender -l debug-wintab -d 'enable debug messages for Wintab'
complete -c blender -l debug-gpu -d 'enable GPU debug context and infromation for OpenGL'
complete -c blender -l debug-gpu-force-workarounds -d 'enable workarounds for typical GPU issues'
complete -c blender -l debug-gpu-disable-ssbo -d 'disable shader storage buffer objects'
complete -c blender -l debug-gpu-renderdoc -d 'enable Renderdoc integration'
complete -c blender -l debug-wm -d 'enable debug messages for window manager'
complete -c blender -l debug-xr -d 'enable debug messages for virtual reality contexts'
complete -c blender -l debug-xr-time -d 'enable debug messages for virtual reality frame rendering times'
complete -c blender -l debug-all -d 'enable all debug messages'
complete -c blender -l debug-io -d 'enable debug for I/O'
complete -c blender -l debug-exit-on-error -d 'whether exit on internal error'
complete -c blender -l disable-crash-handler -d 'disable crash handler'
complete -c blender -l disable-abort-handler -d 'disable abort handler'
complete -c blender -l verbose -d 'specify logging verbosity level' -x

complete -c blender -l gpu-backend -a 'vulkan metal opengl' -d 'specify GPI backend' -x

complete -c blender -l open-last -d 'open the most recent .blend file'
complete -c blender -l open-last -a 'default' -d 'specify app template' -r
complete -c blender -l factory-startup -d 'do not read startup.blend'
complete -c blender -l enable-event-simulate -d 'enable event simulation'
complete -c blender -l env-system-datafiles -d 'set BLENDER_SYSTEM_DATAFILES variable'
complete -c blender -l env-system-scripts -d 'set BLENDER_SYSTEM_SCRIPTS variable'
complete -c blender -l env-system-python -d 'set BLENDER_SYSTEM_PYTHON variable'
complete -c blender -o noaudio -d 'disable sound'
complete -c blender -o setaudio -a 'None SDL OpenAL CoreAudio JACK PulseAudio WASAPI' -d 'specify sound device' -x
complete -c blender -s R -d 'register .blend extension'
complete -c blender -s r -d 'silently register .blend extension'

