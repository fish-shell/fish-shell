function __fish_print_magento_url_protocols -d "Shows all available URL protocols"
    echo http://
    echo https://
end

function __fish_print_magento_mysql_engines
    echo MyISAM\t"MyISAM storage engine"
    echo InnoDB\t"Supports transactions, row-level locking, and foreign keys"
    echo BLACKHOLE\t"/dev/null storage engine (anything you write to it disappears)"
    echo MEMORY\t"Hash based, stored in memory, useful for temporary tables"
    echo CSV\t"CSV storage engine"
    echo ARCHIVE\t"Archive storage engine"
end

###

function __fish_print_magento_modules -d "Lists all Magento modules"
    set -l config_path app/etc/config.php
    test -f $config_path; or return

    set -l in_modules 0
    cat $config_path | while read -l line
        if test "$in_modules" -eq 0
            if string match -rq '[\'"]modules[\'"]\s*=>.*\[' -- $line
                set in_modules 1
            end
        else
            if string match -rq '^\s*]\s*,\s*$' -- $line
                break
            end
            string replace -rf '\s*[\'"](.*?)[\'"]\s*=>.*' '$1' -- $line
        end
    end
end

function __fish_print_magento_i18n_packing_modes -d "Shows all available packing modes"
    echo replace\t"replace language pack by new one"
    echo merge\t"merge language packages"
end

function __fish_print_magento_available_ides -d "Shows all IDEs supported by magento"
    echo phpstorm\t"JetBrains PhpStorm"
end

function __fish_print_magento_available_tests -d "Shows all available magento tests"
    echo all\t"all tests"
    echo unit\t"unit tests only"
    echo integration\t"integration tests only"
    echo integration-all\t"all integration tests"
    echo static\t"static file tests only"
    echo static-all\t"all static file tests"
    echo integrity\t"integrity tests only"
    echo legacy\t"legacy tests only"
    echo default\t"all tests"
end

function __fish_print_magento_theme_areas -d "Shows all available magento theme areas"
    echo frontend\t"Frontend"
    echo adminhtml\t"Backend"
end

