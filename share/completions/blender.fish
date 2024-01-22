function __blender_player -d 'Check if -a option has been used, to run animation player'
    return (__fish_contains_opt -s a; and not __fish_contains_opt -s b background)
end

function __blender_echo_input_file_name
    # Find last argument ending in .blend (or .blend1, etc.)
    # This is because a Blender invocation can open multiple blend file
    # sequentially, so we need to find the last one up to this point.
    set -l path (commandline -pxc |
        string match -r '.*\\.blend[0-9]*$' |
        tail --lines=1)
    echo $path
end

function __blender_list_scenes
    blender --background (__blender_echo_input_file_name) --python-expr 'import bpy
for scene in bpy.data.scenes:
    print(f"\t{scene.name}")' 2>/dev/null |
        string replace -r -f '^\t' ''
end

function __blender_list_texts
    blender --background (__blender_echo_input_file_name) --python-expr 'import bpy
for text in bpy.data.texts:
    print(f"\t{text.name}")' 2>/dev/null |
        string replace -r -f '^\t' ''
end

function __blender_list_engines
    blender --background --engine help 2>/dev/null | string replace -r -f '^\t' ''
end

function __blender_list_addons
    blender --background --python-expr 'import addon_utils
for mod in addon_utils.modules():
    print(f"\t{mod.__name__}")' 2>/dev/null |
        string replace -r -f '^\t' ''
end

complete -c blender -n 'not __blender_player' -o h -l help -d 'Show help'
complete -c blender -n 'not __blender_player' -o v -l version -d 'Show version'

# Render Options:
complete -c blender -n 'not __blender_player' -o b -l background -d 'Hide UI'
complete -c blender -n 'not __blender_player' -o a -l render-anim -d 'Render animation'
complete -c blender -n 'not __blender_player; and test -e (__blender_echo_input_file_name)' -o S -l scene -a '(__blender_list_scenes)' -d 'Specify scene' -x
complete -c blender -n 'not __blender_player' -o f -l render-frame -d 'Render specified frame(s)' -x

complete -c blender -n 'not __blender_player' -o s -l frame-start -d 'Specify start frame' -x
complete -c blender -n 'not __blender_player' -o e -l frame-end -d 'Specify end frame' -x
complete -c blender -n 'not __blender_player' -o j -l frame-jump -d 'Skip frame count' -x
complete -c blender -n 'not __blender_player' -o o -l render-output -d 'Specify render output' -r
complete -c blender -n 'not __blender_player' -o E -l engine -a '(__blender_list_engines)' -d 'Render engine' -x
complete -c blender -n 'not __blender_player' -o t -l threads -d 'Specify thread count'

# Format Options:
complete -c blender -n 'not __blender_player' -o F -l render-format -a 'TGA\tTarga
RAWTGA\tTarga\ Raw
JPEG\tJPEG
IRIS\tIRIS
AVIRAW\tAVI\ Raw
AVIJPEG\tAVI\ with\ JPEG\ codec
PNG\tPNG
BMP\tBMP
HDR\tHDR
TIFF\tTIFF
OPEN_EXR\tOpenEXR
OPEN_EXR_MULTILAYER\tOpenEXR\ Multilayer
FFMPEG\tFFmpeg\ Video
CINEON\tCineon
DPX\tDPX
JP2\tJPEG\ 2000
WEBP\tWebP' -d 'Specify render format' -x
complete -c blender -n 'not __blender_player' -o x -l use-extension -a '0\tfalse
1\ttrue' -d 'Whether to add a file extension to an end of a file' -x

# Animation Playback Options:
complete -c blender -n 'not __blender_player' -o a -d 'Run as animation player'

