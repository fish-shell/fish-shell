.. _fish_for_bash_users:

Fish for bash users
###################

This is to give you a quick overview if you come from bash (or to a lesser extent other shells zsh or ksh) and want to know how fish differs. Fish is intentionally not POSIX-compatible and as such some of the things you are used to work differently.

Many things are similar - they both fundamentally expand commandlines to execute commands, have pipes, redirections, variables, globs, use command output in various ways. This document is there to quickly show you the differences.

Command substitutions
---------------------

Fish spells command substitutions as ``(command)`` instead of ``$(command)`` (or ```command```).

In addition, it only splits them on newlines instead of $IFS. If you want to split on something else, use :ref:`string split <cmd-string-split>`, :ref:`string split0 <cmd-string-split>` or :ref:`string collect <cmd-string-collect>`. If those are used as the last command in a command substitution the splits they create are carried over. So::

  for i in (find . -print0 | string split0)

will correctly handle all possible filenames.

Variables
---------

Fish sets and erases variables with :ref:`set <cmd-set>` instead of ``VAR=VAL`` and ``declare`` and ``unset`` and ``export``. ``set`` takes options to determine the scope and exportedness of a variable::

  set -gx PAGER less # $PAGER is now global and exported, so this is like `export PAGER=less`

  set -l alocalvariable foo # $alocalvariable is now only locally defined.

or to erase variables::

  set -e PAGER


``VAR=VAL`` statements are available as environment overrides::

  PAGER=cat git log


Fish does not perform word splitting. Once a variable has been set to a value, that value stays as it is, so double-quoting variable expansions isn't the necessity it is in bash. [#]_

For instance, here's bash

.. code-block:: sh

  > foo="bar baz"
  > printf '"%s"\n' $foo # will print two lines, because we didn't double-quote, so the variable is split
  "bar"
  "baz"

And here is fish::

  > set foo "bar baz"
  > printf '"%s"\n' $foo # foo was set as one element, so it will be passed as one element, so this is one line
  "bar baz"

