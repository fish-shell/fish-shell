function __fish_print_portage_repository_paths --description 'Print the paths of all configured repositories'
    set -l a /etc/portage/repos.conf
    set -l b
    set -l c /usr/share/portage/config/repos.conf
    test -d $a
    and set b (path filter -f -- $a/**)
    test -f $a
    and set b $a
    test -n "$b"
    and string match -q "[gentoo]" -- (cat $b 2>/dev/null)
    and set c $b
    or set -a c $b
    path is -r -- $c
    or return
    cat $c 2>/dev/null | string match -g -r '^location = (.*$)'
end
