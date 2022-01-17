# Flags
complete -f -c topgrade -s c -l cleanup -d "Cleanup temporary or old files"
complete -f -c topgrade -l disable-predefined-git-repos -d "Don't pull the predefined git repos"
complete -f -c topgrade -s n -l dry-run -d "Print what would be done"
complete -f -c topgrade -l edit-config -d "Edit the configuration file"
complete -f -c topgrade -s h -l help -d "Prints help information"
complete -f -c topgrade -s k -l keep -d "Prompt for a key before exiting"
complete -f -c topgrade -l no-retry -d "Do not ask to retry failed steps"
complete -f -c topgrade -s t -l tmux -d "Run inside tmux"
complete -f -c topgrade -l config-reference -d "Show config reference"
complete -f -c topgrade -l show-skipped -d "Show the reason for skipped steps"
complete -f -c topgrade -s V -l version -d "Prints version information"
complete -f -c topgrade -s v -l verbose -d "Output logs"

# Options
complete -f -c topgrade -l config -r -d "Alternative configuration file"
complete -f -c topgrade -l disable -r -a "asdf atom brew_cask brew_formula bin cargo chezmoi chocolatey choosenim composer custom_commands deno dotnet emacs firmware flatpak flutter fossil gcloud gem git_repos haxelib gnome_shell_extensions home_manager jetpack krew macports mas micro myrepos nix node opam pacdiff pacstall pearl pipx pip3 pkg pkgin pnpm powershell raco remotes restarts rtcl rustup scoop sdkman silnite sheldon shell snap spicetify stack system tldr tlmgr tmux vagrant vcpkg vim winget wsl yadm" -d "Do not perform upgrades for the given steps"

complete -f -c topgrade -l only -r -a "asdf atom brew_cask brew_formula bin cargo chezmoi chocolatey choosenim composer custom_commands deno dotnet emacs firmware flatpak flutter fossil gcloud gem git_repos haxelib gnome_shell_extensions home_manager jetpack krew macports mas micro myrepos nix node opam pacdiff pacstall pearl pipx pip3 pkg pkgin pnpm powershell raco remotes restarts rtcl rustup scoop sdkman silnite sheldon shell snap spicetify stack system tldr tlmgr tmux vagrant vcpkg vim winget wsl yadm" -d "Perform only the specified steps (experimental)"

complete -f -c topgrade -l remote-host-limit -r -d "A regular expression for restricting remote host execution"
complete -f -c topgrade -s y -l yes -d "Say yes to package manager's prompt"
