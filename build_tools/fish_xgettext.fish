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

    set -l extraction_dir (mktemp -d)

    # We need to build to ensure that the proc macro for extracting strings runs.
    FISH_GETTEXT_EXTRACTION_DIR=$extraction_dir cargo check
    or exit 1

    set -l rust_string_file $extraction_dir/literals

    # Extract constants which get passed to gettext
    #
    # The input file contains the name of one constant per line.
    # Each of these constants is passed to gettext, so we want to extract the corresponding string.
    # This is not possible in a proc macro, because such a macro just sees the name of the constant,
    # without having a way to get the corresponding literal.
    # Therefore, it writes the names of these constants to a file,
    # and we use these names to extract the corresponding literals from the source files using find+sed.

    # We create a pattern which matches all of our constant names.
    # This is done by joining the lines of the source file with '|', to create regex alternatives.
    set -l consts (string join '|' <$extraction_dir/consts)
    # This is used to identify the first line of constant definitions spanning two lines.
    set -l const_pattern_first_line '^.*const ('$consts'): &str ='
    # The pattern for identifying constant definitions.
    # The first group is required to enclose the '|' separated constant names.
    # The second group is for the string literal we want to extract.
    set -l const_pattern $const_pattern_first_line'\s*(".*");'

    # This is the directory we want to search in.
    # At the time of writing, gettext calls are only present in this directory.
    # If gettext calls are added in sub-crates, this needs to be modified.
    set -l src_dir (status dirname)/../src

    # `find` is used to pass the paths of all relevant files to sed.
    # `sed` is used to find the definitions of the constants and to extract the string literals.
    # Explanation of the sed command:
    # 1. sed is line-based, which is a problem if there is a line break after the '=' in the
    #    definition. (Line breaks at other locations are not handled and would break this.)
    #    Therefore, if we find a constant definition whose line ends on '=', we add the next line to
    #    the pattern space. This line should contain the string literal.
    #    This leaves us with a pattern space containing the entire constant definition.
    # 2. With the pattern space prepared, we check if it matches our constant definition pattern.
    #    The remaining commands are only executed if we find a match.
    # 3. If this step is reached, we have found a pattern definition,
    #    so extract the string literal and print it.
    find $src_dir -type f -name '*.rs' -exec sed -nE -e '/'$const_pattern_first_line'$/N; /'$const_pattern'/ { s/'$const_pattern'/\2/; p }' {} '+' >>$rust_string_file

    # Sort the extracted strings and remove duplicates.
    # Double quotes are removed for sorting, because they can result in a string being placed before
    # a proper prefix of that string.
    # Once sorted, transform the strings into the po format.
    # If a string contains a '%' it is considered a format string and marked with a '#, c-format'.
    # This allows msgfmt to identify issues with translations whose format string does not match the
    # original.
    sed -E 's/^"(.*)"$/\1/' $rust_string_file |
        sort -u  |
        sed -E -e '/%/ i\
#, c-format
' -e 's/^(.*)$/msgid "\1"\nmsgstr ""\n/'

    rm -r $extraction_dir

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
