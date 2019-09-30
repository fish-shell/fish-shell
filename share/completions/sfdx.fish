# Tab completion for sfdx (https://developer.salesforce.com/tools/sfdxcli).

function __fish_sfdx_needs_command
    set cmd (commandline -opc)
    if [ (count $cmd) -eq 1 ]
        return 0
    end
    return 1
end

function __fish_sfdx_using_command
    set cmd (commandline -opc)
    if [ (count $cmd) -gt 1 ]
        if [ $argv[1] = $cmd[2] ]
            return 0
        end
    end
    return 1
end

set -l sfdx_looking -c sfdx -n '__fish_sfdx_needs_command'

#
# commands
#
complete $sfdx_looking -xa commands  -d 'list all the commands'

#
# force
#
complete $sfdx_looking -xa force            -d 'tools for the Salesforce developer'
complete $sfdx_looking -xa force:alias      -d 'manage username aliases'
complete $sfdx_looking -xa force:alias:list -d 'list username aliases for the Salesforce CLI'
complete $sfdx_looking -xa force:alias:set  -d 'set username aliases for the Salesforce CLI'

complete $sfdx_looking -xa force:apex                -d 'work with Apex code'
complete $sfdx_looking -xa force:apex:execute        -d 'execute anonymous Apex code'
complete $sfdx_looking -xa force:apex:class          -d 'create an Apex class'
complete $sfdx_looking -xa force:apex:class:create   -d 'create an Apex class'
complete $sfdx_looking -xa force:apex:log            -d 'work with Apex logs'
complete $sfdx_looking -xa force:apex:log:get        -d 'fetch the last debug log'
complete $sfdx_looking -xa force:apex:log:list       -d 'list debug logs'
complete $sfdx_looking -xa force:apex:log:tail       -d 'start debug logging and display logs'
complete $sfdx_looking -xa force:apex:test           -d 'work with Apex tests'
complete $sfdx_looking -xa force:apex:test:report    -d 'display test results'
complete $sfdx_looking -xa force:apex:test:run       -d 'invoke Apex tests'
complete $sfdx_looking -xa force:apex:trigger        -d 'create an Apex trigger'
complete $sfdx_looking -xa force:apex:trigger:create -d 'create an Apex trigger'

complete $sfdx_looking -xa force:auth               -d 'authorize an org for use with the Salesforce'
complete $sfdx_looking -xa force:auth:list          -d 'list auth connection information'
complete $sfdx_looking -xa force:auth:logout        -d 'log out from authorized orgs'
complete $sfdx_looking -xa force:auth:jwt           -d 'authorize an org using JWT'
complete $sfdx_looking -xa force:auth:jwt:grant     -d 'authorize an org using the JWT flow'
complete $sfdx_looking -xa force:auth:sfdxurl       -d 'authorize an org using sfdxurl'
complete $sfdx_looking -xa force:auth:sfdxurl:store -d 'authorize an org using an SFDX auth URL'
complete $sfdx_looking -xa force:auth:web           -d 'authorize an org using a web browser'
complete $sfdx_looking -xa force:auth:web:login     -d 'authorize an org using the web login flow'

complete $sfdx_looking -xa force:config      -d 'configure the Salesforce CLI'
complete $sfdx_looking -xa force:config:get  -d 'get config var values for given names'
complete $sfdx_looking -xa force:config:list -d 'list config vars for the Salesforce CLI'
complete $sfdx_looking -xa force:config:set  -d 'set config vars for the Salesforce CLI'

