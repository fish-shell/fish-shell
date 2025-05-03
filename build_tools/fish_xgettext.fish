#!/usr/bin/env fish
#
# Tool to generate messages.pot

# Create temporary directory for these operations. OS X `mktemp` is somewhat restricted, so this block
# works around that - based on share/functions/funced.fish.
set -q TMPDIR
or set -l TMPDIR /tmp
set tmpdir (mktemp -d $TMPDIR/fish.XXXXXX)
or exit 1

# This is a gigantic crime.
# xgettext still does not support rust *at all*, so we use cargo-expand to get all our wgettext invocations.
set -l expanded (cargo expand --lib; for f in fish{,_indent,_key_reader}; cargo expand --bin $f; end)

# Extract any gettext call
set -l strs (printf '%s\n' $expanded | grep -A1 wgettext_static_str |
             grep 'widestring::internals::core::primitive::str =' |
             string match -rg '"(.*)"' | string match -rv '^%ls$|^$' |
             # escaping difference between gettext and cargo-expand: single-quotes
             string replace -a "\'" "'" | sort -u)

# Extract any constants
set -a strs (string match -rv 'BUILD_VERSION:|PACKAGE_NAME' -- $expanded |
             string match -rg 'const [A-Z_]*: &str = "(.*)"' | string replace -a "\'" "'")

# We construct messages.pot ourselves instead of forcing this into msgmerge or whatever.
# The escaping so far works out okay.
for str in $strs
    # grep -P needed for string escape to be compatible (PCRE-style),
    # -H gives the filename, -n the line number.
    # If you want to run this on non-GNU grep: Don't.
    echo "#:" (grep -PHn -r -- (string escape --style=regex -- $str) src/ |
    head -n1 | string replace -r ':\s.*' '')
    echo "msgid \"$str\""
    echo 'msgstr ""'
end >messages.pot

function extract_fish_script_messages --argument-names name regex;
    mkdir -p $tmpdir/$name/share/completions $tmpdir/$name/share/functions
    for f in share/config.fish share/completions/*.fish share/functions/*.fish
        string replace --filter --regex $regex '$1' <$f | string unescape \
            | string replace --all '"' '\\"' | string replace -r '(.*)' 'N_ "$1"' >$tmpdir/$name/$f
    end
end

# This regex handles descriptions for `complete` and `function` statements. These messages are not
# particularly important to translate. Hence the "implicit" label.
set -l implicit_regex '(?:^| +)(?:complete|function).*? (?:-d|--description) (([\'"]).+?(?<!\\\\)\\2).*'
extract_fish_script_messages implicit $implicit_regex

# This regex handles explicit requests to translate a message. These are more important to translate
# than messages which should be implicitly translated.
set -l explicit_regex '.*\( *_ (([\'"]).+?(?<!\\\\)\\2) *\).*'
extract_fish_script_messages explicit $explicit_regex

xgettext -j -k -kN_ -LShell --from-code=UTF-8 -cDescription --no-wrap -o messages.pot $tmpdir/{ex,im}plicit/share/*/*.fish

# Remove the tmpdir from the location to avoid churn
sed -i 's_^#: /.*/share/_#: share/_' messages.pot

rm -r $tmpdir
