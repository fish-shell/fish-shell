# RUN: %fish %s

echo $fish_bind_mode
# CHECK: default

fish_vi_key_bindings
echo $fish_bind_mode
# CHECK: insert

fish_vi_key_bindings default
echo $fish_bind_mode
# CHECK: default

fish_vi_key_bindings insert
echo $fish_bind_mode
# CHECK: insert
