# Returns 0 if the command has not had a subcommand yet
# Does not currently account for -chdir
function __fish_terraform_needs_command
    set -l cmd (commandline -xpc)

    if test (count $cmd) -eq 1
        return 0
    end

    return 1
end

function __fish_terraform_workspaces
    terraform workspace list | string replace -r "^[\s\*]*" ""
end

# general options
complete -f -c terraform -n "not __fish_terraform_needs_command" -o version -d "Print version information"
complete -f -c terraform -o help -d "Show help"

### apply/destroy
set -l apply apply destroy

complete -f -c terraform -n __fish_terraform_needs_command -a apply -d "Build or change infrastructure"
complete -f -c terraform -n __fish_terraform_needs_command -a destroy -d "Destroy infrastructure"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o auto-approve -d "Skip interactive approval"
complete -r -c terraform -n "__fish_seen_subcommand_from $apply" -o backup -d "Path to backup the existing state file"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o compact-warnings -d "Show only error summaries"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o lock=false -d "Don't hold a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o input=true -d "Ask for input for variables if not directly set"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o no-color -d "If specified, output won't contain any color"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o parallelism -d "Limit the number of concurrent operations"
complete -r -c terraform -n "__fish_seen_subcommand_from $apply" -o state -d "Path to a Terraform state file"
complete -r -c terraform -n "__fish_seen_subcommand_from $apply" -o state-out -d "Path to write state"

### console
complete -f -c terraform -n __fish_terraform_needs_command -a console -d "Interactive console for Terraform interpolations"
complete -r -c terraform -n "__fish_seen_subcommand_from console" -o state -d "Path to a Terraform state file"
complete -f -c terraform -n "__fish_seen_subcommand_from console" -o var -d "Set a variable in the Terraform configuration"
complete -r -c terraform -n "__fish_seen_subcommand_from console" -o var-file -d "Set variables from a file"

### fmt
complete -f -c terraform -n __fish_terraform_needs_command -a fmt -d "Rewrite config files to canonical format"
complete -f -c terraform -n "__fish_seen_subcommand_from fmt" -o list=false -d "Don't list files whose formatting differs"
complete -f -c terraform -n "__fish_seen_subcommand_from fmt" -o write=false -d "Don't write to source files"
complete -f -c terraform -n "__fish_seen_subcommand_from fmt" -o diff -d "Display diffs of formatting changes"
complete -f -c terraform -n "__fish_seen_subcommand_from fmt" -o check -d "Check if the input is formatted"
complete -f -c terraform -n "__fish_seen_subcommand_from fmt" -o no-color -d "If specified, output won't contain any color"
complete -f -c terraform -n "__fish_seen_subcommand_from fmt" -o recursive -d "Also process files in subdirectories"

### get
complete -f -c terraform -n __fish_terraform_needs_command -a get -d "Download and install modules for the configuration"
complete -f -c terraform -n "__fish_seen_subcommand_from get" -o update -d "Check modules for updates"
complete -f -c terraform -n "__fish_seen_subcommand_from get" -o no-color -d "If specified, output won't contain any color"

### graph
complete -f -c terraform -n __fish_terraform_needs_command -a graph -d "Create a visual graph of Terraform resources"
complete -f -c terraform -n "__fish_seen_subcommand_from graph" -o plan -d "Use specified plan file instead of current directory"
complete -f -c terraform -n "__fish_seen_subcommand_from graph" -o draw-cycles -d "Highlight any cycles in the graph"
complete -f -c terraform -n "__fish_seen_subcommand_from graph" -o type=plan -d "Output plan graph"
complete -f -c terraform -n "__fish_seen_subcommand_from graph" -o type=plan-refresh-only -d "Output plan graph assuming refresh only"
complete -f -c terraform -n "__fish_seen_subcommand_from graph" -o type=plan-destroy -d "Output plan graph assuming destroy"
complete -f -c terraform -n "__fish_seen_subcommand_from graph" -o type=apply -d "Output apply graph"

### import
complete -f -c terraform -n __fish_terraform_needs_command -a import -d "Import existing infrastructure into Terraform"
complete -r -c terraform -n "__fish_seen_subcommand_from import" -o backup -d "Path to backup the existing state file"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o config -d "Path to a directory of configuration files"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o allow-missing-config -d "Allow import without resource block"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o input=false -d "Disable interactive input prompts"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o lock=false -d "Don't hold a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o no-color -d "If specified, output won't contain any color"
complete -r -c terraform -n "__fish_seen_subcommand_from import" -o state -d "Path to a Terraform state file"
complete -r -c terraform -n "__fish_seen_subcommand_from import" -o state-out -d "Path to write state"
complete -f -c terraform -n "__fish_seen_subcommand_from import" -o var -d "Set a variable in the Terraform configuration"
complete -r -c terraform -n "__fish_seen_subcommand_from import" -o var-file -d "Set variables from a file"

