#!/usr/bin/env fish
#
# Tool to generate messages.pot

set -g output_file (status dirname)/../messages.pot

# Write header. This is required by msguniq.
# Note that this results in the file being overwritten.
# This is desired behavior, to get rid of the results of prior invocations
# of this script.
begin
    echo 'msgid ""'
    echo 'msgstr ""'
    echo '"Content-Type: text/plain; charset=UTF-8\n"'
    echo ""
end >$output_file

# This is a gigantic crime.
# We use cargo-expand to get all our wgettext invocations.
# This might be replaced once we have a tool which properly handles macro expansions.
set -l expanded (cargo expand --lib; for f in fish fish_indent fish_key_reader; cargo expand --bin $f; end)

# Extract any gettext call
set -l strs (printf '%s\n' $expanded | grep -A1 wgettext_static_str |
             grep 'widestring::internals::core::primitive::str =' |
             string match -rg '"(.*)"' | string match -rv '^%ls$|^$' |
             # escaping difference between gettext and cargo-expand: single-quotes
             string replace -a "\'" "'")

# Extract any constants
set -a strs (string match -rv 'BUILD_VERSION:|PACKAGE_NAME' -- $expanded |
             string match -rg 'const [A-Z_]*: &str = "(.*)"' | string replace -a "\'" "'")

# Sort the extracted strings and remove duplicates.
# This is optional.
set -l strs (string join \n -- $strs | sort -u)

# We construct messages.pot ourselves instead of forcing this into msgmerge or whatever.
# The escaping so far works out okay.
for str in $strs
    echo "msgid \"$str\""
    echo 'msgstr ""'
    echo ""
end >>$output_file

function extract_fish_script_messages --argument-names regex

    # Using xgettext causes more trouble than it helps.
    # This is due to handling of escaping in fish differing from formats xgettext understands
    # (e.g. POSIX shell strings).
    # We work around this issue by manually writing the file content.

    # Steps:
    # 1. We extract strings to be translated from the file f and drop the rest. This step
    #    depends on the regex matching the entire line, and the first capture group matching the
    #    string.
    # 2. We unescape. This gets rid of some escaping necessary in fish strings.
    # 3. Single backslashes are replaced by double backslashes. This results in the backslashes
    #    being interpreted as literal backslashes by gettext tooling.
    # 4. Double quotes are escaped, such that they are not interpreted as the start or end of
    #    a msgid.
    # 5. The resulting strings are sorted alphabetically. This step is optional. Not sorting would
    #    result in strings from the same file appearing together. Removing duplicates is also
    #    optional, since msguniq takes care of that later on as well.
    # 6. We transform the string into the format expected in a PO file.
    for f in share/config.fish share/completions/*.fish share/functions/*.fish
        string replace --filter --regex $regex '$1' <$f |
            string unescape |
            string replace --all '\\' '\\\\' |
            string replace --all '"' '\\"'
    end | sort -u | string replace --regex '^(.*)$' 'msgid "$1"'\n'msgstr ""'\n >>$output_file
end

# This regex handles explicit requests to translate a message. These are more important to translate
# than messages which should be implicitly translated.
set -l explicit_regex '.*\( *_ (([\'"]).+?(?<!\\\\)\\2) *\).*'
extract_fish_script_messages $explicit_regex

# This regex handles descriptions for `complete` and `function` statements. These messages are not
# particularly important to translate. Hence the "implicit" label.
set -l implicit_regex '^(?:\s|and |or )*(?:complete|function).*? (?:-d|--description) (([\'"]).+?(?<!\\\\)\\2).*'
extract_fish_script_messages $implicit_regex

# At this point, messages.pot contains all extracted strings,
# starting with the ones taken from the Rust sources,
# followed by strings explicitly marked for translation in fish scripts,
# and finally the strings from fish scripts which get translated implicitly.
# Because we do not eliminate duplicates across these categories,
# we do it here, since other gettext tools expect no duplicates.
msguniq --no-wrap --output-file $output_file $output_file
