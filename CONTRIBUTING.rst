Guidelines For Developers
=========================

This document provides guidelines for making changes to the fish-shell
project. This includes rules for how to format the code, naming
conventions, et cetera.

In short:

- Be conservative in what you need (``C++11``, few dependencies)
- Use automated tools to help you (including ``make test``, ``build_tools/style.fish`` and ``make lint``)

Contributing completions
------------------------

Completion scripts are the most common contribution to fish, and they are very welcome.

In general, we'll take all well-written completion scripts for a command that is publically available.
This means no private tools or personal scripts, and we do reserve the right to reject for other reasons.

Completion scripts should

1. Use as few dependencies as possible - try to use fish's builtins like ``string`` instead of ``grep`` and ``awk``,
   use ``python`` to read json instead of ``jq`` (because it's already a soft dependency for fish's tools)
2. If it uses a common unix tool, use posix-compatible invocations - ideally it would work on GNU/Linux, macOS, the BSDs and other systems
3. Option and argument descriptions should be kept short.
   The shorter the description, the more likely it is that fish can use more columns.
4. Function names should start with ``__fish``, and functions should be kept in the completion file unless they're used elsewhere.
5. Run ``fish_indent`` on your script.

Put your completion script into share/completions/name-of-command.fish.

If you want to add tests, you probably want to add a littlecheck test. See below for details.

Contributing to fish's C++ core
-------------------------------

Fish uses C++11. Newer C++ features should not be used to make it possible to use on older systems.

It does not use exceptions, they are disabled at build time with ``-fno-exceptions``.

Don't introduce new dependencies unless absolutely necessary, and if you do,
please make it optional with graceful failure if possible.
Add any new dependencies to the README.rst under the *Running* and/or *Building* sections.

This also goes for completion scripts and functions - if at all possible, they should only use
POSIX-compatible invocations of any tools, and no superfluous dependencies.

E.g. some completions deal with JSON data. In those it's preferable to use python to handle it,
as opposed to ``jq``, because fish already optionally uses python elsewhere. (It also happens to be quite a bit *faster*)

Linters
-------

Automated analysis tools like cppcheck can point out
potential bugs or code that is extremely hard to understand. They also
help ensure the code has a consistent style and that it avoids patterns
that tend to confuse people.

To make linting the code easy there are two make targets: ``lint``,
to lint any modified but not committed ``*.cpp`` files, and
``lint-all`` to lint all files.

Fish has custom cppcheck rules in the file ``.cppcheck.rule``. These
help catch mistakes such as using ``wcwidth()`` rather than
``fish_wcwidth()``. Please add a new rule if you find similar mistakes
being made.

Suppressing Lint Warnings
~~~~~~~~~~~~~~~~~~~~~~~~~

Once in a while the lint tools emit a false positive warning. For
example, cppcheck might suggest a memory leak is present when that is
not the case. To suppress that cppcheck warning you should insert a line
like the following immediately prior to the line cppcheck warned about:

::

   // cppcheck-suppress memleak // addr not really leaked

The explanatory portion of the suppression comment is optional. For
other types of warnings replace “memleak” with the value inside the
parenthesis (e.g., “nullPointerRedundantCheck”) from a warning like the
following:

::

   [src/complete.cpp:1727]: warning (nullPointerRedundantCheck): Either the condition 'cmd_node' is redundant or there is possible null pointer dereference: cmd_node.

Code Style
----------

To ensure your changes conform to the style rules run

::

   build_tools/style.fish

before committing your change. That will run ``git-clang-format`` to
rewrite only the lines you’re modifying.

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

Suppressing Reformatting of C++ Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can tell ``clang-format`` to not reformat a block by enclosing it in
comments like this:

::

   // clang-format off
   code to ignore
   // clang-format on

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

C++ Style Guide
---------------

1. The `Google C++ Style
   Guide <https://google.github.io/styleguide/cppguide.html>`__ forms
   the basis of the fish C++ style guide. There are two major deviations
   for the fish project. First, a four, rather than two, space indent.
   Second, line lengths up to 100, rather than 80, characters.

2. The ``clang-format`` command is authoritative with respect to
   indentation, whitespace around operators, etc.

3. All names in code should be ``small_snake_case``. No Hungarian
   notation is used. The names for classes and structs should be
   followed by ``_t``.

4. Always attach braces to the surrounding context.

5. Indent with spaces, not tabs and use four spaces per indent.

6. Document the purpose of a function or class with doxygen-style
   comment blocks. e.g.:

::

   /**
    * Sum numbers in a vector.
    *
    * @param values Container whose values are summed.
    * @return sum of `values`, or 0.0 if `values` is empty.
    */
   double sum(std::vector<double> & const values) {
       ...
   }
    */

or