All variables are "arrays" (we use the term "lists"), and expanding a variable expands to all its elements, with each element as its own argument (like bash's ``"${var[@]}"``::

  > set var "foo bar" banana
  > printf %s\n $var
  foo bar
  banana

Specific elements of a list can be selected::

  echo $list[5..7]

.. [#] zsh also does not perform word splitting by default (the SH_WORD_SPLIT option controls this)

Wildcards (globs)
-----------------

Fish only supports the ``*`` and ``**`` glob (and the deprecated ``?`` glob). If a glob doesn't match it fails the command (like with bash's ``failglob``) unless the command is ``for``, ``set`` or ``count`` or the glob is used with an environment override (``VAR=* command``), in which case it expands to nothing (like with bash's ``nullglob`` option).

Globbing doesn't happen on expanded variables, so::

  set foo "*"
  echo $foo

will not match any files.

There are no options to control globbing so it always behaves like that.

Quoting
-------

Fish has two quoting styles: ``""`` and ``''``. Variables are expanded in double-quotes, nothing is expanded in single-quotes.

There is no ``$''``, instead the sequences that would transform are transformed *when unquoted*::

  > echo a\nb
  a
  b

String manipulation
-------------------

Fish does not have ``${foo%bar}``, ``${foo#bar}`` and ``${foo/bar/baz}``. Instead string manipulation is done by the :ref:`string <cmd-string>` builtin.

Special variables
-----------------

Some bash variables and their closest fish equivalent:

- ``$*``, ``$@``, ``$1`` and so on: ``$argv``
- ``$?``: ``$status``
- ``$$``: ``$fish_pid``
- ``$#``: No variable, instead use ``count $argv``
- ``$!``: ``$last_pid``
- ``$0``: ``status filename``
- ``$-``: Mostly ``status is-interactive`` and ``status is-login``

Process substitution
----------------------

Instead of ``<(command)`` fish uses ``(command | psub)``. There is no equivalent to ``>(command)``.

Note that both of these are bashisms, and most things can easily be expressed without. E.g. instead of::

  source (command | psub)

just use::

  command | source

as fish's :ref:`source <cmd-source>` can read from stdin.

Heredocs
--------

Fish does not have ``<<EOF`` "heredocs". Instead of::

  cat <<EOF
  some string
  some more string
  EOF

use::

  printf %s\n "some string" "some more string"

or::

  echo "some string
  some more string"

Quotes are followed across newlines.

Test (``test``, ``[``, ``[[``)
------------------------------

Fish has a POSIX-compatible ``test`` or ``[`` builtin. There is no ``[[`` and ``test`` does not accept ``==`` as a synonym for ``=``. It can compare floating point numbers, however.

``set -q`` can be used to determine if a variable exists or has a certain number of elements (``set -q foo[2]``).

Arithmetic Expansion
---------------------

Fish does not have ``$((i+1))`` arithmetic expansion, computation is handled by :ref:`math <cmd-math>`::

  math $i + 1

It can handle floating point numbers::

  > math 5 / 2
  2.5

Prompts
-------

Fish does not use the ``$PS1``, ``$PS2`` and so on variables. Instead the prompt is the output of the :ref:`fish_prompt <cmd-fish_prompt>` function, plus the :ref:`fish_mode_prompt <cmd-fish_mode_prompt>` function if vi-mode is enabled and the :ref:`fish_right_prompt <cmd-fish_right_prompt>` function for the right prompt.

As an example, here's a relatively simple bash prompt:

.. code-block:: sh

    # <$HOSTNAME> <$PWD in blue> <Prompt Sign in Yellow> <Rest in default light white>
    export PS1='\h\[\e[1;34m\]\w\[\e[m\] \[\e[1;32m\]\$\[\e[m\] '

and a rough fish equivalent::

  function fish_prompt
      set -l prompt_symbol '$'
      fish_is_root_user; and set prompt_symbol '#'

      echo -s $hostname (set_color blue) (prompt_pwd) \
      (set_color yellow) $prompt_symbol (set_color normal)
  end

This shows a few differences:

- Fish provides :ref:`set_color <cmd-set_color>` to color text. It can use the 16 named colors and also RGB sequences (so you could also use ``set_color 5555FF``)
- Instead of introducing specific escapes like ``\h`` for the hostname, the prompt is simply a function, so you can use variables like ``$hostname``.
- Fish offers helper functions for adding things to the prompt, like :ref:`fish_vcs_prompt <cmd-fish_vcs_prompt>` for adding a display for common version control systems (git, mercurial, svn) and :ref:`prompt_pwd <cmd-prompt_pwd>` for showing a shortened $PWD (the user's home directory becomes ``~`` and any path component is shortened). 

The default prompt is reasonably full-featured and its code can be read via ``type fish_prompt``.

Fish does not have ``$PS2`` for continuation lines, instead it leaves the lines indented to show that the commandline isn't complete yet.

Blocks and loops
----------------

Fish's blocking constructs look a little different. They all start with a word, end in ``end`` and don't have a second starting word::

  for i in 1 2 3; do
     echo $i
  done

  # becomes
  
  for i in 1 2 3
     echo $i
  end

  while true; do
     echo Weeee
  done

  # becomes

  while true
     echo Weeeeeee
  end

  {
     echo Hello
  }

  # becomes
  
  begin
     echo Hello
  end

  if true; then
     echo Yes I am true
  else
     echo "How is true not true?"
  fi

  # becomes

  if true
     echo Yes I am true
  else
     echo "How is true not true?"
  end

  foo() {
     echo foo
  }

  # becomes

  function foo
      echo foo
  end

  # (note that bash specifically allows the word "function" as an extension, but POSIX only specifies the form without, so it's more compatible to just use the form without)

Fish does not have an ``until``. Use ``while not`` or ``while !``.

Builtins and other commands
---------------------------

By now it has become apparent that fish puts much more of a focus on its builtins and external commands rather than its syntax. So here are some helpful builtins and their rough equivalent in bash:

- :ref:`string <cmd-string>` - this replaces most of the string transformation (``${i%foo}`` et al) and can also be used instead of ``grep`` and ``sed`` and such.
- :ref:`math <cmd-math>` - this replaces ``$((i + 1))`` arithmetic and can also do floats and some simple functions (sine and friends).
- :ref:`argparse <cmd-argparse>` - this can handle a script's option parsing, for which bash would probably use ``getopt`` (zsh provides ``zparseopts``).
- :ref:`count <cmd-count>` can be used to count things and therefore replaces ``$#`` and can be used instead of ``wc``.
- :ref:`status <cmd-status>` provides information about the shell status, e.g. if it's interactive or what the current linenumber is. This replaces ``$-`` and ``$BASH_LINENO`` and other variables.

- ``seq(1)`` can be used as a replacement for ``{1..10}`` range expansion. If your OS doesn't ship a `seq` fish includes a replacement function.
