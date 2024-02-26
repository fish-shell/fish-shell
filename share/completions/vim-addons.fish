# Completion for vim-addons
# =========================
#
# Adrien Grellier <adrien.grellier@laposte.net>
#

function __fish_vim-addons_subcommand -d 'Test if vim-addons has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i list status install remove disable amend files show
            return 1
        end
    end
    return 0
end

# Commands
# --------
# list
complete -c vim-addons -n __fish_vim-addons_subcommand -a list -f -d "list the names of the addons available in the system"
# status
complete -c vim-addons -n __fish_vim-addons_subcommand -a status -f -d "show the status of the addons available in the system"
complete -c vim-addons -n "contains status (commandline -pxc)" -a "(vim-addons list)" -x
# install
complete -c vim-addons -n __fish_vim-addons_subcommand -a install -x -d "install one or more addons under ~/.vim"
complete -c vim-addons -n "contains install (commandline -pxc)" -a "(vim-addons status | awk '\$2 != \"installed\" && \$1 != \"#\" { print \$1}')" -x
# remove
complete -c vim-addons -n __fish_vim-addons_subcommand -a remove -x -d "remove one or more addons from ~/.vim"
complete -c vim-addons -n "contains remove (commandline -pxc)" -a "(vim-addons status | awk '\$2 != \"removed\" && \$1 != \"#\" { print \$1}')" -x
# disable
complete -c vim-addons -n __fish_vim-addons_subcommand -a disable -x -d "disable  one  or more addons to be used by the current user"
complete -c vim-addons -n "contains disable (commandline -pxc)" -a "(vim-addons status | awk '\$2 != \"disable\" && \$1 != \"#\" { print \$1}')" -x
# amend
complete -c vim-addons -n __fish_vim-addons_subcommand -a amend -x -d "undo the effects of a previous disable command"
complete -c vim-addons -n "contains amend (commandline -pxc)" -a "(vim-addons list)" -x
# files
complete -c vim-addons -n __fish_vim-addons_subcommand -a files -x -d "list, one per line, the files composing the specified addons"
complete -c vim-addons -n "contains files (commandline -pxc)" -a "(vim-addons list)" -x
# show
complete -c vim-addons -n __fish_vim-addons_subcommand -a show -x -d "displays detailed information about the specified addons"
complete -c vim-addons -n "contains show (commandline -pxc)" -a "(vim-addons list)" -x

# Options
# --------
complete -c vim-addons -s h -l help -d "show this usage message and exit"
complete -c vim-addons -s q -l query -d "be quiet and make the output more parseable"
complete -c vim-addons -s r -l registry-dir -d "set the registry directory"
complete -c vim-addons -s s -l source-dir -d "set the addons source directory"
complete -c vim-addons -s t -l target-dir -d "set the addons target directory"
complete -c vim-addons -s v -l verbose -d "increase verbosity"
complete -c vim-addons -s y -l system-dir -d "set the system-wide target directory"
complete -c vim-addons -s w -l system-wide -d "set target directory to the system-wide one"