function __fish_print_magento_languages -d "Shows all existing magento languages"
    echo af_ZA\t"Afrikaans (South Africa)"
    echo sq_AL\t"Albanian (Albania)"
    echo ar_DZ\t"Arabic (Algeria)"
    echo ar_EG\t"Arabic (Egypt)"
    echo ar_KW\t"Arabic (Kuwait)"
    echo ar_MA\t"Arabic (Morocco)"
    echo ar_SA\t"Arabic (Saudi Arabia)"
    echo az_Latn_AZ\t"Azerbaijani (Azerbaijan)"
    echo eu_ES\t"Basque (Spain)"
    echo be_BY\t"Belarusian (Belarus)"
    echo bn_BD\t"Bengali (Bangladesh)"
    echo bs_Latn_BA\t"Bosnian (Bosnia and Herzegovina)"
    echo bg_BG\t"Bulgarian (Bulgaria)"
    echo ca_ES\t"Catalan (Spain)"
    echo zh_Hans_CN\t"Chinese (China)"
    echo zh_Hant_HK\t"Chinese (Hong Kong SAR China)"
    echo zh_Hant_TW\t"Chinese (Taiwan)"
    echo hr_HR\t"Croatian (Croatia)"
    echo cs_CZ\t"Czech (Czech Republic)"
    echo da_DK\t"Danish (Denmark)"
    echo nl_BE\t"Dutch (Belgium)"
    echo nl_NL\t"Dutch (Netherlands)"
    echo en_AU\t"English (Australia)"
    echo en_CA\t"English (Canada)"
    echo en_IE\t"English (Ireland)"
    echo en_NZ\t"English (New Zealand)"
    echo en_GB\t"English (United Kingdom)"
    echo en_US\t"English (United States)"
    echo et_EE\t"Estonian (Estonia)"
    echo fil_PH\t"Filipino (Philippines)"
    echo fi_FI\t"Finnish (Finland)"
    echo fr_BE\t"French (Belgium)"
    echo fr_CA\t"French (Canada)"
    echo fr_FR\t"French (France)"
    echo gl_ES\t"Galician (Spain)"
    echo ka_GE\t"Georgian (Georgia)"
    echo de_AT\t"German (Austria)"
    echo de_DE\t"German (Germany)"
    echo de_CH\t"German (Switzerland)"
    echo el_GR\t"Greek (Greece)"
    echo gu_IN\t"Gujarati (India)"
    echo he_IL\t"Hebrew (Israel)"
    echo hi_IN\t"Hindi (India)"
    echo hu_HU\t"Hungarian (Hungary)"
    echo is_IS\t"Icelandic (Iceland)"
    echo id_ID\t"Indonesian (Indonesia)"
    echo it_IT\t"Italian (Italy)"
    echo it_CH\t"Italian (Switzerland)"
    echo ja_JP\t"Japanese (Japan)"
    echo km_KH\t"Khmer (Cambodia)"
    echo ko_KR\t"Korean (South Korea)"
    echo lo_LA\t"Lao (Laos)"
    echo lv_LV\t"Latvian (Latvia)"
    echo lt_LT\t"Lithuanian (Lithuania)"
    echo mk_MK\t"Macedonian (Macedonia)"
    echo ms_Latn_MY\t"Malay (Malaysia)"
    echo mn_Cyrl_MN\t"Mongolian (Mongolia)"
    echo nb_NO\t"Norwegian Bokmål (Norway)"
    echo nn_NO\t"Norwegian Nynorsk (Norway)"
    echo fa_IR\t"Persian (Iran)"
    echo pl_PL\t"Polish (Poland)"
    echo pt_BR\t"Portuguese (Brazil)"
    echo pt_PT\t"Portuguese (Portugal)"
    echo ro_RO\t"Romanian (Romania)"
    echo ru_RU\t"Russian (Russia)"
    echo sr_Cyrl_RS\t"Serbian (Serbia)"
    echo sk_SK\t"Slovak (Slovakia)"
    echo sl_SI\t"Slovenian (Slovenia)"
    echo es_AR\t"Spanish (Argentina)"
    echo es_CL\t"Spanish (Chile)"
    echo es_CO\t"Spanish (Colombia)"
    echo es_CR\t"Spanish (Costa Rica)"
    echo es_MX\t"Spanish (Mexico)"
    echo es_PA\t"Spanish (Panama)"
    echo es_PE\t"Spanish (Peru)"
    echo es_ES\t"Spanish (Spain)"
    echo es_VE\t"Spanish (Venezuela)"
    echo sw_KE\t"Swahili (Kenya)"
    echo sv_SE\t"Swedish (Sweden)"
    echo th_TH\t"Thai (Thailand)"
    echo tr_TR\t"Turkish (Turkey)"
    echo uk_UA\t"Ukrainian (Ukraine)"
    echo vi_VN\t"Vietnamese (Vietnam)"
    echo cy_GB\t"Welsh (United Kingdom)"
end

function __fish_print_magento_source_theme_file_types -d "Shows all available source theme file types"
    echo less\t"Currently only LESS is supported"
end

function __fish_print_magento_deploy_modes -d "Shows all available deploy modes"
    echo developer\t"Development mode"
    echo production\t"Production mode"
end

function __fish_print_magento_cache_types -d "Shows all available cache types"
    echo config\t"Configuration"
    echo layout\t"Layout"
    echo block_html\t"Block HTML output"
    echo collections\t"Collections data"
    echo db_ddl\t"Database schema"
    echo eav\t"Entity attribute value (EAV)"
    echo full_page\t"Page cache"
    echo reflection\t"Reflection"
    echo translate\t"Translations"
    echo config_integration\t"Integration configuration"
    echo config_integration_api\t"Integration API configuration"
    echo config_webservice\t"Web services configuration"
end

function __fish_print_magento_verbosity_levels -d "Shows all available verbosity levels"
    echo 1\t"normal output"
    echo 2\t"more verbose output"
    echo 3\t"debug"
end

function __fish_print_magento_list_formats -d "Shows all available output formats"
    echo txt\t"Print as plain text"
    echo json\t"Print as JSON"
    echo xml\t"Print as XML"
end

