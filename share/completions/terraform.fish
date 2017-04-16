function __fish_terraform_needs_command
    set cmd (commandline -opc)
    if [ (count $cmd) -eq 1 ]
        return 0
    end

    for arg in $cmd[2..-1]
        switch $arg
            case '--*'
                # ignore global flags
            case '*'
                return 1
        end
    end
    return 0
end

# general options
complete -f -c terraform -n '__fish_terraform_needs_command' -l version -d 'Print version information'
complete -f -c terraform -n '__fish_terraform_needs_command' -l help -d 'Show help'

### apply
complete -f -c terraform -n '__fish_terraform_needs_command' -a apply -d 'Builds or changes infrastructure'

### console
complete -f -c terraform -n '__fish_terraform_needs_command' -a console -d 'Interactive console for Terraform interpolations'

### destroy
complete -f -c terraform -n '__fish_terraform_needs_command' -a destroy -d 'Destroy Terraform-managed infrastructure'

### env
complete -f -c terraform -n '__fish_terraform_needs_command' -a env -d 'Environment management'

### fmt
complete -f -c terraform -n '__fish_terraform_needs_command' -a fmt -d 'Rewrites config files to canonical format'

### get
complete -f -c terraform -n '__fish_terraform_needs_command' -a get -d 'Download and install modules for the configuration'

### graph
complete -f -c terraform -n '__fish_terraform_needs_command' -a graph -d 'Create a visual graph of Terraform resources'

### import
complete -f -c terraform -n '__fish_terraform_needs_command' -a import -d 'Import existing infrastructure into Terraform'

### init
complete -f -c terraform -n '__fish_terraform_needs_command' -a init -d 'Initialize a new or existing Terraform configuration'

### output
complete -f -c terraform -n '__fish_terraform_needs_command' -a output -d 'Read an output from a state file'

### plan
complete -f -c terraform -n '__fish_terraform_needs_command' -a plan -d 'Generate and show an execution plan'

### push
complete -f -c terraform -n '__fish_terraform_needs_command' -a push -d 'Upload this Terraform module to Atlas to run'

### refresh
complete -f -c terraform -n '__fish_terraform_needs_command' -a refresh -d 'Update local state file against real resources'

### show
complete -f -c terraform -n '__fish_terraform_needs_command' -a show -d 'Inspect Terraform state or plan'

### taint
complete -f -c terraform -n '__fish_terraform_needs_command' -a taint -d 'Manually mark a resource for recreation'

### untaint
complete -f -c terraform -n '__fish_terraform_needs_command' -a untaint -d 'Manually unmark a resource as tainted'

### validate
complete -f -c terraform -n '__fish_terraform_needs_command' -a validate -d 'Validates the Terraform files'

### version
complete -f -c terraform -n '__fish_terraform_needs_command' -a version -d 'Prints the Terraform version'
