####################
Contributing To Fish
####################

This document tells you how you can contribute to fish.

Fish is free and open source software, distributed under the terms of the GPLv2.

Contributions are welcome, and there are many ways to contribute!

Whether you want to change some of the core Rust source, enhance or add a completion script or function,
improve the documentation or translate something, this document will tell you how.


Mailing List
============

Send patches to the public mailing list: mailto:~krobelus/fish-shell@lists.sr.ht.
Archives are available at https://lists.sr.ht/~krobelus/fish-shell/.

GitHub
======

Fish is available on GitHub, at https://github.com/fish-shell/fish-shell.

First, you'll need an account there, and you'll need a git clone of fish.
Fork it on GitHub and then run::

  git clone https://github.com/<USERNAME>/fish-shell.git

This will create a copy of the fish repository in the directory fish-shell in your current working directory.

Also, for most changes you want to run the tests and so you'd get a setup to compile fish.
For that, you'll require:

-  Rust - when in doubt, try rustup
-  CMake
-  PCRE2 (headers and libraries) - optional, this will be downloaded if missing
-  gettext (only the msgfmt tool) - optional, for translation support
-  Sphinx - optional, to build the documentation

Of course not everything is required always - if you just want to contribute something to the documentation you'll just need Sphinx,
and if the change is very simple and obvious you can just send it in. Use your judgement!

Once you have your changes, open a pull request on https://github.com/fish-shell/fish-shell/pulls.

Guidelines
==========

In short:

- Be conservative in what you need (keep to the agreed minimum supported Rust version, limit new dependencies)
- Use automated tools to help you (``cargo xtask check``)

Commit History
==============

We use a linear, `recipe-style <https://www.bitsnbites.eu/git-history-work-log-vs-recipe/>`__ history.
Every commit should pass our checks.
We do not want "fixup" commits in our history.
If you notice an issue with a commit in a pull request, or get feedback suggesting changes,
you should rewrite the commit history and fix the relevant commits directly,
instead of adding new "fixup" commits.
When a pull request is ready, we rebase it on top of the current master branch,
so don't be shy about rewriting the history of commits which are not on master yet.
Rebasing (not merging) your pull request on the latest version of master is also welcome, especially if it resolves conflicts.

If you're using Git, consider using `jj <https://www.jj-vcs.dev/>`__ to make this easier.

If a commit should close an issue, add a ``Fixes #<issue-number>`` line at the end of the commit description.

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
2. If it uses a common unix tool, use POSIX-compatible invocations - ideally it would work on GNU/Linux, macOS, the BSDs and other systems
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
The builtins and various functions shipped with fish are documented in ``doc_src/cmds/``.

To build an HTML version of the docs locally, run::

    cargo xtask html-docs

will output to ``target/fish-docs/html`` or, if you use CMake::

    cmake --build build -t sphinx-docs

will output to ``build/cargo/fish-docs/html/``. You can also run ``sphinx-build`` directly, which allows choosing the output directory::

    sphinx-build -j auto -b html doc_src/ /tmp/fish-doc/

will output HTML docs to ``/tmp/fish-doc``.

After building them, you can open the HTML docs in a browser and see that it looks okay.


Code Style
==========

For formatting, we use:

- ``rustfmt`` for Rust
- ``fish_indent`` (shipped with fish) for fish script
- ``ruff format`` for Python

To reformat files, there is an xtask

::

   cargo xtask format --all
   cargo xtask format somefile.rs some.fish

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

Minimum Supported Rust Version (MSRV) Policy
--------------------------------------------

We support at least the version of ``rustc`` available in Debian Stable.

Testing
=======

The source code for fish includes a large collection of tests. If you
are making any changes to fish, running these tests is a good way to make
sure the behaviour remains consistent and regressions are not
introduced. Even if you don’t run the tests on your machine, they will
still be run via GitHub Actions.

You are strongly encouraged to add tests when changing the functionality
of fish, especially if you are fixing a bug to help ensure there are no
regressions in the future (i.e., we don’t reintroduce the bug).

