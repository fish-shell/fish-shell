# Tab completion for sfdx (https://developer.salesforce.com/tools/sfdxcli).

function __fish_sfdx_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

function __fish_sfdx_find_packagexml
    # To find manifest (in other words package.xml)
    printf '%s\n' (find . -type f -regex ".*/package.xml" | string sub -s 3)
end

set -l sfdx_looking -c sfdx -n __fish_use_subcommand

set -l sfdx_loglevels 'trace debug info warn error fatal TRACE DEBUG INFO WARN ERROR FATAL'

#
# commands
#
complete $sfdx_looking -xa commands -d 'list all the commands'
complete -c sfdx -n '__fish_sfdx_using_command commands' -s h -l help -d 'show CLI help'
complete -c sfdx -n '__fish_sfdx_using_command commands' -s j -l json -d 'output in json format'
complete -c sfdx -n '__fish_sfdx_using_command commands' -l hidden -d 'also show hidden commands'

#
# force
#
complete $sfdx_looking -xa force -d 'tools for the Salesforce developer'
complete -c sfdx -n '__fish_sfdx_using_command force' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:alias -d 'manage username aliases'

complete $sfdx_looking -xa force:alias:list -d 'list username aliases for the Salesforce CLI'
complete -c sfdx -n '__fish_sfdx_using_command force:alias:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:alias:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:alias:set -d 'set username aliases for the Salesforce CLI'
complete -c sfdx -n '__fish_sfdx_using_command force:alias:set' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:alias:set' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:apex -d 'work with Apex code'

complete $sfdx_looking -xa force:apex:execute -d 'execute anonymous Apex code'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:execute' -s f -l apexcodefile -d 'path to a local file containing Apex code'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:execute' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:execute' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:execute' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:execute' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:apex:class -d 'create an Apex class'

complete $sfdx_looking -xa force:apex:class:create -d 'create an Apex class'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:class:create' -s a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:class:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:class:create' -s n -l classname -d '(required) name of the generated Apex class'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:class:create' -s t -l template -d '[default: DefaultApexClass] template to use for file creation' -xa 'DefaultApexClass ApexException ApexUnitTest InboundEmailService'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:class:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:class:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:apex:log -d 'work with Apex logs'

complete $sfdx_looking -xa force:apex:log:get -d 'fetch the last debug log'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -s c -l color -d 'colorize noteworthy log lines'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -s i -l logid -d 'ID of the log to display'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -s n -l number -d 'number of most recent logs to display'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:get' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:apex:log:list -d 'list debug logs'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:list' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:apex:log:tail -d 'start debug logging and display logs'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:tail' -s c -l color -d 'colorize noteworthy log lines'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:tail' -s d -l debuglevel -d 'debug level for trace flag'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:tail' -s s -l skiptraceflag -d 'skip trace flag setup'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:tail' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:tail' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:log:tail' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:apex:test -d 'work with Apex tests'

complete $sfdx_looking -xa force:apex:test:report -d 'display test results'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -s c -l codecoverage -d 'retrieve code coverage results'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -s d -l outputdir -d 'directory to store test run files'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -s i -l testrunid -d '(required) ID of test run'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -s r -l resultformat -d '[default: human] result format emitted to stdout; --json flag overrides this parameter' -xa 'human tap junit json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -s w -l wait -d '[default: 6 minutes] the streaming client socket timeout (in minutes)'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:report' -l verbose -d 'display Apex test processing details'

complete $sfdx_looking -xa force:apex:test:run -d 'invoke Apex tests'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s c -l codecoverage -d 'retrieve code coverage results'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s d -l outputdir -d 'directory to store test run files'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s l -l testlevel -d 'testlevel enum value' -xa 'RunLocalTests RunAllTestsInOrg RunSpecifiedTests'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s n -l classnames -d 'comma-separated list of Apex test class names to run'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s r -l resultformat -d 'result format emitted to stdout; --json flag overrides this parameter' -xa 'human tap junit json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s s -l suitenames -d 'comma-separated list of Apex test suite names to run'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s t -l tests -d 'comma-separated list of Apex test class names or IDs and, if applicable, test methods to run'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s w -l wait -d 'the streaming client socket timeout (in minutes)'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -s y -l synchronous -d 'run tests from a single class synchronously'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:apex:test:run' -l verbose -d 'display Apex test processing details'

complete $sfdx_looking -xa force:apex:trigger -d 'create an Apex trigger'

complete $sfdx_looking -xa force:apex:trigger:create -d 'create an Apex trigger'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -s a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -s e -l triggerevents -d 'events that fire trigger'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -s n -l triggername -d '(required) name of the generated Apex trigger'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -s s -l sobject -d '[default: SOBJECT] sObject to create a trigger on'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -s t -l template -d '[default: ApexTrigger] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:apex:trigger:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:auth -d 'authorize an org for use with the Salesforce'

complete $sfdx_looking -xa force:auth:list -d 'list auth connection information'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:auth:logout -d 'log out from authorized orgs'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:logout' -s a -l all -d 'include all authenticated orgs'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:logout' -s p -l noprompt -d 'do not prompt for confirmation'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:logout' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:logout' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:logout' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:logout' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:auth:jwt -d 'authorize an org using JWT'