### init
complete -f -c terraform -n __fish_terraform_needs_command -a init -d "Initialize a new or existing Terraform configuration"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o backend=false -d "Disable backend initialization"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o cloud=false -d "Disable backend initialization"
complete -r -c terraform -n "__fish_seen_subcommand_from init" -o backend-config -d "Backend configuration"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o force-copy -d "Suppress prompts about copying state data"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o from-module -d "Copy the module into target directory before init"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o get=false -d "Disable downloading modules for this configuration"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o input=false -d "Disable interactive prompts"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o lock=false -d "Don't hold state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o no-color -d "If specified, output won't contain any color"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o plugin-dir -d "Directory containing plugin binaries"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o reconfigure -d "Ignore any saved configuration"
complete -r -c terraform -n "__fish_seen_subcommand_from init" -o migrate-state -d "Reconfigure backend, migrating existing state"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o upgrade -d "Install latest dependencies, ignoring lockfile"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o lockfile=readonly -d "Set dependency lockfile mode to readonly"
complete -f -c terraform -n "__fish_seen_subcommand_from init" -o ignore-remote-version -d "Ignore local and remote backend compatibility check"

### login
complete -f -c terraform -n __fish_terraform_needs_command -a login -d "Retrieves auth token for the given hostname"
complete -f -c terraform -n "__fish_seen_subcommand_from login" -a "(__fish_print_hostnames)"

### logout
complete -f -c terraform -n __fish_terraform_needs_command -a logout -d "Removes auth token for the given hostname"
complete -f -c terraform -n "__fish_seen_subcommand_from logout" -a "(__fish_print_hostnames)"

### output
complete -f -c terraform -n __fish_terraform_needs_command -a output -d "Read an output from a state file"
complete -r -c terraform -n "__fish_seen_subcommand_from output" -o state -d "Path to the state file to read"
complete -f -c terraform -n "__fish_seen_subcommand_from output" -o no-color -d "If specified, output won't contain any color"
complete -f -c terraform -n "__fish_seen_subcommand_from output" -o json -d "Print output in JSON format"
complete -f -c terraform -n "__fish_seen_subcommand_from output" -o raw -d "Print raw strings directly"

### plan
complete -f -c terraform -n __fish_terraform_needs_command -a plan -d "Generate and show an execution plan"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o compact-warnings -d "Show only error summaries"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o detailed-exitcode -d "Return detailed exit codes"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o input=true -d "Ask for input for variables if not directly set"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o lock=false -d "Don't hold a state lock during the operation"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o no-color -d "If specified, output won't contain any color"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o out -d "Write a plan file to the given path"
complete -f -c terraform -n "__fish_seen_subcommand_from plan" -o parallelism -d "Limit the number of concurrent operations"
complete -r -c terraform -n "__fish_seen_subcommand_from plan" -o state -d "Path to a Terraform state file"

### plan customization options are reusable across apply, destroy, and plan
set -l plan apply destroy plan

complete -f -c terraform -n "__fish_seen_subcommand_from $plan" -o destroy -d "Select \"destroy\" planning mode"
complete -f -c terraform -n "__fish_seen_subcommand_from $plan" -o refresh-only -d "Select \"refresh only\" planning mode"
complete -f -c terraform -n "__fish_seen_subcommand_from $plan" -o refresh=false -d "Skip checking for external changes"
complete -f -c terraform -n "__fish_seen_subcommand_from $plan" -o replace -d "Force replacement of resource using its address"
complete -f -c terraform -n "__fish_seen_subcommand_from $plan" -o target -d "Resource to target"
complete -f -c terraform -n "__fish_seen_subcommand_from $plan" -o var -d "Set a variable in the Terraform configuration"
complete -r -c terraform -n "__fish_seen_subcommand_from $plan" -o var-file -d "Set variables from a file"

### providers
complete -f -c terraform -n __fish_terraform_needs_command -a providers -d "Print tree of modules with their provider requirements"
complete -f -c terraform -n "__fish_seen_subcommand_from providers" -a "lock mirror schema"

### refresh
complete -f -c terraform -n __fish_terraform_needs_command -a refresh -d "Update local state file against real resources"
complete -f -c terraform -n "__fish_seen_subcommand_from $apply" -o compact-warnings -d "Show only error summaries"
complete -r -c terraform -n "__fish_seen_subcommand_from refresh" -o backup -d "Path to backup the existing state file"
complete -f -c terraform -n "__fish_seen_subcommand_from refresh" -o input=true -d "Ask for input for variables if not directly set"
complete -f -c terraform -n "__fish_seen_subcommand_from refresh" -o lock=false -d "Don't hold a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from refresh" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from refresh" -o no-color -d "If specified, output won't contain any color"
complete -r -c terraform -n "__fish_seen_subcommand_from refresh" -o state -d "Path to a Terraform state file"
complete -r -c terraform -n "__fish_seen_subcommand_from refresh" -o state-out -d "Path to write state"
complete -f -c terraform -n "__fish_seen_subcommand_from refresh" -o target -d "Resource to target"
complete -f -c terraform -n "__fish_seen_subcommand_from refresh" -o var -d "Set a variable in the Terraform configuration"
complete -r -c terraform -n "__fish_seen_subcommand_from refresh" -o var-file -d "Set variables from a file"

