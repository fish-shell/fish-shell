# The path below is where brew documents installing its fish completions,
# see https://docs.brew.sh/Shell-Completion. `brew.fish` symlink is not guaranteed to exist,
# but should cover the majority of installations.
# This script is not reached by brew's fish, which has brew's vendor_completions.d path built in,
# or if that directory was added to $fish_complete_path as brew's docs suggest.
set -l brew_completions (brew --prefix)/share/fish/vendor_completions.d/brew.fish
test -r "$brew_completions" && source $brew_completions