complete $sfdx_looking -xa force:auth:jwt:grant -d 'authorize an org using the JWT flow'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s a -l setalias -d 'set an alias for the authenticated org'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s d -l setdefaultdevhubusername -d 'set the authenticated org as the default dev hub org for scratch org creation'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s f -l jwtkeyfile -d '(required) path to a file containing the private key'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s i -l clientid -d '(required) OAuth client ID (sometimes called the consumer key)'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s r -l instanceurl -d 'the login URL of the instance the org lives on'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s s -l setdefaultusername -d 'set the authenticated org as the default username that all commands run against'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -s u -l username -d '(required) authentication username'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:jwt:grant' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:auth:sfdxurl -d 'authorize an org using sfdxurl'

complete $sfdx_looking -xa force:auth:sfdxurl:store -d 'authorize an org using an SFDX auth URL'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:sfdxurl:store' -s a -l setalias -d 'set an alias for the authenticated org'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:sfdxurl:store' -s d -l setdefaultdevhubusername -d 'set the authenticated org as the default dev hub org for scratch org creation'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:sfdxurl:store' -s f -l sfdxurlfile -d '(required) path to a file containing the sfdx url'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:sfdxurl:store' -s s -l setdefaultusername -d 'set the authenticated org as the default username that all commands run against'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:sfdxurl:store' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:sfdxurl:store' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:auth:web -d 'authorize an org using a web browser'

complete $sfdx_looking -xa force:auth:web:login -d 'authorize an org using the web login flow'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -s a -l setalias -d 'set an alias for the authenticated org'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -s d -l setdefaultdevhubusername -d 'set the authenticated org as the default dev hub org for scratch org creation'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -s i -l clientid -d '(required) OAuth client ID (sometimes called the consumer key)'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -s r -l instanceurl -d 'the login URL of the instance the org lives on'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -s s -l setdefaultusername -d 'set the authenticated org as the default username that all commands run against'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:auth:web:login' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:config -d 'configure the Salesforce CLI'

complete $sfdx_looking -xa force:config:get -d 'get config var values for given names'
complete -c sfdx -n '__fish_sfdx_using_command force:config:get' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:config:get' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:config:get' -l verbose -d 'emit additional command output to stdout'

complete $sfdx_looking -xa force:config:list -d 'list config vars for the Salesforce CLI'
complete -c sfdx -n '__fish_sfdx_using_command force:config:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:config:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:config:set -d 'set config vars for the Salesforce CLI'
complete -c sfdx -n '__fish_sfdx_using_command force:config:set' -s g -l global -d 'set config var globally (to be used from any directory)'
complete -c sfdx -n '__fish_sfdx_using_command force:config:set' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:config:set' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:data -d 'manipulate records in your org'

complete $sfdx_looking -xa force:data:bulk -d 'manipulate records using the bulk API'

complete $sfdx_looking -xa force:data:bulk:delete -d 'bulk delete records from a csv file'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -s f -l csvfile -d '(required) the path to the CSV file containing the ids of the records to delete'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -s s -l sobjecttype -d '(required) the sObject type of the records you’re deleting'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -s w -l wait -d 'number of minutes to wait for the command to complete'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:delete' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:data:bulk:status -d 'view the status of a bulk data load job or batch'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:status' -s b -l batchid -d 'the ID of the batch whose status you want to view'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:status' -s i -l jobid -d '(required) the ID of the job you want to view or of the job whose batch you want to view'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:status' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:status' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:status' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:status' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:data:bulk:upsert -d 'bulk upsert records from a CSV file'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -s f -l csvfile -d '(required) the path to the CSV file that defines the records to upsert'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -s i -l externalid -d '(required) the column name of the external ID'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -s s -l sobjecttype -d '(required) the sObject type of the records you want to upsert'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -s w -l wait -d 'number of minutes to wait for the command to complete'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:bulk:upsert' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:data:record -d 'manipulate records using the enterprise API'

complete $sfdx_looking -xa force:data:record:create -d 'create a record'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -s s -l sobjecttype -d '(required) the type of the record you’re creating'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -s t -l usetoolingapi -d 'create the record with tooling api'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -s v -l values -d '(required) the <fieldName>=<value> pairs you’re creating'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:create' -l perflog -d 'get API performance data'

complete $sfdx_looking -xa force:data:record:delete -d 'delete a record'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -s i -l sobjectid -d 'the ID of the record you’re deleting'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -s s -l sobjecttype -d '(required) the type of the record you’re creating'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -s t -l usetoolingapi -d 'create the record with tooling api'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -s w -l where -d 'a list of <fieldName>=<value> pairs to search for'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:delete' -l perflog -d 'get API performance data'

complete $sfdx_looking -xa force:data:record:get -d 'view a record'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -s i -l sobjectid -d 'the ID of the record you’re deleting'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -s s -l sobjecttype -d '(required) the type of the record you’re creating'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -s t -l usetoolingapi -d 'create the record with tooling api'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -s w -l where -d 'a list of <fieldName>=<value> pairs to search for'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:get' -l perflog -d 'get API performance data'

complete $sfdx_looking -xa force:data:record:update -d 'update a record'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -s i -l sobjectid -d 'the ID of the record you’re deleting'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -s s -l sobjecttype -d '(required) the type of the record you’re creating'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -s t -l usetoolingapi -d 'create the record with tooling api'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -s w -l where -d 'a list of <fieldName>=<value> pairs to search for'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:data:record:update' -l perflog -d 'get API performance data'

complete $sfdx_looking -xa force:data:soql -d 'fetch records using SOQL'

complete $sfdx_looking -xa force:data:soql:query -d 'execute a SOQL query'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -s q -l query -d '(required) SOQL query to execute'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -s r -l resultformat -d '[default: human] result format emitted to stdout; --json flag overrides this parameter' -xa 'human csv json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -s t -l usetoolingapi -d 'execute query with Tooling API'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:data:soql:query' -l perflog -d 'get API performance data'

