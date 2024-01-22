function __fish_complete_unrar -d "Peek inside of archives and list all files"
    set -l cmd (commandline -pxc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case '-*'
                continue

            case '*.rar'
                if test -f $i
                    set -l file_list (unrar vb $i)
                    printf "%s\tArchived file\n" $file_list
                end
                return
        end
    end
end

complete -c unrar -a "(__fish_complete_unrar)"

complete -x -c unrar -n __fish_use_subcommand -a e -d "Extract files to current directory"
complete -x -c unrar -n __fish_use_subcommand -a l -d "List archive"
complete -x -c unrar -n __fish_use_subcommand -a lt -d "List archive (technical)"
complete -x -c unrar -n __fish_use_subcommand -a lb -d "List archive (bare)"
complete -x -c unrar -n __fish_use_subcommand -a p -d "Print file to stdout"
complete -x -c unrar -n __fish_use_subcommand -a t -d "Test archive files"
complete -x -c unrar -n __fish_use_subcommand -a v -d "Verbosely list archive"
complete -x -c unrar -n __fish_use_subcommand -a vt -d "Verbosely list archive (technical)"
complete -x -c unrar -n __fish_use_subcommand -a vb -d "Verbosely list archive (bare)"
complete -x -c unrar -n __fish_use_subcommand -a x -d "Extract files with full path"