function __fish_print_magento_commands_list -d "Lists magento commands"
    set -l commands help list admin:user:create admin:user:unlock \
        app:config:dump cache:clean cache:disable cache:enable cache:flush \
        cache:status catalog:images:resize catalog:product:attributes:cleanup \
        cron:run customer:hash:upgrade deploy:mode:set deploy:mode:show \
        dev:source-theme:deploy dev:tests:run dev:urn-catalog:generate \
        dev:xml:convert i18n:collect-phrases i18n:pack i18n:uninstall \
        indexer:info indexer:reindex indexer:reset indexer:set-mode \
        indexer:show-mode indexer:status info:adminuri info:backups:list \
        info:currency:list info:dependencies:show-framework \
        info:dependencies:show-modules info:dependencies:show-modules-circular \
        info:language:list info:timezone:list maintenance:allow-ips \
        maintenance:disable maintenance:enable maintenance:status module:disable \
        module:enable module:status module:uninstall sampledata:deploy \
        sampledata:remove sampledata:reset setup:backup setup:config:set \
        setup:cron:run setup:db-data:upgrade setup:db-schema:upgrade \
        setup:db:status setup:di:compile setup:install \
        setup:performance:generate-fixtures setup:rollback \
        setup:static-content:deploy setup:store-config:set \
        setup:uninstall setup:upgrade theme:uninstall
    for i in $commands
        echo $i
    end
end

#########################################################

function __fish_magento_not_in_command -d "Checks that prompt is not inside of magento command"
    set -l cmd (commandline -xpc)
    for i in $cmd
        if contains -- $i (__fish_print_magento_commands_list)
            return 1
        end
    end
    return 0
end

#########################################################

# Perforce command is a single word that comes either as first argument
# or directly after global options.
# To test whether we're in command, it's enough that a command will appear
# in the arguments, even though if more than a single command is specified,
# p4 will complain.
function __fish_magento_is_using_command -d "Checks if prompt is in a specific command"
    if contains -- $argv[1] (commandline -xpc)
        return 0
    end
    return 1
end

#########################################################

function __fish_magento_register_command -d "Adds a completion for a specific command"
    complete -c magento -n __fish_magento_not_in_command -a $argv[1] $argv[2..-1]
end

#########################################################

function __fish_magento_register_command_option -d "Adds a specific option for a command"
    complete -c magento -n "__fish_magento_is_using_command $argv[1]" $argv[2..-1]
end

#########################################################

function __fish_magento_parameter_missing -d "Checks if a parameter has been given already"
    if __fish_contains_opt $argv
        return 1
    end

    return 0
end

##################
# Global options #
##################
complete -x -c magento -s h -l help -d "Show help for a command"
complete -x -c magento -s q -l quiet -d "Do not output any message"
complete -x -c magento -s v -l verbose -a "(__fish_print_magento_verbosity_levels)" -d "Increase verbosity: 1 for normal, 2 for verbose and 3 for debug"
complete -x -c magento -o -vv
complete -x -c magento -o vvv
complete -x -c magento -s V -l version -d "Show version"
complete -x -c magento -l ansi -d "Force colored output"
complete -x -c magento -l no-ansi -d "Disable colored output"
complete -x -c magento -s n -l no-interaction -d "Don't ask any interactive question"

################
# Sub-Commands #
################
__fish_magento_register_command help -a "(__fish_print_magento_commands_list)" -d "Show help for a command"
__fish_magento_register_command list -d "List commands"

__fish_magento_register_command admin:user:create -x -d "Create admin account"
__fish_magento_register_command admin:user:unlock -x -d "Unlock admin account"

__fish_magento_register_command app:config:dump -d "Create application dump"

__fish_magento_register_command cache:clean -d "Clean cache types"
__fish_magento_register_command cache:disable -d "Disable cache types"
__fish_magento_register_command cache:enable -d "Enable cache types"
__fish_magento_register_command cache:flush -d "Flush cache storage used by cache types"
__fish_magento_register_command cache:status -d "Check cache status"

__fish_magento_register_command catalog:images:resize -d "Create resized product images"
__fish_magento_register_command catalog:product:attributes:cleanup -d "Remove unused product attributes"

__fish_magento_register_command cron:run -d "Run jobs by schedule"

__fish_magento_register_command customer:hash:upgrade -d "Upgrade customer's hashes according to latest algorithm"

__fish_magento_register_command deploy:mode:set -d "Set application mode"
__fish_magento_register_command deploy:mode:show -d "Show current application mode"

__fish_magento_register_command dev:source-theme:deploy -d "Collect and publish source files for theme"
__fish_magento_register_command dev:tests:run -d "Run tests"
__fish_magento_register_command dev:urn-catalog:generate -d "Generate catalog of URNs to *.xsd mappings for IDEs to highlight XML"
__fish_magento_register_command dev:xml:convert -d "Convert XML file using XSL stylesheets"