complete $sfdx_looking -xa force:data:tree -d 'manipulate records using the tree API'

complete $sfdx_looking -xa force:data:tree:export -d 'export data from an org into sObject tree format for force:data:tree:import consumption'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -s d -l outputdir -d 'directory to store files'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -s p -l plan -d 'generate multiple sobject tree files and a plan definition file for aggregated import'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -s q -l query -d '(required) soql query, or filepath of file containing a soql query, to retrieve records'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -s x -l prefix -d 'prefix of generated files'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:export' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:data:tree:import -d 'import data into an org using SObject Tree Save API'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -s f -l sobjecttreefiles -d 'comma-delimited, ordered paths of json files containing collection of record trees to insert'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -s p -l plan -d 'path to plan to insert multiple data files that have master-detail relationships'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -l confighelp -d 'display schema information for the --plan configuration file to stdout'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:data:tree:import' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:doc -d 'display help for force commands'

complete $sfdx_looking -xa force:doc:commands -d 'display help for force commands'

complete $sfdx_looking -xa force:doc:commands:display -d 'display help for force commands'
complete -c sfdx -n '__fish_sfdx_using_command force:doc:commands:display' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:doc:commands:display' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:doc:commands:list -d 'list the force commands'
complete -c sfdx -n '__fish_sfdx_using_command force:doc:commands:list' -s u -l usage -d 'list only docopt usage strings'
complete -c sfdx -n '__fish_sfdx_using_command force:doc:commands:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:doc:commands:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:lightning -d 'create Aura components and Lightning web'

complete $sfdx_looking -xa force:lightning:lint -d 'analyze (lint) Aura component code'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -s i -l ignore -d 'pattern used to ignore some folders'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -l config -d 'path to a custom ESLint configuration file'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -l exit -d 'exit with error code 1 if there are lint issues'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -l files -d 'pattern used to include specific files'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:lint' -l verbose -d 'report warnings in addition to errors'

complete $sfdx_looking -xa force:lightning:app -d 'create a Lightning app'

complete $sfdx_looking -xa force:lightning:app:create -d 'create a Lightning app'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:app:create' -s a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:app:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:app:create' -s n -l appname -d '(required) name of the generated Lightning app'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:app:create' -s t -l template -d '[default: DefaultLightningApp] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:app:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:app:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:lightning:component -d 'create a bundle for an Aura component or a Lightning web component'

complete $sfdx_looking -xa force:lightning:component:create -d 'create a bundle for an Aura component or a Lightning web component'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -s a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -s n -l componentname -d '(required) name of the generated Lightning component'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -s t -l template -d '[default: DefaultLightningCmp] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:component:create' -l type -d '[default: aura] type of the Lightning component' -xa 'aura lwc'

complete $sfdx_looking -xa force:lightning:event -d 'create a Lightning event'

complete $sfdx_looking -xa force:lightning:event:create -d 'create a Lightning event'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:event:create' -s a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:event:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:event:create' -s n -l eventname -d '(required) name of the generated Lightning event'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:event:create' -s t -l template -d '[default: DefaultLightningEvt] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:event:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:event:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:lightning:interface -d 'create a Lightning interface'

complete $sfdx_looking -xa force:lightning:interface:create -d 'create a Lightning interface'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:interface:create' -s a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:interface:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:interface:create' -s n -l interfacename -d '(required) name of the generated Lightning interface'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:interface:create' -s t -l template -d '[default: DefaultLightningIntf] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:interface:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:interface:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:lightning:test -d 'test Aura components'

complete $sfdx_looking -xa force:lightning:test:create -d 'create a Lightning test'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:create' -s n -l testname -d '(required) name of the generated Lightning test'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:create' -s t -l template -d '[default: DefaultLightningTest] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:lightning:test:install -d 'install Lightning Testing Service unmanaged package in your org'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -s r -l releaseversion -d '[default: latest] release version of Lightning Testing Service'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -s t -l packagetype -d '[default: full] type of unmanaged package. \'full\' option contains both jasmine and mocha, plus examples' -xa 'jasmine mocha full'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -s w -l wait -d 'number of minutes to wait for installation status (default 2)'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:install' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:lightning:test:run -d 'invoke Aura component tests'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s a -l appname -d 'name of your Lightning test application'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s d -l outputdir -d 'directory path to store test run artifacts: for example, log files and test results'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s f -l configfile -d 'path to config file for the test'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s o -l leavebrowseropen -d 'leave browser open'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s r -l resultformat -d '[default: human] result format emitted to stdout; --json flag overrides this parameter' -xa 'human tap junit json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s t -l timeout -d '[default: 60000] time (ms) to wait for results element in dom'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:lightning:test:run' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:limits -d 'view your org’s limits'

complete $sfdx_looking -xa force:limits:api -d 'view your org’s API limits'

complete $sfdx_looking -xa force:limits:api:display -d 'display current org’s limits'
complete -c sfdx -n '__fish_sfdx_using_command force:limits:api:display' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:limits:api:display' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:limits:api:display' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:limits:api:display' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:mdapi -d 'retrieve and deploy metadata using Metadata'

