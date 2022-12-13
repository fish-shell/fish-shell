function __fish_print_xrandr_outputs --description 'Print xrandr outputs'
    xrandr | string replace -r --filter '^(\S+)\s+(.*)$' '$1\t$2' | string match -v -e Screen
end

function __fish_print_xrandr_modes --description 'Print xrandr modes'
    set -l output
    xrandr | string match -v -r '^(Screen|\s{4,})' | while read -l line
        switch $line
            case '  *'
                string trim $line | string replace -r '^(\S+)\s+(.*)$' "\$1\t\$2 [$output]"
            case '*'
                set output (string match -r '^\S+' $line)
        end
    end
end

complete -c xrandr -l verbose -d 'Be more verbose'
complete -c xrandr -l dryrun -d 'Make no changes'
complete -c xrandr -l nograb -d 'Apply modifications without grabbing the screen'
complete -c xrandr -o help -l help -d 'Print out a summary of the usage and exit'
complete -c xrandr -s v -l version -d 'Print out the RandR version reported by the X server and exit'
complete -c xrandr -s q -l query -d 'Display the current state of the system'
complete -c xrandr -s d -o display -l display -d 'Select X display to use' -x
complete -c xrandr -l screen -d 'Select which screen to manipulate' -x
complete -c xrandr -l q1 -d 'Use RandR version 1.1 protocol'
complete -c xrandr -l q12 -d 'Use RandR version 1.2 protocol'
complete -c xrandr -s s -l size -d 'Set the screen size (index or width x height)' -x
complete -c xrandr -s r -l rate -l refresh -d 'Set the refresh rate closest to the specified value' -x
complete -c xrandr -s o -l orientation -d 'Specify the orientation of the screen' -xa 'normal inverted left right'
complete -c xrandr -s x -d 'Reflect across the X axis'
complete -c xrandr -s y -d 'Reflect across the Y axis'
complete -c xrandr -l listmonitors -d 'Print all defined monitors'
complete -c xrandr -l listactivemonitors -d 'Print all active monitors'
complete -c xrandr -l setmonitor -d 'Define new monitor' -x
complete -c xrandr -l delmonitor -d 'Delete monitor' -x
complete -c xrandr -l listproviders -d 'Print all available providers'
complete -c xrandr -l setprovideroutputsource -d 'Set source for a given provider'
complete -c xrandr -l setprovideroffloadsink -d 'Set provider for a given sink'
complete -c xrandr -l noprimary -d "Don't define a primary output"
complete -c xrandr -l current -d 'Print current screen configuration'
complete -c xrandr -l panning -d 'Set panning parameters' -x
complete -c xrandr -l transform -d 'Set transformation matrix' -x
complete -c xrandr -l scale -d 'Set screen scale' -x
complete -c xrandr -l primary -d 'Set the output as primary'
complete -c xrandr -l filter -d 'Set scaling filter method' -xa 'bilinear nearest'
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
complete -c xrandr -l left-of -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
complete -c xrandr -l right-of -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
complete -c xrandr -l above -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
complete -c xrandr -l below -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
complete -c xrandr -l same-as -d 'Set position relative to the output' -xa '(__fish_print_xrandr_outputs)'
complete -c xrandr -l set -d 'Set the property value: --set <prop> <value>' -x
complete -c xrandr -l off -d 'Disables the output'
complete -c xrandr -l crtc -d 'Set the crtc' -x
complete -c xrandr -l gamma -d 'Set gamma correction [red:green:blue]' -x
complete -c xrandr -l brightness -d 'Set brightness. Multiplies gamma values by brightness value'
