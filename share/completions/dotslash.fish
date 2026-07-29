# DotSlash, https://dotslash-cli.com/
#
# DotSlash has an unusual CLI syntax. There are four possible kinds
# of invocations:
#    dotslash DOTSLASH_FILE [OPTIONS...]
#    dotslash --help
#    dotslash --version
#    dotslash -- DOTSLASH_SUBCOMMAND [ARG]
#
# For the first of these, we leave fish's default file completion to
# complete the DOTSLASH_FILE.  We don't attempt to deal with [OPTIONS...];
# invocations that need those usually happen from a shebang rather than
# from the command line.
#
# The remaining three kinds of invocations:
complete -c dotslash -n __fish_is_first_arg -l help -d "Print usage"
complete -c dotslash -n __fish_is_first_arg -l version -d "Print the dotslash version"
complete -c dotslash -n __fish_is_first_arg -a -- -d "Run a dotslash subcommand"

# The rest of the completions are dedicated to completing various
# dotslash subcommands.

function __fish_dotslash_needs_subcommand -d "Test if completing a dotslash subcommand, immediately following a --"
    set -l tokens (commandline -pxc)
    test (count $tokens) -eq 2; and test "$tokens[2]" = --
end

function __fish_dotslash_after_dashdash -d "Test if the dotslash subcommand form is being used"
    set -l tokens (commandline -pxc)
    test (count $tokens) -ge 2; and test "$tokens[2]" = --
end

function __fish_dotslash_arg_of_these_subcommands -d "Test if completing the argument of any of the given subcommands"
    set -l tokens (commandline -pxc)
    test (count $tokens) -eq 3
    and test "$tokens[2]" = --
    and contains -- $tokens[3] $argv
end

# Nothing after `dotslash --` is a file unless a subcommand below says so.
complete -c dotslash -f -n __fish_dotslash_after_dashdash

# Subcommands current as of DotSlash 0.5.9.
complete -c dotslash -n __fish_dotslash_needs_subcommand -a b3sum -d "Compute the blake3 hash of a file"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a cache-dir -d "Print the cache directory"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a clean -d "Delete the cache directory"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a create-url-entry -d "Fetch given URL, generate JSON template with size and hash filled in"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a fetch -d "Fetch the artifact, print its path instead of running it"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a get-extracted-cache-path -d "Print where the artifact would be cached, without fetching it"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a parse -d "Parse a DotSlash file and print it as JSON"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a sha256 -d "Compute the sha256 hash of a file"

# `version` and `help` commands are undocumented but work; `-- help` is the same as `--help`.
complete -c dotslash -n __fish_dotslash_needs_subcommand -a help -d "Print usage"
complete -c dotslash -n __fish_dotslash_needs_subcommand -a version -d "Print the dotslash version"

# fetch, get-extracted-cache-path and parse take a DotSlash file; b3sum and
# sha256 hash any file at all.
set -l subcommands_taking_a_path b3sum fetch get-extracted-cache-path parse sha256
complete -c dotslash -F -n "__fish_dotslash_arg_of_these_subcommands $subcommands_taking_a_path"

# create-url-entry takes a URL, where we can't provide useful completions,
# and the remaining subcommands take no arguments at all. So, nothing more to
# do for those.
