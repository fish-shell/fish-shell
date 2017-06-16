#!/usr/bin/env fish
#
# This script was originally motivated to work around a quirk (or bug depending on your viewpoint)
# of the xgettext command. See https://lists.gnu.org/archive/html/bug-gettext/2014-11/msg00006.html.
# However, it turns out that even if that quirk did not exist we would still need something like
# this script to properly extract descriptions. That's because we need to normalize the strings to
# a format that xgettext will handle correctly. Also, `xgettext -LShell` doesn't correctly extract
# all the strings we want translated. So we extract and normalize all such strings into a format
# that `xgettext` can handle.

# This regex handles descriptions for `complete` and `function` statements. These messages are not
# particularly important to translate. Hence the "implicit" label.
set implicit_regex '(?:^| +)(?:complete|function) .*? (?:-d|--description) (([\'"]).+?(?<!\\\\)\\2).*'

# This regex handles explicit requests to translate a message. These are more important to translate
# than messages which should be implicitly translated.
set explicit_regex '.*\( *_ (([\'"]).+?(?<!\\\\)\\2) *\).*'

mkdir -p /tmp/fish/implicit/share/completions /tmp/fish/implicit/share/functions
mkdir -p /tmp/fish/explicit/share/completions /tmp/fish/explicit/share/functions

for f in share/config.fish share/completions/*.fish share/functions/*.fish
    # Extract explicit attempts to translate a message. That is, those that are of the form
    # `(_ "message")`.
    string replace --filter --regex $explicit_regex 'echo $1' <$f | fish >/tmp/fish/explicit/$f.tmp ^/dev/null
    while read description
        echo 'N_ "'(string replace --all '"' '\\"' -- $description)'"'
    end </tmp/fish/explicit/$f.tmp >/tmp/fish/explicit/$f
    rm /tmp/fish/explicit/$f.tmp

    # Handle `complete` / `function` description messages. The `| fish` is subtle. It basically
    # avoids the need to use `source` with a command substituion that could affect the current
    # shell.
    string replace --filter --regex $implicit_regex 'echo $1' <$f | fish >/tmp/fish/implicit/$f.tmp ^/dev/null
    while read description
        # We don't use `string escape` as shown in the next comment because it produces output that
        # is not parsed correctly by xgettext. Instead just escape double-quotes and quote the
        # resulting string.
        echo 'N_ "'(string replace --all '"' '\\"' -- $description)'"'
    end </tmp/fish/implicit/$f.tmp >/tmp/fish/implicit/$f
    rm /tmp/fish/implicit/$f.tmp
end
