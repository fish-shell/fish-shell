# helm - is a tool for managing Kubernetes charts. Charts are packages
# of pre-configured Kubernetes resources.
# See: https://github.com/kubernetes/helm

function __helm_using_command
    set -l cmd (commandline -poc)
    set -l found

    test (count $cmd) -gt (count $argv)
    or return 1

    set -e cmd[1]

    for i in $argv
        contains -- $i $cmd
        and set found $found $i
    end

    test "$argv" = "$found"
end

function __helm_using_option
    set -l cmd (commandline -poc)
    set -l query '('(string join -- '|' $argv)')'

    if test (count $cmd) -gt 1
        if string match -qr -- $query $cmd[-1]
            return 0
        end
    end
    return 1
end

function __helm_using_option_value -a option -a value
    set -l cmd (commandline -poc)

    if test (count $cmd) -gt 1
        string match -qr $option'[= ]'$value -- $cmd
        return $status
    end

    return 1
end

function __helm_seen_any_subcommand_from -a cmd
    __fish_seen_subcommand_from (__helm_subcommands $cmd | string replace -r '\t.*$' '')
end

function __helm_subcommands -a cmd
    switch $cmd
        case ''
            echo create\t'Create a new chart with the given name'
            echo delete\t'Delete the release from Kubernetes'
            echo dependency\t"Manage a chart's dependencies"
            echo fetch\t'Download a chart from a repository'
            echo get\t'Download a named release'
            echo history\t'Fetch release history'
            echo home\t'Display the location of HELM_HOME'
            echo init\t'Initialize Helm on both client and server'
            echo inspect\t'Inspect a chart'
            echo install\t'Install a chart archive'
            echo lint\t'Examine a chart for possible issues'
            echo list\t'List releases'
            echo package\t'Package a chart directory into a chart archive'
            echo repo\t'Operate on chart repositories'
            echo rollback\t'Roll back a release to a previous revision'
            echo search\t'Search for a keyword in charts'
            echo serve\t'Start a local http web server'
            echo status\t'Display the status of the named release'
            echo upgrade\t'Upgrade a release'
            echo verify\t'Verify that a chart has been signed and is valid'
            echo version\t'Print the client/server version information'
        case 'dependency'
            echo build\t'Rebuild the charts/ directory'
            echo list\t'List the dependencies for the given chart'
            echo update\t'Update charts/'
        case 'get'
            echo hooks\t'Download all hooks for a named release'
            echo manifest\t'Download the manifest for a named release'
            echo values\t'Download the values file for a named release'
        case 'inspect'
            echo chart\t'Show inspect chart'
            echo values\t'Show inspect values'
        case 'repo'
            echo add\t'Add a chart repository'
            echo index\t'Generate an index file'
            echo list\t'List chart repositories'
            echo remove\t'Remove a chart repository'
            echo update\t'Update information on available charts'
    end
end

function __helm_releases
    helm ls --short
end

function __helm_release_completions
    helm ls | awk 'NR >= 2 { print $1"\tRelease of "$NF  }'
end

function __helm_release_revisions
    set -l cmd (commandline -poc)

    for pair in (helm list | awk 'NR >= 2 { print $1" "$2 }')
        echo $pair | read -l release revision

        if test $release = $cmd[-1]
            seq 1 $revision
        end
    end
end

function __helm_repositories
    helm repo list | awk 'NR >= 2 { print $1 }'
end

function __helm_charts
    helm search | awk 'NR >= 2 && !/^local\// { print $1 }'
end

#
# Global Flags
#
complete -c helm -l debug -d 'Enable verbose output'
complete -c helm -l home -d 'Location of your Helm config'
complete -c helm -l host -d 'Address of tiller'
complete -c helm -l kube-context -d 'Name of the kubeconfig context to use'
complete -c helm -s h -l help -d 'More information about a command'

#
# Commands
#

# helm [command]
complete -c helm -f -n 'not __helm_seen_any_subcommand_from ""' -a '(__helm_subcommands "")'

# helm create NAME [flags]
complete -c helm -f -n '__helm_using_command create' -s p -l starter -d 'The named Helm starter scaffold'

# helm delete [flags] RELEASE [...]
complete -c helm -f -n '__helm_using_command delete' -a '(__helm_release_completions)' -d 'Release'

complete -c helm -f -n '__helm_using_command delete' -l dry-run -d 'Simulate a delete'
complete -c helm -f -n '__helm_using_command delete' -l no-hooks -d 'Prevent hooks from running during deletion'
complete -c helm -f -n '__helm_using_command delete' -l purge -d 'Remove the release from the store'

# helm dependency [command]
complete -c helm -f -n '__helm_using_command dependency; and not __helm_seen_any_subcommand_from dependency' -a '(__helm_subcommands dependency)'

