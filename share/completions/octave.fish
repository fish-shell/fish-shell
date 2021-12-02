complete --command octave --short-option h --long-option help --description 'Show help'
complete --command octave --short-option v --long-option version --description 'Show version'

complete --command octave --long-option built-in-docstrings-file --require-parameter \
  --description 'Use docs for built-ins'
complete --command octave --short-option d --long-option debug \
  --description 'Enter parser debugging mode'
complete --command octave --long-option debug-jit --description 'Enable JIT compiler debugging/tracing'
complete --command octave --long-option doc-cache-file --require-parameter \
  --description 'Use doc cache file'
complete --command octave --short-option x --long-option echo-commands \
  --description 'Echo commands as they are executed'
complete --command octave --long-option eval --no-files --require-parameter \
  --description 'Evaluate code'
complete --command octave --long-option exec-path --require-parameter \
  --description 'Set path for executing subprograms'
complete --command octave --long-option gui \
  --condition 'not __fish_seen_argument --long gui --long no-gui --long no-line-editing' \
  --description 'Start the graphical user interface'
complete --command octave --long-option image-path --require-parameter \
  --description 'Add path to head of image search path'
complete --command octave --long-option info-file --require-parameter \
  --description 'Use top-level info file'
complete --command octave --long-option info-program --require-parameter \
  --description 'Specify program for reading info files'
complete --command octave --short-option i --long-option interactive \
  --description 'Force interactive behavior'
complete --command octave --long-option jit-compiler --description 'Enable the JIT compiler'
complete --command octave --long-option line-editing \
  --condition 'not __fish_seen_argument --long line-editing --long no-line-editing' \
  --description 'Force readline use for command-line editing'
complete --command octave --long-option no-gui \
  --condition 'not __fish_seen_argument --long gui --long no-gui' \
  --description 'Disable the graphical user interface'
complete --command octave --short-option H --long-option no-history \
  --description 'Don\'t save commands to the history list'
complete --command octave --long-option no-init-file \
  --description 'Don\'t read the ~/.octaverc or .octaverc files'
complete --command octave --long-option no-init-path \
  --description 'Don\'t initialize function search path'
complete --command octave --long-option no-line-editing \
  --condition 'not __fish_seen_argument --long line-editing --long no-line-editing --long gui' \
  --description 'Don\'t use readline for command-line editing'
complete --command octave --long-option no-site-file \
  --description 'Don\'t read the site-wide octaverc file'
complete --command octave --short-option W --long-option no-window-system \
  --description 'Disable window system, including graphics'
complete --command octave --short-option f --long-option norc \
  --description 'Don\'t read any initialization files'
complete --command octave --short-option p --long-option path --require-parameter \
  --description 'Add path to head of function search path'
complete --command octave --long-option persist --description 'Go interactive after --eval or reading'
complete --command octave --short-option q --long-option quiet --long-option silent \
  --description 'Don\'t print message at startup'
complete --command octave --long-option texi-macros-file --require-parameter \
  --description 'Use Texinfo macros for makeinfo command'
complete --command octave --long-option traditional \
  --description 'Set variables for closer MATLAB compatibility'
complete --command octave --short-option V --long-option verbose \
  --description 'Enable verbose output in some cases'
