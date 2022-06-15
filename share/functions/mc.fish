function mc --description='Midnight Commander'
    set -gx _fish_MC_USER (whoami)
    set -q TMPDIR || set -gx TMPDIR /tmp
    set -gx _fish_MC_PWD_FILE $TMPDIR/mc-(id -un)/mc.pwd.$fish_pid
    trap "rm -f $_fish_MC_PWD_FILE" EXIT
    command mc -P "$_fish_MC_PWD_FILE" $argv

    if test -r $_fish_MC_PWD_FILE
        set -gx _fish_MC_PWD (cat $_fish_MC_PWD_FILE)
        if test -n $_fish_MC_PWD && test $_fish_MC_PWD != $PWD && test -d $_fish_MC_PWD
            cd $_fish_MC_PWD
        end
        set -e _fish_MC_PWD
    end

    set -e _fish_MC_PWD_FILE
    set -e _fish_MC_USER
end
