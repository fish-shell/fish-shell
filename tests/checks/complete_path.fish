#RUN: %fish --interactive %s

mkdir -p __fish_complete_path
cd __fish_complete_path
touch foo
touch -- --bar

__fish_complete_path --bar
# CHECK: --bar{{\t}}

__fish_complete_path --bar=

touch -- --bar=baz
__fish_complete_path --bar=
# CHECK: --bar=baz{{\t}}

echo done
# CHECK: done
