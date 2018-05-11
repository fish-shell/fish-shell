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

function __helm_kube_contexts
    kubectl config get-contexts -o name 2>/dev/null
end

function __helm_kube_namespaces
    kubectl get namespaces -o name | string replace 'namespace/' ''
end

function __helm_releases
    helm ls --short 2>/dev/null
end

function __helm_release_completions
    helm ls 2>/dev/null | awk 'NR >= 2 { print $1"\tRelease of "$NF  }'
end

function __helm_release_revisions
    set -l cmd (commandline -poc)

    for pair in (helm ls | awk 'NR >= 2 { print $1" "$2 }')
        echo $pair | read -l release revision

        if contains $release $cmd
            seq 1 $revision
            return
        end
    end
end

function __helm_repositories
    helm repo list | awk 'NR >= 2 { print $1 }'
end

function __helm_charts
    helm search | awk 'NR >= 2 && !/^local\// { print $1 }'
end

function __helm_chart_versions
    set -l cmd (commandline -poc)

    for pair in (helm search -l | awk 'NR >= 2 { print $1" "$2 }')
        echo $pair | read -l chart version

        if contains $chart $cmd
            echo $version
        end
    end
end

#
# Global Flags
#
complete -c helm -l debug -f -d 'Enable verbose output'
complete -c helm -l home -r -d 'Location of your Helm config'
complete -c helm -l host -x -d 'Address of tiller'
complete -c helm -l kube-context -x -a '(__helm_kube_contexts)' -d 'Name of the kubeconfig context to use'
complete -c helm -s h -l help -f -d 'More information about a command'

#
# Commands
#

# helm [command]
complete -c helm -n 'not __helm_seen_any_subcommand_from ""' -x -a '(__helm_subcommands "")'

# helm create NAME [flags]
complete -c helm -n '__helm_using_command create' -s p -l starter -x -d 'The named Helm starter scaffold'

# helm delete [flags] RELEASE [...]
complete -c helm -n '__helm_using_command delete' -f -a '(__helm_release_completions)' -d 'Release'

complete -c helm -n '__helm_using_command delete' -l dry-run -f -d 'Simulate a delete'
complete -c helm -n '__helm_using_command delete' -l no-hooks -f -d 'Prevent hooks from running during deletion'
complete -c helm -n '__helm_using_command delete' -l purge -f -d 'Remove the release from the store'

# helm dependency [command]
complete -c helm -n '__helm_using_command dependency; and not __helm_seen_any_subcommand_from dependency' -x -a '(__helm_subcommands dependency)'

# helm dependency build [flags] CHART
complete -c helm -n '__helm_using_command dependency build' -l keyring -r -d 'Keyring containing public keys'
complete -c helm -n '__helm_using_command dependency build' -l verify -f -d 'Verify the packages against signatures'

# helm dependency update [flags] CHART
complete -c helm -n '__helm_using_command dependency update' -l keyring -r -d 'Keyring containing public keys'
complete -c helm -n '__helm_using_command dependency update' -l verify -f -d 'Verify the packages against signatures'

# helm fetch [flags] [chart URL | repo/chartname] [...]
complete -c helm -n '__helm_using_command fetch; and not __fish_seen_subcommand_from (__helm_charts)' -f -a '(__helm_charts)' -d 'Chart'

complete -c helm -n '__helm_using_command fetch' -s d -l destination -r -d 'Location to write the chart'
complete -c helm -n '__helm_using_command fetch' -l keyring -r -d 'Keyring containing public keys'
complete -c helm -n '__helm_using_command fetch' -l prov -f -d 'Fetch the provenance file'
complete -c helm -n '__helm_using_command fetch' -l untar -f -d 'Will untar the chart after downloading it'
complete -c helm -n '__helm_using_command fetch --untar' -l untardir -r -d 'Directory into which the chart is expanded'
complete -c helm -n '__helm_using_command fetch' -l verify -f -d 'Verify the package against its signature'
complete -c helm -n '__helm_using_command fetch' -l version -x -a '(__helm_chart_versions)' -d 'Chart version'

# helm get [command]
complete -c helm -n '__helm_using_command get; and not __helm_seen_any_subcommand_from get' -f -a '(__helm_subcommands get)'

