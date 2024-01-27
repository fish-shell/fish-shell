function __fish_argparse_exclusive_args --description 'Helper function to list unused options'
    set --local all_tokens (commandline --tokens-expanded)
    set --erase all_tokens[1]
    set --local current_token (commandline --current-token)

    if set --local index (contains --index -- '--' $all_tokens)
        set index (math $index - 1)
    else
        set index (count $all_tokens)
    end

    set --local specifications
    while test $index -gt 0
        and set -l tok $all_tokens[$index]
        and not string match --quiet -- '-*' $tok

        if string match --quiet '*,*' $tok
            set index (math $index - 1)
            continue
        end

        set -l opt (string replace --regex '=.*' '' $tok)

        if string match --quiet '*/*' $opt
            set --append specifications (string split '/' $opt)
        else if string match --quiet '*#*' $opt
            set --append specifications (string split '#' $opt)
        else
            set --append specifications $opt
        end
        set index (math $index - 1)
    end

    if string match --quiet -- '-x*' $current_token
        set current_token (string replace --regex -- '^-x' '' $current_token)
    end

    if test $index -gt 1
        and string match --regex --quiet -- '^(-x|--exclusive)$' $all_tokens[(math $index - 1)]
        and not test -z $current_token
        set --erase specifications[(count $specifications)]
    end

    set --local used_options (string split -- ',' $current_token)
    set --local unused_specifications

    for item in $specifications
        if not contains -- $item $used_options
            set --append unused_specifications $item
        end
    end

    string join \n $unused_specifications
end

complete --command argparse --no-files

complete --command argparse --short-option h --long-option help --description 'Show help'

complete --command argparse --short-option n --long-option name --require-parameter --no-files \
    --arguments '(functions --all | string replace ", " "\n")' --description 'Use function name'
complete --command argparse --short-option x --long-option exclusive --no-files --require-parameter \
    --arguments '(__fish_append "," (__fish_argparse_exclusive_args))' \
    --description 'Specify mutually exclusive options'
complete --command argparse --short-option N --long-option min-args --no-files --require-parameter \
    --description 'Specify minimum non-option argument count'
complete --command argparse --short-option X --long-option max-args --no-files --require-parameter \
    --description 'Specify maximum non-option argument count'
complete --command argparse --short-option i --long-option ignore-unknown \
    --description 'Ignore unknown options'
complete --command argparse --short-option s --long-option stop-nonopt \
    --description 'Exit on subcommand'
