function __fish_complete_svn_diff --description 'Complete "svn diff" arguments'
    set -l cmdl (commandline -cop)
    #set -l cmdl svn diff --diff-cmd diff --extensions '-a -b'
    set -l diff diff
    set -l args
    while set -q cmdl[1]
        switch $cmdl[1]
            case --diff-cmd
                if set -q cmdl[2]
                    set diff $cmdl[2]
                    set -e cmd[2]
                end

            case --extensions
                if set -q cmdl[2]
                    set args $cmdl[2]
                    set -e cmdl[2]
                end
        end
        set -e cmdl[1]
    end
    set -l token (commandline -cpt)
    complete -C"$diff $args $token"

end
