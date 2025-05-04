####################
Contributing To Fish
####################

This document tells you how you can contribute to fish.

Fish is free and open source software, distributed under the terms of the GPLv2.

Contributions are welcome, and there are many ways to contribute!

Whether you want to change some of the core Rust source, enhance or add a completion script or function,
improve the documentation or translate something, this document will tell you how.

Getting Set Up
==============

Fish is developed on Github, at https://github.com/fish-shell/fish-shell.

First, you'll need an account there, and you'll need a git clone of fish.
Fork it on Github and then run::

  git clone https://github.com/<USERNAME>/fish-shell.git

This will create a copy of the fish repository in the directory fish-shell in your current working directory.

Also, for most changes you want to run the tests and so you'd get a setup to compile fish.
For that, you'll require:

-  Rust - when in doubt, try rustup
-  CMake
-  PCRE2 (headers and libraries) - optional, this will be downloaded if missing
-  gettext (headers and libraries) - optional, for translation support
-  Sphinx - optional, to build the documentation

Of course not everything is required always - if you just want to contribute something to the documentation you'll just need Sphinx,
and if the change is very simple and obvious you can just send it in. Use your judgement!

Once you have your changes, open a pull request on https://github.com/fish-shell/fish-shell/pulls.

Guidelines
==========

In short:

- Be conservative in what you need (keep to the agreed minimum supported Rust version, limit new dependencies)
- Use automated tools to help you (including ``make fish_run_tests`` and ``build_tools/style.fish``)

Contributing completions
========================

Completion scripts are the most common contribution to fish, and they are very welcome.

In general, we'll take all well-written completion scripts for a command that is publicly available.
This means no private tools or personal scripts, and we do reserve the right to reject for other reasons.

Before you try to contribute them to fish, consider if the authors of the tool you are completing want to maintain the script instead.
Often that makes more sense, specifically because they can add new options to the script immediately once they add them,
and don't have to maintain one completion script for multiple versions. If the authors no longer wish to maintain the script,
they can of course always contact the fish maintainers to hand it over, preferably by opening a PR.
This isn't a requirement - if the authors don't want to maintain it, or you simply don't want to contact them,
you can contribute your script to fish.

Completion scripts should

1. Use as few dependencies as possible - try to use fish's builtins like ``string`` instead of ``grep`` and ``awk``,
   use ``python`` to read json instead of ``jq`` (because it's already a soft dependency for fish's tools)
2. If it uses a common unix tool, use posix-compatible invocations - ideally it would work on GNU/Linux, macOS, the BSDs and other systems
3. Option and argument descriptions should be kept short.
   The shorter the description, the more likely it is that fish can use more columns.
4. Function names should start with ``__fish``, and functions should be kept in the completion file unless they're used elsewhere.
5. Run ``fish_indent`` on your script.
6. Try not to use minor convenience features right after they are available in fish - we do try to keep completion scripts backportable.
   If something has a real impact on the correctness or performance, feel free to use it,
   but if it is just a shortcut, please leave it.

Put your completion script into share/completions/name-of-command.fish. If you have multiple commands, you need multiple files.

If you want to add tests, you probably want to add a littlecheck test. See below for details.

Contributing documentation
==========================

The documentation is stored in ``doc_src/``, and written in ReStructured Text and built with Sphinx.

To build it locally, run from the main fish-shell directory::

    sphinx-build -j 8 -b html -n doc_src/ /tmp/fish-doc/

which will build the docs as html in /tmp/fish-doc. You can open it in a browser and see that it looks okay.

The builtins and various functions shipped with fish are documented in doc_src/cmds/.

Code Style
==========

To ensure your changes conform to the style rules run

::

   build_tools/style.fish

before committing your change. That will run our autoformatters:

- ``rustfmt`` for Rust
- ``fish_indent`` (shipped with fish) for fish script
- ``black`` for python

If you’ve already committed your changes that’s okay since it will then
check the files in the most recent commit. This can be useful after
you’ve merged another person’s change and want to check that it’s style
is acceptable. However, in that case it will run ``clang-format`` to
ensure the entire file, not just the lines modified by the commit,
conform to the style.

If you want to check the style of the entire code base run

::

   build_tools/style.fish --all

That command will refuse to restyle any files if you have uncommitted
changes.

Fish Script Style Guide
-----------------------

1. All fish scripts, such as those in the *share/functions* and *tests*
   directories, should be formatted using the ``fish_indent`` command.

