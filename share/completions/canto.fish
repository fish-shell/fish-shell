function __fish_canto_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
        if test count $argv -gt 2
            if test $argv[2] = $cmd[2]
                return 0
            end
        end
    end
    return 1
end

complete -f -c canto -s h -l help -d 'Show Help'
complete -f -c canto -s v -l version -d 'Show version'
complete -f -c canto -s u -l update -d 'Update before running'
complete -f -c canto -s l -l list -d 'List feeds'
complete -f -c canto -s a -l checkall -d 'Show number of new items'

complete -f -c canto -s n -l checknew -d 'Show number of new items for feed'
complete -f -c canto -n '__fish_canto_using_command -l --checknew' -d Feed -a '(command canto -l)'

complete -c canto -s o -l opml -d 'Print conf as OPML'
complete -c canto -s i -l import -d 'Import from OPML'
complete -f -c canto -s r -l url -d 'Add feed'

complete -c canto -s D -l dir -d 'Set configuration directory'
complete -c canto -s C -l conf -d 'Set configuration file'
complete -c canto -s L -l log -d 'Set client log file'
complete -c canto -s F -l fdir -d 'Set feed directory'
complete -c canto -s S -l sdir -d 'Set script directory'