__fish_magento_register_command i18n:collect-phrases -d "Discover phrases in the codebase"
__fish_magento_register_command i18n:pack -d "Save language package"
__fish_magento_register_command i18n:uninstall -d "Uninstall language packages"

__fish_magento_register_command indexer:info -d "Show allowed indexers"
__fish_magento_register_command indexer:reindex -d "Reindex data"
__fish_magento_register_command indexer:reset -d "Reset indexer status to invalid"
__fish_magento_register_command indexer:set-mode -d "Set index mode type"
__fish_magento_register_command indexer:show-mode -d "Show index mode"
__fish_magento_register_command indexer:status -d "Show status of indexer"

__fish_magento_register_command info:adminuri -d "Show Magento Admin URI"
__fish_magento_register_command info:backups:list -d "Show available backup files"
__fish_magento_register_command info:currency:list -d "Show available currencies"
__fish_magento_register_command info:dependencies:show-framework -d "Show dependencies on Magento framework"
__fish_magento_register_command info:dependencies:show-modules -d "Show dependencies between modules"
__fish_magento_register_command info:dependencies:show-modules-circular -d "Show circular dependencies between modules"
__fish_magento_register_command info:language:list -d "Show available languages"
__fish_magento_register_command info:timezone:list -d "Show available timezones"

__fish_magento_register_command maintenance:allow-ips -d "Set maintenance mode exempt IPs"
__fish_magento_register_command maintenance:disable -d "Disable maintenance mode"
__fish_magento_register_command maintenance:enable -d "Enable maintenance mode"
__fish_magento_register_command maintenance:status -d "Show maintenance mode status"

__fish_magento_register_command module:disable -d "Disable specified modules"
__fish_magento_register_command module:enable -d "Enable specified modules"
__fish_magento_register_command module:status -d "Show status of modules"
__fish_magento_register_command module:uninstall -d "Uninstall modules installed by composer"

__fish_magento_register_command sampledata:deploy -d "Deploy sample data modules"
__fish_magento_register_command sampledata:remove -d "Remove sample data packages"
__fish_magento_register_command sampledata:reset -d "Reset sample data modules for re-installation"

__fish_magento_register_command setup:backup -d "Take backup of application code base, media and database"
__fish_magento_register_command setup:config:set -d "Create or modifies the deployment configuration"
__fish_magento_register_command setup:cron:run -d "Run cron job scheduled for setup application"
__fish_magento_register_command setup:db-data:upgrade -d "Install and upgrades data in the DB"
__fish_magento_register_command setup:db-schema:upgrade -d "Install and upgrade the DB schema"
__fish_magento_register_command setup:db:status -d "Check if DB schema or data requires upgrade"
__fish_magento_register_command setup:di:compile -d "Generate DI configuration and all missing classes that can be auto-generated"
__fish_magento_register_command setup:install -d "Install Magento application"
__fish_magento_register_command setup:performance:generate-fixtures -d "Generate fixtures"
__fish_magento_register_command setup:rollback -d "Roll back Magento Application codebase, media and database"
__fish_magento_register_command setup:static-content:deploy -d "Deploy static view files"
__fish_magento_register_command setup:store-config:set -d "Install store configuration"
__fish_magento_register_command setup:uninstall -d "Uninstall Magento application"
__fish_magento_register_command setup:upgrade -d "Upgrade Magento application, DB data, and schema"

__fish_magento_register_command theme:uninstall -d "Uninstall theme"

#
# help
#
__fish_magento_register_command_option help -x -a "(__fish_print_magento_commands_list)" -d "Show help for a command"

#
# list
#
__fish_magento_register_command_option list -f -l xml -d "Output as XML"
__fish_magento_register_command_option list -f -l raw -d "Output as plaintext"
__fish_magento_register_command_option list -f -l format -a "(__fish_print_magento_list_formats)" -d "Output other formats (default: txt)"

