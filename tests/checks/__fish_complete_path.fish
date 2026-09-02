#RUN: %fish %s

mkdir empty
cd empty

touch foo
touch -- --bar

__fish_complete_path --bar
# CHECK: --bar{{\t}}

__fish_complete_path --bar=

touch -- --bar=baz
__fish_complete_path --bar=
# CHECK: --bar=foo{{\t}}
# CHECK: --bar=--bar{{\t}}
# CHECK: --bar=foo{{\t}}
# CHECK: --bar=--bar{{\t}}
# CHECK: --bar=--bar=baz{{\t}}

__fish_complete_path '--bar\='
# CHECK: --bar=baz{{\t}}
