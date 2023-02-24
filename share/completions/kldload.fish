# Completions for the FreeBSD `kldload` kernel module load utility
function __fish_list_kldload_options
    set -l klds (__fish_complete_suffix --complete=/boot/kernel/(commandline -ct) ".ko" | string replace -r '.*/(.+)\\.ko' '$1')
    # Completing available klds is fast, but completing it with a call to __fish_whatis
    # is decidedly not. With 846 modules (FreeBSD 11.1), fish --profile 'complete -C"kldload "' returns the following:
    # 10671   11892698    > complete -C"kldload "
    # A 12 second completion delay is obviously out of the question, so don't provide a description unless there are
    # fewer than 50 results.
    # Additionally, we can halve the time by not shelling out to `whatis` if we know the man file for the kernel module
    # in question does not exist, since the paths are hardcoded.
    set -l kld_count (count $klds)
    if test $kld_count -le 50 -a $kld_count -gt 0
        # print name and description
        for kld in $klds
            printf '%s\t%s\n' $kld (test -e /usr/share/man/man4/$kld.4.gz;
				and __fish_whatis $kld; or echo "kernel module")
        end
    else if test $kld_count -gt 0
        # print name only
        printf '%s\n' $klds
    else
        # print name only (description won't exist since the kernel module isn't installed)
        __fish_complete_suffix .ko
    end
end


# Only attempt to match a local file if there isn't a match in /boot/kernel,
# as odds are that is the desired source.
complete -c kldload -xa '(__fish_list_kldload_options)'