Unit tests live next to the implementation in Rust source files, in inline submodules (``mod tests {}``).

System tests live in ``tests/``:

- ``tests/checks`` are run by `littlecheck <https://github.com/ridiculousfish/littlecheck>`__
  and test noninteractive (script) behavior,
  except for ``tests/checks/tmux-*`` which test interactive scenarios.
- ``tests/pexpects`` tests interactive scenarios using `pexpect <https://pexpect.readthedocs.io/en/stable/>`__

When in doubt, the bulk of the tests should be added as a littlecheck test in tests/checks, as they are the easiest to modify and run, and much faster and more dependable than pexpect tests.
The syntax is fairly self-explanatory.
It's a fish script with the expected output in ``# CHECK:`` or ``# CHECKERR:`` (for stderr) comments.
If your littlecheck test has a specific dependency, use ``# REQUIRE: ...`` with a POSIX sh script.

The pexpect tests are written in Python and can simulate input and output to/from a terminal, so they are needed for anything that needs actual interactivity.
The runner is in tests/pexpect_helper.py, in case you need to modify something there.

These tests can be run via the tests/test_driver.py Python script, which will set up the environment.
It sets up a temporary $HOME and also uses it as the current directory, so you do not need to create a temporary directory in them.

If you need a command to do something weird to test something, maybe add it to the ``fish_test_helper`` binary (in ``tests/fish_test_helper.c``).

Local testing
-------------

The tests can be run on your local system::

    cargo build
    # Run unit tests
    cargo test
    # Run system tests
    tests/test_driver.py target/debug
    # Run a specific system test.
    tests/test_driver.py target/debug tests/checks/abbr.fish

Here, the first argument to test_driver.py refers to a directory with ``fish``, ``fish_indent`` and ``fish_key_reader`` in it.
In this example we're in the root of the workspace and have run ``cargo build`` without ``--release``, so it's a debug build.

To run all tests and linters, use::

    cargo xtask check

Contributing Translations
=========================

Fish can localize messages present in its Rust source code,
as well as messages in the various fish scripts present in this repository.
The latter include a large amount of automatically identified messages,
originating for example from fish function descriptions.
When translating, prioritize the messages from the Rust source code.

Fish uses two different localization systems:
`GNU gettext <https://www.gnu.org/software/gettext/>`__ and `Fluent <https://projectfluent.org/>`__.
The former is used for all messages from fish scripts.
For messages from the Rust source code, we are in the process of replacing gettext with Fluent.
At the moment, both are used side-by-side,
with some messages localized with gettext and the others with Fluent.

We use custom tools for extracting messages from source files and for gettext localization at runtime.
This means that we do not have a runtime dependency on the gettext library.
It also means that some of gettext's features are not supported, such as message context and plurals.
We expect all files to be UTF-8-encoded.

Translation sources for gettext are stored in the ``localization/po`` directory and named ``ll_CC.po``,
whereas Fluent uses the ``localization/fluent`` directory and names of the shape ``ll_CC.ftl``,
where ``ll`` is the two (or possibly three) letter ISO 639-1 language code of the target language
(e.g. ``pt`` for Portuguese).
``CC`` is an ISO 3166 country/territory code, (e.g. ``BR`` for Brazil).
An example for a valid name is ``pt_BR.po`` for gettext and ``pt_BR.ftl`` for Fluent,
indicating Brazilian Portuguese.
These are the files you will interact with when adding translations.

In some cases, we also use language identifiers without a county code, i.e. ``ll.po``/``ll.ftl``.
Which variant is chosen involves various trade-offs for fallback behavior.
If you want to add a new language or language variant, feel free to ask about this.
Generally, if people who understand any variant of the language
are expected to understand the version you add
and there are no existing translations for another variant of the language,
it probably makes sense to omit the country code, otherwise to use it.

Adding translations for a new language
--------------------------------------