complete $sfdx_looking -xa force:data               -d 'manipulate records in your org'
complete $sfdx_looking -xa force:data:bulk          -d 'manipulate records using the bulk API'
complete $sfdx_looking -xa force:data:bulk:delete   -d 'bulk delete records from a csv file'
complete $sfdx_looking -xa force:data:bulk:status   -d 'view the status of a bulk data load job or batch'
complete $sfdx_looking -xa force:data:bulk:upsert   -d 'bulk upsert records from a CSV file'
complete $sfdx_looking -xa force:data:record        -d 'manipulate records using the enterprise API'
complete $sfdx_looking -xa force:data:record:create -d 'create a record'
complete $sfdx_looking -xa force:data:record:delete -d 'delete a record'
complete $sfdx_looking -xa force:data:record:get    -d 'view a record'
complete $sfdx_looking -xa force:data:record:update -d 'update a record'
complete $sfdx_looking -xa force:data:soql          -d 'fetch records using SOQL'
complete $sfdx_looking -xa force:data:soql:query    -d 'execute a SOQL query'
complete $sfdx_looking -xa force:data:tree          -d 'manipulate records using the tree API'
complete $sfdx_looking -xa force:data:tree:export   -d 'export data from an org into sObject tree format for force:data:tree:import consumption'
complete $sfdx_looking -xa force:data:tree:import   -d 'import data into an org using SObject Tree Save API'

complete $sfdx_looking -xa force:doc                  -d 'display help for force commands'
complete $sfdx_looking -xa force:doc:commands         -d 'display help for force commands'
complete $sfdx_looking -xa force:doc:commands:display -d 'display help for force commands'
complete $sfdx_looking -xa force:doc:commands:list    -d 'list the force commands'

complete $sfdx_looking -xa force:lightning                  -d 'create Aura components and Lightning web'
complete $sfdx_looking -xa force:lightning:lint             -d 'analyze (lint) Aura component code'
complete $sfdx_looking -xa force:lightning:app              -d 'create a Lightning app'
complete $sfdx_looking -xa force:lightning:app:create       -d 'create a Lightning app'
complete $sfdx_looking -xa force:lightning:component        -d 'create a bundle for an Aura component or a Lightning web component'
complete $sfdx_looking -xa force:lightning:component:create -d 'create a bundle for an Aura component or a Lightning web component'
complete $sfdx_looking -xa force:lightning:event            -d 'create a Lightning event'
complete $sfdx_looking -xa force:lightning:event:create     -d 'create a Lightning event'
complete $sfdx_looking -xa force:lightning:interface        -d 'create a Lightning interface'
complete $sfdx_looking -xa force:lightning:interface:create -d 'create a Lightning interface'
complete $sfdx_looking -xa force:lightning:test             -d 'test Aura components'
complete $sfdx_looking -xa force:lightning:test:create      -d 'create a Lightning test'
complete $sfdx_looking -xa force:lightning:test:install     -d 'install Lightning Testing Service unmanaged package in your org'
complete $sfdx_looking -xa force:lightning:test:run         -d 'invoke Aura component tests'

complete $sfdx_looking -xa force:limits             -d 'view your org’s limits'
complete $sfdx_looking -xa force:limits:api         -d 'view your org’s API limits'
complete $sfdx_looking -xa force:limits:api:display -d 'display current org’s limits'

complete $sfdx_looking -xa force:mdapi                  -d 'retrieve and deploy metadata using Metadata'
complete $sfdx_looking -xa force:mdapi:convert          -d 'convert metadata from the Metadata API format into the source format'
complete $sfdx_looking -xa force:mdapi:deploy           -d 'deploy metadata using Metadata API'
complete $sfdx_looking -xa force:mdapi:describemetadata -d 'display the metadata types enabled for your org'
complete $sfdx_looking -xa force:mdapi:listmetadata     -d 'display properties of metadata components of a specified type'
complete $sfdx_looking -xa force:mdapi:retrieve         -d 'retrieve metadata using Metadata API'
complete $sfdx_looking -xa force:mdapi:deploy           -d 'deploy metadata using Metadata API'
complete $sfdx_looking -xa force:mdapi:deploy:cancel    -d 'cancel a metadata deployment'
complete $sfdx_looking -xa force:mdapi:deploy:report    -d 'check the status of a metadata deployment'
complete $sfdx_looking -xa force:mdapi:retrieve         -d 'retrieve metadata using Metadata API'
complete $sfdx_looking -xa force:mdapi:retrieve:report  -d 'check the status of a metadata retrieval'

