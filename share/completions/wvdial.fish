function __fish_complete_wvdial_peers --description 'Complete wvdial peers' --argument-names cfgfiles
    set -q cfgfiles[1]
    or set -l cfgfiles /etc/wvdial.conf ~/.wvdialrc

    # test if there is an alternative config file specified
    set -l store_next
    for opt in (commandline -cpx)
        if set -q store_next[1]
            set store_next
            set cfgfiles $opt
            continue
        end

        switch $opt
            case -C --config
                set store_next true
            case '--config=*'
                set cfgfiles (string replace '--config=' '' -- $opt)
        end
    end

    for file in (path filter -rf -- $cfgfiles)
        string match -r '\[Dialer' <$file | string replace -r '\[Dialer (.+)\]' '$1'
    end | path sort -u | string match -v Defaults

end

complete -c wvdial -xa "(__fish_complete_wvdial_peers)" -d "wvdial connections"
complete -c wvdial -s c -l chat -d 'Run wvdial as chat replacement from within pppd'
complete -c wvdial -s C -l config -r -d 'Run wvdial with alternative config file'
complete -c wvdial -s n -l no-syslog -d 'Don\'t output debug information'