Creating new translations for gettext requires the gettext tools.
More specifically, you will need ``msguniq`` and ``msgmerge`` for creating translations for a new
language.
To create a new translation, run::

    build_tools/update_translations.fish localization/po/ll_CC.po

This will create a new PO file containing all messages available for translation.
If the file already exists, it will be updated.

For Fluent, it is sufficient to create a new, empty file
with the language-appropriate name in ``localization/fluent``.

After modifying a translation file, you can recompile fish,
and it will integrate the modifications you made.
This requires that the ``msgfmt`` utility is installed (comes as part of ``gettext``).
For messages localized with Fluent, recompiling fish is not necessary when you use a debug build.
Then, restarting fish is sufficient for seeing updated translations.
It is important that the ``localize-messages`` Cargo feature is enabled, which it is by default.
You can explicitly enable it using::

    cargo build --features=localize-messages

Use environment variables to tell fish which language to use, e.g.::

    LANG=pt_BR.utf8 fish

or within the running fish shell::

    set LANG pt_BR.utf8

Alternatively, you can also use the built-in ``status language`` command, e.g.::

    status language set pt_BR

Use

::

    status language list-available

to see a list of the available language identifiers.
This might also be helpful for checking that your new translations are recognized as expected.
Note that using environment variables enables fallback behavior,
e.g. if you specify ``LANG=de_DE.utf8`` and we do not have a ``de_DE`` catalog but a ``de`` catalog,
you will see messages from the latter.
With ``status language``, only exact matches are supported,
giving you more control over the fallback order.
If ``status language`` is used, it overrides the environment variable configuration.

For more options regarding how to choose languages via environment variables, see
`the corresponding gettext documentation
<https://www.gnu.org/software/gettext/manual/html_node/Locale-Environment-Variables.html>`__.
One neat thing you can do is set a list of languages to check for translations in the order defined
using the ``LANGUAGE`` variable, e.g.::

    set LANGUAGE pt_BR de_DE

or using::

    status language set pt_BR de

to try to translate messages to Portuguese, if that fails try German, and if that fails too you will
see the default English version.

Modifying existing translations
-------------------------------

If you want to work on translations for a language which already has translations, it
is sufficient to edit the existing files.
No other changes are necessary.

After recompiling fish, you should be able to see your translations in action. See the previous
section for details.

Editing PO files
----------------

Many tools are available for editing translation files, including
command-line and graphical user interface programs. For simple use, you can use your text editor.

Open up the PO file, for example ``localization/po/sv.po``, and you'll see something like::

    msgid "%s: No suitable job\n"
    msgstr ""

The ``msgid`` here is the "name" of the string to translate, typically the English string to translate.
The second line (``msgstr``) is where your translation goes.

For example::

    msgid "%s: No suitable job\n"
    msgstr "%s: Inget passande jobb\n"

Any ``%s`` or ``%d`` are placeholders that fish will use for formatting at runtime. It is important that they match - the translated string must have the same placeholders in the same order.

Also any escaped characters, like that ``\n`` newline at the end, should be kept so the translation has the same behavior.

Our tests run ``msgfmt --check-format /path/to/file``, so they would catch mismatched placeholders - otherwise fish would crash at runtime when the string is about to be used.

Be cautious about blindly updating an existing translation file.
``msgid`` strings should never be updated manually, only by running the appropriate script.

Editing FTL files
-----------------

To get familiar with Fluent's FTL format,
you can read `Fluent's guide <https://projectfluent.org/fluent/guide/>`__.

The core principle is that each message has an ID.
This ID is specified in the source code.
At runtime, Fluent checks FTL files according to the user's language settings
to try to map the ID to a localized message.
All messages are localized into English because the source code only contains IDs, not proper messages.
Use ``localization/fluent/en.ftl`` to see which IDs are in use and what the corresponding messages are.

Some messages receive arguments, called variables in Fluent.
In Fluent, each variable has a name, which allows reordering them,
so they can appear in the order which makes most sense for the language.
The general format of a variable in an FTL file is ``{ $variable_name }``.