complete $sfdx_looking -xa force:mdapi:convert -d 'convert metadata from the Metadata API format into the source format'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:convert' -s d -l outputdir -d 'the output directory to store the source–formatted files'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:convert' -s r -l rootdir -d '(required) the root directory containing the Metadata API–formatted metadata'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:convert' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:convert' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:mdapi:deploy -d 'deploy metadata using Metadata API'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s c -l checkonly -d 'validate deploy but don’t save to the org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s d -l deploydir -d 'root of directory tree of files to deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s f -l zipfile -d 'path to .zip file of metadata to deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s g -l ignorewarnings -d 'whether a warning will allow a deployment to complete successfully'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s l -l testlevel -d 'deployment testing level' -xa 'NoTestRun RunSpecifiedTests RunLocalTests RunAllTestsInOrg'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s o -l ignoreerrors -d 'ignore any errors and do not roll back deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s q -l validateddeployrequestid -d 'request ID of the validated deployment to run a Quick Deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s r -l runtests -d 'tests to run if --testlevel RunSpecifiedTests'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -s w -l wait -d 'wait time for command to finish in minutes (default: 0)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy' -l verbose -d 'verbose output of deploy results'

complete $sfdx_looking -xa force:mdapi:deploy:cancel -d 'cancel a metadata deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:cancel' -s i -l jobid -d 'job ID of the deployment you want to cancel (default: most recent CLI deployment)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:cancel' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:cancel' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes 33'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:cancel' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:cancel' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:cancel' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:mdapi:deploy:report -d 'check the status of a metadata deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -s i -l jobid -d 'job ID of the deployment you want to cancel (default: most recent CLI deployment)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes 33'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:deploy:report' -l verbose -d 'verbose output of deploy results'

complete $sfdx_looking -xa force:mdapi:describemetadata -d 'display the metadata types enabled for your org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:describemetadata' -s a -l apiversion -d 'API version to use (the default is 46.0)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:describemetadata' -s f -l resultfile -d 'filter metadata known by the CLI'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:describemetadata' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:describemetadata' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:describemetadata' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:mdapi:listmetadata -d 'display properties of metadata components of a specified type'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -s a -l apiversion -d 'API version to use (the default is 46.0)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -s f -l resultfile -d 'path to the file where results are stored'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -s m -l metadatatype -d '(required) metadata type to be retrieved'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -l folder -d 'folder associated with the component'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:listmetadata' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:mdapi:retrieve -d 'retrieve metadata using Metadata API'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s a -l apiversion -d 'target API version for the retrieve (default 46.0)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s d -l sourcedir -d 'source dir to use instead of the default package dir in sfdx-project.json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s k -l unpackaged -d 'file path of manifest of components to retrieve'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s p -l packagenames -d 'a comma-separated list of packages to retrieve'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s r -l retrievetargetdir -d '(required) directory root for the retrieved files'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s s -l singlepackage -d 'a single-package retrieve (default: false)'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -s w -l wait -d 'wait time for command to finish in minutes (default: -1 (no limit))'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve' -l verbose -d 'verbose output of retrieve result'

complete $sfdx_looking -xa force:mdapi:retrieve:report -d 'check the status of a metadata retrieval'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -s i -l jobid -d 'job ID of the retrieve you want to check'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -s r -l retrievetargetdir -d 'directory root for the retrieved files'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -s w -l wait -d 'wait time for command to finish in minutes (default: -1 (no limit))'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:mdapi:retrieve:report' -l verbose -d 'verbose output of retrieve result'

complete $sfdx_looking -xa force:org -d 'manage your orgs'

complete $sfdx_looking -xa force:org:clone -d 'clone a sandbox org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -s a -l setalias -d 'alias for the created org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -s f -l definitionfile -d 'path to an org definition file'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -s s -l setdefaultusername -d 'set the created org as the default username'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -s t -l type -d '(required) type of org to create' -xa sandbox
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -s w -l wait -d '[default: 6 minutes] the streaming client socket timeout (in minutes)'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:clone' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:create -d 'create a scratch or sandbox org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s a -l setalias -d 'alias for the created org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s c -l noancestors -d 'do not include second-generation package ancestors in the scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s d -l durationdays -d 'duration of the scratch org (in days) (default:7, min:1, max:30)'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s f -l definitionfile -d 'path to an org definition file'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s i -l clientid -d 'connected app consumer key'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s n -l nonamespace -d 'create the scratch org with no namespace'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s s -l setdefaultusername -d 'set the created org as the default username'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s t -l type -d '[default: scratch] type of org to create' -xa 'scratch sandbox'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -s w -l wait -d '[default: 6 minutes] the streaming client socket timeout (in minutes)'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:delete -d 'mark a scratch org for deletion'
complete -c sfdx -n '__fish_sfdx_using_command force:org:delete' -s p -l noprompt -d 'no prompt to confirm deletion'
complete -c sfdx -n '__fish_sfdx_using_command force:org:delete' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:delete' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:delete' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:delete' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:delete' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:display -d 'get org description'
complete -c sfdx -n '__fish_sfdx_using_command force:org:display' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:display' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:display' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:display' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:org:display' -l verbose -d 'emit additional command output to stdout'

complete $sfdx_looking -xa force:org:list -d 'list all orgs you’ve created or authenticated to'
complete -c sfdx -n '__fish_sfdx_using_command force:org:list' -s p -l noprompt -d 'do not prompt for confirmation'
complete -c sfdx -n '__fish_sfdx_using_command force:org:list' -l all -d 'include expired, deleted, and unknown-status scratch orgs'
complete -c sfdx -n '__fish_sfdx_using_command force:org:list' -l clean -d 'remove all local org authorizations for non-active orgs'
complete -c sfdx -n '__fish_sfdx_using_command force:org:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:org:list' -l verbose -d 'list more information about each org'

