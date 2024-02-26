set -l subcommands apk files manifest dex resources

set -l apk_subcommands summary file-size download-size features compare
set -l files_subcommands list cat
set -l manifest_subcommands print application-id version-name version-code min-sdk target-sdk permissions debuggable
set -l dex_subcommands list references packages code
set -l resources_subcommands package configs value name xml

complete -f -n "not __fish_seen_subcommand_from $subcommands" -c apkanalyzer -a apk -d 'Analyze APK file attributes'
complete -f -n "not __fish_seen_subcommand_from $subcommands" -c apkanalyzer -a files -d 'Analyze the files inside the APK file'
complete -f -n "not __fish_seen_subcommand_from $subcommands" -c apkanalyzer -a manifest -d 'Analyze the contents of the manifest file'
complete -f -n "not __fish_seen_subcommand_from $subcommands" -c apkanalyzer -a dex -d 'Analyze the DEX files inside the APK file'
complete -f -n "not __fish_seen_subcommand_from $subcommands" -c apkanalyzer -a resources -d 'View text, image and string resources'

# global-option
complete -n "not __fish_seen_subcommand_from $apk_subcommands $files_subcommands $manifest_subcommands $dex_subcommands $resources_subcommands" -c apkanalyzer -s h -l human-readable -d 'Human-readable output'

# apk
complete -f -n "__fish_seen_subcommand_from apk; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a summary -d 'Prints the application ID, version code, and version name'
complete -f -n "__fish_seen_subcommand_from apk; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a file-size -d 'Prints the total file size of the APK'
complete -f -n "__fish_seen_subcommand_from apk; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a download-size -d 'Prints an estimate of the download size of the APK'
complete -f -n "__fish_seen_subcommand_from apk; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a features -d 'Prints features used by the APK that trigger Play Store filtering'
complete -f -n "__fish_seen_subcommand_from apk; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a compare -d 'Compares the sizes of apk-file and apk-file'
# apk options
complete -n '__fish_seen_subcommand_from apk; and __fish_seen_subcommand_from features' -c apkanalyzer -l not-required -d 'Include features marked as not required in the output'
complete -n '__fish_seen_subcommand_from apk; and __fish_seen_subcommand_from compare' -c apkanalyzer -l different-only -d 'Prints directories and files with differences'
complete -n '__fish_seen_subcommand_from apk; and __fish_seen_subcommand_from compare' -c apkanalyzer -l files-only -d 'Does not print directory entries'
complete -n '__fish_seen_subcommand_from apk; and __fish_seen_subcommand_from compare' -c apkanalyzer -l patch-size -d 'Shows an estimate of the file-by-file patch instead of a raw difference'

complete -n "__fish_seen_subcommand_from apk; and __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -ka '(__fish_complete_suffix .apk)'

# files 
complete -f -n "__fish_seen_subcommand_from files; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a list -d 'Lists all files in the APK'
complete -f -n "__fish_seen_subcommand_from files; and not __fish_seen_subcommand_from $apk_subcommands" -c apkanalyzer -a cat -d 'Prints out the file contents'
# files options
complete -n '__fish_seen_subcommand_from files; and __fish_seen_subcommand_from list' -c apkanalyzer -l file -d 'Specify a path inside the APK' -r

complete -n "__fish_seen_subcommand_from files; and __fish_seen_subcommand_from $files_subcommands" -c apkanalyzer -ka '(__fish_complete_suffix .apk)'

# manifest
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a print -d 'Prints the APK manifest in XML format'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a application-id -d 'Prints the application ID value'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a version-name -d 'Prints the version name value'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a version-code -d 'Prints the version code value'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a min-sdk -d 'Prints the minimum SDK version'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a target-sdk -d 'Prints the target SDK version'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a permissions -d 'Prints the list of permissions'
complete -f -n "__fish_seen_subcommand_from manifest; and not __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -a debuggable -d 'Prints whether the APK is debuggable'

