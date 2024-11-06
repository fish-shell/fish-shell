.. _commands:

Commands
============

This is a list of all the commands fish ships with.

Broadly speaking, these fall into a few categories:

Keywords
^^^^^^^^

Core language keywords that make up the syntax, like

- :doc:`if <cmds/if>` and :doc:`else <cmds/else>` for conditions.
- :doc:`for <cmds/for>` and :doc:`while <cmds/while>` for loops.
- :doc:`break <cmds/break>` and :doc:`continue <cmds/continue>` to control loops.
- :doc:`function <cmds/function>` to define functions.
- :doc:`return <cmds/return>` to return a status from a function.
- :doc:`begin <cmds/begin>` to begin a block and :doc:`end <cmds/end>` to end any block (including ifs and loops).
- :doc:`and <cmds/and>`, :doc:`or <cmds/or>` and :doc:`not <cmds/not>` to combine commands logically.
- :doc:`switch <cmds/switch>` and :doc:`case <cmds/case>` to make multiple blocks depending on the value of a variable.
- :doc:`command <cmds/command>` or :doc:`builtin <cmds/builtin>` to tell fish what sort of thing to execute
- :doc:`time <cmds/time>` to time execution
- :doc:`exec <cmds/exec>` tells fish to replace itself with a command.
- :doc:`end <cmds/end>` to end a block

Tools
^^^^^

Builtins to do a task, like

- :doc:`cd <cmds/cd>` to change the current directory.
- :doc:`echo <cmds/echo>` or :doc:`printf <cmds/printf>` to produce output.
- :doc:`set_color <cmds/set_color>` to colorize output.
- :doc:`set <cmds/set>` to set, query or erase variables.
- :doc:`read <cmds/read>` to read input.
- :doc:`string <cmds/string>` for string manipulation.
- :doc:`path <cmds/path>` for filtering paths and handling their components.
- :doc:`math <cmds/math>` does arithmetic.
- :doc:`argparse <cmds/argparse>` to make arguments easier to handle.
- :doc:`count <cmds/count>` to count arguments.
- :doc:`type <cmds/type>` to find out what sort of thing (command, builtin or function) fish would call, or if it exists at all.
- :doc:`test <cmds/test>` checks conditions like if a file exists or a string is empty.
- :doc:`contains <cmds/contains>` to see if a list contains an entry.
- :doc:`eval <cmds/eval>` and :doc:`source <cmds/source>` to run fish code from a string or file.
- :doc:`status <cmds/status>` to get shell information, like whether it's interactive or a login shell, or which file it is currently running.
- :doc:`abbr <cmds/abbr>` manages :ref:`abbreviations`.
- :doc:`bind <cmds/bind>` to change bindings.
- :doc:`complete <cmds/complete>` manages :ref:`completions <tab-completion>`.
- :doc:`commandline <cmds/commandline>` to get or change the commandline contents.
- :doc:`fish_config <cmds/fish_config>` to easily change fish's configuration, like the prompt or colorscheme.
- :doc:`random <cmds/random>` to generate random numbers or pick from a list.

Known functions
^^^^^^^^^^^^^^^^

Known functions are a customization point. You can change them to change how your fish behaves. This includes:

- :doc:`fish_prompt <cmds/fish_prompt>` and :doc:`fish_right_prompt <cmds/fish_right_prompt>` and :doc:`fish_mode_prompt <cmds/fish_mode_prompt>` to print your prompt.
- :doc:`fish_command_not_found <cmds/fish_command_not_found>` to tell fish what to do when a command is not found.
- :doc:`fish_title <cmds/fish_title>` to change the terminal's title.
- :doc:`fish_greeting <cmds/fish_greeting>` to show a greeting when fish starts.
- :doc:`fish_should_add_to_history <cmds/fish_should_add_to_history>` to determine if a command should be added to history

Helper functions
^^^^^^^^^^^^^^^^

Some helper functions, often to give you information for use in your prompt:

- :doc:`fish_git_prompt <cmds/fish_git_prompt>` and :doc:`fish_hg_prompt <cmds/fish_hg_prompt>` to print information about the current git or mercurial repository.
- :doc:`fish_vcs_prompt <cmds/fish_vcs_prompt>` to print information for either.
- :doc:`fish_svn_prompt <cmds/fish_svn_prompt>` to print information about the current svn repository.
- :doc:`fish_status_to_signal <cmds/fish_status_to_signal>` to give a signal name from a return status.
- :doc:`prompt_pwd <cmds/prompt_pwd>` to give the current directory in a nicely formatted and shortened way.
- :doc:`prompt_login <cmds/prompt_login>` to describe the current login, with user and hostname, and to explain if you are in a chroot or connected via ssh.
- :doc:`prompt_hostname <cmds/prompt_hostname>` to give the hostname, shortened for use in the prompt.
- :doc:`fish_is_root_user <cmds/fish_is_root_user>` to check if the current user is an administrator user like root.
- :doc:`fish_add_path <cmds/fish_add_path>` to easily add a path to $PATH.
- :doc:`alias <cmds/alias>` to quickly define wrapper functions ("aliases").
- :doc:`fish_delta <cmds/fish_delta>` to show what you have changed from the default configuration.
- :doc:`export <cmds/export>` as a compatibility function for other shells.

Helper commands
^^^^^^^^^^^^^^^

fish also ships some things as external commands so they can be easily called from elsewhere.

This includes :doc:`fish_indent <cmds/fish_indent>` to format fish code and :doc:`fish_key_reader <cmds/fish_key_reader>` to show you what escape sequence a keypress produces.

The full list
^^^^^^^^^^^^^

And here is the full list:

.. toctree::
   :glob:
   :maxdepth: 1
   
   cmds/*
