 #RUN: %fish %s

fish_config theme show non-existant-theme >/dev/null

fish_config prompt show non-existant-prompt >/dev/null

# TODO This shouldn't print terminal escape sequences when stdout is not a TTY..
fish_config prompt show default
# CHECK: {{\x1b\[4m}}default{{\x1b\[m}}
# CHECK: {{.*}}@{{.*}}>{{.*}}

fish_config theme show "fish default"
# CHECK: {{\x1b\[m}}{{\x1b}}[4mCurrent{{\x1b\[m}}
# CHECK: /bright/vixens{{\x1b\[m}} jump{{\x1b\[m}} |{{\x1b\[m}} "fowl"{{\x1b\[m}} > quack{{\x1b\[m}} &{{\x1b\[m}} # This is a comment
# CHECK: {{\x1b}}[mecho{{\x1b\[m}} 'Errors are the portal to discovery
# CHECK: {{\x1b}}[mTh{{\x1b}}[mis an autosuggestion

fish_config theme show
# CHECK: {{\x1b\[m}}{{\x1b}}[4mCurrent{{\x1b\[m}}
# CHECK: /bright/vixens{{\x1b\[m}} jump{{\x1b\[m}} |{{\x1b\[m}} "fowl"{{\x1b\[m}} > quack{{\x1b\[m}} &{{\x1b\[m}} # This is a comment
# CHECK: {{\x1b}}[mecho{{\x1b\[m}} 'Errors are the portal to discovery
# CHECK: {{\x1b}}[mTh{{\x1b}}[mis an autosuggestion
