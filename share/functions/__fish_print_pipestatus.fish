function __fish_print_pipestatus --description "Print pipestatus for prompt"

    # allow users to customize these variables
    if not set -q __fish_prompt_pipestatus_left_brace
        set -g __fish_prompt_pipestatus_left_brace $argv[1]
    end

    if not set -q __fish_prompt_pipestatus_right_brace
        set -g __fish_prompt_pipestatus_right_brace $argv[2]
    end

    if not set -q __fish_prompt_pipestatus_separator
        set -g __fish_prompt_pipestatus_separator $argv[3]
    end

    if not set -q __fish_prompt_pipestatus_brace_sep_color
        set -g __fish_prompt_pipestatus_brace_sep_color $argv[4]
    end

    if not set -q __fish_prompt_pipestatus_color
        set -g __fish_prompt_pipestatus_color $argv[5]
    end

    set -e argv[1 2 3 4 5]

    # only output $pipestatus if there was a pipe and any part of it had non-zero exit status
    if set -q argv[2] && string match -qvr '^0$' $argv
        set -l sep (set_color normal){$__fish_prompt_pipestatus_brace_sep_color}{$__fish_prompt_pipestatus_separator}(set_color normal){$__fish_prompt_pipestatus_color}
        set -l last_pipestatus_string (string join "$sep" (__fish_pipestatus_with_signal $argv))
        printf "%s%s%s%s%s%s%s%s%s%s" (set_color normal) $__fish_prompt_pipestatus_brace_sep_color \
            $__fish_prompt_pipestatus_left_brace (set_color normal) $__fish_prompt_pipestatus_color \
            $last_pipestatus_string (set_color normal) $__fish_prompt_pipestatus_brace_sep_color \
            $__fish_prompt_pipestatus_right_brace (set_color normal)
    end
end
