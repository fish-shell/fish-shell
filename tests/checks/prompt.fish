#RUN: %fish %s

prompt_pwd -d 1 /foo/bar/baz
# CHECK: /f/b/baz

prompt_pwd /usr/share/fish/prompts
# CHECK: /u/s/f/prompts

prompt_pwd -D 2 /usr/share/fish/prompts
# CHECK: /u/s/fish/prompts

prompt_pwd -D 0 /usr/share/fish/prompts
# CHECK: /u/s/f/p

prompt_pwd -d1 -D 3 /usr/local/share/fish/prompts
# CHECK: /u/l/share/fish/prompts
