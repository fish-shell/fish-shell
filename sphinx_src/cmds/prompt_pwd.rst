prompt_pwd - Print pwd suitable for prompt
==========================================

Synopsis
--------

prompt_pwd


Description
------------

prompt_pwd is a function to print the current working directory in a way suitable for prompts. It will replace the home directory with "~" and shorten every path component but the last to a default of one character.

To change the number of characters per path component, set $fish_prompt_pwd_dir_length to the number of characters. Setting it to 0 or an invalid value will disable shortening entirely.

Examples
------------

\fish{cli-dark}
>_ cd ~/
>_ echo $PWD
<outp>/home/alfa</outp>

>_ prompt_pwd
<outp>~</outp>

>_ cd /tmp/banana/sausage/with/mustard
>_ prompt_pwd
<outp>/t/b/s/w/mustard</outp>

>_ set -g fish_prompt_pwd_dir_length 3
>_ prompt_pwd
<outp>/tmp/ban/sau/wit/mustard</outp>
\endfish
