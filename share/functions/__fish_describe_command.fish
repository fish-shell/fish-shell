#
# This function is used internally by the fish command completion code
#

function __fish_describe_command -d "Command used to find descriptions for commands"
    command -sq apropos; or return
    # Some systems could use -s 1,8 here, but FreeBSD doesn't have that.
    apropos $argv 2>/dev/null | string replace -rf '^(\S+) \(\S+\)\s+- (.*)' '$1\t$2' \
    | string match (string replace -a '*' '\*' -- "$argv")"*"
end