# helm dependency build [flags] CHART
complete -c helm -f -n '__helm_using_command dependency build' -l keyring -d 'Keyring containing public keys'
complete -c helm -f -n '__helm_using_command dependency build' -l verify -d 'Verify the packages against signatures'

# helm dependency update [flags] CHART
complete -c helm -f -n '__helm_using_command dependency update' -l keyring -d 'Keyring containing public keys'
complete -c helm -f -n '__helm_using_command dependency update' -l verify -d 'Verify the packages against signatures'

# helm fetch [flags] [chart URL | repo/chartname] [...]
complete -c helm -f -n '__helm_using_command fetch; and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -f -n '__helm_using_command fetch' -s d -l destination -d 'Location to write the chart'
complete -c helm -f -n '__helm_using_command fetch' -l keyring -d 'Keyring containing public keys'
complete -c helm -f -n '__helm_using_command fetch' -l prov -d 'Fetch the provenance file'
complete -c helm -f -n '__helm_using_command fetch' -l untar -d 'Will untar the chart after downloading it'
complete -c helm -f -n '__helm_using_command fetch; and __helm_using_option --untar' -l untardir -d 'Directory into which the chart is expanded'
complete -c helm -f -n '__helm_using_command fetch' -l verify -d 'Verify the package against its signature'
complete -c helm -f -n '__helm_using_command fetch' -l version -d 'Specific version of a chart'

# helm get [command]
complete -c helm -f -n '__helm_using_command get; and not __helm_seen_any_subcommand_from get' -a '(__helm_subcommands get)'

# helm get [flags] RELEASE
complete -c helm -f -n '__helm_using_command get' -a '(__helm_release_completions)' -d 'Release'

complete -c helm -f -n '__helm_using_command get' -l revision -d 'Get the named release with revision'

# helm get values [flags] RELEASE
complete -c helm -f -n '__helm_using_command get values' -s a -l all -d 'Dump all (computed) values'

# helm history [flags] RELEASE
complete -c helm -f -n '__helm_using_command history' -a '(__helm_release_completions)' -d 'Release'

complete -c helm -f -n '__helm_using_command history' -l max -d 'Maximum number of revision to include in history'

# helm init [flags]
complete -c helm -f -n '__helm_using_command init' -l canary-image -d 'Use the canary tiller image'
complete -c helm -f -n '__helm_using_command init' -s c -l client-only -d 'Do not install tiller'
complete -c helm -f -n '__helm_using_command init' -l dry-run -d 'Do not install local or remote'
complete -c helm -f -n '__helm_using_command init' -s i -l tiller-image -d 'Override tiller image'

# helm inspect [command]
complete -c helm -f -n '__helm_using_command inspect; and not __helm_seen_any_subcommand_from inspect' -a '(__helm_subcommands inspect)'

# helm inspect [CHART] [flags]
complete -c helm -n '__helm_using_command inspect; and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -f -n '__helm_using_command inspect' -l keyring -d 'Keyring containing public verification keys'
complete -c helm -f -n '__helm_using_command inspect' -l verify -d 'Verify the provenance data for this chart'
complete -c helm -f -n '__helm_using_command inspect' -l version -d 'Version of the chart'

# helm install [CHART] [flags]
complete -c helm -n '__helm_using_command install; and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -f -n '__helm_using_command install' -l dry-run -d 'Simulate an install'
complete -c helm -f -n '__helm_using_command install' -l keyring -d 'Keyring containing public verification keys'
complete -c helm -f -n '__helm_using_command install' -s n -l name -d 'Release name'
complete -c helm -f -n '__helm_using_command install' -l name-template -d 'Specify template used to name the release'
complete -c helm -f -n '__helm_using_command install' -l namespace -d 'Namespace to install the release into'
complete -c helm -f -n '__helm_using_command install' -l no-hooks -d 'Prevent hooks from running during install'
complete -c helm -f -n '__helm_using_command install' -l replace -d 'Re-use the given name if already used'
complete -c helm -f -n '__helm_using_command install' -l set -d 'Set values on the command line'
complete -c helm -f -n '__helm_using_command install' -s f -l values -d 'Specify values in a YAML file'
complete -c helm -f -n '__helm_using_command install' -l verify -d 'Verify the package before installing it'
complete -c helm -f -n '__helm_using_command install' -l version -d 'Specify the exact chart version to install'

# helm lint [flags] PATH
complete -c helm -f -n '__helm_using_command lint' -l strict -d 'Fail on lint warnings'

