#
# Completions for the chsh command
#

# This grep tries to match nonempty lines that do not start with hash
complete -c chsh -s s -l shell -x -a "(string match -r '^[^#].*' < /etc/shells)" -d "Specify your login shell"
complete -c chsh -s u -l help -d "Display help and exit	"
complete -c chsh -s v -l version -d "Display version and exit"
complete -x -c chsh -a "(__fish_complete_users)"

# util-linux's chsh also has a "-l"/"--list-shells" option.
# While both FreeBSD and macOS also have "-l", it means something different and probably uninteresting.
if set -l chshversion (chsh --version 2>/dev/null); and string match -qr '.*util-linux.*' -- $chshversion
    complete -f -c chsh -s l -l list-shells -d "List available shells and exit"
end
