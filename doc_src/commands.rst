.. _commands:

Commands
============

This is a list of all the commands ghoti ships with.

Broadly speaking, these fall into a few categories:

Keywords
^^^^^^^^

Core language keywords that make up the syntax, like

- :doc:`if <cmds/if>` for conditions.
- :doc:`for <cmds/for>` and :doc:`while <cmds/while>` for loops.
- :doc:`break <cmds/break>` and :doc:`continue <cmds/continue>` to control loops.
- :doc:`function <cmds/function>` to define functions.
- :doc:`return <cmds/return>` to return a status from a function.
- :doc:`begin <cmds/begin>` to begin a block and :doc:`end <cmds/end>` to end any block (including ifs and loops).
- :doc:`and <cmds/and>`, :doc:`or <cmds/or>` and :doc:`not <cmds/not>` to combine commands logically.
- :doc:`switch <cmds/switch>` and :doc:`case <cmds/case>` to make multiple blocks depending on the value of a variable.
- :doc:`command <cmds/command>` or :doc:`builtin <cmds/builtin>` to tell ghoti what sort of thing to execute
- :doc:`time <cmds/time>` to time execution
- :doc:`exec <cmds/exec>` tells ghoti to replace itself with a command.

Tools
^^^^^

Builtins to do a task, like

- :doc:`cd <cmds/cd>` to change the current directory.
- :doc:`echo <cmds/echo>` or :doc:`printf <cmds/printf>` to produce output.
- :doc:`set_color <cmds/set_color>` to colorize output.
- :doc:`set <cmds/set>` to set, query or erase variables.
- :doc:`read <cmds/read>` to read input.
- :doc:`string <cmds/string>` for string manipulation.
- :doc:`math <cmds/math>` does arithmetic.
- :doc:`argparse <cmds/argparse>` to make arguments easier to handle.
- :doc:`count <cmds/count>` to count arguments.
- :doc:`type <cmds/type>` to find out what sort of thing (command, builtin or function) ghoti would call, or if it exists at all.
- :doc:`test <cmds/test>` checks conditions like if a file exists or a string is empty.
- :doc:`contains <cmds/contains>` to see if a list contains an entry.
- :doc:`eval <cmds/eval>` and :doc:`source <cmds/source>` to run ghoti code from a string or file.
- :doc:`status <cmds/status>` to get shell information, like whether it's interactive or a login shell, or which file it is currently running.
- :doc:`abbr <cmds/abbr>` manages :ref:`abbreviations`.
- :doc:`bind <cmds/bind>` to change bindings.
- :doc:`complete <cmds/complete>` manages :ref:`completions <tab-completion>`.
- :doc:`commandline <cmds/commandline>` to get or change the commandline contents.
- :doc:`ghoti_config <cmds/ghoti_config>` to easily change ghoti's configuration, like the prompt or colorscheme.
- :doc:`random <cmds/random>` to generate random numbers or pick from a list.

Known functions
^^^^^^^^^^^^^^^^

Known functions are a customization point. You can change them to change how your ghoti behaves. This includes:

- :doc:`ghoti_prompt <cmds/ghoti_prompt>` and :doc:`ghoti_right_prompt <cmds/ghoti_right_prompt>` and :doc:`ghoti_mode_prompt <cmds/ghoti_mode_prompt>` to print your prompt.
- :doc:`ghoti_command_not_found <cmds/ghoti_command_not_found>` to tell ghoti what to do when a command is not found.
- :doc:`ghoti_title <cmds/ghoti_title>` to change the terminal's title.
- :doc:`ghoti_greeting <cmds/ghoti_greeting>` to show a greeting when ghoti starts.

Helper functions
^^^^^^^^^^^^^^^^

Some helper functions, often to give you information for use in your prompt:

- :doc:`ghoti_git_prompt <cmds/ghoti_git_prompt>` and :doc:`ghoti_hg_prompt <cmds/ghoti_hg_prompt>` to print information about the current git or mercurial repository.
- :doc:`ghoti_vcs_prompt <cmds/ghoti_vcs_prompt>` to print information for either.
- :doc:`ghoti_svn_prompt <cmds/ghoti_svn_prompt>` to print information about the current svn repository.
- :doc:`ghoti_status_to_signal <cmds/ghoti_status_to_signal>` to give a signal name from a return status.
- :doc:`prompt_pwd <cmds/prompt_pwd>` to give the current directory in a nicely formatted and shortened way.
- :doc:`prompt_login <cmds/prompt_login>` to describe the current login, with user and hostname, and to explain if you are in a chroot or connected via ssh.
- :doc:`prompt_hostname <cmds/prompt_hostname>` to give the hostname, shortened for use in the prompt.
- :doc:`ghoti_is_root_user <cmds/ghoti_is_root_user>` to check if the current user is an administrator user like root.
- :doc:`ghoti_add_path <cmds/ghoti_add_path>` to easily add a path to $PATH.
- :doc:`alias <cmds/alias>` to quickly define wrapper functions ("aliases").
- :doc:`ghoti_delta <cmds/ghoti_delta>` to show what you have changed from the default configuration.

Helper commands
^^^^^^^^^^^^^^^

ghoti also ships some things as external commands so they can be easily called from elsewhere.

This includes :doc:`ghoti_indent <cmds/ghoti_indent>` to format ghoti code and :doc:`ghoti_key_reader <cmds/ghoti_key_reader>` to show you what escape sequence a keypress produces.

The full list
^^^^^^^^^^^^^

And here is the full list:

.. toctree::
   :glob:
   :maxdepth: 1
   
   cmds/*