2. Function names should be in all lowercase with words separated by
   underscores. Private functions should begin with an underscore. The
   first word should be ``fish`` if the function is unique to fish.

3. The first word of global variable names should generally be ``fish``
   for public vars or ``_fish`` for private vars to minimize the
   possibility of name clashes with user defined vars.

Configuring Your Editor for Fish Scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you use Vim: Install `vim-fish <https://github.com/dag/vim-fish>`__,
make sure you have syntax and filetype functionality in ``~/.vimrc``:

::

   syntax enable
   filetype plugin indent on

Then turn on some options for nicer display of fish scripts in
``~/.vim/ftplugin/fish.vim``:

::

   " Set up :make to use fish for syntax checking.
   compiler fish

   " Set this to have long lines wrap inside comments.
   setlocal textwidth=79

   " Enable folding of block structures in fish.
   setlocal foldmethod=expr

If you use Emacs: Install
`fish-mode <https://github.com/wwwjfy/emacs-fish>`__ (also available in
melpa and melpa-stable) and ``(setq-default indent-tabs-mode nil)`` for
it (via a hook or in ``use-package``\ s “:init” block). It can also be
made to run fish_indent via e.g.

.. code:: elisp

   (add-hook 'fish-mode-hook (lambda ()
       (add-hook 'before-save-hook 'fish_indent-before-save)))

Rust Style Guide
----------------

Use ``cargo fmt`` and ``cargo clippy``. Clippy warnings can be turned off if there's a good reason to.

Testing
=======

The source code for fish includes a large collection of tests. If you
are making any changes to fish, running these tests is a good way to make
sure the behaviour remains consistent and regressions are not
introduced. Even if you don’t run the tests on your machine, they will
still be run via Github Actions.

You are strongly encouraged to add tests when changing the functionality
of fish, especially if you are fixing a bug to help ensure there are no
regressions in the future (i.e., we don’t reintroduce the bug).

The tests can be found in three places:

- src/tests for unit tests.
- tests/checks for script tests, run by `littlecheck <https://github.com/ridiculousfish/littlecheck>`__
- tests/pexpects for interactive tests using `pexpect <https://pexpect.readthedocs.io/en/stable/>`__

When in doubt, the bulk of the tests should be added as a littlecheck test in tests/checks, as they are the easiest to modify and run, and much faster and more dependable than pexpect tests. The syntax is fairly self-explanatory. It's a fish script with the expected output in ``# CHECK:`` or ``# CHECKERR:`` (for stderr) comments.
If your littlecheck test has a specific dependency, use ``# REQUIRE: ...`` with a posix sh script.

The pexpects are written in python and can simulate input and output to/from a terminal, so they are needed for anything that needs actual interactivity. The runner is in tests/pexpect_helper.py, in case you need to modify something there.

These tests can be run via the tests/test_driver.py python script, which will set up the environment.
It sets up a temporary $HOME and also uses it as the current directory, so you do not need to create a temporary directory in them.

If you need a command to do something weird to test something, maybe add it to the ``fish_test_helper`` binary (in tests/fish_test_helper.c), or see if it can already do it.

Local testing
-------------

The tests can be run on your local computer on all operating systems.

::

   cmake path/to/fish-shell
   make fish_run_tests

Or you can run them on a fish, without involving cmake::

  cargo build
  cargo test # for the unit tests
  tests/test_driver.py --cachedir=/tmp target/debug # for the script and interactive tests

Here, the first argument to test_driver.py refers to a directory with ``fish``, ``fish_indent`` and ``fish_key_reader`` in it.
In this example we're in the root of the git repo and have run ``cargo build`` without ``--release``, so it's a debug build.
The ``--cachedir /tmp`` argument means it will keep the fish_test_helper binary in /tmp instead of recompiling it for every test.
This saves some time, but isn't strictly necessary.

Git hooks
---------

Since developers sometimes forget to run the tests, it can be helpful to
use git hooks (see githooks(5)) to automate it.

One possibility is a pre-push hook script like this one:

