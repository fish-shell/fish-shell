# Guidelines For Developers

This document provides guidelines for making changes to the fish-shell project. This includes rules for how to format the code, naming conventions, et cetera. Generally known as the style of the code. It also includes recommended best practices such as creating a Travis CI account so you can verify that your changes pass all the tests before making a pull request.

See the bottom of this document for help on installing the linting and style reformatting tools discussed in the following sections.

Fish source should limit the C++ features it uses to those available in C++11. It should not use exceptions.

Before introducing a new dependency, please make it optional with graceful failure if possible. Add
any new dependencies to the README.md under the *Running* and/or *Building* sections.

## Versioning

The fish version is constructed by the *build_tools/git_version_gen.sh* script. For developers the version is the branch name plus the output of `git describe --always --dirty`. Normally the main part of the version will be the closest annotated tag. Which itself is usually the most recent release number (e.g., `2.6.0`).

## Include What You Use

You should not depend on symbols being visible to a `*.cpp` module from `#include` statements inside another header file. In other words if your module does `#include "common.h"` and that header does `#include "signal.h"` your module should not assume the sub-include is present. It should instead directly `#include "signal.h"` if it needs any symbol from that header. That makes the actual dependencies much clearer. It also makes it easy to modify the headers included by a specific header file without having to worry that will break any module (or header) that includes a particular header.

