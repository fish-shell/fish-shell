#RUN: %fish %s

prompt_pwd -d 1 /foo/bar/baz
# CHECK: /f/b/baz

prompt_pwd /usr/share/fish/tools/web_config/sample_prompts
# CHECK: /u/s/f/t/w/sample_prompts

prompt_pwd -D 2 /usr/share/fish/tools/web_config/sample_prompts
# CHECK: /u/s/f/t/web_config/sample_prompts

prompt_pwd -D 0 /usr/share/fish/tools/web_config/sample_prompts
# CHECK: /u/s/f/t/w/s

prompt_pwd -d1 -D 3 /usr/share/fish/tools/web_config/-sample_prompts
# CHECK: /u/s/f/tools/web_config/-sample_prompts
