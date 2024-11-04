# gh, at least as of version 1.17.5, does not write errors to stderr, causing
# `checks/completions.fish` to fail if the `gh-completion` module is missing.
# It also does not exit with a non-zero error code, making this harder than it needs to be :(
set completion "$(gh completion --shell fish 2>/dev/null)"
string match -rq '^Error' -- $completion && return -1
echo $completion | source