#
# admin:user:create
#
__fish_magento_register_command_option admin:user:create -n "__fish_magento_parameter_missing admin-user" -f -a --admin-user -d "(Required) Admin user"
__fish_magento_register_command_option admin:user:create -n "__fish_magento_parameter_missing admin-password" -f -a --admin-password -d "(Required) Admin password"
__fish_magento_register_command_option admin:user:create -n "__fish_magento_parameter_missing admin-email" -f -a --admin-email -d "(Required) Admin email"
__fish_magento_register_command_option admin:user:create -n "__fish_magento_parameter_missing admin-firstname" -f -a --admin-firstname -d "(Required) Admin first name"
__fish_magento_register_command_option admin:user:create -n "__fish_magento_parameter_missing admin-lastname" -f -a --admin-lastname -d "(Required) Admin last name"
__fish_magento_register_command_option admin:user:create -f -r -l admin-user -d "(Required) Admin user"
__fish_magento_register_command_option admin:user:create -f -r -l admin-password -d "(Required) Admin password"
__fish_magento_register_command_option admin:user:create -f -r -l admin-email -d "(Required) Admin email"
__fish_magento_register_command_option admin:user:create -f -r -l admin-firstname -d "(Required) Admin first name"
__fish_magento_register_command_option admin:user:create -f -r -l admin-lastname -d "(Required) Admin last name"
__fish_magento_register_command_option admin:user:create -f -l magento-init-params -d "Add to any command to customize Magento initialization parameters"

#
# admin:user:unlock
#
__fish_magento_register_command_option admin:user:unlock -f -d "Admin user to unlock"

#
# cache:clean
#
__fish_magento_register_command_option cache:clean -f -a "(__fish_print_magento_cache_types)" -d "Space-separated list of cache types or omit for all"
__fish_magento_register_command_option cache:clean -f -l bootstrap -d "Add or override parameters of the bootstrap"

#
# cache:enable
#
__fish_magento_register_command_option cache:enable -f -a "(__fish_print_magento_cache_types)" -d "Space-separated list of cache types or omit for all"
__fish_magento_register_command_option cache:enable -f -l bootstrap -d "Add or override parameters of the bootstrap"

#
# cache:disable
#
__fish_magento_register_command_option cache:disable -f -a "(__fish_print_magento_cache_types)" -d "Space-separated list of cache types or omit for all"
__fish_magento_register_command_option cache:disable -f -l bootstrap -d "Add or override parameters of the bootstrap"

#
# cache:status
#
__fish_magento_register_command_option cache:status -f -l bootstrap -d "Add or override parameters of the bootstrap"

#
# cron:run
#
__fish_magento_register_command_option cron:run -f -l group -d "Run jobs only from specified group"
__fish_magento_register_command_option cron:run -f -l bootstrap -d "Add or override parameters of the bootstrap"

#
# deploy:mode:set
#
__fish_magento_register_command_option deploy:mode:set -f -a "(__fish_print_magento_deploy_modes)" -d 'Application mode to set. Available are "developer" or "production"'
__fish_magento_register_command_option deploy:mode:set -f -s s -l skip-compilation -d "Skip regeneration of static content (generated code, preprocessed CSS, assets)"

#
# dev:source-theme:deploy
#
__fish_magento_register_command_option dev:source-theme:deploy -d 'Files to pre-process (file should be specified without extension) (default: "css/styles-m","css/styles-l")'
__fish_magento_register_command_option dev:source-theme:deploy -f -l type -a "(__fish_print_magento_source_theme_file_types)" -d 'Type of source files (default: "less")'
__fish_magento_register_command_option dev:source-theme:deploy -f -l locale -a "(__fish_print_magento_languages)" -d 'Locale (default: "en_US")'
__fish_magento_register_command_option dev:source-theme:deploy -f -l area -a "(__fish_print_magento_theme_areas)" -d 'Area (default: "frontend")'
__fish_magento_register_command_option dev:source-theme:deploy -l theme -d 'Theme [Vendor/theme] (default: "Magento/luma")'

#
# dev:tests:run
#
__fish_magento_register_command_option dev:tests:run -f -a "(__fish_print_magento_available_tests)" -d 'Type of test to run (default: "default")'

#
# dev:urn-catalog:generate
#
__fish_magento_register_command_option dev:urn-catalog:generate -f -l ide -a "(__fish_print_magento_available_ides)" -d 'Catalog generation format. Supported: [phpstorm] (default: "phpstorm")'

#
# dev:xml:convert
#
__fish_magento_register_command_option dev:xml:convert -s o -l overwrite -d 'Overwrite XML file'

#
# i18n:collect-phrases
#
__fish_magento_register_command_option i18n:collect-phrases -s o -l output -d 'Path (including filename) to an output file. With no file specified, defaults to stdout'
__fish_magento_register_command_option i18n:collect-phrases -f -s m -l magento -d 'Use --magento to parse current Magento codebase. Omit parameter if a directory is specified'

