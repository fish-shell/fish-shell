#
# Completions for the dnf command
#

# magic completion safety check (do not remove this comment)
if not type -q dnf
    exit
end

complete -c dnf -w yum
