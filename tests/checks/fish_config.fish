 #RUN: %fish %s
 #REQUIRES: command -v diff

fish_config prompt list | string match -r '^(?:acidhub|disco|nim)$'
# CHECK: acidhub
# CHECK: disco
# CHECK: nim

diff \
    (fish_config prompt list | psub -s config-prompt-list) \
    (fish_config prompt | psub -s config-prompt)

fish_config prompt show non-existent-prompt

fish_config prompt show default
# CHECK: {{\x1b\[4m}}default{{\x1b\[m}}
# CHECK: {{.*}}@{{.*}}>{{.*}}

type fish_mode_prompt
# CHECK: fish_mode_prompt is a function with definition
# CHECK: # Defined in {{.*}}functions/fish_mode_prompt.fish @ line 2
# CHECK: function fish_mode_prompt --description 'Displays the current mode'
# CHECK:     # {{.*}}
# CHECK:     fish_default_mode_prompt
# CHECK: end

function set-all-the-prompts
    function fish_prompt
        echo left-prompt
    end
    function fish_right_prompt
        echo right-prompt
    end
    function fish_mode_prompt
        echo mode-prompt
    end
end

set-all-the-prompts
echo yes | fish_config prompt save >/dev/null
grep '\S' $__fish_config_dir/functions/{fish_prompt,fish_right_prompt,fish_mode_prompt}.fish
# CHECK: {{.*}}/functions/fish_prompt.fish:function fish_prompt
# CHECK: {{.*}}/functions/fish_prompt.fish:        echo left-prompt
# CHECK: {{.*}}/functions/fish_prompt.fish:end
# CHECK: {{.*}}/functions/fish_right_prompt.fish:function fish_right_prompt
# CHECK: {{.*}}/functions/fish_right_prompt.fish:        echo right-prompt
# CHECK: {{.*}}/functions/fish_right_prompt.fish:end
# CHECK: {{.*}}/functions/fish_mode_prompt.fish:function fish_mode_prompt
# CHECK: {{.*}}/functions/fish_mode_prompt.fish:        echo mode-prompt
# CHECK: {{.*}}/functions/fish_mode_prompt.fish:end

echo yes | fish_config prompt save nim >/dev/null
grep -q nim@Hattori $__fish_config_dir/functions/fish_prompt.fish ||
echo 'failed to save prompt?'
not path is $__fish_config_dir/functions/fish_right_prompt.fish
or echo "fish_right_prompt.fish ought to be deleted"
cat $__fish_config_dir/functions/fish_mode_prompt.fish
# CHECK: function fish_mode_prompt
# CHECK: end

fish_config prompt choose nim
type fish_prompt fish_right_prompt fish_mode_prompt |
    grep -EA1 '^function.*|.*\[nim@Hattori:~\].*'
# CHECK: function fish_prompt
# CHECK: # This prompt shows:
# CHECK: --
# CHECK: # ┬─[nim@Hattori:~]─[11:39:00]
# CHECK: # ╰─>$ echo here
# CHECK: --
# CHECKERR: type: Could not find 'fish_right_prompt'
# CHECK: function fish_mode_prompt
# CHECK:

fish_config prompt choose disco
type fish_prompt fish_right_prompt fish_mode_prompt |
grep -EA1 '^function.*|.*cksum$'
# CHECK: function fish_prompt
# CHECK: set -l last_status $status
# CHECK: --
# CHECK: if command -sq cksum
# CHECK:     # randomized cwd color
# CHECK: --
# CHECK: function fish_right_prompt
# CHECK: set -g __fish_git_prompt_showdirtystate 1
# CHECK: --
# CHECK: function fish_mode_prompt {{.*}}
# CHECK:     # {{.*}}

fish_config prompt choose default
type fish_prompt fish_right_prompt fish_mode_prompt |
    grep '^function' -A1
