# Completions for Docutils common options
__fish_complete_docutils rst2s5

# Completions for Docutils standalone reader options
__fish_complete_docutils_standalone_reader rst2s5

# Completions for Docutils HTML options
__fish_complete_docutils_html rst2s5

# Completions for Docutils S5 Slideshow options
complete -c rst2s5 -l theme -d "Specify a S5 theme"
complete -c rst2s5 -l theme-url -d "Specify a S5 theme URL"
complete -c rst2s5 -l overwrite-theme-files -d "Allow overwriting existing theme files"
complete -c rst2s5 -l keep-theme-files -d "Keep existing theme files"
complete -x -c rst2s5 -l view-mode -a "slideshow outline" -d "Set the initial view mode"
complete -c rst2s5 -l hidden-controls -d "Normally hide the controls"
complete -c rst2s5 -l visible-controls -d "Always show the controls"
complete -c rst2s5 -l current-slide -d "Enable the current slide indicator"
complete -c rst2s5 -l no-current-slide -d "Disable the current slide indicator"
