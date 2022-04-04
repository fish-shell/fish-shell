function pushd --description 'Push directory to stack'
    set -l rot_r
    set -l rot_l

    if count $argv >/dev/null
        # check for --help
        switch $argv[1]
            case -h --h --he --hel --help
                __fish_print_help pushd
                return 0
        end

        # emulate bash by checking if argument of form +n or -n
        if string match -qr '^-[0-9]+$' -- $argv[1]
            set rot_r (string sub -s 2 -- $argv[1])
        else if string match -qr '^\+[0-9]+$' -- $argv[1]
            set rot_l (string sub -s 2 -- $argv[1])
        end
    end

    set -q dirstack
    or set -g dirstack

    # emulate bash: an empty pushd should switch the top of dirs
    if not set -q argv[1]
        # check that the stack isn't empty
        if not set -q dirstack[1]
            echo "pushd: no other directory" >&2
            return 1
        end

        # get the top two values of the dirs stack ... the first is pwd
        set -l top_dir $PWD
        set -l next_dir $dirstack[1]

        # alter the top of dirstack and move to directory
        set -g dirstack[1] $top_dir
        cd $next_dir
        return
    end

    # emulate bash: check for rotations
    if test -n "$rot_l" -o -n "$rot_r"
        # grab the current stack
        set -l stack $PWD $dirstack

        # translate a right rotation to a left rotation
        if test -n "$rot_r"
            # check the rotation in range
            if test $rot_r -ge (count $stack)
                echo "pushd: -$rot_r: directory stack index out of range" >&2
                return 1
            end

            set rot_l (math (count $stack) - 1 - $rot_r)
        end

        # check the rotation in range
        if test $rot_l -ge (count $stack)
            echo "pushd: +$rot_l: directory stack index out of range" >&2
            return 1
        else
            # rotate stack unless rot_l is 0
            if test $rot_l -gt 0
                set stack $stack[(math $rot_l + 1)..(count $stack)] $stack[1..$rot_l]
            end

            # now reconstruct dirstack and change directory
            set -g dirstack $stack[2..(count $stack)]
            cd $stack[1]
        end

        # print the new stack
        dirs
        return
    end

    # argv[1] is a directory
    set -l old_pwd $PWD
    cd $argv[1]; and set -g -p dirstack $old_pwd
end
