#!/usr/bin/env fish
#
# Tool to generate gettext messages template file.
# Writes to stdout.
# Intended to be called from `update_translations.fish`.

argparse use-existing-template= -- $argv
or exit $status

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

    set -g workspace_root (path resolve (status dirname)/..)

    set -l rust_extraction_file
    if set -l --query _flag_use_existing_template
        set rust_extraction_file $_flag_use_existing_template
    else
        set rust_extraction_file (mktemp)
        # We need to build to ensure that the proc macro for extracting strings runs.
        FISH_GETTEXT_EXTRACTION_FILE=$rust_extraction_file cargo check --features=gettext-extract
        or exit 1
    end

    function mark_section
        set -l section_name $argv[1]
        echo 'msgid "fish-section-'$section_name'"'
        echo 'msgstr ""'
        echo ''
    end

    mark_section tier1-from-rust

    # Get rid of duplicates and sort.
    msguniq --no-wrap --sort-output $rust_extraction_file
    or exit 1

    if not set -l --query _flag_use_existing_template
        rm $rust_extraction_file
    end

    function extract_fish_script_messages_impl
        set -l regex $argv[1]
        set -e argv[1]
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
        cat $argv |
            string replace --filter --regex $regex '$1' |
            string unescape |
            sort -u |
            sed -E -e 's_\\\\_\\\\\\\\_g' -e 's_"_\\\\"_g' -e 's_^(.*)$_msgid "\1"\nmsgstr ""\n_'
    end

    function extract_fish_script_messages
        set -l tier $argv[1]
        set -e argv[1]
        if not set -q argv[1]
            return
        end
        # This regex handles explicit requests to translate a message. These are more important to translate
        # than messages which should be implicitly translated.
        set -l explicit_regex '.*\( *_ (([\'"]).+?(?<!\\\\)\\2) *\).*'
        mark_section "$tier-from-script-explicitly-added"
        extract_fish_script_messages_impl $explicit_regex $argv

        # This regex handles descriptions for `complete` and `function` statements. These messages are not
        # particularly important to translate. Hence the "implicit" label.
        set -l implicit_regex '^(?:\s|and |or )*(?:complete|function).*? (?:-d|--description) (([\'"]).+?(?<!\\\\)\\2).*'
        mark_section "$tier-from-script-implicitly-added"
        extract_fish_script_messages_impl $implicit_regex $argv
    end

    set -g share_dir $workspace_root/share

    set -l tier1 $share_dir/config.fish
    set -l tier2
    set -l tier3

    for file in $share_dir/completions/*.fish $share_dir/functions/*.fish
        # set -l tier (string match -r '^# localization: .*' <$file)
        set -l tier (string replace -rf -m1 \
            '^# localization: (.*)$' '$1' <$file)
        if set -q tier[1]
            switch "$tier"
                case tier1 tier2 tier3
                    set -a $tier $file
                case 'skip*'
                case '*'
                    echo >&2 "$file:1 unexpected localization tier: $tier"
                    exit 1
            end
            continue
        end
        set -l dirname (path basename (path dirname $file))
        set -l command_name (path basename --no-extension $file)
        if test $dirname = functions &&
                string match -q -- 'fish_*' $command_name
            set -a tier1 $file
            continue
        end
        if test $dirname != completions
            echo >&2 "$file:1 missing localization tier for function file"
            exit 1
        end
        if test -e $workspace_root/doc_src/cmds/$command_name.rst
            set -a tier1 $file
        else
            set -a tier3 $file
        end
    end

    extract_fish_script_messages tier1 $tier1
    extract_fish_script_messages tier2 $tier2
    extract_fish_script_messages tier3 $tier3
end |
    # At this point, all extracted strings have been written to stdout,
    # starting with the ones taken from the Rust sources,
    # followed by strings explicitly marked for translation in fish scripts,
    # and finally the strings from fish scripts which get translated implicitly.
    # Because we do not eliminate duplicates across these categories,
    # we do it here, since other gettext tools expect no duplicates.
    msguniq --no-wrap
