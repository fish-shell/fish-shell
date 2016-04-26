### Import What You Use

http://include-what-you-use.org/
https://github.com/include-what-you-use/include-what-you-use

https://github.com/jasonmp85/homebrew-iwyu
brew tap jasonmp85/iwyu
brew install iwyu

# Guidelines For Developers

This document provides guidelines for making changes to the fish-shell project. This includes rules for how to format the code, naming conventions, etc. It also includes recommended best practices such as creating a Travis-CI account so you can verify your changes pass all the tests before making a pull-request.

See the bottom of this document for help on installing the linting and style reformatting tools discussed in the following sections.

Fish source should limit the C++ features it uses to those available in C++03. That allows fish to use a few components from [C++TR1](https://en.wikipedia.org/wiki/C%2B%2B_Technical_Report_1) such as `shared_ptr`. It also allows fish to be built and run on OS X Snow Leopard (released in 2009); the oldest OS X release we still support.

## Lint Free Code

Automated analysis tools like cppcheck and oclint can point out potential bugs. They also help ensure the code has a consistent style and that it avoids patterns that tend to confuse people.

Ultimately we want lint free code. However, at the moment a lot of cleanup is required to reach that goal. For now simply try to avoid introducing new lint.

To make linting the code easy there are two make targets: `lint` and `lint-all`. The latter does just what the name implies. The former will lint any modified but not committed `*.cpp` files. If there is no uncommitted work it will lint the files in the most recent commit.

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

before commiting your change. That will run `git-clang-format` to rewrite just the lines you're modifying.

If you've already committed your changes that's okay since it will then check the files in the most recent commit. This can be useful after you've merged someone elses change and want to check that it's style is acceptable. However, in that case it will run `clang-format` to ensure the entire file, not just the lines modified by the commit, conform to the style.

If you want to check the style of the entire code base run

```
make style-all
```

That command will refuse to restyle any files if you have uncommitted changes.

### Suppressing Reformatting of the Code

If you have a good reason for doing so you can tell `clang-format` to not reformat a block of code by enclosing it in comments like this:

```
// clang-format off
code to ignore
// clang-format on
```

## Fish Script Style Guide

Fish scripts such as those in the *share/functions* and *tests* directories should be formatted using the `fish_indent` command.

Function names should be all lowercase with undescores separating words. Private functions should begin with an underscore. The first word should be `fish` if the function is unique to fish.

The first word of global variable names should generally be `fish` for public vars or `_fish` for private vars to minimize the possibility of name clashes with user defined vars.

## C++ Style Guide

1. The [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) forms the basis of the fish C++ style guide. There are two major deviations for the fish project. First, a four, rather than two, space indent. Second, line lengths up to 100, rather than 80, characters.

1. The `clang-format` command is authoritative with respect to indentation, whitespace around operators, etc. **Note**: this rule should be ignored at this time. After the code is cleaned up this rule will become mandatory.

1. All names in code should be `small_snake_case`. No Hungarian notation is used. Classes and structs names should be followed by `_t`.

1. Always attach braces to the surrounding context.

1. Indent with spaces, not tabs and use four spaces per indent.

1. Comments should always use the C++ style; i.e., each line of the comment should begin with a `//` and should be limited to 100 characters. Comments that do not begin a line should be separated from the previous text by two spaces.

1. Comments that document the purpose of a function or class should begin with three slashes, `///`, so that OS X Xcode (and possibly other ideas) will extract the comment and show it in the "Quick Help" window when the cursor is on the symbol.

## Testing

The source code for fish includes a large collection of tests. If you are making any changes to fish, running these tests is highly recommended to make sure the behaviour remains consistent.

You are also strongly encouraged to add tests when changing the functionality of fish. Especially if you are fixing a bug to help ensure there are no regressions in the future (i.e., we don't reintroduce the bug).

### Local testing

The tests can be run on your local computer on all operating systems.

Running the tests is only supported from the autotools build and not xcodebuild. On OS X, you will need to install autoconf &mdash; we suggest using [Homebrew](http://brew.sh/) to install these tools.

    autoconf
    ./configure
    make test [gmake on BSD]

### Travis CI Build and Test

The Travis Continuous Integration services can be used to test your changes using multiple configurations. This is the same service that the fish shell project uses to ensure new changes haven't broken anything. Thus it is a really good idea that you leverage Travis CI before making a pull-request to avoid embarrasment at breaking the build.

You will need to [fork the fish-shell repository on GitHub](https://help.github.com/articles/fork-a-repo/). Then setup Travis to test your changes before you make a pull-request:

1. [Sign in to Travis CI](https://travis-ci.org/auth) with your GitHub account, accepting the GitHub access permissions confirmation.
1. Once you're signed in, and your repositories are synchronised, go to your [profile page](https://travis-ci.org/profile) and enable the fish-shell repository.
1. Push your changes to GitHub.

You'll receive an email when the tests are complete telling you whether or not any tests failed.

You'll find the configuration used to control Travis in the `.travis.yml` file.

## Installing the Required Tools

### Installing the Linting Tools

To install the lint checkers on Mac OS X using HomeBrew:

```
brew tap oclint/formulae
brew install oclint
brew install cppcheck
```

To install the lint checkers on Linux distros that use Apt:

```
sudo apt-get install clang
sudo apt-get install oclint
sudo apt-get install cppcheck
```

### Installing the Reformatting Tools

To install the reformatting tool on Mac OS X using HomeBrew:

```
brew install clang-format
```

To install the reformatting tool on Linux distros that use Apt:

```
apt-cache search clang-format
```

That will list the versions available. Pick the newest one available (3.6 for Ubuntu 14.04 as I write this) and install it:

```
sudo apt-get install clang-format-3.6
sudo ln -s /usr/bin/clang-format-3.6 /usr/bin/clang-format

```
