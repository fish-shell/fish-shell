function __blender_echo_input_file_name
    # Find last argument ending in .blend (or .blend1, etc.)
    # This is because a Blender invocation can open multiple blend file
    # sequentially, so we need to find the last one up to this point.
    set -l path (commandline -poc |
        string match -r '.*\\.blend[0-9]*$' |
        tail --lines=1)
    # Using eval to expand ~ and variables specified on the commandline.
    eval echo $path
end

function __blender_list_scenes
    blender --background (__blender_echo_input_file_name) --python-expr 'import bpy
for scene in bpy.data.scenes:
    print(f"\t{scene.name}")' |
        string replace -r -f '^\t' ''
end

function __blender_list_texts
    blender --background (__blender_echo_input_file_name) --python-expr 'import bpy
for text in bpy.data.texts:
    print(f"\t{text.name}")' |
        string replace -r -f '^\t' ''
end

function __blender_list_engines
    blender --background --engine help | string replace -r -f '^\t' ''
end

function __blender_list_addons
    blender --background --python-expr 'import addon_utils
for mod in addon_utils.modules():
    print(f"\t{mod.__name__}")' |
        string replace -r -f '^\t' ''
end

function __blender_complete_addon_list
    set -l current_token (commandline -t | string replace -r '[^,]*$' '')
    set -l addons (__blender_list_addons)
    for a in $addons
        echo "$current_token$a,"
    end
end

complete -c blender -o h -l help -d 'show help'
complete -c blender -o v -l version -d 'show version'

# Render Options:
complete -c blender -o b -l background -d 'hide UI'
complete -c blender -o a -l render-anim -d 'specify render frames' -r
complete -c blender -o S -l scene -n 'test -e (__blender_echo_input_file_name)' -a '(__blender_list_scenes)' -d 'specify scene' -x
complete -c blender -o f -l render-frame -d "render specified frame(s)" -x

complete -c blender -o s -l frame-start -d 'specify start frame' -x
complete -c blender -o e -l end-start -d 'specify end frame' -x
complete -c blender -o j -l frame-jump -d 'skip frame count' -x
complete -c blender -o o -l render-output -d 'specify render output' -r
complete -c blender -o E -l engine -a '(__blender_list_engines)' -d 'render engine' -x
complete -c blender -o t -l threads -d 'specify thread count'

# Format Options:
complete -c blender -o F -l render-format -a 'TGA\tTarga
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
WEBP\tWebP' -d 'specify render format' -x
complete -c blender -o x -l use-extension -a '0\tfalse
1\ttrue' -d 'whether to add a file extension to an end of a file' -x

# Animation Playback Options:
complete -c blender -o a -d 'run as animation player' -x

# Window Options:
complete -c blender -o w -l window-border -d 'show window borders'
complete -c blender -o W -l window-fullscreen -d 'show in fullscreen'
complete -c blender -o p -l window-geometry -d 'specify position and size' -x
complete -c blender -o M -l window-maximized -d 'maximize window'
complete -c blender -o con -l start-console -d 'open console'
complete -c blender -l no-native-pixels -d 'do not use native pixel size'
complete -c blender -l no-native-pixels -d 'open unfocused'

# Python Options:
complete -c blender -o y -l enable-autoexec -d 'enable Python scripts automatic execution'
complete -c blender -o Y -l disable-autoexec -d 'disable Python scripts automatic execution'

complete -c blender -o P -l python -d 'specify Python script' -r
complete -c blender -l python-text -n 'test -e (__blender_echo_input_file_name)' -a '(__blender_list_texts)' -d 'specify Python text block' -x
complete -c blender -l python-expr -d 'specify Python expression' -x
complete -c blender -l python-console -d 'open interactive console'
complete -c blender -l python-exit-code -d 'specify Python exit code on exception'
complete -c blender -l python-use-system-env -d 'use system env vars and user site-packages'
complete -c blender -l addons -a '(__blender_complete_addon_list)' -d 'specify addons' -x

# Logging Options:
complete -c blender -l log -d 'enable logging categories' -x
complete -c blender -l log-level -d 'specify log level' -x
complete -c blender -l log-show-basename -d 'hide file leading path'
complete -c blender -l log-show-backtrace -d 'show backtrace'
complete -c blender -l log-show-timestamp -d 'show timestamp'
complete -c blender -l log-file -d 'specify log file' -r