complete -n "__fish_seen_subcommand_from manifest; and __fish_seen_subcommand_from $manifest_subcommands" -c apkanalyzer -ka '(__fish_complete_suffix .apk)'

# dex
complete -f -n "__fish_seen_subcommand_from dex; and not __fish_seen_subcommand_from $dex_subcommands" -c apkanalyzer -a list -d 'Prints a list of the DEX files in the APK'
complete -f -n "__fish_seen_subcommand_from dex; and not __fish_seen_subcommand_from $dex_subcommands" -c apkanalyzer -a references -d 'Prints the number of method references in the specified DEX files'
complete -f -n "__fish_seen_subcommand_from dex; and not __fish_seen_subcommand_from $dex_subcommands" -c apkanalyzer -a packages -d 'Prints the class tree from DEX'
complete -f -n "__fish_seen_subcommand_from dex; and not __fish_seen_subcommand_from $dex_subcommands" -c apkanalyzer -a code -d 'Prints the bytecode of a class or method in smali format'
# dex options
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from references' -c apkanalyzer -l files -d 'Indicate specific files that you want to include' -r
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l defined-only -d 'Includes only classes defined in the APK in the output'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l files -d 'Specifies the DEX file names to include'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l proguard-folder -d 'Specifies the Proguard output folder to search for mappings'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l proguard-mapping -d 'Specifies the Proguard mapping file'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l proguard-seeds -d 'Specifies the Proguard seeds file'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l proguard-usage -d 'Specifies the Proguard usage file'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from packages' -c apkanalyzer -l show-removed -d 'Shows classes and members that were removed by Proguard'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from code' -c apkanalyzer -l class -d 'Specifies the class name to print'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from code' -c apkanalyzer -l method -d 'Specifies the method name to print'

complete -n "__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from $dex_subcommands" -c apkanalyzer -ka '(__fish_complete_suffix .apk)'
complete -n '__fish_seen_subcommand_from dex; and __fish_seen_subcommand_from code' -c apkanalyzer -ka '(__fish_complete_suffix .class)'

# resources 
complete -f -n "__fish_seen_subcommand_from resources; and not __fish_seen_subcommand_from $resources_subcommands" -c apkanalyzer -a packages -d 'Prints a list of the packages that are defined in the resources table'
complete -f -n "__fish_seen_subcommand_from resources; and not __fish_seen_subcommand_from $resources_subcommands" -c apkanalyzer -a configs -d 'Prints a list of configurations for the specified type'
complete -f -n "__fish_seen_subcommand_from resources; and not __fish_seen_subcommand_from $resources_subcommands" -c apkanalyzer -a value -d 'Prints the value of the resource specified by config, name, and type'
complete -f -n "__fish_seen_subcommand_from resources; and not __fish_seen_subcommand_from $resources_subcommands" -c apkanalyzer -a names -d 'Prints a list of resource names for a configuration and type'
complete -f -n "__fish_seen_subcommand_from resources; and not __fish_seen_subcommand_from $resources_subcommands" -c apkanalyzer -a xml -d 'Prints the human-readable form of a binary XML file'
# resources options
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from configs' -c apkanalyzer -l type -d 'Specifies the resource type to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from configs' -c apkanalyzer -l packages -d 'Specifies the packages to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from value' -c apkanalyzer -l config -d 'Specifies the configuration to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from value' -c apkanalyzer -l name -d 'Specifies the resource name to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from value' -c apkanalyzer -l type -d 'Specifies the resource type to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from value' -c apkanalyzer -l packages -d 'Specifies the packages to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from names' -c apkanalyzer -l config -d 'Specifies the configuration to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from names' -c apkanalyzer -l type -d 'Specifies the resource type to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from names' -c apkanalyzer -l packages -d 'Specifies the packages to print' -r
complete -n '__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from xml' -c apkanalyzer -l file -d 'Specifies the file to print' -r

complete -n "__fish_seen_subcommand_from resources; and __fish_seen_subcommand_from $resources_subcommands" -c apkanalyzer -ka '(__fish_complete_suffix .apk)'
