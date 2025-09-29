# RUN: fish=%fish %fish %s
#REQUIRES: command -v msgfmt
#REQUIRES: %fish -c 'status buildinfo | grep localize-messages'

set -l workspace_root (path resolve -- (status dirname)/../../)

set -g ok true

for file in $workspace_root/share/functions/*.fish
    function error --inherit-variable workspace_root --inherit-variable file
        echo "$(string replace $workspace_root/ '' $file):1: error: $argv"
        set -g ok false
    end
    set -l basename (path basename $file)
    set -l tier
    set -l localize_directives (string match -r '^# localization:.*' <$file)
    if test (count $localize_directives) -gt 1
        error 'multiple '# localization:' directives'
        continue
    end
    if set -q localize_directives[1]
        if not set tier (string replace -rf -- \
                '^# localization: (tier[123]|skip(?:\(\S*\))?)$' '$1' \
                $localize_directives)
            error 'invalid '# localization:' directive'
            continue
        end
    end
    if string match -q -- 'fish_*' $basename
        if set -q tier[1] && test $tier != 'skip(barely-used)'
            error "unexpected '# localization:' directive in fish_* file, those are currently implicitly tier1"
        end
        continue
    end
    if not set -q tier[1]
        error "missing '^# localization: (tier[123]|skip)\$' directive"
        continue
    end
end

if $ok
    return
end

# If the test fails, output some flight rules. Here's some rationale:
#
# Files named 'share/functions/fish_*' are implicitly tier1 unless overridden.
#
# Private functions (starting with __fish) are probably not worth bothering translators.
# The only thing that's translated is usually the function description.
# Most of those private functions will barely ever be seen by most users.
# They are shown when:
#   - typing '__fish_' TAB, but only if the function has been loaded before.
#   - running 'type __fish_ps' or 'functions __fish_ps'.
# The exception is if a function is directly or indirectly called by a default
# binding; then the user might look it up with 'type __fish_list_current_token'.
# So we use tier1 (for interesting ones) until these are un-dundered and properly documented.
#
# Beyond functions, completions for share/completions/<name>.fish are implicitly 'tier3',
# unless doc_src/cmds/<name>.rst exists, in which case they are 'tier1'.  This can be overridden.
# For common tools like coreutils, use 'tier2'.
#
# Wrapper functions like grep don't need translations because we use 'apropos grep'.

echo "
- Use '# localization: tier1' for
  - user-visible functions provided by fish such as 'll'
  - functions used in bindings like '__fish_list_current_token'
- Use '# localization: tier2' for:
  - rarely-used or less important functions provided by fish.
  - completions for common tools like coreutils
- Use '# localization: tier3'
  - for completions that add for third-party commands, or functions that do the same
    E.g. the ones that contain 'complete foo ... -d some-translatable-string'.
- Use '# localization: skip(<reason>)' in function files that should not be translated.
  - 'skip(private)' for private functions, unless they are used in bindings
  - 'skip(uses-apropos)' for wrapper functions
  - 'skip(deprecated)' for functions that have been superseded
  - 'skip(barely-used)' for functions that are not documented or probably barely used
"
