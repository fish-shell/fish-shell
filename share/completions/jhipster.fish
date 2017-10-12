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

# Commands
complete -f -c jhipster -n '__fish_prog_needs_command' -a app --description 'Create a new JHipster application based on the selected options'
complete -f -c jhipster -n '__fish_prog_needs_command' -a aws --description 'Deploy the current application to Amazon Web Services'
complete -f -c jhipster -n '__fish_prog_needs_command' -a ci-cd --description 'Create pipeline scripts for popular Continuous Integration/Continuous Deployment tools'
complete -f -c jhipster -n '__fish_prog_needs_command' -a client --description 'Create a new JHipster client-side application based on the selected options.'
complete -f -c jhipster -n '__fish_prog_needs_command' -a cloudfoundry --description 'Generate a `deploy/cloudfoundry` folder with a specific manifest.yml to deploy to Cloud Foundry'
complete -f -c jhipster -n '__fish_prog_needs_command' -a docker-compose --description 'Create all required Docker deployment configuration for the selected applications'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a entity --description 'Create a new JHipster entity: JPA entity, Spring server-side components and Angular client-side components'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a export-jdl --description 'Create a JDL file from the existing entities'
complete -f -c jhipster -n '__fish_prog_needs_command' -a heroku --description 'Deploy the current application to Heroku'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a import-jdl --description 'Create entities from the JDL file passed in argument'
complete -f -c jhipster -n '__fish_prog_needs_command' -a info --description 'Display information about your current project and system'
complete -f -c jhipster -n '__fish_prog_needs_command' -a kubernetes --description 'Deploy the current application to Kubernetes'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a languages --description 'Select languages from a list of available languages. The i18n files will be copied to the /webapp/i18n folder'
complete -f -c jhipster -n '__fish_prog_needs_command' -a openshift --description 'Deploy the current application to OpenShift'
complete -f -c jhipster -n '__fish_prog_needs_command' -a rancher-compose --description 'Deploy the current application to Rancher'
complete -f -c jhipster -n '__fish_prog_needs_command' -a server --description 'Create a new JHipster server-side application'
complete -f -c jhipster -n '__fish_prog_needs_command' -r -a service --description 'Create a new Spring service bean'
complete -f -c jhipster -n '__fish_prog_needs_command' -a upgrade --description 'Upgrade the JHipster version, and upgrade the generated application'
complete -f -c jhipster -n '__fish_prog_needs_command' -a completion --description 'Print command completion script'
