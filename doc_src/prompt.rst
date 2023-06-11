Writing your own prompt
=======================

Fish ships a number of prompts that you can view with the :doc:`fish_config <cmds/fish_config>` command, and many users have shared their prompts online.

However, if you are anything like the author of this document, you sooner or later want to write your own or at least adjust the prompt you have.

Unlike other shells, fish's prompt is built by running a function - :doc:`fish_prompt <cmds/fish_prompt>`. Or, more specifically, three functions:

- :doc:`fish_prompt <cmds/fish_prompt>`
- :doc:`fish_right_prompt <cmds/fish_right_prompt>` - which is shown on the right side of the terminal.
- :doc:`fish_mode_prompt <cmds/fish_mode_prompt>` - which it runs if :ref:`vi-mode <vi-mode>` is used.

These functions are run, and whatever they print is displayed as the prompt (minus one trailing newline).

.. note::
   This document uses formatting to show what a prompt would look like. If you are viewing this in the man page,
   you probably want to switch to looking at the web version instead. Run ``help custom-prompt`` to view it.

Our first prompt
----------------

Let's look at a very simple example::

  function fish_prompt
      echo $PWD '>'
  end

This just prints the current working directory (:envvar:`PWD`) and a ``>`` symbol to show where the prompt ends.

Because we've used :doc:`echo <cmds/echo>`, it added spaces between the two so it ends up looking like (assuming ``_`` is your cursor):

.. role:: white
.. parsed-literal::
    :class: highlight

    :white:`/home/tutorial >`\ _

Formatting
----------

If you don't want the space, there are other ways to print this:

- ``echo -s``, which tells echo to not print a space.
- We could construct one string: ``echo "$PWD>"``
- :doc:`printf <cmds/printf>`, which allows us to give a format string to format our... string
- :doc:`string join <cmds/string-join>`, if we want to have one thing appear between all our components.

For now, let's use ``string join ''`` to write all our components without anything inbetween::

  function fish_prompt
      string join '' -- $PWD '>'
  end

The ``--`` tells ``string`` that no options can come after it, in case we extend this with something that can start with a ``-``.

Adding color
------------

This prompt is functional, but a bit boring. We could add some color.

In general, telling a terminal to print text in color involves sending it an escape sequence. For example you could send it

.. role:: red
.. parsed-literal::
    :class: highlight

    ESCAPE [ 3 1 m

where "escape" is the ascii escape character stored as 0x1b.

We could do this in fish, by writing escape as ``\e`` or ``\x1b``::

  echo \e\[31mfoo

This will tell the terminal to print "foo" in red and will look like this:

.. parsed-literal::
    :class: highlight

    :red:`foo`

To make it blue, we would use ``\e\[34mfoo``. However, obviously it's a bit annoying to have to remember the numbers for these colors, and modern terminals can do quite a bit more - most can not only do 256 colors but even 24-bit RGB.

So, fish offers the :doc:`set_color <cmds/set_color>` command instead, so you can do::

  echo (set_color red)foo

``set_color`` can also handle RGB colors like ``set_color 23b455``, and other formatting options including bold and italics.

So, taking our previous prompt and adding some color::

  function fish_prompt
      string join '' -- (set_color green) $PWD (set_color normal) '>'
  end

A "normal" color tells the terminal to go back to its normal formatting options.

Shortening $PWD
---------------

This is alright, but our $PWD can be a bit long, and we are typically only interested in the last few directories. We can shorten this with the :doc:`prompt_pwd <cmds/prompt_pwd>` helper that will give us a shortened working directory::

  function fish_prompt
      string join '' -- (set_color green) (prompt_pwd) (set_color normal) '>'
  end

``prompt_pwd`` takes options to control how much to shorten. For instance if we want to display the last two directories, we'd use ``prompt_pwd --full-length-dirs=2`` or ``prompt_pwd -D 2``::

  function fish_prompt
      string join '' -- (set_color green) (prompt_pwd -D 2) (set_color normal) '>'
  end

With a current directory of "/home/tutorial/Music/Lena Raine/Oneknowing", this would print

.. role:: green
.. parsed-literal::
    :class: highlight

    :green:`~/M/Lena Raine/Oneknowing`>_

Status
------

One important bit [#]_ of information that every command returns is the :ref:`status <variables-status>`. This is either 0 if the command returned successfully, or a whole number from 1 to 255 if not, so it's best understood as an error code.

It's useful to display this in your prompt, but showing it when it's 0 seems kind of wasteful.

First of all, since every command (except for :doc:`set <cmds/set>`) changes the status, you need to store it for later use as the first thing in your prompt. Use a local variable so it will be confined to your prompt function::

  set -l last_status $status
  
And after that, you can set a string if it not zero::
  
  # Prompt status only if it's not 0
  set -l stat
  if test $last_status -ne 0
      set stat (set_color red)"[$last_status]"(set_color normal)
  end

And to print it, we add it to our ``string join``::

  string join '' -- (set_color green) (prompt_pwd) (set_color normal) $stat '>'
  
In case that $last_status was 0, $stat is empty, and so it will simply disappear.

So our entire prompt is now::

  function fish_prompt
      set -l last_status $status
      # Prompt status only if it's not 0
      set -l stat
      if test $last_status -ne 0
          set stat (set_color red)"[$last_status]"(set_color normal)
      end

      string join '' -- (set_color green) (prompt_pwd) (set_color normal) $stat '>'
  end

And it looks like:

.. role:: green
.. parsed-literal::
    :class: highlight

    :green:`~/M/L/Oneknowing`\ :red:`[1]`>_

after we run ``false`` (which returns 1).

.. [#] Actually a byte, i.e. 8 bits - it can go from 0 to 255

Where to go from here?
----------------------

We have now built a simple but working and usable prompt, but of course more can be done.

- Fish offers more helper functions:
  - ``prompt_login`` to describe the user/hostname/container or ``prompt_hostname`` to describe just the host
  - ``fish_is_root_user`` to help with changing the symbol for root.
  - ``fish_vcs_prompt`` to show version control information (or ``fish_git_prompt`` / ``fish_hg_prompt`` / ``fish_svn_prompt`` to limit it to specific systems)
- You can add a right prompt by changing :doc:`fish_right_prompt <cmds/fish_right_prompt>` or a vi-mode prompt by changing :doc:`fish_mode_prompt <cmds/fish_mode_prompt>`.
- Some prompts have interesting or advanced features
  - Add a date?
  - Show various integrations like python's venv
  - Color the parts differently.

You can look at fish's sample prompts for inspiration.