# helm list [flags] [FILTER]
complete -c helm -f -n '__helm_using_command list' -l all -d 'Show all releases'
complete -c helm -f -n '__helm_using_command list' -s d -l date -d 'Sort by release date'
complete -c helm -f -n '__helm_using_command list' -l deleted -d 'Show deleted releases'
complete -c helm -f -n '__helm_using_command list' -l deleting -d 'Show releases that are currently being deleted'
complete -c helm -f -n '__helm_using_command list' -l deployed -d 'Show deployed releases'
complete -c helm -f -n '__helm_using_command list' -l failed -d 'Show failed releases'
complete -c helm -f -n '__helm_using_command list' -s m -l max -d 'Maximum number of releases to fetch'
complete -c helm -f -n '__helm_using_command list' -s o -l offset -d 'Next release name in the list'
complete -c helm -f -n '__helm_using_command list' -s r -l reverse -d 'Reverse the sort order'
complete -c helm -f -n '__helm_using_command list' -s q -l short -d 'Output short listing format'

# helm package [flags] [CHART] [...]
complete -c helm -n '__helm_using_command install; and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -f -n '__helm_using_command package' -l key -d 'Name of the key to use when signing'
complete -c helm -f -n '__helm_using_command package' -l keyring -d 'Keyring containing public keys'
complete -c helm -f -n '__helm_using_command package' -l save -d 'Save packaged chart to local chart repository'
complete -c helm -f -n '__helm_using_command package' -l sign -d 'Use a PGP private key to sign this package'

# helm repo [command]
complete -c helm -f -n '__helm_using_command repo; and not __helm_seen_any_subcommand_from repo' -a '(__helm_subcommands repo)'

# helm repo add [flags] [NAME] [URL]
complete -c helm -f -n '__helm_using_command repo add' -l no-update -d 'Raise error if repo is already registered'

# helm repo index [flags] [DIR]
complete -c helm -f -n '__helm_using_command repo index' -l merge -d 'Merge the generated index into the given index'
complete -c helm -f -n '__helm_using_command repo index' -l url -d 'URL of chart repository'

# helm repo remove [flags] [NAME]
complete -c helm -f -n '__helm_using_command repo remove' -a '(__helm_repositories)' -d 'Repository'

# helm rollback [RELEASE] [REVISION] [flags]
complete -c helm -f -n '__helm_using_command rollback; and not __fish_seen_subcommand_from (__helm_releases)' -a '(__helm_release_completions)' -d 'Release'
complete -c helm -f -n '__helm_using_command rollback' -a '(__helm_release_revisions)' -d 'Revision'

complete -c helm -f -n '__helm_using_command rollback' -l dry-run -d 'Simulate a rollback'
complete -c helm -f -n '__helm_using_command rollback' -l no-hooks -d 'Prevent hooks from running during rollback'

# helm search [keyword] [flags]
complete -c helm -f -n '__helm_using_command search' -s r -l regexp -d 'Use regular expressions for searching'
complete -c helm -f -n '__helm_using_command search' -s l -l versions -d 'Show the long listing'

# helm serve [flags]
complete -c helm -f -n '__helm_using_command serve' -l address -d 'Address to listen on'
complete -c helm -f -n '__helm_using_command serve' -l repo-path -d 'Path from which to serve charts'

# helm status [flags] RELEASE
complete -c helm -f -n '__helm_using_command status' -a '(__helm_release_completions)' -d 'Release'

complete -c helm -f -n '__helm_using_command status' -l revision -d 'Display the status of the named release with revision'

# helm upgrade [RELEASE] [CHART] [flags]
complete -c helm -f -n '__helm_using_command upgrade; and not __fish_seen_subcommand_from (__helm_releases)' -a '(__helm_release_completions)' -d 'Release'
complete -c helm -n '__helm_using_command upgrade; and __fish_seen_subcommand_from (__helm_releases); and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -f -n '__helm_using_command upgrade' -l dry-run -d 'Simulate an upgrade'
complete -c helm -f -n '__helm_using_command upgrade' -s i -l install -d "Run an install if the release don't exists"
complete -c helm -f -n '__helm_using_command upgrade' -l keyring -d 'Keyring containing public keys'
complete -c helm -f -n '__helm_using_command upgrade' -l namespace -d 'Namespace to install the release into'
complete -c helm -f -n '__helm_using_command upgrade' -l no-hooks -d 'Disable pre/post upgrade hooks'
complete -c helm -f -n '__helm_using_command upgrade' -l set -d 'Set values on the command line'
complete -c helm -f -n '__helm_using_command upgrade' -s f -l values -d 'Specify values in a YAML file'
complete -c helm -f -n '__helm_using_command upgrade' -l verify -d 'Verify the provenance of the chart before upgrading'
complete -c helm -f -n '__helm_using_command upgrade' -l version -d 'Specify the exact chart version to use'

# helm verify [flags] PATH
complete -c helm -f -n '__helm_using_command verify' -l keyring -d 'Keyring containing public keys'

# helm version [flags]
complete -c helm -f -n '__helm_using_command version' -s c -l client -d 'Show the client version'
complete -c helm -f -n '__helm_using_command version' -s s -l server -d 'Show the server version'