.. code:: sh

   #!/bin/sh
   #### A pre-push hook for the fish-shell project
   # This will run the tests when a push to master is detected, and will stop that if the tests fail
   # Save this as .git/hooks/pre-push and make it executable

   protected_branch='master'

   # Git gives us lines like "refs/heads/frombranch SOMESHA1 refs/heads/tobranch SOMESHA1"
   # We're only interested in the branches
   while read from _ to _; do
       if [ "x$to" = "xrefs/heads/$protected_branch" ]; then
           isprotected=1
       fi
   done
   if [ "x$isprotected" = x1 ]; then
       echo "Running tests before push to master"
       make fish_run_tests
       RESULT=$?
       if [ $RESULT -ne 0 ]; then
           echo "Tests failed for a push to master, we can't let you do that" >&2
           exit 1
       fi
   fi
   exit 0

This will check if the push is to the master branch and, if it is, only
allow the push if running ``make fish_run_tests`` succeeds. In some circumstances
it may be advisable to circumvent this check with
``git push --no-verify``, but usually that isn’t necessary.

To install the hook, place the code in a new file
``.git/hooks/pre-push`` and make it executable.

Coverity Scan
-------------

We use Coverity’s static analysis tool which offers free access to open
source projects. While access to the tool itself is restricted,
fish-shell organization members should know that they can login
`here <https://scan.coverity.com/projects/fish-shell-fish-shell?tab=overview>`__
with their GitHub account. Currently, tests are triggered upon merging
the ``master`` branch into ``coverity_scan_master``. Even if you are not
a fish developer, you can keep an eye on our statistics there.

Contributing Translations
=========================

Fish uses the GNU gettext library to translate messages from English to
other languages.

Creating and updating translations requires the Gettext tools, including
``xgettext``, ``msgfmt`` and ``msgmerge``. Translation sources are
stored in the ``po`` directory, named ``LANG.po``, where ``LANG`` is the
two letter ISO 639-1 language code of the target language (eg ``de`` for
German).

To create a new translation:

* generate a ``messages.pot`` file by running ``build_tools/fish_xgettext.fish`` from
  the source tree
* copy ``messages.pot`` to ``po/LANG.po``

To update a translation:

* generate a ``messages.pot`` file by running
  ``build_tools/fish_xgettext.fish`` from the source tree

* update the existing translation by running
  ``msgmerge --update --no-fuzzy-matching --no-wrap po/LANG.po messages.pot``

The ``--no-fuzzy-matching`` is important as we have had terrible experiences with gettext's "fuzzy" translations in the past.

Many tools are available for editing translation files, including
command-line and graphical user interface programs. For simple use, you can just use your text editor.

Open up the po file, for example ``po/sv.po``, and you'll see something like::

  msgid "%ls: No suitable job\n"
  msgstr "" 

The ``msgid`` here is the "name" of the string to translate, typically the english string to translate. The second line (``msgstr``) is where your translation goes.

For example::

  msgid "%ls: No suitable job\n"
  msgstr "%ls: Inget passande jobb\n"

Any ``%s`` / ``%ls`` or ``%d`` are placeholders that fish will use for formatting at runtime. It is important that they match - the translated string should have the same placeholders in the same order.

Also any escaped characters, like that ``\n`` newline at the end, should be kept so the translation has the same behavior.

Our tests run ``msgfmt --check-format /path/to/file``, so they would catch mismatched placeholders - otherwise fish would crash at runtime when the string is about to be used.

Be cautious about blindly updating an existing translation file. Trivial
changes to an existing message (eg changing the punctuation) will cause
existing translations to be removed, since the tools do literal string
matching. Therefore, in general, you need to carefully review any
recommended deletions.

Setting Code Up For Translations
--------------------------------

All non-debug messages output for user consumption should be marked for
translation. In Rust, this requires the use of the ``wgettext!`` or ``wgettext_fmt!``
macros:

::

   streams.out.append(wgettext_fmt!("%ls: There are no jobs\n", argv[0]));

All messages in fish script must be enclosed in single or double quote
characters for our message extraction script to find them.
They must also be translated via a command substitution. This means
that the following are **not** valid:

::

   echo (_ hello)
   _ "goodbye"

Above should be written like this instead:

::

   echo (_ "hello")
   echo (_ "goodbye")

You can use either single or double quotes to enclose the
message to be translated. You can also optionally include spaces after
the opening parentheses or before the closing parentheses.

Versioning
==========

The fish version is constructed by the *build_tools/git_version_gen.sh*
script. For developers the version is the branch name plus the output of
``git describe --always --dirty``. Normally the main part of the version
will be the closest annotated tag. Which itself is usually the most
recent release number (e.g., ``2.6.0``).
