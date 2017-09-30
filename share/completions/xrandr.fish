complete -c xrandr -l verbose -d 'Be more verbose'
complete -c xrandr -l dryrun  -d 'Make no changes'
complete -c xrandr -l nograb  -d 'Apply modifications without grabbing the screen'
complete -c xrandr -o help  -d 'Print out a summary of the usage and exit'
complete -c xrandr -s v -l version -d 'Print out the RandR version reported by the X server and exit'
complete -c xrandr -s q -l query   -d 'Display the current state of the system'
complete -c xrandr -s d -o display -d 'Select X display to use' -x
complete -c xrandr -l screen -d 'Select which screen to manipulate' -x
complete -c xrandr -l q1    -d 'Use RandR version 1.1 protocol'
complete -c xrandr -l q12   -d 'Use RandR version 1.2 protocol'

set -l ver (xrandr -v | string replace -rf '.*RandR version ([0-9.]+)$' '$1' | string split ".")
if not set -q ver[1]
    set ver 10 10
end

if not set -q ver[2]
    set ver[2] 0
end

complete -c xrandr -s s -l size -d 'Set the screen size (index or width x height)' -x
complete -c xrandr -s r -l rate -l refresh -d 'Set the refresh rate closest to the specified value' -x
complete -c xrandr -s o -l orientation -d 'Specify the orientation of the screen' -xa 'normal inverted left right'
complete -c xrandr -s x                -d 'Reflect across the X axis'
complete -c xrandr -s y                -d 'Reflect across the Y axis'

# Version > 1.1
if test $ver[1] -gt 1; or test "$ver[2]" -gt 1
    complete -c xrandr -l prop -l properties -d 'Display the contents of properties for each output'
    complete -c xrandr -l fb -d 'Set screen size' -x
    complete -c xrandr -l fbmm -d 'Set reported physical screen size' -x
    complete -c xrandr -l dpi -d 'Set dpi to calculate reported physical screen size'
    complete -c xrandr -l newmode -d 'Add new mode' -r
    complete -c xrandr -l rmmode -d 'Removes a mode from the server' -xa '(__fish_print_xrandr_modes)'
    complete -c xrandr -l addmode -d 'Add a mode to the set of valid modes for an output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l delmode -d 'Remove a mode from the set of valid modes for an output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l output -d 'Selects an output to reconfigure' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l auto -d 'Enable connected but disabled outputs'
    complete -c xrandr -l mode -d 'This selects a mode' -xa '(__fish_print_xrandr_modes)'
    complete -c xrandr -l preferred -d 'Select the same mode as --auto, but it do not automatically enable or disable the output'
    complete -c xrandr -l pos -d 'Set output position within the secreen in pixels' -x
    complete -c xrandr -l rate -d 'Set refresh rate' -x
    complete -c xrandr -l reflect -d 'Set reflection' -xa 'normal x y xy'
    complete -c xrandr -l rotate -d 'Set rotation' -xa 'normal left right inverted'
    complete -c xrandr -l left-of  -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l right-of -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l above    -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l below    -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l same-as  -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
    complete -c xrandr -l set -d 'Set the property value: --set <prop> <value>' -x
    complete -c xrandr -l off -d 'Disables the output'
    complete -c xrandr -l crtc -d 'Set the crtc' -x
    complete -c xrandr -l gamma -d 'Set gamma correction [red:green:blue]' -x
    complete -c xrandr -l brightness -d 'Set brightness. Multiplies gamma galues by brightness value'
end

if test $ver[1] -gt 1; or test "$ver[2]" -gt 2
    complete -c xrandr -l noprimary -d "Don't define a primary output"
    complete -c xrandr -l current -d 'Print current screen configuration'
    complete -c xrandr -l panning  -d 'Set panning: widthxheight[+x+y[/track_widthxtrack_height+track_x+track_y[/border_left/border_top/border_right/border_bottom]]]' -x
    complete -c xrandr -l transform -d 'Set transformation matrix: a,b,c,d,e,f,g,h,i for [ [a,b,c], [d,e,f], [g,h,i] ]' -x
    complete -c xrandr -l scale -d 'Set scren scale' -x
    complete -c xrandr -l primary -d 'Set the output as primary'
end

