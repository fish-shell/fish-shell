# Sources:
# stow --help 
# https://www.gnu.org/software/stow/manual/stow.html
# https://git.savannah.gnu.org/cgit/stow.git/tree/NEWS

# options
complete -c stow -s d -l dir -r -d 'Set stow dir, default $STOW_DIR or current dir'
complete -c stow -s t -l target -r -d 'Set target dir, default parent of stow dir'
complete -c stow -l ignore -x -d 'Ignore files ending in this Perl regex'
complete -c stow -l defer -x -d "Don't stow files beginning with this Perl regex if already stowed"
complete -c stow -l override -x -d "Force stowing files beginning with this Perl regex if already stowed"
complete -c stow -l no-folding -d "Create dirs instead of symlinks to whole dirs"
complete -c stow -l adopt -d "Move existing files into stow dir if target exists (AND OVERWRITE!)"
complete -c stow -s n -l no -l simulate -d "Don't modify the file system"
complete -c stow -s v -l verbose -d "Increase verbosity by 1 (levels are from 0 to 5)"
complete -c stow -l verbose -a "0 1 2 3 4 5" -d "Increase verbosity by 1 or set it with verbose=N [0..5]"
complete -c stow -s p -l compat -d "Use legacy algorithm for unstowing"
complete -c stow -s V -l version -d "Show stow version number"
complete -c stow -s h -l help -d "Show help"
complete -c stow -l dotfiles -d "Stow dot-file_or_dir_name as .file_or_dir_name" # not yet in the manual (February 2023)

# action flags
complete -c stow -s D -l delete -r -d "Unstow the package names that follow this option"
complete -c stow -s S -l stow -r -d "Stow the package names that follow this option"
complete -c stow -s R -l restow -r -d "Restow: delete and then stow again"
