function __fish_clasp_list_projects
    clasp list --noShorten true 2>/dev/null |
        string replace --regex '(.*) - https://script.google.com/d/(.*)/edit' '$2\\t$1'
end

function __fish_clasp_list_versions
    clasp versions 2>/dev/null |
        string replace --regex '(\\d+) - (.*)' '$1\\t$2' |
        sed -n '2,$p'
end

function __fish_clasp_list_deployments
    clasp deployments 2>/dev/null |
        string replace --regex -- '- (\\S+) @(\\S+).*' '$1\\tversion $2' |
        sed -n '2,$p'
end

function __fish_clasp_list_advanced_services
    printf '%s\\t%s service\n' admin-sdk-directory 'Admin SDK Directory' \
        admin-sdk-license-manager 'Admin SDK Enterprise License Manager' \
        admin-sdk-groups-migration 'Admin SDK Groups Migration' \
        admin-sdk-groups-settings 'Admin SDK Groups Settings' \
        admin-sdk-reseller 'Admin SDK Google Workspace Reseller' \
        admin-sdk-reports 'Admin SDK Reports' \
        calendar 'Advanced Calendar' \
        chat 'Advanced Chat' \
        docs 'Advanced Docs' \
        drive 'Advanced Drive' \
        drive-activity 'Google Drive Activity' \
        drive-labels 'Advanced Drive Labels' \
        gmail 'Advanced Gmail' \
        sheets 'Advanced Sheets' \
        slides 'Advanced Slides' \
        classroom Classroom \
        people 'Advanced People' \
        contacts-people 'Migrate from Contacts service to People API' \
        tasks Tasks
end

function __fish_clasp_list_functions
    find . -name '*.js' -exec sed -nE '/^\s*function/ s/^\s*function\s+(\w+).*$/\1/p' '{}' \;
end

function __fish_clasp_list_subcommands
    clasp --help |
        string match --regex '^  [a-z]' --entire |
        string replace --regex '^\\s{2}([a-z]+).*' '$1'
end

function __fish_clasp_seen_subcommands_from
    set -l cmd (commandline -poc)
    set -e cmd[1]

    test (count $argv) -gt (count $cmd) && return 1

    set -l i (count $argv)
    set -l j (count $cmd)
    while test $i -gt 0
        contains -- $argv[$i] $cmd[$j] || return 1
        set i (math $i - 1)
        set j (math $j - 1)
    end

    return 0
end

# options
complete -c clasp -s h -l help -d 'Show [h]elp'
complete -c clasp -s v -l version -d 'Show [v]ersion'

complete -c clasp -s A -l auth -d "Path to a '.clasprc.json' or to a directory with it"
complete -c clasp -s I -l ignore -d "Path to a '.claspignore.json' or to a directory with it"
complete -c clasp -s P -l project -d "Path to a '.clasp.json' or to a directory with it"
complete -c clasp -s W -l why -d "Display some debugging info upon exit"

# subcommands
complete -c clasp -n __fish_use_subcommand -xa login -d "Log in to script.google.com"
complete -c clasp -n __fish_use_subcommand -xa logout -d "Log out"
complete -c clasp -n __fish_use_subcommand -xa create -d "Create a script"
complete -c clasp -n __fish_use_subcommand -xa clone -d "Clone a project"
complete -c clasp -n __fish_use_subcommand -xa pull -d "Fetch a remote project"
complete -c clasp -n __fish_use_subcommand -xa push -d "Update the remote project"
complete -c clasp -n __fish_use_subcommand -xa status -d "Lists files that will be pushed by 'push' subcommand"
complete -c clasp -n __fish_use_subcommand -xa open -d "Open a script"
complete -c clasp -n __fish_use_subcommand -xa deployments -d "List deployment IDs of a script"
complete -c clasp -n __fish_use_subcommand -xa deploy -d "Deploy a project"
complete -c clasp -n __fish_use_subcommand -xa undeploy -d "Undeploy a project"
complete -c clasp -n __fish_use_subcommand -xa version -d "Create an immutable version of a script"
complete -c clasp -n __fish_use_subcommand -xa versions -d "List versions of a script"
complete -c clasp -n __fish_use_subcommand -xa list -d "List projects"
complete -c clasp -n __fish_use_subcommand -xa logs -d "Show StackDriver logs"
complete -c clasp -n __fish_use_subcommand -xa run -d "Run a function in your Apps Scripts project"
complete -c clasp -n __fish_use_subcommand -xa apis -d "List, enable, or disable APIs"
complete -c clasp -n __fish_use_subcommand -xa help -d "Show help for a command"