#
# i18n:pack
#
__fish_magento_register_command_option i18n:pack -f -s m -l mode -a "(__fish_print_magento_i18n_packing_modes)" -d 'Save mode for dictionary (default: "replace")'
__fish_magento_register_command_option i18n:pack -s d -l allow-duplicates -d 'Use --allow-duplicates to allow saving duplicates of translate. Otherwise omit parameter'

#
# i18n:uninstall
#
__fish_magento_register_command_option i18n:uninstall -f -s b -l backup-code -d 'Take code and configuration files backup (excluding temporary files)'
__fish_magento_register_command_option i18n:uninstall -f -a "(__fish_print_magento_languages)" -d 'Language package name'

#
# info:dependencies:show-framework
#
__fish_magento_register_command_option info:dependencies:show-framework -f -s o -l output -d 'Report filename (default: "framework-dependencies.csv")'

#
# info:dependencies:show-modules
#
__fish_magento_register_command_option info:dependencies:show-modules -f -s o -l output -d 'Report filename (default: "modules-dependencies.csv")'

#
# info:dependencies:show-modules-circular
#
__fish_magento_register_command_option info:dependencies:show-modules-circular -f -s o -l output -d 'Report filename (default: "modules-circular-dependencies.csv")'

#
# maintenance:allow-ips
#
__fish_magento_register_command_option maintenance:allow-ips -l none -d 'Clear allowed IP addresses'

#
# maintenance:disable
#
__fish_magento_register_command_option maintenance:disable -l ip -d "Allowed IP addresses (use 'none' to clear list)"

#
# maintenance:enable
#
__fish_magento_register_command_option maintenance:enable -l ip -d "Allowed IP addresses (use 'none' to clear list)"

#
# module:disable
#
__fish_magento_register_command_option module:disable -f -a "(__fish_print_magento_modules)" -d "Module name"
__fish_magento_register_command_option module:disable -s f -l force -d "Bypass dependencies check"
__fish_magento_register_command_option module:disable -l all -d "Disable all modules"
__fish_magento_register_command_option module:disable -s c -l clear-static-content -d "Clear generated static view files. Necessary if module(s) have static view files"

#
# module:enable
#
__fish_magento_register_command_option module:enable -f -a "(__fish_print_magento_modules)" -d "Module name"
__fish_magento_register_command_option module:enable -f -s f -l force -d "Bypass dependencies check"
__fish_magento_register_command_option module:enable -f -l all -d "Enable all modules"
__fish_magento_register_command_option module:enable -f -s c -l clear-static-content -d "Clear generated static view files. Necessary if module(s) have static view files"

#
# module:status
#
__fish_magento_register_command_option module:status -f -a "(__fish_print_magento_modules)" -d "Module name"
__fish_magento_register_command_option module:status -f -l enabled -d "Print only enabled modules"
__fish_magento_register_command_option module:status -f -l disabled -d "Print only disabled modules"

#
# module:uninstall
#
__fish_magento_register_command_option module:uninstall -f -a "(__fish_print_magento_modules)" -d "Module name"
__fish_magento_register_command_option module:uninstall -f -s r -l remove-data -d "Remove data installed by module(s)"
__fish_magento_register_command_option module:uninstall -f -l backup-code -d "Take code and configuration files backup (excluding temporary files)"
__fish_magento_register_command_option module:uninstall -f -l backup-media -d "Take media backup"
__fish_magento_register_command_option module:uninstall -f -l backup-db -d "Take complete database backup"
__fish_magento_register_command_option module:uninstall -f -s c -l clear-static-content -d "Clear generated static view files. Necessary if module(s) have static view files"

#
# setup:backup
#
__fish_magento_register_command_option setup:backup -f -l code -d "Take code and configuration files backup (excluding temporary files)"
__fish_magento_register_command_option setup:backup -f -l media -d "Take media backup"
__fish_magento_register_command_option setup:backup -f -l db -d "Take complete database backup"

