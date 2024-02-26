function __fish_get_vmctl_vms
    for line in (vmctl status | string match -e -v "MAXMEM")
        set -l a (string split " " $line)
        and printf "%s " $a[-1]
    end
end

complete -c vmctl -xa 'console create load log reload reset start status stop pause unpause send receive' -n 'not __fish_seen_subcommand_from list console create load log reload reset start status stop pause unpause send receive'
complete -c vmctl -n '__fish_seen_subcommand_from console reload reset start status stop pause unpause send receive' -xa '(__fish_get_vmctl_vms)'
