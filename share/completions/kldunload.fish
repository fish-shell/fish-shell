function __fish_list_loaded_klds
    set -l klds (kldstat | string match -r '\b\S+.ko$')
    for kld in $klds
        if set -l description (__fish_whatis (string replace '.ko' '' -- $kld) "kernel module")
            printf '%s\t%s\n' $kld $description
        else
            printf '%s\n' $kld
        end
    end
end

complete -c kldunload -xa '(__fish_list_loaded_klds)'
