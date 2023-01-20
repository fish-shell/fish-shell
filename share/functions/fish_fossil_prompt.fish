function fish_fossil_prompt --description 'Write out the fossil prompt'
    # Bail if fossil is not available
    if not command -sq fossil
        return 1
    end

    # Bail if not a fossil checkout
    if not test -e .fslckout
        return 127
    end

    # Parse fossil info
    set -l fossil_info (fossil info)
    string match --regex --quiet \
        '^project-name:\s*(?<fossil_project_name>.*)$' $fossil_info
    string match --regex --quiet \
        '^tags:\s*(?<fossil_tags>.*)$' $fossil_info

    echo -n '['
    set_color --bold magenta
    echo -n $fossil_project_name
    set_color normal
    echo -n ':'
    set_color --bold yellow
    echo -n $fossil_tags
    set_color normal

    # Parse fossil status
    set -l fossil_status (fossil status)
    if string match --index --regex --quiet '^ADDED' $fossil_status
        set_color green
        echo -n '+'
    end
    if string match --index --regex --quiet '^DELETED' $fossil_status
        set_color red
        echo -n '-'
    end
    if string match --index --regex --quiet '^MISSING' $fossil_status
        set_color red
        echo -n '!'
    end
    if string match --index --regex --quiet '^RENAMED' $fossil_status
        set_color yellow
        echo -n '→'
    end
    if string match --index --regex --quiet '^CONFLICT' $fossil_status
        set_color green
        echo -n '×'
    end

    set_color normal
    echo -n ']'
end
