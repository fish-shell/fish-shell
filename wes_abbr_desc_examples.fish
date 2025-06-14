
abbr --description="uninstall" -a pmr 'sudo pacman -R --recursive'
abbr --description="install" -a pm_install 'sudo pacman --noconfirm -S'
abbr --description="search" --set-cursor=! -a pmss "sudo pacman -Ss '^!'"
abbr --description="upgrade" -a pmsu 'sudo pacman -Syu'

# source wes_abbr_desc_examples.fish
# pm<TAB> => look at the description to right of # (after replacement)
