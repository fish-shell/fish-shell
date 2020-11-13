#!/usr/bin/env fish
#
# Tool to generate messages.pot
# Extended to replace the old Makefile rule which did not port easily to CMake

# This script was originally motivated to work around a quirk (or bug depending on your viewpoint)
# of the xgettext command. See https://lists.gnu.org/archive/html/bug-gettext/2014-11/msg00006.html.
# However, it turns out that even if that quirk did not exist we would still need something like
# this script to properly extract descriptions. That's because we need to normalize the strings to
# a format that xgettext will handle correctly. Also, `xgettext -LShell` doesn't correctly extract
# all the strings we want translated. So we extract and normalize all such strings into a format
# that `xgettext` can handle.

# Start with the C++ source
xgettext -k -k_ -kN_ -LC++ --no-wrap -o messages.pot src/*.cpp src/*.h

# This regex handles descriptions for `complete` and `function` statements. These messages are not
# particularly important to translate. Hence the "implicit" label.
set -l implicit_regex '(?:^| +)(?:complete|function).*? (?:-d|--description) (([\'"]).+?(?<!\\\\)\\2).*'

# This regex handles explicit requests to translate a message. These are more important to translate
# than messages which should be implicitly translated.
set -l explicit_regex '.*\( *_ (([\'"]).+?(?<!\\\\)\\2) *\).*'

# Create temporary directory for these operations. OS X `mktemp` is somewhat restricted, so this block
# works around that - based on share/functions/funced.fish.
set -q TMPDIR
or set -l TMPDIR /tmp
set -l tmpdir (mktemp -d $TMPDIR/fish.XXXXXX)
or exit 1

mkdir -p $tmpdir/implicit/share/completions $tmpdir/implicit/share/functions
mkdir -p $tmpdir/explicit/share/completions $tmpdir/explicit/share/functions

for f in share/config.fish share/completions/*.fish share/functions/*.fish
    # Extract explicit attempts to translate a message. That is, those that are of the form
    # `(_ "message")`.
    string replace --filter --regex $explicit_regex 'echo $1' <$f | fish >$tmpdir/explicit/$f.tmp 2>/dev/null
    while read description
        echo 'N_ "'(string replace --all '"' '\\"' -- $description)'"'
    end <$tmpdir/explicit/$f.tmp >$tmpdir/explicit/$f
    rm $tmpdir/explicit/$f.tmp

    # Handle `complete` / `function` description messages. The `| fish` is subtle. It basically
    # avoids the need to use `source` with a command substitution that could affect the current
    # shell.
    string replace --filter --regex $implicit_regex 'echo $1' <$f | fish >$tmpdir/implicit/$f.tmp 2>/dev/null
    while read description
        # We don't use `string escape` as shown in the next comment because it produces output that
        # is not parsed correctly by xgettext. Instead just escape double-quotes and quote the
        # resulting string.
        echo 'N_ "'(string replace --all '"' '\\"' -- $description)'"'
    end <$tmpdir/implicit/$f.tmp >$tmpdir/implicit/$f
    rm $tmpdir/implicit/$f.tmp
end

xgettext -j -k -kN_ -LShell --from-code=UTF-8 -cDescription --no-wrap -o messages.pot $tmpdir/explicit/share/*/*.fish
xgettext -j -k -kN_ -LShell --from-code=UTF-8 -cDescription --no-wrap -o messages.pot $tmpdir/implicit/share/*/*.fish

rm -r $tmpdir