# Debug Options:
complete -c blender -o d -l debug -d 'enable debugging'
complete -c blender -l debug-value -d 'specify debug value'

complete -c blender -l debug-events -d 'enable debug messages from the event system'
complete -c blender -l debug-ffmpeg -d 'enable debug messages from FFmpeg library'
complete -c blender -l debug-handlers -d 'enable debug messages for event handling'
complete -c blender -l debug-libmv -d 'enable debug messages for libmv library'
complete -c blender -l debug-cycles -d 'enable debug messages for Cycles'
complete -c blender -l debug-memory -d 'enable fully guarded memory allocation and debugging'
complete -c blender -l debug-jobs -d 'enable time profiling for background jobs'
complete -c blender -l debug-python -d 'enable debug messages for Python'
complete -c blender -l debug-depsgraph -d 'enable all debug messages for dependency graph'
complete -c blender -l debug-depsgraph-eval -d 'enable debug messages for dependency graph related on evalution'
complete -c blender -l debug-depsgraph-build -d 'enable debug messages for dependency graph related on its construction'
complete -c blender -l debug-depsgraph-tag -d 'enable debug messages for dependency graph related on tagging'
complete -c blender -l debug-depsgraph-no-threads -d 'enable single treaded evaluation for dependency graph'
complete -c blender -l debug-depsgraph-time -d 'enable debug messages for dependency graph related on timing'
complete -c blender -l debug-depsgraph-pretty -d 'enable colors for dependency graph debug messages'
complete -c blender -l debug-depsgraph-uuid -d 'enable virefication for dependency graph session-wide identifiers'
complete -c blender -l debug-ghost -d 'enable debug messages for Ghost'
complete -c blender -l debug-wintab -d 'enable debug messages for Wintab'
complete -c blender -l debug-gpu -d 'enable GPU debug context and infromation for OpenGL'
complete -c blender -l debug-gpu-force-workarounds -d 'enable workarounds for typical GPU issues'
complete -c blender -l debug-wm -d 'enable debug messages for window manager'
complete -c blender -l debug-xr -d 'enable debug messages for virtual reality contexts'
complete -c blender -l debug-xr-time -d 'enable debug messages for virtual reality frame rendering times'
complete -c blender -l debug-all -d 'enable all debug messages'
complete -c blender -l debug-io -d 'enable debug for I/O'
complete -c blender -l debug-fpe -d 'enable floating point exceptions'
complete -c blender -l debug-exit-on-error -d 'exit on internal error'
complete -c blender -l debug-freestyle -d 'enable debug messages for Freestyle'
complete -c blender -l disable-crash-handler -d 'disable crash handler'
complete -c blender -l disable-abort-handler -d 'disable abort handler'
complete -c blender -l verbose -d 'specify logging verbosity level' -x

# GPU Options:
complete -c blender -l gpu-backend -a 'vulkan metal opengl' -d 'specify GPU backend' -x

# Misc Options:
complete -c blender -l open-last -d 'open the most recent .blend file'
complete -c blender -l app-template -a default -d 'specify app template' -x
complete -c blender -l factory-startup -d 'do not read startup.blend'
complete -c blender -l enable-event-simulate -d 'enable event simulation'

complete -c blender -l env-system-datafiles -d 'set BLENDER_SYSTEM_DATAFILES variable' -r
complete -c blender -l env-system-scripts -d 'set BLENDER_SYSTEM_SCRIPTS variable' -r
complete -c blender -l env-system-python -d 'set BLENDER_SYSTEM_PYTHON variable' -r

complete -c blender -o noaudio -d 'disable sound'
complete -c blender -o setaudio -a 'None SDL OpenAL CoreAudio JACK PulseAudio WASAPI' -d 'specify sound device' -x

complete -c blender -o r -l register -d "register .blend extension for current user"
complete -c blender -l register-allusers -d "register .blend extension for all users"
complete -c blender -l unregister -d "unregister .blend extension for current user"
complete -c blender -l unregister-allusers -d "unregister .blend extension for all users"

complete -c blender -o - -d "end option processing, following arguments passed to python"
