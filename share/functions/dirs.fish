function dirs --description 'Print directory stack'
    # process options
    if set -q argv[1]
        switch $argv[1]
            case -c
                # clear directory stack
                set -e -g dirstack
                return 0
        end
    end

    # replace $HOME with ~
    string replace -r '^'"$HOME"'($|/)' '~$1' -- $PWD $dirstack | string join " "
    echo
end
