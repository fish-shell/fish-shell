# RUN: %fish %s
# REQUIRES: command -v diff
# REQUIRES: test -z "$CI"

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
# CHECKERR: Searched {{/\S* (/\S*|and `status list-files themes`)}}

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
'^(?:ayu-dark|base16-default-light|coolbeans|default|none|'\
'tomorrow-night-bright|tomorrow-night|tomorrow)$'
# CHECK: ayu-dark
# CHECK: base16-default-light
# CHECK: coolbeans
# CHECK: default
# CHECK: none
# CHECK: tomorrow-night-bright
# CHECK: tomorrow-night
# CHECK: tomorrow

fish_config theme show "default"
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}Current{{\x1b\[m}}
# CHECK: /bright/vixens{{\x1b\[m}} jump{{\x1b\[m}} |{{\x1b\[m}} "fowl"{{\x1b\[m}} > quack{{\x1b\[m}} &{{\x1b\[m}} # This is a comment
# CHECK: {{\x1b\[m}}echo{{\x1b\[m}} 'Errors are the portal to discovery
# CHECK: {{\x1b\[m}}Th{{\x1b\[m}}is an autosuggestion
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}default{{\x1b\[m}}
# CHECK: {{\x1b\[m}}/bright/vixens{{\x1b\[m}} {{\x1b\[36m}}jump{{\x1b\[m}} {{\x1b\[36m}}{{\x1b\[1m}}|{{\x1b\[m}} {{\x1b\[33m}}"fowl"{{\x1b\[m}} {{\x1b\[36m}}{{\x1b\[1m}}> quack{{\x1b\[m}} {{\x1b\[32m}}&{{\x1b\[m}}{{\x1b\[31m}} # This is a comment
# CHECK: {{\x1b\[m}}{{\x1b\[m}}echo{{\x1b\[m}} {{\x1b\[91m}}'{{\x1b\[33m}}Errors are the portal to discovery
# CHECK: {{\x1b\[m}}{{\x1b\[m}}Th{{\x1b\[m}}{{\x1b\[90m}}is an autosuggestion

fish_config theme show ayu-dark ayu-light | string match -r '^.*ayu.*'
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}ayu-dark{{\x1b\[m}}
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}ayu-light{{\x1b\[m}}

mkdir $__fish_config_dir/themes
touch $__fish_config_dir/themes/custom-from-userconf.theme
fish_config theme show | grep -E '[^-]default|base16-default-dark|custom-from-userconf'
# CHECK: {{.*}}default{{\x1b\[m}}
# CHECK: {{.*}}custom-from-userconf{{\x1b\[m}}
# CHECK: {{.*}}base16-default-dark{{\x1b\[m}}
# CHECK: {{.*}}default-dark{{\x1b\[m}}
# CHECK: {{.*}}default-light{{\x1b\[m}}

# Override a default theme with different colors.
__fish_data_with_file themes/none.theme \
    cat >$__fish_config_dir/themes/default.theme
fish_config theme show | grep -E '[^-]default|base16-default-dark' -A1
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}default{{\x1b\[m}}
# CHECK: {{\x1b\[m}}/bright/vixens{{\x1b\[m}} {{\x1b\[m}}jump{{\x1b\[m}}{{.*}}
# CHECK: --
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}base16-default-dark{{\x1b\[m}}
# CHECK: {{.*}}/bright/vixens{{.*}}
# CHECK: --
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}default-dark{{\x1b\[m}}
# CHECK: {{.*}}/bright/vixens{{.*}}
# CHECK: --
# CHECK: {{\x1b\[m}}{{\x1b\[4m}}default-light{{\x1b\[m}}
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

    fish_config theme choose 'default'
    print-sample-colors
    # CHECK: normal=normal
    # CHECK: autosuggestion=brblack

    set -S fish_color_normal
    # CHECK: $fish_color_normal: set in global scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |normal|
    # CHECK: $fish_color_normal: set in universal scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |yellow|

    echo yes | fish_config theme save 'default'
    set -S fish_color_normal
    # CHECK: $fish_color_normal: set in universal scope, unexported, with 1 elements
    # CHECK: $fish_color_normal[1]: |normal|
}

fish_config theme dump badarg
# CHECKERR: Too many arguments