# login options
set login_condition '__fish_seen_subcommand_from login'
complete -c clasp -n "$login_condition" -l no-localhost -d 'Do not run a local server, manually enter code instead'
complete -c clasp -n "$login_condition" -l creds -d 'Specify a relative path to credentials'
complete -c clasp -n "$login_condition" -l status -d 'Show who is logged in'

# create option
set create_condition '__fish_seen_subcommand_from create'
complete -c clasp -n "$create_condition" -l type -xa 'standalone docs sheets slides forms webapp api' -d "A project type"
complete -c clasp -n "$create_condition" -l title -d "A project title"
complete -c clasp -n "$create_condition" -l parentId -d "A project container ID"
complete -c clasp -n "$create_condition" -l rootDir -d "Path to a directory with project files"

# clone options
set clone_condition '__fish_seen_subcommand_from clone'
complete -c clasp -n "$clone_condition" -l rootDir -d "Path to a directory project files"
complete -c clasp -n "$clone_condition" -xa '(__fish_clasp_list_projects)'

# pull options
complete -c clasp -n '__fish_seen_subcommand_from pull' -l versionNumber -xa '(__fish_clasp_list_versions)' -d "A project version to pull"

# push options
set push_condition '__fish_seen_subcommand_from push'
complete -c clasp -n "$push_condition" -s f -l force -d "Forcibly overwrite a remote manifest"
complete -c clasp -n "$push_condition" -s w -l watch -d "Watch for local changes in non-ignored files and push when they occur"

# status options
complete -c clasp -n '__fish_seen_subcommand_from status' -l json -d "Show in JSON format"

# open options
set open_condition '__fish_seen_subcommand_from open'
complete -c clasp -n "$open_condition" -l webapp -d "Open a web application in a browser"
complete -c clasp -n "$open_condition" -l creds -d "Open the URL to create credentials"
complete -c clasp -n "$open_condition" -l addon -d "List parent IDs and open the URL of the first one"
complete -c clasp -n "$open_condition" -l deploymentId -d "Use a custom deployment ID with a web application"

# deploy options
set deploy_condition '__fish_seen_subcommand_from deploy'
complete -c clasp -n "$deploy_condition" -s V -l versionNumber -d "A project version"
complete -c clasp -n "$deploy_condition" -s d -l description -d "A deployment description"
complete -c clasp -n "$deploy_condition" -s i -l deploymentId -xa '(__fish_clasp_list_deployments)' -d "A deployment ID to redeploy"

# undeploy options
set undeploy_condition '__fish_seen_subcommand_from undeploy'
complete -c clasp -n "$undeploy_condition" -l all -d "Undeploy all deployments"
complete -c clasp -n "$undeploy_condition" -xa '(__fish_clasp_list_deployments)'

# list options
complete -c clasp -n '__fish_seen_subcommand_from list' -l noShorten -d "Do not shorten long names"

# logs options
set logs_condition '__fish_seen_subcommand_from logs'
complete -c clasp -n "$logs_condition" -l json -d "Show logs in JSON form"
complete -c clasp -n "$logs_condition" -l open -d "Open the StackDriver logs in a browser"
complete -c clasp -n "$logs_condition" -l setup -d "Setup StackDriver logs"
complete -c clasp -n "$logs_condition" -l watch -d "Watch and list new logs"
complete -c clasp -n "$logs_condition" -l simplified -d "Hide timestamps with logs"

# run options
set run_condition '__fish_seen_subcommand_from run'
complete -c clasp -n "$run_condition" -l nondev -d "Run a function in non-development mode"
complete -c clasp -n "$run_condition" -s p -l params -d "Specify parameters for a function as a JSON array"
complete -c clasp -n "$run_condition" -xa '(__fish_clasp_list_functions)'

# apis subcommands and options
set apis_condition '__fish_seen_subcommand_from apis'
set apis_subcommand_condition '__fish_clasp_seen_subcommands_from apis enable || __fish_clasp_seen_subcommands_from apis disable'
complete -c clasp -n "$apis_condition" -xa list -d 'List APIs'
complete -c clasp -n "$apis_condition" -xa enable -d 'Enable APIs'
complete -c clasp -n "$apis_condition" -xa disable -d 'Disable APIs'
complete -c clasp -n "$apis_condition" -l open -d "Open API Console in a browser"
complete -c clasp -n "$apis_subcommand_condition" -a '(__fish_clasp_list_advanced_services)'

# help subcommands
complete -c clasp -n '__fish_seen_subcommand_from help' -xa '(__fish_clasp_list_subcommands)'