The Fluent ecosystem is not as mature as gettext, meaning there is less available tooling.
Therefore, we provide some of our own tools.
Running ``cargo xtask fluent`` provides an overview.

For translators, the following can be useful::

    cargo xtask fluent format

to make FTL files conform to our expected format,

::

    cargo xtask fluent show-missing

to show which message ID do not have a translation yet, and

::

    cargo xtask fluent check

to run checks on the FTL files, which can catch some mistakes.
Each of these commands takes optional path arguments,
so if you are working on a certain file like ``pt_BR.ftl``,
you might want to use

::

    cargo xtask fluent check localization/fluent/pt_BR.ftl

from the repository root directory,
or, if you are in the ``localization/fluent`` directory,

::

    cargo xtask fluent check pt_BR.ftl

If you want formatting in your editor,

::

    cargo --quiet xtask fluent format -

might be useful, which reads FTL text from stdin and writes a formatted version to stdout,
or a copy of stdin if formatting failed.
Instead of invoking Cargo each time, you could also invoke the ``xtask`` binary if it exists.

A simple format-on-write setup in Vim:

.. code:: vim

    function FormatFTL()
        let cursor = getpos('.')
        :%!cargo --quiet xtask fluent format -
        call setpos('.', cursor)
    endfunction

    augroup ftl
        autocmd!
        autocmd! BufWritePre *.ftl :call FormatFTL()
    augroup END

or equivalently in Lua for NeoVim:

.. code:: lua

    local augroup_ftl = vim.api.nvim_create_augroup("ftl", { clear = true })
    vim.api.nvim_create_autocmd("BufWritePre", {
        group = augroup_ftl,
        pattern = "*.ftl",
        callback = function()
            local cursor = vim.fn.getpos(".")
            vim.cmd("%!cargo --quiet xtask fluent format -")
            vim.fn.setpos(".", cursor)
        end,
    })

There is also a `Vim plugin <https://github.com/projectfluent/fluent.vim>`__ for syntax highlighting.

Modifications to strings in source files (gettext-only)
-------------------------------------------------------

If a string changes in the sources, the old translations will no longer work.
If you add/remove/change a translatable strings in a source file,
run ``build_tools/update_translations.fish`` to propagate this to all translation files (``localization/po/*.po``).
This is only relevant for developers modifying the source files of fish or fish scripts.

Setting Code Up For Translations
--------------------------------

All non-debug messages output for user consumption should be marked for translation.
In Rust, this requires the use of the ``localize!`` macro for Fluent localization, e.g.:

.. code:: rust

    streams.err.appendln(&localize!(
        "argparse-invalid-option-spec",
        command_name = &opts.name,
        option_spec = option_spec,
        bad_char = s.char_at(0)
    ));

where the first argument is the message ID, and the rest are key-value pairs specifying Fluent variables and their values.

For changing message IDs or associated variable names accross all FTL files, the

::

    cargo xtask fluent rename

command can be helpful.

Legacy gettext localization uses the ``wgettext!`` or ``wgettext_fmt!`` macros.
New code should use Fluent instead.

.. code:: rust

    streams.out.appendln(&wgettext_fmt!("%s: There are no jobs", argv[0]));

For explicit localization in fish scripts,
all messages must be enclosed in single or double quote characters
for our message extraction script to find them.
They must also be translated via a command substitution.
This means that the following are **not** valid:

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

Updating Dependencies
=====================

To update dependencies, run ``build_tools/update-dependencies.sh``.
This currently requires `updatecli <https://github.com/updatecli/updatecli>`__ and a few other tools.

Versioning
==========

The fish version is constructed by the *build_tools/git_version_gen.sh*
script. For developers the version is the branch name plus the output of
``git describe --always --dirty``. Normally the main part of the version
will be the closest annotated tag. Which itself is usually the most
recent release number (e.g., ``2.6.0``).
