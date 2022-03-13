function __bower_cmds
    echo -en "cache\tManage bower cache
help\tDisplay help information about Bower
home\tOpens a package homepage into your favorite browser
info\tInfo of a particular package
init\tInteractively create a bower.json file
install\tInstall a package locally
link\tSymlink a package folder
list\tList local packages - and possible updates
login\tAuthenticate with GitHub and store credentials
lookup\tLook up a single package URL by name
prune\tRemoves local extraneous packages
register\tRegister a package
search\tSearch for packages by name
update\tUpdate a local package
uninstall\tRemove a local package
unregister\tRemove a package from the registry
version\tBump a package version
"
end

function __bower_args
    echo -en "-f\tMakes various commands more forceful
--force\tMakes various commands more forceful
-j\tOutput consumable JSON
--json\tOutput consumable JSON
-l\tWhat level of logs to report
--loglevel\tWhat level of logs to report
-o\tDo not hit the network
--offline\tDo not hit the network
-q\tOnly output important information
--quiet\tOnly output important information
-s\tDo not output anything, besides errors
--silent\tDo not output anything, besides errors
-V\tMakes output more verbose
--verbose\tMakes output more verbose
--allow-root\tAllows running commands as root
-v\tOutput Bower version
--version\tOutput Bower version
--no-color\tDisable colors"
end

function __bower_matching_pkgs
    bower search (commandline -ct) | string match -r "\S+[^\s]" | string match -v Search
end

# Output of `bower list` is a) slow, b) convoluted. Use `python` or `jq` instead.
function __bower_list_installed
    if not test -e bower.json
        return 1
    end

    if set -l python (__fish_anypython)
        # Warning: That weird indentation is necessary, because python.
        $python -S -c 'import json, sys; data = json.load(sys.stdin);
for k,v in data["dependencies"].items(): print(k + "\t" + v[:18])' <bower.json 2>/dev/null
        return
    end

    if not type -q jq
        return 1
    end

    jq -r '.dependencies | to_entries[] | .key' bower.json
end

complete -c bower -n "__fish_is_nth_token 1" -x -a '(__bower_cmds)'
complete -c bower -n __fish_should_complete_switches -x -a '(__bower_args)'
complete -c bower -n "__fish_seen_subcommand_from install" -x -a '(__bower_matching_pkgs)'
complete -c bower -n "__fish_seen_subcommand_from uninstall" -x -a '(__bower_list_installed)'
