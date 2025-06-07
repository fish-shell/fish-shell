#!/usr/bin/env fish
#
# Tool to generate gettext messages template file.
# Writes to stdout.

begin
    # Write header. This is required by msguniq.
    # Note that this results in the file being overwritten.
    # This is desired behavior, to get rid of the results of prior invocations
    # of this script.
    begin
        echo 'msgid ""'
        echo 'msgstr ""'
        echo '"Content-Type: text/plain; charset=UTF-8\n"'
        echo ""
    end

    set -l rust_extraction_file (mktemp)

    # We need to build to ensure that the proc macro for extracting strings runs.
    FISH_GETTEXT_EXTRACTION_FILE=$rust_extraction_file cargo check
    or exit 1

    # Get rid of duplicates and sort.
    msguniq --no-wrap --strict --sort-output $rust_extraction_file
    or exit 1

    rm $rust_extraction_file

    function extract_fish_script_messages --argument-names regex

        # Using xgettext causes more trouble than it helps.
        # This is due to handling of escaping in fish differing from formats xgettext understands
        # (e.g. POSIX shell strings).
        # We work around this issue by manually writing the file content.

        # Steps:
        # 1. We extract strings to be translated from the relevant files and drop the rest. This step
        #    depends on the regex matching the entire line, and the first capture group matching the
        #    string.
        # 2. We unescape. This gets rid of some escaping necessary in fish strings.
        # 3. The resulting strings are sorted alphabetically. This step is optional. Not sorting would
        #    result in strings from the same file appearing together. Removing duplicates is also
        #    optional, since msguniq takes care of that later on as well.
        # 4. Single backslashes are replaced by double backslashes. This results in the backslashes
        #    being interpreted as literal backslashes by gettext tooling.
        # 5. Double quotes are escaped, such that they are not interpreted as the start or end of
        #    a msgid.
        # 6. We transform the string into the format expected in a PO file.
        cat share/config.fish share/completions/*.fish share/functions/*.fish |
            string replace --filter --regex $regex '$1' |
            string unescape |
            sort -u |
            sed -E -e 's_\\\\_\\\\\\\\_g' -e 's_"_\\\\"_g' -e 's_^(.*)$_msgid "\1"\nmsgstr ""\n_'
    end

    # This regex handles explicit requests to translate a message. These are more important to translate
    # than messages which should be implicitly translated.
    set -l explicit_regex '.*\( *_ (([\'"]).+?(?<!\\\\)\\2) *\).*'
    extract_fish_script_messages $explicit_regex

    # This regex handles descriptions for `complete` and `function` statements. These messages are not
    # particularly important to translate. Hence the "implicit" label.
    set -l implicit_regex '^(?:\s|and |or )*(?:complete|function).*? (?:-d|--description) (([\'"]).+?(?<!\\\\)\\2).*'
    extract_fish_script_messages $implicit_regex
end |
    # At this point, all extracted strings have been written to stdout,
    # starting with the ones taken from the Rust sources,
    # followed by strings explicitly marked for translation in fish scripts,
    # and finally the strings from fish scripts which get translated implicitly.
    # Because we do not eliminate duplicates across these categories,
    # we do it here, since other gettext tools expect no duplicates.
    msguniq --no-wrap