To help enforce this rule the `make lint` (and `make lint-all`) command will run the [include-what-you-use](https://include-what-you-use.org/) tool. You can find the IWYU project on [github](https://github.com/include-what-you-use/include-what-you-use).

To install the tool on OS X you'll need to add a [formula](https://github.com/jasonmp85/homebrew-iwyu) then install it:

```
brew tap jasonmp85/iwyu
brew install iwyu
```

On Ubuntu you can install it via `apt-get`:

```
sudo apt-get install iwyu
```

## Lint Free Code

Automated analysis tools like cppcheck and oclint can point out potential bugs or code that is extremely hard to understand. They also help ensure the code has a consistent style and that it avoids patterns that tend to confuse people.

Ultimately we want lint free code. However, at the moment a lot of cleanup is required to reach that goal. For now simply try to avoid introducing new lint.

To make linting the code easy there are two make targets: `lint` and `lint-all`. The latter does only what the name implies. The former will lint any modified but not committed `*.cpp` files. If there is no uncommitted work it will lint the files in the most recent commit.

Fish has custom cppcheck rules in the file `.cppcheck.rule`. These help catch mistakes such as using `wcwidth()` rather than `fish_wcwidth()`. Please add a new rule if you find similar mistakes being made.

Fish also depends on `diff` and `expect` for its tests.

### Dealing With Lint Warnings

You are strongly encouraged to address a lint warning by refactoring the code, changing variable names, or whatever action is implied by the warning.

### Suppressing Lint Warnings

Once in a while the lint tools emit a false positive warning. For example, cppcheck might suggest a memory leak is present when that is not the case. To suppress that cppcheck warning you should insert a line like the following immediately prior to the line cppcheck warned about:

```
// cppcheck-suppress memleak // addr not really leaked
```

The explanatory portion of the suppression comment is optional. For other types of warnings replace "memleak" with the value inside the parenthesis (e.g., "nullPointerRedundantCheck") from a warning like the following:

```
[src/complete.cpp:1727]: warning (nullPointerRedundantCheck): Either the condition 'cmd_node' is redundant or there is possible null pointer dereference: cmd_node.
```

Suppressing oclint warnings is more complicated to describe so I'll refer you to the [OCLint HowTo](http://docs.oclint.org/en/latest/howto/suppress.html#annotations) on the topic.

## Ensuring Your Changes Conform to the Style Guides

The following sections discuss the specific rules for the style that should be used when writing fish code. To ensure your changes conform to the style rules you simply need to run

```
make style
```

before committing your change. That will run `git-clang-format` to rewrite only the lines you're modifying.

If you've already committed your changes that's okay since it will then check the files in the most recent commit. This can be useful after you've merged another person's change and want to check that it's style is acceptable. However, in that case it will run `clang-format` to ensure the entire file, not just the lines modified by the commit, conform to the style.

If you want to check the style of the entire code base run

```
make style-all
```

That command will refuse to restyle any files if you have uncommitted changes.

### Configuring Your Editor for Fish C++ Code

#### ViM

As of ViM 7.4 it does not recognize triple-slash comments as used by Doxygen and the OS X Xcode IDE to flag comments that explain the following C symbol. This means the `gq` key binding to reformat such comments doesn't behave as expected. You can fix that by adding the following to your vimrc:

```
autocmd Filetype c,cpp setlocal comments^=:///
```

If you use ViM I recommend the [vim-clang-format plugin](https://github.com/rhysd/vim-clang-format) by [@rhysd](https://github.com/rhysd).

You can also get ViM to provide reasonably correct behavior by installing

http://www.vim.org/scripts/script.php?script_id=2636

#### Emacs

If you use Emacs: TBD

### Configuring Your Editor for Fish Scripts

If you use ViM: Install [vim-fish](https://github.com/dag/vim-fish), make sure you have syntax and filetype functionality in `~/.vimrc`:

```
syntax enable
filetype plugin indent on
```

Then turn on some options for nicer display of fish scripts in `~/.vim/ftplugin/fish.vim`:

```
" Set up :make to use fish for syntax checking.
compiler fish

" Set this to have long lines wrap inside comments.
setlocal textwidth=79

" Enable folding of block structures in fish.
setlocal foldmethod=expr
```

If you use Emacs: Install [fish-mode](https://github.com/wwwjfy/emacs-fish) (also available in melpa and melpa-stable) and `(setq-default indent-tabs-mode nil)` for it (via a hook or in `use-package`s ":init" block). It can also be made to run fish_indent via e.g.

```elisp
(add-hook 'fish-mode-hook (lambda ()
    (add-hook 'before-save-hook 'fish_indent-before-save)))
```

### Suppressing Reformatting of C++ Code

If you have a good reason for doing so you can tell `clang-format` to not reformat a block of code by enclosing it in comments like this:

```
// clang-format off
code to ignore
// clang-format on
```

However, as I write this there are no places in the code where we use this and I can't think of any legitimate reasons for exempting blocks of code from clang-format.

## Fish Script Style Guide

1. All fish scripts, such as those in the *share/functions* and *tests* directories, should be formatted using the `fish_indent` command.

1. Function names should be in all lowercase with words separated by underscores. Private functions should begin with an underscore. The first word should be `fish` if the function is unique to fish.

1. The first word of global variable names should generally be `fish` for public vars or `_fish` for private vars to minimize the possibility of name clashes with user defined vars.

## C++ Style Guide

1. The [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) forms the basis of the fish C++ style guide. There are two major deviations for the fish project. First, a four, rather than two, space indent. Second, line lengths up to 100, rather than 80, characters.

1. The `clang-format` command is authoritative with respect to indentation, whitespace around operators, etc.

1. All names in code should be `small_snake_case`. No Hungarian notation is used. The names for classes and structs should be followed by `_t`.

1. Always attach braces to the surrounding context.

1. Indent with spaces, not tabs and use four spaces per indent.

1. Comments should always use the C++ style; i.e., each line of the comment should begin with a `//` and should be limited to 100 characters. Comments that do not begin a line should be separated from the previous text by two spaces.

1. Comments that document the purpose of a function or class should begin with three slashes, `///`, so that OS X Xcode (and possibly other IDEs) will extract the comment and show it in the "Quick Help" window when the cursor is on the symbol.

## Testing

The source code for fish includes a large collection of tests. If you are making any changes to fish, running these tests is mandatory to make sure the behaviour remains consistent and regressions are not introduced. Even if you don't run the tests on your machine, they will still be run via the [Travis CI](https://travis-ci.org/fish-shell/fish-shell) service.

You are strongly encouraged to add tests when changing the functionality of fish, especially if you are fixing a bug to help ensure there are no regressions in the future (i.e., we don't reintroduce the bug).

### Local testing

The tests can be run on your local computer on all operating systems.

Running the tests is only supported from the autotools build and not xcodebuild. On OS X, you will need to install autoconf &mdash; we suggest using [Homebrew](https://brew.sh/) to install these tools.

```
autoconf
./configure
make test # or "gmake test" on BSD
```

### Travis CI Build and Test

The Travis Continuous Integration services can be used to test your changes using multiple configurations. This is the same service that the fish-shell project uses to ensure new changes haven't broken anything. Thus it is a really good idea that you leverage Travis CI before making a pull request to avoid potential embarrassment at breaking the build.

You will need to [fork the fish-shell repository on GitHub](https://help.github.com/articles/fork-a-repo/), then setup Travis to test your changes before making a pull request.

1. [Sign in to Travis CI](https://travis-ci.org/auth) with your GitHub account, accepting the GitHub access permissions confirmation.
1. Once you're signed in and your repositories are synchronized, go to your [profile page](https://travis-ci.org/profile) and enable the fish-shell repository.
1. Push your changes to GitHub.

You'll receive an email when the tests are complete telling you whether or not any tests failed.

You'll find the configuration used to control Travis in the `.travis.yml` file.

### Git hooks

Since developers sometimes forget to run the tests, it can be helpful to use git hooks (see githooks(5)) to automate it.

One possibility is a pre-push hook script like this one:

```sh
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
```

This will check if the push is to the master branch and, if it is, only allow the push if running `make test` succeeds. In some circumstances it may be advisable to circumvent this check with `git push --no-verify`, but usually that isn't necessary.

To install the hook, place the code in a new file `.git/hooks/pre-push` and make it executable.

### Coverity Scan

We use Coverity's static analysis tool which offers free access to open source projects. While access to the tool itself is restricted, fish-shell organization members should know that they can login [here](https://scan.coverity.com/projects/fish-shell-fish-shell?tab=overview) with their GitHub account. Currently, tests are triggered upon merging the `master` branch into `coverity_scan_master`. Even if you are not a fish developer, you can keep an eye on our statistics there.

## Installing the Required Tools

### Installing the Linting Tools

To install the lint checkers on Mac OS X using Homebrew:

```
brew tap oclint/formulae
brew install oclint
brew install cppcheck
```

To install the lint checkers on Debian-based Linux distributions:

```
sudo apt-get install clang
sudo apt-get install oclint
sudo apt-get install cppcheck
```

### Installing the Reformatting Tools

Mac OS X:

```
brew install clang-format
```

Debian-based:

```
apt-cache search clang-format
```

Above will list all the versions available. Pick the newest one available (3.9 for Ubuntu 16.10 as I write this) and install it:

```
sudo apt-get install clang-format-3.9
sudo ln -s /usr/bin/clang-format-3.9 /usr/bin/clang-format
```

## Message Translations

Fish uses the GNU gettext library to translate messages from English to other languages. To create or update a translation run `make po/[LANGUAGE CODE].po` where `LANGUAGE CODE` is the two letter ISO 639-1 language code of the language you are translating to (e.g. `de` for German). Make sure that you have the `xgettext`, `msgfmt` and `msgmerge` commands installed in order to do this.

All messages in fish script must be enclosed in single or double quote characters. They must also be translated via a subcommand. This means that the following are **not** valid:

```
echo (_ hello)
_ "goodbye"
```

Above should be written like this instead:

```
echo (_ "hello")
echo (_ "goodbye")
```

Note that you can use either single or double quotes to enclose the message to be translated. You can also optionally include spaces after the opening parentheses and once again before the closing parentheses.

Be cautious about blindly updating an existing translation file. Trivial changes to an existing message (e.g., changing the punctuation) will cause existing translations to be removed, since the tools do literal string matching. Therefore, in general, you need to carefully review any recommended deletions.

Read the [translations wiki](https://github.com/fish-shell/fish-shell/wiki/Translations) for more information.
