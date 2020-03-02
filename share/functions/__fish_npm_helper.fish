# This file is explicitly sourced by the npm and yarn completions
# It is used to enumerate entries from the npm registry, interact with a locally installed
# package.json file, and more.
# Other completions that would depend on this include jspm, pnpm, etc.

# If all-the-package-names is installed, it will be used to generate npm completions.
# Install globally with `sudo npm install -g all-the-package-names`. Keep it up to date.
function __yarn_helper_installed
    # This function takes the command to globally install a package as $argv[1]
    if not type -q all-the-package-names
        if not set -qg __fish_yarn_pkg_info_shown
            set -l old (commandline)
            commandline -r ""
            echo \nfish: Run `$argv[1] all-the-package-names` to gain intelligent \
                package completion >&2
            commandline -f repaint
            commandline -r $old
            set -g __fish_yarn_pkg_info_shown 1
        end
        return 1
    end
end

# Entire list of packages is too long to be used efficiently in a `complete` subcommand.
# Search it for matches instead.
function __yarn_filtered_list_packages
    # This function takes the command to globally install a package as $argv[1]
    if not __yarn_helper_installed $argv[1]
        return
    end

    # Do not provide any completions if nothing has been entered yet to avoid long hang.
    if string match -r . (commandline -ct)
        # Filter the results here rather than in the C++ code due to #5267
        all-the-package-names | string match -er -- "(?:\\b|_)"(commandline -ct |
            string escape --style=regex) | head -n1000
    end
end

function __yarn_find_package_json
    set parents (__fish_parent_directories (pwd -P))

    for p in $parents
        if test -f "$p/package.json"
            echo "$p/package.json"
            return 0
        end
    end

    return 1
end

function __yarn_installed_packages
    set -l package_json (__yarn_find_package_json)
    if not test $status -eq 0
        # no package.json in tree
        return 1
    end

    if set -l python (__fish_anypython)
        $python -S -c 'import json, sys; data = json.load(sys.stdin);
print("\n".join(data["dependencies"])); print("\n".join(data["devDependencies"]))' <$package_json 2>/dev/null
    else if type -q jq
        jq -r '.dependencies as $a1 | .devDependencies as $a2 | ($a1 + $a2) | to_entries[] | .key' $package_json
    else
        set -l depsFound 0
        for line in (cat $package_json)
            # echo "evaluating $line"
            if test $depsFound -eq 0
                # echo "mode: noDeps"
                if string match -qr '(devD|d)ependencies"' -- $line
                    # echo "switching to mode: deps"
                    set depsFound 1
                    continue
                end
                continue
            end

            if string match -qr '\}' -- $line
                # echo "switching to mode: noDeps"
                set depsFound 0
                continue
            end

            # echo "mode: deps"

            string replace -r '^\s*"([^"]+)".*' '$1' -- $line
        end
    end
end


