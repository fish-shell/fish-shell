complete -c gimp -s h -l help -d 'show help'
complete -c gimp -l help-all -d 'show help with advanced options'
complete -c gimp -l help-gtk -d 'show help with GTK+ options'
complete -c gimp -l help-gegl -d 'show help with GEGL options'
complete -c gimp -s v -l version -d 'show version'
complete -c gimp -l license -d 'show licence'

complete -c gimp -l verbose -d 'show verbosely'
complete -c gimp -s n -l new-instance -d 'open new instance'
complete -c gimp -s a -l as-new -d 'open with new images'

complete -c gimp -s i -l no-interface -d 'hide UI'
complete -c gimp -s d -l no-data -d 'do not load patterns, gradients, palettes, and brushes'
complete -c gimp -s f -l no-fonts -d 'do not load fonts'
complete -c gimp -s s -l no-splash -d 'hide splash screen'
complete -c gimp -l no-shm -d 'do not use shared memory'
complete -c gimp -l no-cpu-accel -d 'do not use CPU acceleration'

complete -c gimp -l display -d 'open with X display' -r
complete -c gimp -l session -d 'open with alternative sessionrc' -r
complete -c gimp -s g -l gimprc -d 'open with alternative gimprc' -r
complete -c gimp -l system-gimprc -d 'open with alternative system gimprc' -r
complete -c gimp -l dump-gimprc -d 'show gimprc'
complete -c gimp -l console-messages -d 'show messages on the console'
complete -c gimp -l debug-handlers -d 'enable debug handlers'
complete -c gimp -l stack-trace-mode -d 'whether generate stack-trace in case of fatal signals' -a 'never query always' -x
complete -c gimp -l pdb-compat-mode -d 'whether PDB provides aliases for deprecated functions' -a 'off on warn' -x
complete -c gimp -l batch-interpreter -d 'run procedure to use to process batch events' -r
complete -c gimp -s b -l batch -d 'run command non-interactively' -a - -r
