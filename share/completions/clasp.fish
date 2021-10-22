# options
complete -c clasp -s v -l version -d "output the current version"
complete -c clasp -s A -l auth -d "path to an auth file or a folder with a '.clasprc.json' file."
complete -c clasp -s I -l ignore -d "path to an ignore file or a folder with a '.claspignore' file."
complete -c clasp -s P -l project -d "path to a project file or to a folder with a '.clasp.json' file."
complete -c clasp -s W -l why -d "Display some debugging info upon exit."
complete -c clasp -s h -l help -d "display help for command"

# subcommands
complete -f -c clasp -n '__fish_use_subcommand' -xa login -d "Log in to script.google.com"
complete -f -c clasp -n '__fish_use_subcommand' -xa logout -d "Log out"
complete -f -c clasp -n '__fish_use_subcommand' -xa create -d "Create a script"
complete -f -c clasp -n '__fish_use_subcommand' -xa clone -d "Clone a project"
complete -f -c clasp -n '__fish_use_subcommand' -xa pull -d "Fetch a remote project"
complete -f -c clasp -n '__fish_use_subcommand' -xa push -d "Update the remote project"
complete -f -c clasp -n '__fish_use_subcommand' -xa status -d "Lists files that will be pushed by clasp"
complete -f -c clasp -n '__fish_use_subcommand' -xa open -d "Open a script"
complete -f -c clasp -n '__fish_use_subcommand' -xa deployments -d "List deployment ids of a script"
complete -f -c clasp -n '__fish_use_subcommand' -xa deploy -d "Deploy a project"
complete -f -c clasp -n '__fish_use_subcommand' -xa undeploy -d "Undeploy a deployment of a project"
complete -f -c clasp -n '__fish_use_subcommand' -xa version -d "Creates an immutable version of the script"
complete -f -c clasp -n '__fish_use_subcommand' -xa versions -d "List versions of a script"
complete -f -c clasp -n '__fish_use_subcommand' -xa list -d "List App Scripts projects"
complete -f -c clasp -n '__fish_use_subcommand' -xa logs -d "Shows the StackDriver logs"
complete -f -c clasp -n '__fish_use_subcommand' -xa run -d "Run a function in your Apps Scripts project"
complete -f -c clasp -n '__fish_use_subcommand' -xa apis -d "List, enable, or disable APIs"
complete -f -c clasp -n '__fish_use_subcommand' -xa help -d "display help for command"

# login options
complete -c clasp -n '__fish_seen_subcommand_from login' -l no-localhost -d 'Do not run a local server, manually enter code instead'
complete -c clasp -n '__fish_seen_subcommand_from login' -l creds -d 'Relative path to credentials (from GCP).'
complete -c clasp -n '__fish_seen_subcommand_from login' -l status -d 'Print who is logged in'

# create option
complete -c clasp -n '__fish_seen_subcommand_from create' -l type -d "Creates a new Apps Script project attached to a new Document, Spreadsheet, Presentation, Form, or as a standalone script, web app, or API."
complete -c clasp -n '__fish_seen_subcommand_from create' -l parentId -d "A project parent Id."
complete -c clasp -n '__fish_seen_subcommand_from create' -l rootDir -d "Local root directory in which clasp will store your project files."

# clone options
complete -c clasp -n '__fish_seen_subcommand_from clone' -l rootDir -d "Local root directory in which clasp will store your project files."

# pull options
complete -c clasp -n '__fish_seen_subcommand_from pull' -l versionNumber -d "The version number of the project to retrieve."

# push options
complete -c clasp -n '__fish_seen_subcommand_from push' -s f -l force -d "Forcibly overwrites the remote manifest."
complete -c clasp -n '__fish_seen_subcommand_from push' -s w -l watch -d "Watches for local file changes. Pushes when a non-ignored file changes."

# status options
complete -c clasp -n '__fish_seen_subcommand_from status' -l json -d "Show status in JSON form"

# open options
complete -c clasp -n '__fish_seen_subcommand_from open' -l webapp -d "Open web application in the browser"
complete -c clasp -n '__fish_seen_subcommand_from open' -l creds -d "Open the URL to create credentials"
complete -c clasp -n '__fish_seen_subcommand_from open' -l addon -d "List parent IDs and open the URL of the first one"
complete -c clasp -n '__fish_seen_subcommand_from open' -l deploymentId -d "Use custom deployment ID with webapp"

# deploy options
complete -c clasp -n '__fish_seen_subcommand_from deploy' -s V -l versionNumber -d "The project version"
complete -c clasp -n '__fish_seen_subcommand_from deploy' -s d -l description -d "The deployment description"
complete -c clasp -n '__fish_seen_subcommand_from deploy' -s i -l deploymentId -d "The deployment ID to redeploy"

# undeploy options
complete -c clasp -n '__fish_seen_subcommand_from undeploy' -l all -d "Undeploy all deployments"
complete -c clasp -n '__fish_seen_subcommand_from undeploy' -l help -d "display help for command"

# list options
complete -c clasp -n '__fish_seen_subcommand_from list' -l noShorten -d "Do not shorten long names (default: false)"

# logs options
complete -c clasp -n '__fish_seen_subcommand_from logs' -l json -d "Show logs in JSON form"
complete -c clasp -n '__fish_seen_subcommand_from logs' -l open -d "Open the StackDriver logs in the browser"
complete -c clasp -n '__fish_seen_subcommand_from logs' -l setup -d "Setup StackDriver logs"
complete -c clasp -n '__fish_seen_subcommand_from logs' -l watch -d "Watch and print new logs"
complete -c clasp -n '__fish_seen_subcommand_from logs' -l simplified -d "Hide timestamps with logs"

# run options
complete -c clasp -n '__fish_seen_subcommand_from run' -l nondev -d "Run script function in non-devMode"
complete -c clasp -n '__fish_seen_subcommand_from run' -s p -l params -d "Add parameters required for the function as a JSON String Array"

# apis subcommands and options
complete -c clasp -n '__fish_seen_subcommand_from apis' -xa list
complete -c clasp -n '__fish_seen_subcommand_from apis' -xa enable
complete -c clasp -n '__fish_seen_subcommand_from apis' -xa disable
complete -c clasp -n '__fish_seen_subcommand_from apis' -l open -d "Open the API Console in the browser"

# help subcommands
complete -c clasp -n '__fish_seen_subcommand_from help' -xa login
complete -c clasp -n '__fish_seen_subcommand_from help' -xa logout
complete -c clasp -n '__fish_seen_subcommand_from help' -xa create
complete -c clasp -n '__fish_seen_subcommand_from help' -xa clone
complete -c clasp -n '__fish_seen_subcommand_from help' -xa pull
complete -c clasp -n '__fish_seen_subcommand_from help' -xa push
complete -c clasp -n '__fish_seen_subcommand_from help' -xa status
complete -c clasp -n '__fish_seen_subcommand_from help' -xa open
complete -c clasp -n '__fish_seen_subcommand_from help' -xa deployments
complete -c clasp -n '__fish_seen_subcommand_from help' -xa deploy
complete -c clasp -n '__fish_seen_subcommand_from help' -xa undeploy
complete -c clasp -n '__fish_seen_subcommand_from help' -xa version
complete -c clasp -n '__fish_seen_subcommand_from help' -xa versions
complete -c clasp -n '__fish_seen_subcommand_from help' -xa list
complete -c clasp -n '__fish_seen_subcommand_from help' -xa logs
complete -c clasp -n '__fish_seen_subcommand_from help' -xa run
complete -c clasp -n '__fish_seen_subcommand_from help' -xa apis
