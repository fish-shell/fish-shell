# JHipster 4.9.0

# Check if command already given
function __fish_prog_needs_command
    set -l cmd (commandline -xpc)
    echo $cmd
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

# Options
complete -f -c jhipster -n __fish_prog_needs_command -s d -l debug -d 'Enable debugger'
complete -f -c jhipster -n __fish_prog_needs_command -s h -l help -d 'Output usage information'
complete -f -c jhipster -n __fish_prog_needs_command -s V -l version -d 'Output version number'

# Commands
complete -f -c jhipster -n __fish_prog_needs_command -a app -d 'Create a new JHipster application'
complete -f -c jhipster -n __fish_prog_needs_command -a aws -d 'Deploy the current application to AWS'
complete -f -c jhipster -n __fish_prog_needs_command -a ci-cd -d 'Create pipeline scripts for popular CI tools'
complete -f -c jhipster -n __fish_prog_needs_command -a client -d 'Create a new JHipster client-side application'
complete -f -c jhipster -n __fish_prog_needs_command -a cloudfoundry -d 'Prepare Cloud Foundry deployment'
complete -f -c jhipster -n __fish_prog_needs_command -a docker-compose -d 'Create required Docker deployment configs for applications'
complete -f -c jhipster -n __fish_prog_needs_command -r -a entity -d 'Create a new JHipster entity: JPA, Spring & Angular components'
complete -f -c jhipster -n __fish_prog_needs_command -r -a export-jdl -d 'Create a JDL file from the existing entities'
complete -f -c jhipster -n __fish_prog_needs_command -a heroku -d 'Deploy the current application to Heroku'
complete -f -c jhipster -n __fish_prog_needs_command -r -a import-jdl -d 'Create entities from the JDL file passed in argument'
complete -f -c jhipster -n __fish_prog_needs_command -a info -d 'Display information about your current project and system'
complete -f -c jhipster -n __fish_prog_needs_command -a kubernetes -d 'Deploy the current application to Kubernetes'
complete -f -c jhipster -n __fish_prog_needs_command -r -a languages -d 'Copy i18n files for selected languages to /webapp/i18n'
complete -f -c jhipster -n __fish_prog_needs_command -a openshift -d 'Deploy the current application to OpenShift'
complete -f -c jhipster -n __fish_prog_needs_command -a rancher-compose -d 'Deploy the current application to Rancher'
complete -f -c jhipster -n __fish_prog_needs_command -a server -d 'Create a new JHipster server-side application'
complete -f -c jhipster -n __fish_prog_needs_command -r -a service -d 'Create a new Spring service bean'
complete -f -c jhipster -n __fish_prog_needs_command -a upgrade -d 'Upgrade the JHipster and the generated application'
complete -f -c jhipster -n __fish_prog_needs_command -a completion -d 'Print command completion script'