# helm get [flags] RELEASE
complete -c helm -n '__helm_using_command get' -f -a '(__helm_release_completions)' -d 'Release'

complete -c helm -n '__helm_using_command get' -l revision -x -a '(__helm_release_revisions)' -d 'Revision'

# helm get values [flags] RELEASE
complete -c helm -n '__helm_using_command get values' -s a -l all -f -d 'Dump all (computed) values'

# helm history [flags] RELEASE
complete -c helm -n '__helm_using_command history' -f -a '(__helm_release_completions)' -d 'Release'

complete -c helm -n '__helm_using_command history' -l max -x -d 'Maximum number of revision to include in history'

# helm init [flags]
complete -c helm -n '__helm_using_command init' -l canary-image -f -d 'Use the canary tiller image'
complete -c helm -n '__helm_using_command init' -s c -l client-only -f -d 'Do not install tiller'
complete -c helm -n '__helm_using_command init' -l dry-run -f -d 'Do not install local or remote'
complete -c helm -n '__helm_using_command init' -s i -l tiller-image -x -d 'Override tiller image'

# helm inspect [command]
complete -c helm -n '__helm_using_command inspect; and not __helm_seen_any_subcommand_from inspect' -f -a '(__helm_subcommands inspect)'

# helm inspect [CHART] [flags]
complete -c helm -n '__helm_using_command inspect; and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -n '__helm_using_command inspect' -l keyring -r -d 'Keyring containing public verification keys'
complete -c helm -n '__helm_using_command inspect' -l verify -f -d 'Verify the provenance data for this chart'
complete -c helm -n '__helm_using_command inspect' -l version -x -a '(__helm_chart_versions)' -d 'Chart version'

# helm install [CHART] [flags]
complete -c helm -n '__helm_using_command install; and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -n '__helm_using_command install' -l dry-run -f -d 'Simulate an install'
complete -c helm -n '__helm_using_command install' -l keyring -r -d 'Keyring containing public verification keys'
complete -c helm -n '__helm_using_command install' -s n -l name -x -d 'Release name'
complete -c helm -n '__helm_using_command install' -l name-template -r -d 'Specify template used to name the release'
complete -c helm -n '__helm_using_command install' -l namespace -x -a '(__helm_kube_namespaces)' -d 'Namespace'
complete -c helm -n '__helm_using_command install' -l no-hooks -f -d 'Prevent hooks from running during install'
complete -c helm -n '__helm_using_command install' -l replace -f -d 'Re-use the given name if already used'
complete -c helm -n '__helm_using_command install' -l set -x -d 'Set values on the command line'
complete -c helm -n '__helm_using_command install' -s f -l values -r -d 'Specify values in a YAML file'
complete -c helm -n '__helm_using_command install' -l verify -f -d 'Verify the package before installing it'
complete -c helm -n '__helm_using_command install' -l version -x -a '(__helm_chart_versions)' -d 'Chart version'

# helm lint [flags] PATH
complete -c helm -n '__helm_using_command lint' -l strict -f -d 'Fail on lint warnings'

# helm list [flags] [FILTER]
complete -c helm -n '__helm_using_command list' -l all -f -d 'Show all releases'
complete -c helm -n '__helm_using_command list' -s d -l date -f -d 'Sort by release date'
complete -c helm -n '__helm_using_command list' -l deleted -f -d 'Show deleted releases'
complete -c helm -n '__helm_using_command list' -l deleting -f -d 'Show releases that are currently being deleted'
complete -c helm -n '__helm_using_command list' -l deployed -f -d 'Show deployed releases'
complete -c helm -n '__helm_using_command list' -l failed -f -d 'Show failed releases'
complete -c helm -n '__helm_using_command list' -s m -l max -x -d 'Maximum number of releases to fetch'
complete -c helm -n '__helm_using_command list' -s o -l offset -x -a '(__helm_release_completions)' -d 'Next release name in the list'
complete -c helm -n '__helm_using_command list' -s r -l reverse -f -d 'Reverse the sort order'
complete -c helm -n '__helm_using_command list' -s q -l short -f -d 'Output short listing format'