complete $sfdx_looking -xa force:org:open -d 'open an org in your browser'
complete -c sfdx -n '__fish_sfdx_using_command force:org:open' -s p -l path -d 'navigation URL path'
complete -c sfdx -n '__fish_sfdx_using_command force:org:open' -s r -l urlonly -d 'display navigation URL, but don’t launch browser'
complete -c sfdx -n '__fish_sfdx_using_command force:org:open' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:open' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:open' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:open' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:status -d 'report sandbox org creation status and headlessly authenticate to org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -s a -l setalias -d 'alias for the created org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -s n -l sandboxname -d '(required) name of the sandbox org to check status for'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -s s -l setdefaultusername -d 'set the created org as the default username'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -s w -l wait -d 'number of minutes to wait while polling for status (default 6)'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:status' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:shape -d 'manage org shape'

complete $sfdx_looking -xa force:org:shape:create -d 'create a snapshot of org edition, features, and licenses'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:create' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:shape:delete -d 'delete all org shapes for a target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:delete' -s p -l noprompt -d 'do not prompt for confirmation'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:delete' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:delete' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:delete' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:delete' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:shape:list -d 'list all org shapes you’ve created'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:org:shape:list' -l verbose -d 'list more information about each org shape'

complete $sfdx_looking -xa force:org:snapshot -d 'snapshot a scratch org'

complete $sfdx_looking -xa force:org:snapshot:create -d 'snapshot a scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -s d -l description -d 'description of snapshot'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -s n -l snapshotname -d '(required) unique name of snapshot'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -s o -l sourceorg -d '(required) ID or locally authenticated username or alias of scratch org to snapshot'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:snapshot:delete -d 'delete a scratch org snapshot'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:delete' -s s -l snapshot -d '(required) name or ID of snapshot to delete'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:delete' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:delete' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:delete' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:delete' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:snapshot:get -d 'get details about a scratch org snapshot'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:get' -s s -l snapshot -d '(required) name or ID of snapshot to retrieve'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:get' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:get' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:get' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:get' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:org:snapshot:list -d 'list scratch org snapshots'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:list' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:org:snapshot:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package -d 'develop and install packages'

complete $sfdx_looking -xa force:package:create -d 'create a package'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s d -l description -d 'package description'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s e -l nonamespace -d 'creates the package with no namespace'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s n -l name -d '(required) package name'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s r -l path -d '(required) path to directory that contains the contents of the package'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s t -l packagetype -d '(required) package type' -xa 'Managed Unlocked'

complete $sfdx_looking -xa force:package:install -d 'install packages'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s a -l apexcompile -d '[default: all] compile all Apex in the org and package, or only Apex in the package' -xa 'all package'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s b -l publishwait -d 'number of minutes to wait for subscriber package version ID to become available'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s k -l installationkey -d 'installation key for key-protected package (default: null)'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s p -l package -d 'ID (starts with 04t) or alias of the package version to install'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s r -l noprompt -d 'do not prompt for confirmation'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s s -l securitytype -d '[default: AllUsers] security access type for the installed package' -xa 'AllUsers AdminsOnly'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s t -l upgradetype -d '[default: Mixed] the upgrade type for the package installation' -xa 'DeprecateOnly Mixed Delete'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:create' -s w -l wait -d 'number of minutes to wait for installation status'

complete $sfdx_looking -xa force:package:list -d 'list all packages in the Dev Hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:list' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:package:list' -l verbose -d 'display extended package detail'

complete $sfdx_looking -xa force:package:uninstall -d 'uninstall packages'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -s p -l package -d 'ID (starts with 04t) or alias of the package version to uninstall'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -s w -l wait -d 'number of minutes to wait for uninstall status'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:update -d 'update package details'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -s d -l description -d 'new package description'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -s n -l name -d 'new package name'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -s p -l package -d '(required) ID (starts with 0Ho) or alias of the package to update'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -s v -l targetdevhubusername -d 'username or alias for the dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:update' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:hammertest -d 'run ISV Hammer tests'

complete $sfdx_looking -xa force:package:hammertest:list -d 'list the statuses of running and completed ISV Hammer tests'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:list' -s i -l packageversionid -d 'ID of the package version to list results for'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:list' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:hammertest:report -d 'display the status or results of a ISV Hammer test'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:report' -s i -l requestid -d '(required) ID of the hammer request to report on'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:report' -s s -l summary -d 'report only a results summary (hide Apex test failures)'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:report' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:hammertest:run -d 'run ISV Hammer'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s d -l scheduledrundatetime -d 'earliest date/time to run the package upgrade test (YYYY-MM-DD HH:mm:ss, in GMT)'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s f -l subscriberfile -d 'file with list of subscriber orgs IDs, one per line'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s i -l packageversionids -d '(required) comma-separated list of package version IDs to test'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s p -l preview -d 'run the package hammer test in the Salesforce preview version'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s s -l subscriberorgs -d 'comma-separated list of subscriber org IDs'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s t -l apextests -d 'after package upgrade validation, run the package\'s Apex tests in the subscriber org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:hammertest:run' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:install -d 'install packages'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s a -l apexcompile -d '[default: all] compile all Apex in the org and package, or only Apex in the package' -xa 'all package'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s b -l publishwait -d 'number of minutes to wait for subscriber package version ID to become available'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s k -l installationkey -d 'installation key for key-protected package (default: null)'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s p -l package -d 'ID (starts with 04t) or alias of the package version to install'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s r -l noprompt -d 'do not prompt for confirmation'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s s -l securitytype -d '[default: AllUsers] security access type for the installed package' -xa 'AllUsers AdminsOnly'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s t -l upgradetype -d '[default: Mixed] the upgrade type for the package installation' -xa 'DeprecateOnly Mixed Delete'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install' -s w -l wait -d 'number of minutes to wait for installation status'