complete $sfdx_looking -xa force:org                 -d 'manage your orgs'
complete $sfdx_looking -xa force:org:clone           -d 'clone a sandbox org'
complete $sfdx_looking -xa force:org:create          -d 'create a scratch or sandbox org'
complete $sfdx_looking -xa force:org:delete          -d 'mark a scratch org for deletion'
complete $sfdx_looking -xa force:org:display         -d 'get org description'
complete $sfdx_looking -xa force:org:list            -d 'list all orgs you’ve created or authenticated to'
complete $sfdx_looking -xa force:org:open            -d 'open an org in your browser'
complete $sfdx_looking -xa force:org:status          -d 'report sandbox org creation status and headlessly authenticate to org'
complete $sfdx_looking -xa force:org:shape           -d 'manage org shape'
complete $sfdx_looking -xa force:org:shape:create    -d 'create a snapshot of org edition, features, and licenses'
complete $sfdx_looking -xa force:org:shape:delete    -d 'delete all org shapes for a target org'
complete $sfdx_looking -xa force:org:shape:list      -d 'list all org shapes you’ve created'
complete $sfdx_looking -xa force:org:snapshot        -d 'snapshot a scratch org'
complete $sfdx_looking -xa force:org:snapshot:create -d 'snapshot a scratch org'
complete $sfdx_looking -xa force:org:snapshot:delete -d 'delete a scratch org snapshot'
complete $sfdx_looking -xa force:org:snapshot:get    -d 'get details about a scratch org snapshot'
complete $sfdx_looking -xa force:org:snapshot:list   -d 'list scratch org snapshots'

complete $sfdx_looking -xa force:package                   -d 'develop and install packages'
complete $sfdx_looking -xa force:package:create            -d 'create a package'
complete $sfdx_looking -xa force:package:install           -d 'install packages'
complete $sfdx_looking -xa force:package:list              -d 'list all packages in the Dev Hub org'
complete $sfdx_looking -xa force:package:uninstall         -d 'uninstall packages'
complete $sfdx_looking -xa force:package:update            -d 'update package details'
complete $sfdx_looking -xa force:package:hammertest        -d 'run ISV Hammer tests'
complete $sfdx_looking -xa force:package:hammertest:list   -d 'list the statuses of running and completed ISV Hammer tests'
complete $sfdx_looking -xa force:package:hammertest:report -d 'display the status or results of a ISV Hammer test'
complete $sfdx_looking -xa force:package:hammertest:run    -d 'run ISV Hammer'
complete $sfdx_looking -xa force:package:install           -d 'install packages'
complete $sfdx_looking -xa force:package:install:report    -d 'retrieve the status of a package installation request'
complete $sfdx_looking -xa force:package:installed         -d 'list installed packages'
complete $sfdx_looking -xa force:package:installed:list    -d 'list the org’s installed packages'
complete $sfdx_looking -xa force:package:uninstall         -d 'uninstall packages'
complete $sfdx_looking -xa force:package:uninstall:report  -d 'retrieve status of package uninstall request'
complete $sfdx_looking -xa force:package:version           -d 'develop package versions'

complete $sfdx_looking -xa force:package1                    -d 'develop first-generation managed and'
complete $sfdx_looking -xa force:package1:version            -d 'develop package versions'
complete $sfdx_looking -xa force:package1:version:create     -d 'report on created package versions'
complete $sfdx_looking -xa force:package1:version:display    -d 'display details about a first-generation package version'
complete $sfdx_looking -xa force:package1:version:list       -d 'list package versions for the specified first-generation package or for the org'
complete $sfdx_looking -xa force:package1:version:create     -d 'report on created package versions'
complete $sfdx_looking -xa force:package1:version:create:get -d 'retrieve the status of a package version creation request'

