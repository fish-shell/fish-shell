# Arch Linux package downgrader tool
complete -c downgrade -f
complete -c downgrade -xa "(__fish_print_packages --installed)"
