function __fish_seen_argument
    argparse 's/short=+' 'o/old=+' 'l/long=+' -- $argv

    set cmd (commandline -co)
    set -e cmd[1]
    for t in $cmd
        for s in $_flag_s
            if string match -qr "^-[A-z0-9]*"$s"[A-z0-9]*\$" -- $t
                return 0
            end
        end

        for o in $_flag_o
            if string match -qr "^-$s\$" -- $t
                return 0
            end
        end

        for l in $_flag_l
            if string match -q -- "--$l" $t
                return 0
            end
        end
    end

    return 1
end