complete -c blender -n __blender_player -o p -x -d 'Specify position and size'
complete -c blender -n __blender_player -o m -d 'Read from disk (do not buffer)'
complete -c blender -n __blender_player -o f -x -d 'Specify FPS to start with'
complete -c blender -n __blender_player -o j -x -d 'Specify frame step'
complete -c blender -n __blender_player -o s -x -d 'Specify start frame'
complete -c blender -n __blender_player -o e -x -d 'Specify end frame'
complete -c blender -n __blender_player -o c -x -d 'Memory in MB for cache'

# Window Options:
complete -c blender -n 'not __blender_player' -o w -l window-border -d 'Show window borders'
complete -c blender -n 'not __blender_player' -o W -l window-fullscreen -d 'Show in fullscreen'
complete -c blender -n 'not __blender_player' -o p -l window-geometry -d 'Specify position and size' -x
complete -c blender -n 'not __blender_player' -o M -l window-maximized -d 'Maximize window'
complete -c blender -n 'not __blender_player' -o con -l start-console -d 'Open console'
complete -c blender -n 'not __blender_player' -l no-native-pixels -d 'Do not use native pixel size'
complete -c blender -n 'not __blender_player' -l no-window-focus -d 'Open unfocused'

# Python Options:
complete -c blender -n 'not __blender_player' -o y -l enable-autoexec -d 'Enable Python scripts automatic execution'
complete -c blender -n 'not __blender_player' -o Y -l disable-autoexec -d 'Disable Python scripts automatic execution'

complete -c blender -n 'not __blender_player' -o P -l python -d 'Specify Python script' -r
complete -c blender -n 'not __blender_player; and test -e (__blender_echo_input_file_name)' -l python-text -a '(__blender_list_texts)' -d 'Specify Python text block' -x
complete -c blender -n 'not __blender_player' -l python-expr -d 'Specify Python expression' -x
complete -c blender -n 'not __blender_player' -l python-console -d 'Open interactive console'
complete -c blender -n 'not __blender_player' -l python-exit-code -d 'Specify Python exit code on exception'
complete -c blender -n 'not __blender_player' -l python-use-system-env -d 'Use system env vars and user site-packages'
complete -c blender -n 'not __blender_player' -l addons -a '(__fish_append , (__blender_list_addons))' -d 'Specify addons' -x

# Logging Options:
complete -c blender -n 'not __blender_player' -l log -d 'Enable logging categories' -x
complete -c blender -n 'not __blender_player' -l log-level -d 'Specify log level' -x
complete -c blender -n 'not __blender_player' -l log-show-basename -d 'Hide file leading path'
complete -c blender -n 'not __blender_player' -l log-show-backtrace -d 'Show backtrace'
complete -c blender -n 'not __blender_player' -l log-show-timestamp -d 'Show timestamp'
complete -c blender -n 'not __blender_player' -l log-file -d 'Specify log file' -r

# Debug Options:
complete -c blender -n 'not __blender_player' -o d -l debug -d 'Enable debugging'
complete -c blender -n 'not __blender_player' -l debug-value -d 'Specify debug value'

