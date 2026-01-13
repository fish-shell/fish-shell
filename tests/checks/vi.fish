# RUN: %fish --interactive %s

fish_vi_key_bindings

commandline '1'; commandline --cursor 0; fish_vi_dec
# CHECK: {{\x1b\[6 q}}
echo
commandline --current-buffer
# CHECK: 0

commandline '0'; commandline --cursor 0; fish_vi_dec
commandline --current-buffer
# CHECK: -1

commandline -- '-1'; commandline --cursor 0; fish_vi_dec
commandline --current-buffer
# CHECK: -2

commandline -- '-1'; commandline --cursor 0; fish_vi_inc
commandline --current-buffer
# CHECK: 0

commandline '0'; commandline --cursor 0; fish_vi_inc
commandline --current-buffer
# CHECK: 1

commandline '123'; commandline --cursor 0; fish_vi_inc
commandline --current-buffer
# CHECK: 124

commandline '123'; commandline --cursor 0; fish_vi_dec
commandline --current-buffer
# CHECK: 122

commandline '123'; commandline --cursor 1; fish_vi_inc
commandline --current-buffer
# CHECK: 124

commandline '123'; commandline --cursor 1; fish_vi_dec
commandline --current-buffer
# CHECK: 122

commandline '123'; commandline --cursor 2; fish_vi_inc
commandline --current-buffer
# CHECK: 124

commandline '123'; commandline --cursor 2; fish_vi_dec
commandline --current-buffer
# CHECK: 122

commandline 'abc123'; commandline --cursor 1; fish_vi_inc
commandline --current-buffer
# CHECK: abc124

commandline 'abc123'; commandline --cursor 1; fish_vi_dec
commandline --current-buffer
# CHECK: abc122

commandline 'abc123def'; commandline --cursor 1; fish_vi_inc
commandline --current-buffer
# CHECK: abc124def

commandline 'abc123def'; commandline --cursor 1; fish_vi_dec
commandline --current-buffer
# CHECK: abc122def

commandline 'abc123def'; commandline --cursor 5; fish_vi_inc
commandline --current-buffer
# CHECK: abc124def

commandline 'abc123def'; commandline --cursor 5; fish_vi_dec
commandline --current-buffer
# CHECK: abc122def

commandline 'abc123def'; commandline --cursor 6; fish_vi_inc
commandline --current-buffer
# CHECK: abc123def

commandline 'abc123def'; commandline --cursor 6; fish_vi_dec
commandline --current-buffer
# CHECK: abc123def

commandline 'abc99def'; commandline --cursor 1; fish_vi_inc
commandline --current-buffer
# CHECK: abc100def

commandline 'abc99def'; commandline --cursor 1; fish_vi_dec
commandline --current-buffer
# CHECK: abc98def

commandline 'abc-99def'; commandline --cursor 1; fish_vi_inc
commandline --current-buffer
# CHECK: abc-98def

commandline 'abc-99def'; commandline --cursor 1; fish_vi_dec
commandline --current-buffer
# CHECK: abc-100def

commandline '2022-04-09'; commandline --cursor 7; fish_vi_inc
commandline --current-buffer
# CHECK: 2022-04-08

commandline 'to 2022-04-09'; commandline --cursor 4; fish_vi_inc
commandline --current-buffer
# CHECK: to 2023-04-09

commandline 'to 2022-04-09'; commandline --cursor 4; fish_vi_dec
commandline --current-buffer
# CHECK: to 2021-04-09

commandline 'to 2022-04-09'; commandline --cursor 11; fish_vi_dec
commandline --current-buffer
# CHECK: to 2022-04-10

commandline 'to 2022-04-09'; commandline --cursor 11; fish_vi_inc
commandline --current-buffer
# CHECK: to 2022-04-08

# CHECK: {{\x1b\[2 q}}
