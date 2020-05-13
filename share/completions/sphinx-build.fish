complete -c sphinx-build -s h -l help -d "Display usage summary"
complete -c sphinx-build -l version -d "Display Sphinx version"

# General Options
complete -x -c sphinx-build -s b -a "html dirhtml singlehtml htmlhelp qthelp devhelp epub applehelp latex man texinfo text gettext doctest linkcheck xml pseudoxml" -d "Builder to use"
complete -x -c sphinx-build -s M -a "latexpdf info" -d "Alternative to -b"
complete -c sphinx-build -s a -d "Write all files"
complete -c sphinx-build -s E -d "Do not use a saved environment"
complete -c sphinx-build -s d -d "Path for the cached environment and doctree files"
complete -c sphinx-build -s j -d "Build in parallel"

# Build Configuration Option
complete -c sphinx-build -s c -d "Path to conf.py"
complete -c sphinx-build -s C -d "Do not look for a conf.py"
complete -c sphinx-build -s D -d "Override a setting in conf.py"
complete -c sphinx-build -s A -d "Pass a value into HTML templates"
complete -c sphinx-build -s t -d "Define tag"
complete -c sphinx-build -s n -d "Nit-picky mode"

# Console Output Options
complete -c sphinx-build -s v -d "Increase verbosity"
complete -c sphinx-build -s q -d "No output on stdout"
complete -c sphinx-build -s Q -d "No output at all"
complete -c sphinx-build -s N -d "Do not emit colored output"
complete -c sphinx-build -s w -d "Write warnings to given file"
complete -c sphinx-build -s W -d "Turn warnings into errors"
complete -c sphinx-build -l keep-going -d "Keep going when getting warnings"
complete -c sphinx-build -s T -d "Show full traceback on exception"
complete -c sphinx-build -s P -d "Run Pdb on exception"
