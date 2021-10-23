.. _commands:

Commands
============

This is a list of all the commands fish ships with.

Broadly speaking, these fall into a few categories:

Keywords
^^^^^^^^

Core language keywords that make up the syntax, like

- :ref:`if <cmd-if>` for conditions.
- :ref:`for <cmd-for>` and :ref:`while <cmd-while>` for loops.
- :ref:`break <cmd-break>` and :ref:`continue <cmd-continue>` to control loops.
- :ref:`function <cmd-function>` to define functions.
- :ref:`return <cmd-return>` to return a status from a function.
- :ref:`begin <cmd-begin>` to begin a block and :ref:`end <cmd-end>` to end any block (including ifs and loops).
- :ref:`and <cmd-and>`, :ref:`or <cmd-or>` and :ref:`not <cmd-not>` to combine commands logically.
- :ref:`switch <cmd-switch>` and :ref:`case <cmd-case>` to make multiple blocks depending on the value of a variable.

Decorations
^^^^^^^^^^^

Command decorations are keywords like :ref:`command <cmd-command>` or :ref:`builtin <cmd-builtin>` to tell fish what sort of thing to execute, and :ref:`time <cmd-time>` to time its execution. :ref:`exec <cmd-exec>` tells fish to replace itself with the command.

Tools to do a task
^^^^^^^^^^^^^^^^^^

Builtins to do a task, like

- :ref:`cd <cmd-cd>` to change the current directory.
- :ref:`echo <cmd-echo>` or :ref:`printf <cmd-printf>` to produce output.
- :ref:`set <cmd-set>` to set, query or erase variables.
- :ref:`read <cmd-read>` to read input.
- :ref:`string <cmd-string>` for string manipulation.
- :ref:`math <cmd-math>` does arithmetic.
- :ref:`argparse <cmd-argparse>` to make arguments easier to handle.
- :ref:`count <cmd-count>` to count arguments.
- :ref:`type <cmd-type>` to find out what sort of thing (command, builtin or function) fish would call, or if it exists at all.
- :ref:`test <cmd-test>` checks conditions like if a file exists or a string is empty.
- :ref:`contains <cmd-contains>` to see if a list contains an entry.
- :ref:`abbr <cmd-abbr>` manages :ref:`abbreviations`.
- :ref:`eval <cmd-eval>` and :ref:`source <cmd-source>` to run fish code from a string or file.
- :ref:`set_color <cmd-set_color>` to colorize your output.
- :ref:`status <cmd-status>` to get shell information, like whether it's interactive or a login shell, or which file it is currently running.
- :ref:`bind <cmd-bind>` to change bindings.
- :ref:`commandline <cmd-commandline>` to get or change the commandline contents.
- :ref:`fish_config <cmd-fish_config>` to easily change fish's configuration, like the prompt or colorscheme.
- :ref:`random <cmd-random>` to generate random numbers or pick from a list.

Known functions
^^^^^^^^^^^^^^^^

Known functions are a customization point. You can change them to change how your fish behaves. This includes:

- :ref:`fish_prompt <cmd-fish_prompt>` and :ref:`fish_right_prompt <cmd-fish_right_prompt>` and :ref:`fish_mode_prompt <cmd-fish_mode_prompt>` to print your prompt.
- :ref:`fish_command_not_found <cmd-fish_command_not_found>` to tell fish what to do when a command is not found.
- :ref:`fish_title <cmd-fish_title>` to change the terminal's title.
- :ref:`fish_greeting <cmd-fish_greeting>` to show a greeting when fish starts.
  
Helper functions
^^^^^^^^^^^^^^^^

Some helper functions, often to give you information for use in your prompt:

- :ref:`fish_git_prompt <cmd-fish_git_prompt>` and :ref:`fish_hg_prompt <cmd-fish_hg_prompt>` to print information about the current git or mercurial repository.
- :ref:`fish_vcs_prompt <cmd-fish_vcs_prompt>` to print information for either.
- :ref:`fish_svn_prompt <cmd-fish_svn_prompt>` to print information about the current svn repository.
- :ref:`fish_status_to_signal <cmd-fish_status_to_signal>` to give a signal name from a return status.
- :ref:`prompt_pwd <cmd-prompt_pwd>` to give the current directory in a nicely formatted and shortened way.
- :ref:`prompt_login <cmd-prompt_login>` to describe the current login, with user and hostname, and to explain if you are in a chroot or connected via ssh.
- :ref:`fish_is_root_user <cmd-fish_is_root_user>` to check if the current user is an administrator user like root.
- :ref:`fish_add_path <cmd-fish_add_path>` to easily add a path to $PATH.
- :ref:`alias <cmd-alias>` to quickly define wrapper functions ("aliases").

Helper commands
^^^^^^^^^^^^^^^

fish also ships some things as external commands so they can be easily called from elsewhere.

This includes :ref:`fish_indent <cmd-fish_indent>` to format fish code and :ref:`fish_key_reader <cmd-fish_key_reader>` to show you what escape sequence a keypress produces.

The full list
^^^^^^^^^^^^^

And here is the full list:

.. toctree::
   :glob:
   :maxdepth: 1
   
   cmds/*