complete $sfdx_looking -xa force:package:install:report -d 'retrieve the status of a package installation request'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install:report' -s i -l requestid -d '(required) ID of the package install request you want to check'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install:report' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:install:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:installed -d 'list installed packages'

complete $sfdx_looking -xa force:package:installed:list -d 'list the org’s installed packages'
complete -c sfdx -n '__fish_sfdx_using_command force:package:installed:list' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:installed:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:installed:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:installed:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:uninstall -d 'uninstall packages'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -s p -l package -d 'ID (starts with 04t) or alias of the package version to uninstall'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -s w -l wait -d 'number of minutes to wait for uninstall status'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:uninstall:report -d 'retrieve status of package uninstall request'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall:report' -s i -l requestid -d '(required) ID of the package uninstall request you want to check'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall:report' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package:uninstall:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package:version -d 'develop package versions'

complete $sfdx_looking -xa force:package1 -d 'develop first-generation managed and'

complete $sfdx_looking -xa force:package1:version -d 'develop package versions'

complete $sfdx_looking -xa force:package1:version:create -d 'report on created package versions'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s d -l description -d 'package version description'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s i -l packageid -d '(required) ID of the metadata package (starts with 033) of which you’re creating a new version'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s k -l installationkey -d 'installation key for key-protected package (default: null)'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s m -l managedreleased -d 'create a managed package version'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s n -l name -d '(required) package version name'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s p -l postinstallurl -d 'post install URL'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s r -l releasenotesurl -d 'release notes URL'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s v -l version -d 'package version in major.minor format, for example, 3.2'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s w -l wait -d 'minutes to wait for the package version to be created (default: 2 minutes)'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package1:version:display -d 'display details about a first-generation package version'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:display' -s i -l packageversionid -d '(required) metadata package version ID (starts with 04t)'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:display' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:display' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:display' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:display' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package1:version:list -d 'list package versions for the specified first-generation package or for the org'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:list' -s i -l packageid -d 'metadata package ID (starts with 033)'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:list' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package1:version:create -d 'report on created package versions'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s d -l description -d 'package version description'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s i -l packageid -d '(required) ID of the metadata package (starts with 033) of which you’re creating a new version'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s k -l installationkey -d 'installation key for key-protected package (default: null)'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s m -l managedreleased -d 'create a managed package version'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s n -l name=name -d '(required) package version name'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s p -l postinstallurl -d 'post install URL'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s r -l releasenotesurl -d 'release notes URL'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s v -l version -d 'package version in major.minor format, for example, 3.2'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -s w -l wait -d 'minutes to wait for the package version to be created (default: 2 minutes)'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:package1:version:create:get -d 'retrieve the status of a package version creation request'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create:get' -s i -l requestid -d '(required) PackageUploadRequest ID'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create:get' -s u -l targetusername -d 'username or alias for the target org'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create:get' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create:get' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:package1:version:create:get' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:project -d 'set up a Salesforce DX project'

complete $sfdx_looking -xa force:project:create -d 'create a Salesforce DX project'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -s d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -s n -l projectname -d '(required) name of the generated project'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -s p -l defaultpackagedir -d '[default: force-app] default package directory name'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -s s -l namespace -d 'project associated namespace'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -s t -l template -d '[default: standard] template to use for project creation'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -s x -l manifest -d 'generate a manifest (package.xml) for change-set-based development'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create; and __fish_contains_opt -s x manifest' -xa '(__fish_sfdx_find_packagexml)'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:project:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:project:upgrade -d 'update project config files to the latest format'
complete -c sfdx -n '__fish_sfdx_using_command force:project:upgrade' -s f -l forceupgrade -d 'run all upgrades even if project has already been upgraded'
complete -c sfdx -n '__fish_sfdx_using_command force:project:upgrade' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:project:upgrade' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:schema -d 'view standard and custom objects'

complete $sfdx_looking -xa force:schema:sobject -d 'view standard and custom objects'

complete $sfdx_looking -xa force:schema:sobject:describe -d 'describe an object'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:describe' -s s -l sobjecttype -d '(required) the API name of the object to describe'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:describe' -s t -l usetoolingapi -d 'execute with Tooling API'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:describe' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:describe' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:describe' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:describe' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:schema:sobject:list -d 'list all objects of a specified category'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:list' -s c -l sobjecttypecategory -d '(required) the type of objects to list (all|custom|standard)'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:list' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:sobject:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source -d 'sync your project with your orgs'