# helm package [flags] [CHART_PATH] [...]
complete -c helm -n '__helm_using_command package' -l key -x -d 'Name of the key to use when signing'
complete -c helm -n '__helm_using_command package' -l keyring -r -d 'Keyring containing public keys'
complete -c helm -n '__helm_using_command package' -l save -f -d 'Save packaged chart to local chart repository'
complete -c helm -n '__helm_using_command package' -l sign -f -d 'Use a PGP private key to sign this package'

# helm repo [command]
complete -c helm -n '__helm_using_command repo; and not __helm_seen_any_subcommand_from repo' -f -a '(__helm_subcommands repo)'

# helm repo add [flags] [NAME] [URL]
complete -c helm -n '__helm_using_command repo add' -l no-update -f -d 'Raise error if repo is already registered'

# helm repo index [flags] [DIR]
complete -c helm -n '__helm_using_command repo index' -l merge -x -d 'Merge the generated index into the given index'
complete -c helm -n '__helm_using_command repo index' -l url -x -d 'URL of chart repository'

# helm repo remove [flags] [NAME]
complete -c helm -n '__helm_using_command repo remove' -f -a '(__helm_repositories)' -d 'Repository'

# helm rollback [RELEASE] [REVISION] [flags]
complete -c helm -n '__helm_using_command rollback; and not __fish_seen_subcommand_from (__helm_releases)' -f -a '(__helm_release_completions)' -d 'Release'
complete -c helm -n '__helm_using_command rollback' -f -a '(__helm_release_revisions)' -d 'Revision'

complete -c helm -n '__helm_using_command rollback' -l dry-run -f -d 'Simulate a rollback'
complete -c helm -n '__helm_using_command rollback' -l no-hooks -f -d 'Prevent hooks from running during rollback'

# helm search [keyword] [flags]
complete -c helm -n '__helm_using_command search' -s r -l regexp -f -d 'Use regular expressions for searching'
complete -c helm -n '__helm_using_command search' -s l -l versions -f -d 'Show the long listing'

# helm serve [flags]
complete -c helm -n '__helm_using_command serve' -l address -x -d 'Address to listen on'
complete -c helm -n '__helm_using_command serve' -l repo-path -r -d 'Path from which to serve charts'

# helm status [flags] RELEASE
complete -c helm -n '__helm_using_command status' -f -a '(__helm_release_completions)' -d 'Release'

complete -c helm -n '__helm_using_command status' -l revision -x -a '(__helm_release_revisions)' -d 'Revision'

# helm upgrade [RELEASE] [CHART] [flags]
complete -c helm -n '__helm_using_command upgrade; and not __fish_seen_subcommand_from (__helm_releases)' -f -a '(__helm_release_completions)' -d 'Release'
complete -c helm -n '__helm_using_command upgrade; and __fish_seen_subcommand_from (__helm_releases); and not __fish_seen_subcommand_from (__helm_charts)' -a '(__helm_charts)' -d 'Chart'

complete -c helm -n '__helm_using_command upgrade' -l dry-run -f -d 'Simulate an upgrade'
complete -c helm -n '__helm_using_command upgrade' -s i -l install -f -d "Run an install if the release don't exists"
complete -c helm -n '__helm_using_command upgrade' -l keyring -r -d 'Keyring containing public keys'
complete -c helm -n '__helm_using_command upgrade' -l namespace -x -a '(__helm_kube_namespaces)' -d 'Namespace'
complete -c helm -n '__helm_using_command upgrade' -l no-hooks -f -d 'Disable pre/post upgrade hooks'
complete -c helm -n '__helm_using_command upgrade' -l set -x -d 'Set values on the command line'
complete -c helm -n '__helm_using_command upgrade' -s f -l values -r -d 'Specify values in a YAML file'
complete -c helm -n '__helm_using_command upgrade' -l verify -f -d 'Verify the provenance of the chart before upgrading'
complete -c helm -n '__helm_using_command upgrade' -l version -x -a '(__helm_chart_versions)' -d 'Chart version'

# helm verify [flags] PATH
complete -c helm -n '__helm_using_command verify' -l keyring -r -d 'Keyring containing public keys'

# helm version [flags]
complete -c helm -n '__helm_using_command version' -s c -l client -f -d 'Show the client version'
complete -c helm -n '__helm_using_command version' -s s -l server -f -d 'Show the server version'
