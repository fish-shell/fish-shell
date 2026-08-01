#RUN: %fish %s

touch foo
touch -- --bar

__fish_complete_path --bar
# CHECK: --bar{{\t}}

__fish_complete_path --bar=

touch -- --bar=baz
__fish_complete_path --bar=
# CHECK: --bar=baz{{\t}}