::

   /// brief description of somefunction()
   void somefunction() {

Testing
-------

The source code for fish includes a large collection of tests. If you
are making any changes to fish, running these tests is a good way to make
sure the behaviour remains consistent and regressions are not
introduced. Even if you don’t run the tests on your machine, they will
still be run via Github Actions.

You are strongly encouraged to add tests when changing the functionality
of fish, especially if you are fixing a bug to help ensure there are no
regressions in the future (i.e., we don’t reintroduce the bug).

The tests can be found in three places:

- src/fish_tests.cpp for tests to the core C++ code
- tests/checks for script tests, run by `littlecheck <https://github.com/ridiculousfish/littlecheck>`__
- tests/pexpects for interactive tests using `pexpect <https://pexpect.readthedocs.io/en/stable/>`__

When in doubt, the bulk of the tests should be added as a littlecheck test in tests/checks, as they are the easiest to modify and run, and much faster and more dependable than pexpect tests. The syntax is fairly self-explanatory. It's a fish script with the expected output in ``# CHECK:`` or ``# CHECKERR:`` (for stderr) comments.

fish_tests.cpp is mostly useful for unit tests - if you wish to test that a function does the correct thing for given input, use it.

The pexpects are written in python and can simulate input and output to/from a terminal, so they are needed for anything that needs actual interactivity. The runner is in build_tools/pexpect_helper.py, in case you need to modify something there.

Local testing
~~~~~~~~~~~~~

The tests can be run on your local computer on all operating systems.

::

   cmake path/to/fish-shell
   make test

Git hooks
~~~~~~~~~

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
       make test
       RESULT=$?
       if [ $RESULT -ne 0 ]; then
           echo "Tests failed for a push to master, we can't let you do that" >&2
           exit 1
       fi
   fi
   exit 0

This will check if the push is to the master branch and, if it is, only
allow the push if running ``make test`` succeeds. In some circumstances
it may be advisable to circumvent this check with
``git push --no-verify``, but usually that isn’t necessary.

To install the hook, place the code in a new file
``.git/hooks/pre-push`` and make it executable.

Coverity Scan
~~~~~~~~~~~~~

We use Coverity’s static analysis tool which offers free access to open
source projects. While access to the tool itself is restricted,
fish-shell organization members should know that they can login
`here <https://scan.coverity.com/projects/fish-shell-fish-shell?tab=overview>`__
with their GitHub account. Currently, tests are triggered upon merging
the ``master`` branch into ``coverity_scan_master``. Even if you are not
a fish developer, you can keep an eye on our statistics there.

Installing the Required Tools
-----------------------------

Installing the Linting Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To install the lint checkers on Mac OS X using Homebrew:

::

   brew install cppcheck

To install the lint checkers on Debian-based Linux distributions:

::

   sudo apt-get install clang
   sudo apt-get install cppcheck

Installing the Formatting Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mac OS X:

::

   brew install clang-format

Debian-based:

::

   sudo apt-get install clang-format

Message Translations
--------------------

Fish uses the GNU gettext library to translate messages from English to
other languages.

All non-debug messages output for user consumption should be marked for
translation. In C++, this requires the use of the ``_`` (underscore)
macro:

::

   streams.out.append_format(_(L"%ls: There are no jobs\n"), argv[0]);

All messages in fish script must be enclosed in single or double quote
characters. They must also be translated via a subcommand. This means
that the following are **not** valid:

::

   echo (_ hello)
   _ "goodbye"

Above should be written like this instead:

::

   echo (_ "hello")
   echo (_ "goodbye")

Note that you can use either single or double quotes to enclose the
message to be translated. You can also optionally include spaces after
the opening parentheses and once again before the closing parentheses.

Creating and updating translations requires the Gettext tools, including
``xgettext``, ``msgfmt`` and ``msgmerge``. Translation sources are
stored in the ``po`` directory, named ``LANG.po``, where ``LANG`` is the
two letter ISO 639-1 language code of the target language (eg ``de`` for
German).

To create a new translation, for example for German:

* generate a ``messages.pot`` file by running ``build_tools/fish_xgettext.fish`` from
  the source tree
* copy ``messages.pot`` to ``po/LANG.po``

To update a translation:

* generate a ``messages.pot`` file by running
  ``build_tools/fish_xgettext.fish`` from the source tree

* update the existing translation by running
  ``msgmerge --update --no-fuzzy-matching po/LANG.po messages.pot``

Many tools are available for editing translation files, including
command-line and graphical user interface programs.

Be cautious about blindly updating an existing translation file. Trivial
changes to an existing message (eg changing the punctuation) will cause
existing translations to be removed, since the tools do literal string
matching. Therefore, in general, you need to carefully review any
recommended deletions.

Read the `translations
wiki <https://github.com/fish-shell/fish-shell/wiki/Translations>`__ for
more information.

Versioning
----------

The fish version is constructed by the *build_tools/git_version_gen.sh*
script. For developers the version is the branch name plus the output of
``git describe --always --dirty``. Normally the main part of the version
will be the closest annotated tag. Which itself is usually the most
recent release number (e.g., ``2.6.0``).

Include What You Use
--------------------

You should not depend on symbols being visible to a ``*.cpp`` module
from ``#include`` statements inside another header file. In other words
if your module does ``#include "common.h"`` and that header does
``#include "signal.h"`` your module should not assume the sub-include is
present. It should instead directly ``#include "signal.h"`` if it needs
any symbol from that header. That makes the actual dependencies much
clearer. It also makes it easy to modify the headers included by a
specific header file without having to worry that will break any module
(or header) that includes a particular header.

To help enforce this rule the ``make lint`` (and ``make lint-all``)
command will run the
`include-what-you-use <https://include-what-you-use.org/>`__ tool. You
can find the IWYU project on
`github <https://github.com/include-what-you-use/include-what-you-use>`__.

To install the tool on OS X you’ll need to add a
`formula <https://github.com/jasonmp85/homebrew-iwyu>`__ then install
it:

::

   brew tap jasonmp85/iwyu
   brew install iwyu

On Ubuntu you can install it via ``apt-get``:

::

   sudo apt-get install iwyu