complete $sfdx_looking -xa force:project         -d 'set up a Salesforce DX project'
complete $sfdx_looking -xa force:project:create  -d 'create a Salesforce DX project'
complete $sfdx_looking -xa force:project:upgrade -d 'update project config files to the latest format'

complete $sfdx_looking -xa force:schema         -d 'view standard and custom objects'
complete $sfdx_looking -xa force:schema:sobject -d 'view standard and custom objects'

complete $sfdx_looking -xa force:source               -d 'sync your project with your orgs'
complete $sfdx_looking -xa force:source:convert       -d 'convert source into Metadata API format'
complete $sfdx_looking -xa force:source:delete        -d 'delete source from your project and from a non-source-tracked org'
complete $sfdx_looking -xa force:source:deploy        -d 'deploy source to an org'
complete $sfdx_looking -xa force:source:open          -d 'edit a Lightning Page with Lightning App Builder'
complete $sfdx_looking -xa force:source:pull          -d 'pull source from the scratch org to the project'
complete $sfdx_looking -xa force:source:push          -d 'push source to a scratch org from the project'
complete $sfdx_looking -xa force:source:retrieve      -d 'retrieve source from an org'
complete $sfdx_looking -xa force:source:status        -d 'list local changes and/or changes in a scratch org'
complete $sfdx_looking -xa force:source:deploy        -d 'deploy source to an org'
complete $sfdx_looking -xa force:source:deploy:cancel -d 'cancel a source deployment'
complete $sfdx_looking -xa force:source:deploy:report -d 'check the status of a metadata deployment'

complete $sfdx_looking -xa force:user                   -d 'perform user-related admin tasks'
complete $sfdx_looking -xa force:user:create            -d 'create a user for a scratch org'
complete $sfdx_looking -xa force:user:display           -d 'displays information about a user of a scratch org'
complete $sfdx_looking -xa force:user:list              -d 'lists all users of a scratch org'
complete $sfdx_looking -xa force:user:password          -d 'perform password-related admin tasks'
complete $sfdx_looking -xa force:user:password:generate -d 'generate a password for scratch org users'
complete $sfdx_looking -xa force:user:permset           -d 'perform permset-related admin tasks'
complete $sfdx_looking -xa force:user:permset:assign    -d 'assign a permission set to one or more users of an org'

complete $sfdx_looking -xa force:visualforce                  -d 'create and edit Visualforce files'
complete $sfdx_looking -xa force:visualforce:component        -d 'create a Visualforce component'
complete $sfdx_looking -xa force:visualforce:component:create -d 'create a Visualforce component'
complete $sfdx_looking -xa force:visualforce:page             -d 'create a Visualforce page'
complete $sfdx_looking -xa force:visualforce:page:create      -d 'create a Visualforce page'

#
# help
#
complete $sfdx_looking -xa help -d 'display help for sfdx'

#
# plugins
#
complete $sfdx_looking -xa plugins              -d 'add/remove/create CLI plug-ins'
complete $sfdx_looking -xa plugins:install      -d 'installs a plugin into the CLI'
complete $sfdx_looking -xa plugins:link         -d 'links a plugin into the CLI for development'
complete $sfdx_looking -xa plugins:uninstall    -d 'removes a plugin from the CLI'
complete $sfdx_looking -xa plugins:update       -d 'update installed plugins'
complete $sfdx_looking -xa plugins:generate     -d 'create a new sfdx-cli plugin'
complete $sfdx_looking -xa plugins:trust        -d 'pack an npm package and produce a tgz file along with a corresponding digital signature'
complete $sfdx_looking -xa plugins:trust:sign   -d 'pack an npm package and produce a tgz file along with a corresponding digital signature'
complete $sfdx_looking -xa plugins:trust:verify -d 'For an npm validate the associated digital signature if it exits.'

#
# update
#
complete $sfdx_looking -xa update -d 'update the sfdx CLI'

#
# which
#
complete $sfdx_looking -xa which -d 'show which plugin a command is in'