### show
complete -f -c terraform -n __fish_terraform_needs_command -a show -d "Inspect Terraform state or plan"
complete -f -c terraform -n "__fish_seen_subcommand_from show" -o no-color -d "If specified, output won't contain any color"
complete -f -c terraform -n "__fish_seen_subcommand_from validate" -o json -d "Produce output in JSON format"

### state
complete -r -c terraform -n __fish_terraform_needs_command -a state -d "Advanced state management"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a list -d "List resources in state"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a mv -d "Move an item in the state"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a pull -d "Pull current state and output to stdout"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a push -d "Update remote state from local state"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a replace-provider -d "Replace provider in the state"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a rm -d "Remove instance from the state"
complete -f -c terraform -n "__fish_seen_subcommand_from state" -a show -d "Show a resource in the state"

### taint
complete -f -c terraform -n __fish_terraform_needs_command -a taint -d "Manually mark a resource for recreation"
complete -f -c terraform -n "__fish_seen_subcommand_from taint" -o allow-missing -d "Succeed even if resource is missing"
complete -r -c terraform -n "__fish_seen_subcommand_from taint" -o backup -d "Path to backup the existing state file"
complete -f -c terraform -n "__fish_seen_subcommand_from taint" -o lock=false -d "Don't hold a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from taint" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from taint" -o ignore-remote-version -d "Ignore local and remote backend compatibility check"
complete -r -c terraform -n "__fish_seen_subcommand_from taint" -o state -d "Path to a Terraform state file"
complete -r -c terraform -n "__fish_seen_subcommand_from taint" -o state-out -d "Path to write state"

### test
complete -f -c terraform -n __fish_terraform_needs_command -a test -d "Runs automated test of shared modules"
complete -f -c terraform -n "__fish_seen_subcommand_from test" -o compact-warnings -d "Show only error summaries"
complete -f -c terraform -n "__fish_seen_subcommand_from test" -o junit-xml -d "Also write test results to provided JUnit XML file"
complete -f -c terraform -n "__fish_seen_subcommand_from test" -o no-color -d "If specified, output won't contain any color"

### untaint
complete -f -c terraform -n __fish_terraform_needs_command -a untaint -d "Manually unmark a resource as tainted"
complete -f -c terraform -n "__fish_seen_subcommand_from untaint" -o allow-missing -d "Succeed even if resource is missing"
complete -r -c terraform -n "__fish_seen_subcommand_from untaint" -o backup -d "Path to backup the existing state file"
complete -f -c terraform -n "__fish_seen_subcommand_from untaint" -o lock=false -d "Don't hold a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from untaint" -o lock-timeout -d "Duration to retry a state lock"
complete -f -c terraform -n "__fish_seen_subcommand_from untaint" -o ignore-remote-version -d "Ignore local and remote backend compatibility check"
complete -r -c terraform -n "__fish_seen_subcommand_from untaint" -o state -d "Path to a Terraform state file"
complete -r -c terraform -n "__fish_seen_subcommand_from untaint" -o state-out -d "Path to write state"

### validate
complete -f -c terraform -n __fish_terraform_needs_command -a validate -d "Validate the Terraform files"
complete -f -c terraform -n "__fish_seen_subcommand_from validate" -o json -d "Produce output in JSON format"
complete -f -c terraform -n "__fish_seen_subcommand_from validate" -o no-color -d "If specified, output won't contain any color"

### version
complete -f -c terraform -n __fish_terraform_needs_command -a version -d "Print the Terraform version"

### workspace
set -l workspace_commands list select new delete

complete -f -c terraform -n __fish_terraform_needs_command -a workspace -d "Workspace management"
complete -f -c terraform -n "__fish_seen_subcommand_from workspace && not __fish_seen_subcommand_from $workspace_commands" -a list -d "List workspaces"
complete -f -c terraform -n "__fish_seen_subcommand_from workspace && not __fish_seen_subcommand_from $workspace_commands" -a select -d "Select an workspace"
complete -f -c terraform -n "__fish_seen_subcommand_from workspace && not __fish_seen_subcommand_from $workspace_commands" -a new -d "Create a new workspace"
complete -f -c terraform -n "__fish_seen_subcommand_from workspace && not __fish_seen_subcommand_from $workspace_commands" -a delete -d "Delete an existing workspace"
complete -f -c terraform -n "__fish_seen_subcommand_from workspace && __fish_seen_subcommand_from select delete" -a "(__fish_terraform_workspaces)"