# CHECK: function fish_prompt --description 'Write out the prompt'
# CHECK: set -l last_pipestatus $pipestatus
# CHECKERR: type: Could not find 'fish_right_prompt'
# CHECK: --
# CHECK: function fish_mode_prompt {{.*}}
# CHECK:     # {{.*}}

type fish_mode_prompt
# CHECK: fish_mode_prompt is a function with definition
# CHECK: # Defined {{in .*/functions/fish_mode_prompt.fish @ line 2|via `source`}}
# CHECK: function fish_mode_prompt --description 'Displays the current mode'
# CHECK:     # {{.*}}
# CHECK:     fish_default_mode_prompt
# CHECK: end

fish_config theme choose non-existent-theme1
# CHECKERR: No such theme: non-existent-theme1
# CHECKERR: Searched {{/\S* (/\S*|and `status list-files tools/web_config/themes`)}}

# This still demos the current theme.
fish_config theme show non-existent-theme2
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Current{{\x1b\[m}}
# CHECK: /bright/vixens{{\x1b\[m}} jump{{\x1b\[m}} |{{\x1b\[m}} "fowl"{{\x1b\[m}} > quack{{\x1b\[m}} &{{\x1b\[m}} # This is a comment
# CHECK: {{\x1b\[m}}echo{{\x1b\[m}} 'Errors are the portal to discovery
# CHECK: {{\x1b\[m}}Th{{\x1b\[m}}is an autosuggestion

diff \
    (fish_config theme list | psub -s config-theme-list) \
    (fish_config theme | psub -s config-theme)

fish_config theme list | string match -r \
'^(?:ayu Dark|Base16 Default Light|coolbeans|fish default|None|'\
'Tomorrow Night Bright|Tomorrow Night|Tomorrow)$'
# CHECK: ayu Dark
# CHECK: Base16 Default Light
# CHECK: coolbeans
# CHECK: fish default
# CHECK: None
# CHECK: Tomorrow Night Bright
# CHECK: Tomorrow Night
# CHECK: Tomorrow

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
# CHECK: {{.*}}custom-from-userconf{{\x1b\[m}}
# CHECK: {{.*}}Base16 Default Dark{{\x1b\[m}}
# CHECK: {{.*}}fish default{{\x1b\[m}}

# Override a default theme with different colors.
__fish_data_with_file tools/web_config/themes/None.theme \
    cat >$__fish_config_dir/themes/"fish default.theme"
fish_config theme show | grep -E 'fish default|Base16 Default Dark' -A1
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}fish default{{\x1b\[m}}
# CHECK: {{\x1b\[m}}/bright/vixens{{\x1b\[m}} {{\x1b\[m}}jump{{\x1b\[m}}{{.*}}
# CHECK: --
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Base16 Default Dark{{\x1b\[m}}
# CHECK: {{.*}}/bright/vixens{{.*}}

function print-sample-colors
    echo "normal=$fish_color_normal"
    echo "autosuggestion=$fish_color_autosuggestion"
end
echo >$__fish_config_dir/themes/custom-from-userconf.theme \
"fish_color_normal yellow"

{
    # Since we're noninteractive, we have not loaded a theme yet.
    print-sample-colors
    # CHECK: normal=
    # CHECK: autosuggestion=

    fish_config theme choose custom-from-userconf
    print-sample-colors
    # CHECK: normal=yellow
    # CHECK: autosuggestion=
    set -S fish_color_normal
    # CHECK: $fish_color_normal: set in global scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |yellow|

    echo yes | fish_config theme save
    set -S fish_color_normal
    # CHECK: $fish_color_normal: set in universal scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |yellow|

    fish_config theme choose 'fish default'
    print-sample-colors
    # CHECK: normal=normal
    # CHECK: autosuggestion=brblack

    set -S fish_color_normal
    # CHECK: $fish_color_normal: set in global scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |normal|
    # CHECK: $fish_color_normal: set in universal scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |yellow|

    echo yes | fish_config theme save 'fish default'
    set -S fish_color_normal
    # CHECK: $fish_color_normal: set in universal scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |normal|
}
