# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# Completion for: GNU Octave 5.2.0

complete -c octave -x -a '(__fish_complete_suffix .m)'

complete -c octave -s h -l help -d 'Show help'
complete -c octave -s v -l version -d 'Show version'

complete -c octave -l built-in-docstrings-file -r -d 'Use docs for built-ins'
complete -c octave -s d -l debug -d 'Enter parser debugging mode'
complete -c octave -l debug-jit -d 'Enable JIT compiler debugging/tracing'
complete -c octave -l doc-cache-file -r -d 'Use doc cache file'
complete -c octave -s x -l echo-commands -d 'Echo commands as they are executed'
complete -c octave -l eval -x -d 'Evaluate code'
complete -c octave -l exec-path -r -d 'Set path for executing subprograms'
complete -c octave -l gui -n 'not __fish_seen_argument --long gui --long no-gui --long no-line-editing' -d 'Start the graphical user interface'
complete -c octave -l image-path -r -a '(.bmp .gif .jpg .jpeg .pbm .pcx .bgm .png .pnm .ppm .ras .tif .tiff .xwd)' -d 'Add path to head of image search path'
complete -c octave -l info-file -r -d 'Use top-level info file'
complete -c octave -l info-program -r -d 'Specify program for reading info files'
complete -c octave -s i -l interactive -d 'Force interactive behavior'
complete -c octave -l jit-compiler -d 'Enable the JIT compiler'
complete -c octave -l line-editing -n 'not __fish_seen_argument --long line-editing --long no-line-editing' -d 'Force readline use for command-line editing'
complete -c octave -l no-gui -n 'not __fish_seen_argument --long gui --long no-gui' -d 'Disable the graphical user interface'
complete -c octave -s H -l no-history -d 'Don\'t save commands to the history list'
complete -c octave -l no-init-file -d 'Don\'t read the ~/.octaverc or .octaverc files'
complete -c octave -l no-init-path -d 'Don\'t initialize function search path'
complete -c octave -l no-line-editing -n 'not __fish_seen_argument --long line-editing --long no-line-editing --long gui' -d 'Don\'t use readline for command-line editing'
complete -c octave -l no-site-file -d 'Don\'t read the site-wide octaverc file'
complete -c octave -s W -l no-window-system -d 'Disable window system, including graphics'
complete -c octave -s f -l norc -d 'Don\'t read any initialization files'
complete -c octave -s p -l path -r -d 'Add path to head of function search path'
complete -c octave -l persist -d 'Go interactive after --eval or reading'
complete -c octave -s q -l quiet --long-option silent -d 'Don\'t print message at startup'
complete -c octave -l texi-macros-file -r -d 'Use Texinfo macros for makeinfo command'
complete -c octave -l traditional -d 'Set variables for closer MATLAB compatibility'
complete -c octave -s V -l verbose -d 'Enable verbose output in some cases'
