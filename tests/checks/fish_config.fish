 #RUN: %fish %s

# This still demos the current theme.
fish_config theme show non-existent-theme
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Current{{\x1b\[m}}
# CHECK: /bright/vixens{{\x1b\[m}} jump{{\x1b\[m}} |{{\x1b\[m}} "fowl"{{\x1b\[m}} > quack{{\x1b\[m}} &{{\x1b\[m}} # This is a comment
# CHECK: {{\x1b\[m}}echo{{\x1b\[m}} 'Errors are the portal to discovery
# CHECK: {{\x1b\[m}}Th{{\x1b\[m}}is an autosuggestion

fish_config prompt show non-existent-prompt

fish_config prompt show default
# CHECK: {{\x1b\[4m}}default{{\x1b\[m}}
# CHECK: {{.*}}@{{.*}}>{{.*}}

fish_config theme show "fish default"
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Current{{\x1b\[m}}
# CHECK: /bright/vixens{{\x1b\[m}} jump{{\x1b\[m}} |{{\x1b\[m}} "fowl"{{\x1b\[m}} > quack{{\x1b\[m}} &{{\x1b\[m}} # This is a comment
# CHECK: {{\x1b\[m}}echo{{\x1b\[m}} 'Errors are the portal to discovery
# CHECK: {{\x1b\[m}}Th{{\x1b\[m}}is an autosuggestion
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}fish default{{\x1b\[m}}
# CHECK: {{\x1b\[m}}/bright/vixens{{\x1b\[m}} {{\x1b\[36m}}jump{{\x1b\[m}} {{\x1b\[36m}}{{\x1b\[1m}}|{{\x1b\[m}} {{\x1b\[33m}}"fowl"{{\x1b\[m}} {{\x1b\[36m}}{{\x1b\[1m}}> quack{{\x1b\[m}} {{\x1b\[32m}}&{{\x1b\[m}}{{\x1b\[31m}} # This is a comment
# CHECK: {{\x1b\[m}}{{\x1b\[m}}echo{{\x1b\[m}} {{\x1b\[91m}}'{{\x1b\[33m}}Errors are the portal to discovery
# CHECK: {{\x1b\[m}}{{\x1b\[m}}Th{{\x1b\[m}}{{\x1b\[90m}}is an autosuggestion

mkdir $__fish_config_dir/themes
touch $__fish_config_dir/themes/custom-from-userconf.theme
fish_config theme show | grep -E 'fish default|Default Dark|custom-from-userconf'
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}custom-from-userconf{{\x1b\[m}}
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Base16 Default Dark{{\x1b\[m}}
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}fish default{{\x1b\[m}}

# Override a default theme with different colors.
{
    status get-file tools/web_config/themes/None.theme 2>/dev/null ||
    cat $__fish_data_dir/tools/web_config/themes/None.theme
} >$__fish_config_dir/themes/"fish default.theme"
fish_config theme show | grep -E 'fish default|Base16 Default Dark' -A1
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}fish default{{\x1b\[m}}
# CHECK: {{\x1b\[m}}/bright/vixens{{\x1b\[m}} {{\x1b\[m}}jump{{\x1b\[m}}{{.*}}
# CHECK: --
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Base16 Default Dark{{\x1b\[m}}
# CHECK: {{.*}}/bright/vixens{{.*}}
