function __fish_print_portage_repository_paths --description 'Print the paths of all configured repositories'
    set a /etc/portage/repos.conf
    set default /usr/share/portage/config/repos.conf
    set confs
    if test -f $a
        set confs $a
    else if test -d $a
        set -p confs (find $a -type f -name '*.conf')
        if not contains "$a/gentoo.conf" $confs
            set -p confs $default
        end
    else
        set confs $default
    end
    for b in $confs
        cat $b | string match -g -r 'location = (.*$)'
    end
end
