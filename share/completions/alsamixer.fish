# SPDX-FileCopyrightText: © 2016 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c alsamixer -s h -l help -d "Show help"
complete -x -c alsamixer -s c -l card -d "Soundcard number or id to use"
complete -x -c alsamixer -s D -l device -d "Mixer device to control"
complete -x -c alsamixer -s V -l view -d "Starting view mode" -a "playback capture all"
complete -c alsamixer -s g -l no-color -d "Toggle the using of colors"
complete -x -c alsamixer -s a -l abstraction -d "Mixer abstraction level" -a "none basic"