complete $sfdx_looking -xa force:source:convert -d 'convert source into Metadata API format'
complete -c sfdx -n '__fish_sfdx_using_command force:source:conevrt' -s d -l outputdir -d 'output directory to store the Metadata API–formatted files in'
complete -c sfdx -n '__fish_sfdx_using_command force:source:conevrt' -s n -l packagename -d 'name of the package to associate with the metadata-formatted files'
complete -c sfdx -n '__fish_sfdx_using_command force:source:conevrt' -s r -l rootdir -d 'a source directory other than the default package to convert'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:convert' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:schema:convert' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:delete -d 'delete source from your project and from a non-source-tracked org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -s m -l metadata -d 'comma-separated list of names of metadata components to delete'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -s p -l sourcepath -d 'comma-separated list of paths to the local metadata to delete'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -s r -l noprompt -d 'do not prompt for delete confirmation'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes 33'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:delete' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:deploy -d 'deploy source to an org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s c -l checkonly -d 'validate deploy but don’t save to the org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s g -l ignorewarnings -d 'whether a warning will allow a deployment to complete successfully'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s l -l testlevel -d 'deployment testing level' -xa 'NoTestRun RunSpecifiedTests RunLocalTests RunAllTestsInOrg'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s m -l metadata -d 'comma-separated list of metadata component names'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s o -l ignoreerrors -d 'ignore any errors and do not roll back deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s p -l sourcepath -d 'comma-separated list of paths to the local source files to deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s q -l validateddeployrequestid -d 'request ID of the validated deployment to run a Quick Deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s r -l runtests -d 'tests to run if --testlevel RunSpecifiedTests'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s x -l manifest -d 'file path for manifest (package.xml) of components to deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy; and __fish_contains_opt -s x manifest' -xa '(__fish_sfdx_find_packagexml)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l verbose -d 'display Apex test processing details'

complete $sfdx_looking -xa force:source:open -d 'edit a Lightning Page with Lightning App Builder'
complete -c sfdx -n '__fish_sfdx_using_command force:source:open' -s f -l sourcefile -d '(required) file to edit'
complete -c sfdx -n '__fish_sfdx_using_command force:source:open' -s r -l urlonly -d 'generate a navigation URL; don’t launch the editor'
complete -c sfdx -n '__fish_sfdx_using_command force:source:open' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:open' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:open' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:open' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:pull -d 'pull source from the scratch org to the project'
complete -c sfdx -n '__fish_sfdx_using_command force:source:pull' -s f -l forceoverwrite -d 'ignore conflict warnings and overwrite changes to the project'
complete -c sfdx -n '__fish_sfdx_using_command force:source:pull' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:pull' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes (default: 33)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:pull' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:pull' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:pull' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:push -d 'push source to a scratch org from the project'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -s f -l forceoverwrite -d 'ignore conflict warnings and overwrite changes to scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -s g -l ignorewarnings -d 'deploy changes even if warnings are generated'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes (default: 33)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:push' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:retrieve -d 'retrieve source from an org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s a -l apiversion -d 'target API version for the retrieve (default 46.0)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s m -l metadata -d 'comma-separated list of metadata component names'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s n -l packagenames -d 'a comma-separated list of packages to retrieve'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s p -l sourcepath -d 'comma-separated list of source file paths to retrieve'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -s x -l manifest -d 'file path for manifest (package.xml) of components to retrieve'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve; and __fish_contains_opt -s x manifest' -xa '(__fish_use_subcommand)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:source:retrieve' -l verbose -d 'verbose output of retrieve result'

complete $sfdx_looking -xa force:source:status -d 'list local changes and/or changes in a scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -s a -l all -d 'list all the changes that have been made'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -s l -l local -d 'list the changes that have been made locally'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -s r -l remote -d 'list the changes that have been made in the scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:status' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:deploy -d 'deploy source to an org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s c -l checkonly -d 'validate deploy but don’t save to the org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s g -l ignorewarnings -d 'whether a warning will allow a deployment to complete successfully'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s l -l testlevel -d 'deployment testing level' -xa 'NoTestRun RunSpecifiedTests RunLocalTests RunAllTestsInOrg'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s m -l metadata -d 'comma-separated list of metadata component names'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s o -l ignoreerrors -d 'ignore any errors and do not roll back deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s p -l sourcepath -d 'comma-separated list of paths to the local source files to deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s q -l validateddeployrequestid -d 'request ID of the validated deployment to run a Quick Deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s r -l runtests -d 'tests to run if --testlevel RunSpecifiedTests'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -s x -l manifest -d 'file path for manifest (package.xml) of components to deploy'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy; and __fish_contains_opt -s x manifest' -xa '(__fish_use_subcommand)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy' -l verbose -d 'display Apex test processing details'

complete $sfdx_looking -xa force:source:deploy:cancel -d 'cancel a source deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:cancel' -s i -l jobid -d 'job ID of the deployment you want to cancel (default: most recent CLI deployment)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:cancel' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:cancel' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes 33'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:cancel' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:cancel' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:cancel' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:source:deploy:report -d 'check the status of a metadata deployment'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:report' -s i -l jobid -d 'job ID of the deployment you want to check (default: most recent CLI deployment)'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:report' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:report' -s w -l wait -d '[default: 33 minutes] wait time for command to finish in minutes 33'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:report' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:report' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:source:deploy:report' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:user -d 'perform user-related admin tasks'

complete $sfdx_looking -xa force:user:create -d 'create a user for a scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:create' -s a -l setalias -d 'set an alias for the created username to reference within the CLI'
complete -c sfdx -n '__fish_sfdx_using_command force:user:create' -s f -l definitionfile -d 'file path to a user definition'
complete -c sfdx -n '__fish_sfdx_using_command force:user:create' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:create' -s v -l targetdevhubusername -d 'username or alias for the dev hub org; overrides default dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:create' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:user:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:user:display -d 'displays information about a user of a scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:display' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:display' -s v -l targetdevhubusername -d 'username or alias for the dev hub org; overrides default dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:display' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:user:display' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:user:display' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:user:list -d 'lists all users of a scratch org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:list' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:list' -s v -l targetdevhubusername -d 'username or alias for the dev hub org; overrides default dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:list' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:user:list' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:user:list' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:user:password -d 'perform password-related admin tasks'