#
# setup:config:set
#
__fish_magento_register_command_option setup:config:set -f -l backend-frontname -d "Backend frontname (will be autogenerated if missing)"
__fish_magento_register_command_option setup:config:set -f -l key -d "Encryption key"
__fish_magento_register_command_option setup:config:set -f -l session-save -d "Session save handler"
__fish_magento_register_command_option setup:config:set -f -l definition-format -d "Type of definitions used by Object Manager"
__fish_magento_register_command_option setup:config:set -f -l db-host -d "Database server host"
__fish_magento_register_command_option setup:config:set -f -l db-name -d "Database name"
__fish_magento_register_command_option setup:config:set -f -l db-user -d "Database server username"
__fish_magento_register_command_option setup:config:set -f -l db-engine -d "Database server engine" -a "(__fish_print_magento_mysql_engines)"
__fish_magento_register_command_option setup:config:set -f -l db-password -d "Database server password"
__fish_magento_register_command_option setup:config:set -f -l db-prefix -d "Database table prefix"
__fish_magento_register_command_option setup:config:set -f -l db-model -d "Database type"
__fish_magento_register_command_option setup:config:set -f -l db-init-statements -d "Database initial set of commands"
__fish_magento_register_command_option setup:config:set -f -l skip-db-validation -s s -d "If specified, then db connection validation will be skipped"
__fish_magento_register_command_option setup:config:set -f -l http-cache-hosts -d "HTTP cache hosts"

#
# setup:install
#
__fish_magento_register_command_option setup:install -n "__fish_magento_parameter_missing admin-user" -f -a --admin-user -d "(Required) Admin user"
__fish_magento_register_command_option setup:install -n "__fish_magento_parameter_missing admin-password" -f -a --admin-password -d "(Required) Admin password"
__fish_magento_register_command_option setup:install -n "__fish_magento_parameter_missing admin-email" -f -a --admin-email -d "(Required) Admin email"
__fish_magento_register_command_option setup:install -n "__fish_magento_parameter_missing admin-firstname" -f -a --admin-firstname -d "(Required) Admin first name"
__fish_magento_register_command_option setup:install -n "__fish_magento_parameter_missing admin-lastname" -f -a --admin-lastname -d "(Required) Admin last name"
__fish_magento_register_command_option setup:install -f -l backend-frontname -d "Backend frontname (will be autogenerated if missing)"
__fish_magento_register_command_option setup:install -f -l key -d "Encryption key"
__fish_magento_register_command_option setup:install -f -l session-save -d "Session save handler"
__fish_magento_register_command_option setup:install -f -l definition-format -d "Type of definitions used by Object Manager"
__fish_magento_register_command_option setup:install -f -l db-host -d "Database server host"
__fish_magento_register_command_option setup:install -f -l db-name -d "Database name"
__fish_magento_register_command_option setup:install -f -l db-user -d "Database server username"
__fish_magento_register_command_option setup:install -f -l db-engine -d "Database server engine" -a "(__fish_print_magento_mysql_engines)"
__fish_magento_register_command_option setup:install -f -l db-password -d "Database server password"
__fish_magento_register_command_option setup:install -f -l db-prefix -d "Database table prefix"
__fish_magento_register_command_option setup:install -f -l db-model -d "Database type"
__fish_magento_register_command_option setup:install -f -l db-init-statements -d "Database initial set of commands"
__fish_magento_register_command_option setup:install -f -l skip-db-validation -s s -d "Skip database connection validation"
__fish_magento_register_command_option setup:install -f -l http-cache-hosts -d "HTTP cache hosts"
__fish_magento_register_command_option setup:install -f -l base-url -a "(__fish_print_magento_url_protocols)" -d "URL the store is supposed to be available at"
__fish_magento_register_command_option setup:install -f -l language -a "(__fish_print_magento_languages)" -d "Default language code"
__fish_magento_register_command_option setup:install -f -l timezone -d "Default time zone code"
__fish_magento_register_command_option setup:install -f -l currency -d "Default currency code"
__fish_magento_register_command_option setup:install -f -l use-rewrites -d "Use rewrites"
__fish_magento_register_command_option setup:install -f -l use-secure -d "Use secure URLs. Enable only if SSL is available"
__fish_magento_register_command_option setup:install -f -l base-url-secure -a "https://" -d "Base URL for SSL connection"
__fish_magento_register_command_option setup:install -f -l use-secure-admin -d "Run admin interface with SSL"
__fish_magento_register_command_option setup:install -f -l admin-use-security-key -d 'Use security key in admin urls/forms'
__fish_magento_register_command_option setup:install -f -r -l admin-user -d "(Required) Admin user"
__fish_magento_register_command_option setup:install -f -r -l admin-password -d "(Required) Admin password"
__fish_magento_register_command_option setup:install -f -r -l admin-email -d "(Required) Admin email"
__fish_magento_register_command_option setup:install -f -r -l admin-firstname -d "(Required) Admin first name"
__fish_magento_register_command_option setup:install -f -r -l admin-lastname -d "(Required) Admin last name"
__fish_magento_register_command_option setup:install -f -l cleanup-database -d "Cleanup database before installation"
__fish_magento_register_command_option setup:install -f -l sales-order-increment-prefix -d "Sales order number prefix"
__fish_magento_register_command_option setup:install -f -l use-sample-data -d "Use sample data"

