# RUN: fish=%fish %fish %s

function provoke-migration
    set -U __fish_initialized 3800
    rm -f \
        $__fish_config_dir/conf.d/fish_frozen_theme.fish \
        $__fish_config_dir/conf.d/fish_frozen_key_bindings.fish
end

function fake-old-uvars
    for varname in (__fish_theme_variables --universal)
        set $varname (string match -rv -- '--theme=.*' $$varname)
    end
end

set -U
echo no default universal variables
# CHECK: no default universal variables

# Existing universal variables are migrated as-is.
{
    echo yes | fish_config theme save default --color-theme=light
    fake-old-uvars
    set -U fish_key_bindings fish_vi_key_bindings
    set -U | head -2
    # CHECK: fish_color_autosuggestion 7f7f7f
    # CHECK: fish_color_cancel 'ffffff' '--background=000000'
    not path is $__fish_config_dir/conf.d/fish_frozen_theme.fish
    and echo ok
    # CHECK: ok

    provoke-migration
    $fish -c __fish_theme_migrate
    # CHECK: {{\x1b\[1m}}fish:{{\x1b\[m}} {{upgraded.*}}
    # CHECK: {{.*Color.*no.longer.*universal.*}}
    # CHECK: Migrated {{.*}} {{\S*}}/xdg_config_home/fish/conf.d/fish_frozen_theme.fish{{\x1b\[m}}
    # CHECK: {{.*restart.*}}
    # CHECK: {{.*fish_key_bindings.*no.longer.*universal.*}}
    # CHECK: Migrated {{.*}} {{\S*}}/xdg_config_home/fish/conf.d/fish_frozen_key_bindings.fish{{\x1b\[m}}
    # CHECK: {{.*help relnotes.*}}
    grep -v '^#' $__fish_config_dir/conf.d/fish_frozen_theme.fish | grep . | head -3
    # CHECK: set --global fish_color_autosuggestion 7f7f7f
    # CHECK: set --global fish_color_cancel ffffff --background=000000
    # CHECK: set --global fish_color_command 0000ee
    grep -v '^#' $__fish_config_dir/conf.d/fish_frozen_key_bindings.fish
    # CHECK: set --global fish_key_bindings fish_vi_key_bindings
    # CHECK: set --erase --universal fish_key_bindings

    # But the migration is only done once, in case the user really wants these as universals.
    set -U fish_color_autosuggestion 8e8e8e
    $fish -c '
        __fish_theme_migrate
        set -eg fish_color_autosuggestion
        echo $fish_color_autosuggestion
        # CHECK: 8e8e8e
    '
    set -Ue fish_color_autosuggestion
}

# If existing universal colors match old defaults exactly (common case), don't migrate but
# delete them.
{
    echo yes | fish_config theme save default --color-theme=unknown
    fake-old-uvars
    provoke-migration
    $fish -c __fish_theme_migrate
    # CHECK: {{\x1b\[1m}}fish:{{\x1b\[m}} {{upgraded.*}}
    # CHECK: {{.*Color.*no.longer.*universal.*}}
    # CHECK: {{.*restart.*}}
    # CHECK: {{.*help relnotes.*}}
    not path is $__fish_config_dir/conf.d/fish_frozen_theme.fish
    and echo ok
    # CHECK: ok
    set -U
    # CHECK: __fish_initialized 4300
}

# Labeled color variables may be updated by fish.
{
    $fish -c '
        set -g fish_color_autosuggestion red
        set -g fish_color_command green --theme=default
        __fish_theme_migrate
        for cmd in "" "__fish_color_theme=unknown __fish_apply_theme"
            eval $cmd
            echo fish_color_autosuggestion $fish_color_autosuggestion
            echo fish_color_command $fish_color_command
        end
        # CHECK: fish_color_autosuggestion red
        # CHECK: fish_color_command green --theme=default
        # CHECK: fish_color_autosuggestion red
        # CHECK: fish_color_command normal --theme=default
    '
}

# If existing universal key bindings match old defaults exactly (common case), don't migrate
# but delete them).
{
    set -U fish_key_bindings fish_vi_key_bindings
    provoke-migration
    $fish -c __fish_theme_migrate
    # CHECK: {{\x1b\[1m}}fish:{{\x1b\[m}} {{upgraded.*}}
    # CHECK: {{.*Color.*no.longer.*universal.*}}
    # CHECK: {{.*restart.*}}
    # CHECK: {{.*fish_key_bindings.*no.longer.*universal.*}}
    # CHECK: Migrated {{.*}} {{\S*}}/xdg_config_home/fish/conf.d/fish_frozen_key_bindings.fish{{\x1b\[m}}
    # CHECK: {{.*help relnotes.*}}
    path is $__fish_config_dir/conf.d/fish_frozen_key_bindings.fish
    and echo ok
    # CHECK: ok
    set -U
    # CHECK: __fish_initialized 4300
}