complete $sfdx_looking -xa force:user:password:generate -d 'generate a password for scratch org users'
complete -c sfdx -n '__fish_sfdx_using_command force:user:password:generate' -s o -l onbehalfof -d 'comma-separated list of usernames for which to generate passwords'
complete -c sfdx -n '__fish_sfdx_using_command force:user:password:generate' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:password:generate' -s v -l targetdevhubusername -d 'username or alias for the dev hub org; overrides default dev hub org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:password:generate' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:user:password:generate' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:user:password:generate' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:user:permset -d 'perform permset-related admin tasks'

complete $sfdx_looking -xa force:user:permset:assign -d 'assign a permission set to one or more users of an org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:permset:assign' -s n -l permsetname -d '(required) the name of the permission set to assign'
complete -c sfdx -n '__fish_sfdx_using_command force:user:permset:assign' -s o -l onbehalfof -d 'comma-separated list of usernames or aliases to assign the permission set to'
complete -c sfdx -n '__fish_sfdx_using_command force:user:permset:assign' -s u -l targetusername -d 'username or alias for the target org; overrides default target org'
complete -c sfdx -n '__fish_sfdx_using_command force:user:permset:assign' -l apiversion -d 'override the api version used for api requests made by this command'
complete -c sfdx -n '__fish_sfdx_using_command force:user:permset:assign' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:user:permset:assign' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:visualforce -d 'create and edit Visualforce files'

complete $sfdx_looking -xa force:visualforce:component -d 'create a Visualforce component'

complete $sfdx_looking -xa force:visualforce:component:create -d 'create a Visualforce component'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -p a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -p d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -p l -l label -d '(required) Visualforce component label'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -p n -l componentname -d '(required) name of the generated Visualforce component'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -p t -l template -d '[default: DefaultVFComponent] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:component:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa force:visualforce:page -d 'create a Visualforce page'

complete $sfdx_looking -xa force:visualforce:page:create -d 'create a Visualforce page'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -p a -l apiversion -d '[default: 46.0] API version number'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -p d -l outputdir -d 'folder for saving the created files'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -p l -l label -d '(required) Visualforce page label'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -p n -l componentname -d '(required) name of the generated Visualforce page'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -p t -l template -d '[default: DefaultVFPage] template to use for file creation'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command force:visualforce:page:create' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

#
# help
#
complete $sfdx_looking -xa help -d 'display help for sfdx'

#
# plugins
#
complete $sfdx_looking -xa plugins -d 'add/remove/create CLI plug-ins'
complete -c sfdx -n '__fish_sfdx_using_command plugins' -l core -d 'show core plugins'

complete $sfdx_looking -xa plugins:install -d 'installs a plugin into the CLI'
complete -c sfdx -n '__fish_sfdx_using_command plugins:install' -s f -l force -d 'yarn install with force flag'
complete -c sfdx -n '__fish_sfdx_using_command plugins:install' -s h -l help -d 'show CLI help'
complete -c sfdx -n '__fish_sfdx_using_command plugins:install' -s v -l verbose

complete $sfdx_looking -xa plugins:link -d 'links a plugin into the CLI for development'
complete -c sfdx -n '__fish_sfdx_using_command plugins:link' -s h -l help -d 'show CLI help'
complete -c sfdx -n '__fish_sfdx_using_command plugins:link' -s v -l verbose

complete $sfdx_looking -xa plugins:uninstall -d 'removes a plugin from the CLI'
complete -c sfdx -n '__fish_sfdx_using_command plugins:uninstall' -s h -l help -d 'show CLI help'
complete -c sfdx -n '__fish_sfdx_using_command plugins:uninstall' -s v -l verbose

complete $sfdx_looking -xa plugins:update -d 'update installed plugins'
complete -c sfdx -n '__fish_sfdx_using_command plugins:update' -s h -l help -d 'show CLI help'
complete -c sfdx -n '__fish_sfdx_using_command plugins:update' -s v -l verbose

complete $sfdx_looking -xa plugins:generate -d 'create a new sfdx-cli plugin'
complete -c sfdx -n '__fish_sfdx_using_command plugins:generate' -s h -l help -d 'show CLI help'
complete -c sfdx -n '__fish_sfdx_using_command plugins:generate' -l defaults -d 'use defaults for every setting'
complete -c sfdx -n '__fish_sfdx_using_command plugins:generate' -l force -d 'overwrite existing files'

complete $sfdx_looking -xa plugins:trust -d 'pack an npm package and produce a tgz file along with a corresponding digital signature'

complete $sfdx_looking -xa plugins:trust:sign -d 'pack an npm package and produce a tgz file along with a corresponding digital signature'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:sign' -s k -l privatekeypath -d '(required) the local file path for the private key.'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:sign' -s p -l publickeyurl -d '(required) the url where the public key/certificate will be hosted.'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:sign' -s s -l signatureurl -d '(required) the url where the signature will be hosted minus name of signature file'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:sign' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:sign' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

complete $sfdx_looking -xa plugins:trust:verify -d 'For an npm validate the associated digital signature if it exits.'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:verify' -s n -l npm -d '(required) Specify the npm name. This can include a tag/version'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:verify' -s r -l registry -d 'The registry name. the behavior is the same as npm.'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:verify' -l json -d 'format output as json'
complete -c sfdx -n '__fish_sfdx_using_command plugins:trust:verify' -l loglevel -d '[default: warn] logging level for this command invocation' -xa $sfdx_loglevels

#
# update
#
complete $sfdx_looking -xa update -d 'update the sfdx CLI'

#
# which
#
complete $sfdx_looking -xa which -d 'show which plugin a command is in'