#
# setup:performance:generate-fixtures
#
__fish_magento_register_command_option setup:performance:generate-fixtures -f -s s -l skip-reindex -d "Skip reindex"

#
# setup:rollback
#
__fish_magento_register_command_option setup:rollback -s c -l code-file -d "Basename of the code backup file in var/backups"
__fish_magento_register_command_option setup:rollback -s m -l media-file -d "Basename of the media backup file in var/backups"
__fish_magento_register_command_option setup:rollback -s d -l db-file -d "Basename of the db backup file in var/backups"

#
# setup:static-content:deploy
#
__fish_magento_register_command_option setup:static-content:deploy -x -a "(__fish_print_magento_languages)" -d "Space-separated list of ISO-636 language codes to output static view files for"
__fish_magento_register_command_option setup:static-content:deploy -f -s d -l dry-run -d "Simulate deployment only"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-javascript -d "Don't deploy JavaScript files"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-css -d "Don't deploy CSS files"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-less -d "Don't deploy LESS files"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-images -d "Don't deploy images"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-fonts -d "Don't deploy font files"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-html -d "Don't deploy HTML files"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-misc -d "Don't deploy other types of files (.md, .jbf, .csv, etc...)"
__fish_magento_register_command_option setup:static-content:deploy -f -l no-html-minify -d "Don't minify HTML files"
__fish_magento_register_command_option setup:static-content:deploy -s t -l theme -d 'Generate static view files only for specified themes (default: "all")'
__fish_magento_register_command_option setup:static-content:deploy -l exclude-theme -d 'Do not generate files for specified themes (default: "none")'
__fish_magento_register_command_option setup:static-content:deploy -f -s l -l language -a "(__fish_print_magento_languages)" -d 'Generate files only for specified languages (default: "all")'
__fish_magento_register_command_option setup:static-content:deploy -f -l exclude-language -d 'Do not generate files for specified languages. (default: "none") (multiple values allowed)'
__fish_magento_register_command_option setup:static-content:deploy -f -s a -l area -a "(__fish_print_magento_theme_areas)" -d 'Generate files only for specified areas (default: "all")'
__fish_magento_register_command_option setup:static-content:deploy -f -l exclude-area -a "(__fish_print_magento_theme_areas)" -d 'Do not generate files for specified areas (default: "none")'
__fish_magento_register_command_option setup:static-content:deploy -x -s j -l jobs -d "Enable parallel processing using specified number of jobs (default: 4)"
__fish_magento_register_command_option setup:static-content:deploy -f -l symlink-locale -d "Create symlinks for files of locales which are passed for deployment but have no customizations"

#
# setup:store-config:set
#
__fish_magento_register_command_option setup:store-config:set -f -l base-url -a "(__fish_print_magento_url_protocols)" -d "URL the store is supposed to be available at"
__fish_magento_register_command_option setup:store-config:set -f -l language -a "(__fish_print_magento_languages)" -d "Default language code"
__fish_magento_register_command_option setup:store-config:set -f -l timezone -d "Default time zone code"
__fish_magento_register_command_option setup:store-config:set -f -l currency -d "Default currency code"
__fish_magento_register_command_option setup:store-config:set -f -l use-rewrites -d "Use rewrites"
__fish_magento_register_command_option setup:store-config:set -f -l use-secure -d "Use secure URLs. Only enable if SSL is available"
__fish_magento_register_command_option setup:store-config:set -f -l base-url-secure -a "https://" -d "Base URL for SSL connection"
__fish_magento_register_command_option setup:store-config:set -f -l use-secure-admin -d "Run admin interface with SSL"
__fish_magento_register_command_option setup:store-config:set -f -l admin-use-security-key -d 'Use security key in admin urls/forms'

#
# setup:upgrade
#
__fish_magento_register_command_option setup:upgrade -l keep-generated -d "Prevents generated files from being deleted"

#
# theme:uninstall
#
__fish_magento_register_command_option theme:uninstall -f -l backup-code -d "Take code backup (excluding temporary files)"
__fish_magento_register_command_option theme:uninstall -f -s c -l clear-static-content -d "Clear generated static view files"
