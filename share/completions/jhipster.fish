# JHipster 4.9.0

# Check if command already given
function __fish_prog_needs_command
    set cmd (commandline -opc)
    echo $cmd
    if [ (count $cmd) -eq 1 -a $cmd[1] = 'jhipster' ]
        return 0
    end
    return 1
end

# Options
complete -f -c jhipster -n '__fish_prog_needs_command' -s d -d 'Enable debugger'
complete -f -c jhipster -n '__fish_prog_needs_command' -s h -d 'Output usage information'
complete -f -c jhipster -n '__fish_prog_needs_command' -s V -d 'Output version number'

# Commands
complete -f -c jhipster -n '__fish_prog_needs_command' -a app -d 'Create a new JHipster application based on the selected options.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a aws -d 'Deploy the current application to Amazon Web Services.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a ci-cd -d 'Create pipeline scripts for popular Continuous Integration/Continuous Deployment tools.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a client -d 'Create a new JHipster client-side application based on the selected options..'
complete -f -c jhipster -n '__fish_prog_needs_command' -a cloudfoundry -d 'Generate a `deploy/cloudfoundry` folder with a specific manifest.yml to deploy to Cloud Foundry.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a docker-compose -d 'Create all required Docker deployment configuration for the selected applications.'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a entity -d 'Create a new JHipster entity: JPA entity, Spring server-side components and Angular client-side components.'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a export-jdl -d 'Create a JDL file from the existing entities.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a heroku -d 'Deploy the current application to Heroku.'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a import-jdl -d 'Create entities from the JDL file passed in argument.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a info -d 'Display information about your current project and system.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a kubernetes -d 'Deploy the current application to Kubernetes.'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a languages -d 'Select languages from a list of available languages. The i18n files will be copied to the /webapp/i18n folder.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a openshift -d 'Deploy the current application to OpenShift.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a rancher-compose -d 'Deploy the current application to Rancher.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a server -d 'Create a new JHipster server-side application.'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a service -d 'Create a new Spring service bean.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a upgrade -d 'Upgrade the JHipster version, and upgrade the generated application.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a completion -d 'Print command completion script.'
