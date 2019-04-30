.. _design:

Design
======

This is a description of the design principles that have been used to design fish. The fish design has three high level goals. These are:

1. Everything that can be done in other shell languages should be possible to do in fish, though fish may rely on external commands in doing so.

2. Fish should be user friendly, but not at the expense of expressiveness. Most tradeoffs between power and ease of use can be avoided with careful design.

3. Whenever possible without breaking the above goals, fish should follow the Posix syntax.

To achieve these high-level goals, the fish design relies on a number of more specific design principles. These are presented below, together with a rationale and a few examples for each.


The law of orthogonality
------------------------

The shell language should have a small set of orthogonal features. Any situation where two features are related but not identical, one of them should be removed, and the other should be made powerful and general enough to handle all common use cases of either feature.

Rationale:

Related features make the language larger, which makes it harder to learn. It also increases the size of the source code, making the program harder to maintain and update.

Examples:

- Here documents are too similar to using echo inside of a pipeline.

- Subshells, command substitution and process substitution are strongly related. ``fish`` only supports command substitution, the others can be achieved either using a block or the psub shellscript function.

- Having both aliases and functions is confusing, especially since both of them have limitations and problems. ``fish`` functions have none of the drawbacks of either syntax.

- The many Posix quoting styles are silly, especially $''.


The law of responsiveness
-------------------------

The shell should attempt to remain responsive to the user at all times, even in the face of contended or unresponsive filesystems. It is only acceptable to block in response to a user initiated action, such as running a command.

Rationale:
Bad performance increases user-facing complexity, because it trains users to recognize and route around slow use cases. It is also incredibly frustrating.

Examples:

- Features like syntax highlighting and autosuggestions must perform all of their disk I/O asynchronously.

- Startup should minimize forks and disk I/O, so that fish can be started even if the system is under load.

Configurability is the root of all evil
---------------------------------------

Every configuration option in a program is a place where the program is too stupid to figure out for itself what the user really wants, and should be considered a failure of both the program and the programmer who implemented it.

Rationale:
Different configuration options are a nightmare to maintain, since the number of potential bugs caused by specific configuration combinations quickly becomes an issue. Configuration options often imply assumptions about the code which change when reimplementing the code, causing issues with backwards compatibility. But mostly, configuration options should be avoided since they simply should not exist, as the program should be smart enough to do what is best, or at least a good enough approximation of it.

Examples:

- Fish allows the user to set various syntax highlighting colors. This is needed because fish does not know what colors the terminal uses by default, which might make some things unreadable. The proper solution would be for text color preferences to be defined centrally by the user for all programs, and for the terminal emulator to send these color properties to fish.

- Fish does not allow you to set the number of history entries, different language substyles or any number of other common shell configuration options.

A special note on the evils of configurability is the long list of very useful features found in some shells, that are not turned on by default. Both zsh and bash support command-specific completions, but no such completions are shipped with bash by default, and they are turned off by default in zsh. Other features that zsh supports that are disabled by default include tab-completion of strings containing wildcards, a sane completion pager and a history file.

The law of user focus
---------------------

When designing a program, one should first think about how to make an intuitive and powerful program. Implementation issues should only be considered once a user interface has been designed.

Rationale:

This design rule is different than the others, since it describes how one should go about designing new features, not what the features should be. The problem with focusing on what can be done, and what is easy to do, is that too much of the implementation is exposed. This means that the user must know a great deal about the underlying system to be able to guess how the shell works, it also means that the language will often be rather low-level.

Examples:
- There should only be one type of input to the shell, lists of commands. Loops, conditionals and variable assignments are all performed through regular commands.

- The differences between built-in commands and shellscript functions should be made as small as possible. Built-ins and shellscript functions should have exactly the same types of argument expansion as other commands, should be possible to use in any position in a pipeline, and should support any I/O redirection.

- Instead of forking when performing command substitution to provide a fake variable scope, all fish commands are performed from the same process, and fish instead supports true scoping.

- All blocks end with the ``end`` built-in.

The law of discoverability
--------------------------

A program should be designed to make its features as easy as possible to discover for the user.

Rationale:
A program whose features are discoverable turns a new user into an expert in a shorter span of time, since the user will become an expert on the program simply by using it.

The main benefit of a graphical program over a command-line-based program is discoverability. In a graphical program, one can discover all the common features by simply looking at the user interface and guessing what the different buttons, menus and other widgets do. The traditional way to discover features in command-line programs is through manual pages. This requires both that the user starts to use a different program, and then they remember the new information until the next time they use the same program.

Examples:
- Everything should be tab-completable, and every tab completion should have a description.

- Every syntax error and error in a built-in command should contain an error message describing what went wrong and a relevant help page. Whenever possible, errors should be flagged red by the syntax highlighter.

- The help manual should be easy to read, easily available from the shell, complete and contain many examples

- The language should be uniform, so that once the user understands the command/argument syntax, they will know the whole language, and be able to use tab-completion to discover new features.