complete -c blender -n 'not __blender_player' -l debug-events -d 'Enable debug messages from the event system'
complete -c blender -n 'not __blender_player' -l debug-ffmpeg -d 'Enable debug messages from FFmpeg library'
complete -c blender -n 'not __blender_player' -l debug-handlers -d 'Enable debug messages for event handling'
complete -c blender -n 'not __blender_player' -l debug-libmv -d 'Enable debug messages for libmv library'
complete -c blender -n 'not __blender_player' -l debug-cycles -d 'Enable debug messages for Cycles'
complete -c blender -n 'not __blender_player' -l debug-memory -d 'Enable fully guarded memory allocation and debugging'
complete -c blender -n 'not __blender_player' -l debug-jobs -d 'Enable time profiling for background jobs'
complete -c blender -n 'not __blender_player' -l debug-python -d 'Enable debug messages for Python'
complete -c blender -n 'not __blender_player' -l debug-depsgraph -d 'Enable all debug messages for dependency graph'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-eval -d 'Enable debug messages for dependency graph related on evalution'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-build -d 'Enable debug messages for dependency graph related on its construction'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-tag -d 'Enable debug messages for dependency graph related on tagging'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-no-threads -d 'Enable single treaded evaluation for dependency graph'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-time -d 'Enable debug messages for dependency graph related on timing'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-pretty -d 'Enable colors for dependency graph debug messages'
complete -c blender -n 'not __blender_player' -l debug-depsgraph-uuid -d 'Enable virefication for dependency graph session-wide identifiers'
complete -c blender -n 'not __blender_player' -l debug-ghost -d 'Enable debug messages for Ghost'
complete -c blender -n 'not __blender_player' -l debug-wintab -d 'Enable debug messages for Wintab'
complete -c blender -n 'not __blender_player' -l debug-gpu -d 'Enable GPU debug context and infromation for OpenGL'
complete -c blender -n 'not __blender_player' -l debug-gpu-force-workarounds -d 'Enable workarounds for typical GPU issues'
complete -c blender -n 'not __blender_player' -l debug-wm -d 'Enable debug messages for window manager'
complete -c blender -n 'not __blender_player' -l debug-xr -d 'Enable debug messages for virtual reality contexts'
complete -c blender -n 'not __blender_player' -l debug-xr-time -d 'Enable debug messages for virtual reality frame rendering times'
complete -c blender -n 'not __blender_player' -l debug-all -d 'Enable all debug messages'
complete -c blender -n 'not __blender_player' -l debug-io -d 'Enable debug for I/O'
complete -c blender -n 'not __blender_player' -l debug-fpe -d 'Enable floating point exceptions'
complete -c blender -n 'not __blender_player' -l debug-exit-on-error -d 'Exit on internal error'
complete -c blender -n 'not __blender_player' -l debug-freestyle -d 'Enable debug messages for Freestyle'
complete -c blender -n 'not __blender_player' -l disable-crash-handler -d 'Disable crash handler'
complete -c blender -n 'not __blender_player' -l disable-abort-handler -d 'Disable abort handler'
complete -c blender -n 'not __blender_player' -l verbose -d 'Specify logging verbosity level' -x

# GPU Options:
complete -c blender -n 'not __blender_player' -l gpu-backend -a 'vulkan metal opengl' -d 'Specify GPU backend' -x

# Misc Options:
complete -c blender -n 'not __blender_player' -l open-last -d 'Open the most recent .blend file'
complete -c blender -n 'not __blender_player' -l app-template -a default -d 'Specify app template' -x
complete -c blender -n 'not __blender_player' -l factory-startup -d 'Do not read startup.blend'
complete -c blender -n 'not __blender_player' -l enable-event-simulate -d 'Enable event simulation'

complete -c blender -n 'not __blender_player' -l env-system-datafiles -d 'Set BLENDER_SYSTEM_DATAFILES variable' -r
complete -c blender -n 'not __blender_player' -l env-system-scripts -d 'Set BLENDER_SYSTEM_SCRIPTS variable' -r
complete -c blender -n 'not __blender_player' -l env-system-python -d 'Set BLENDER_SYSTEM_PYTHON variable' -r

complete -c blender -n 'not __blender_player' -o noaudio -d 'Disable sound'
complete -c blender -n 'not __blender_player' -o setaudio -a 'None SDL OpenAL CoreAudio JACK PulseAudio WASAPI' -d 'Specify sound device' -x

complete -c blender -n 'not __blender_player' -o r -l register -d 'Register .blend extension for current user'
complete -c blender -n 'not __blender_player' -l register-allusers -d 'Register .blend extension for all users'
complete -c blender -n 'not __blender_player' -l unregister -d 'Unregister .blend extension for current user'
complete -c blender -n 'not __blender_player' -l unregister-allusers -d 'Unregister .blend extension for all users'

complete -c blender -n 'not __blender_player' -o - -d 'End option processing, following arguments passed to python'
