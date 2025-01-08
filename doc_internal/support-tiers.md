# "Support" Tierlist

This document is guidance on what platforms and terminals fish supports to what degree.

It is supposed to explain to users to what degree they can expect fish to work, and to help us decide when features impact compatibility.

In an ideal world, everything would work everywhere. But of course, we have limited resources and need to not only decide where to put them,
but also see where they currently *happen to be*. And there are times where we need to decide between supporting most users better,
and supporting a small group of users. This may even mean dropping platforms or terminals when we can't make them work without impacting
more important ones.

It is also a list of targets we want to check before release.

For the purposes of this list, terminal multiplexers like tmux and other forms of terminal-in-a-terminal like mc are "terminals".

None of this is a legal document, and fish remains developed by hobbyists in their free time. Nobody is allowed to call me up at night to fix any of this.

## The Tiers

1. Working git at all times - can you expect a random checkout to be usable on this particular platform/terminal?
   If this is broken, we should revert the offending commits ASAP.
2. Tested before release, we don't want to ship with it broken
3. We hope this works and will try to fix it if it doesn't, but no guarantees
4. We don't care. If it works, that's cool.

These tiers depend on how important a given target is to us, and how much information we get about it.

That means, a tier 1 target requires a core developer to run it regularly or enough users that we are confident we will catch bugs quickly,
and working CI for operating systems/architectures.
Since we don't have automated testing of terminals, CI isn't required there.

A tier 2 target needs us to invest a conscious effort to test it before release, but of course if we don't run it ourselves we may not catch things
that aren't covered in tests. Users can help here. So a tier 2 target is better the more it is used. Note: We may decide to skip some tests for patch releases.

A tier 3 target is best effort. These can move up if they become more important, or someone on the core team uses and maintains it.
Or it can move down if it conflicts with improving other targets.

A tier 4 target is one that we aren't interested in, bug reports will most likely be closed, but we may invest some time trying to help.
If this works often enough it may be promoted to tier 3. We may also reject patches if they are too invasive.

The distinction between tier 1 and 2 is really just an expression of our confidence that any random git checkout or release will work.
The distinction between tier 2 and 3 is much more important - these aren't supported beyond "we'll try if you tell us".
And tier 4 is just a list of platforms we know fish doesn't work on.

## The current targets: Operating Systems / Architectures

Unless anything else is mentioned, these refer to current mainstream versions.

Tier 1:

- Linux, with current glibc, on x86_64
- macOS, aarch64

- WSL (as "basically glibc linux" with some use among the core developers)

Tier 2+ (have CI but not enough use among core devs):

- Linux, musl, x86_64
- Linux, older glibc, x86_64
- FreeBSD

Tier 2:

- NetBSD, x86_64
- Linux, glibc, aarch64?
- macOS x86_64?

Tier 3:

- OpenBSD
- OpenSolaris/Illumos/OpenIndiana?
- Architectures other than x86_64 and aarch64 (and possibly i686)

Tier 4:
- Architectures and operating systems not supported by rust.
- Cygwin/Msys
- Haiku?
- [...]
- [...]
- [Long scroll, look at your watch or phone]
- [...]
- Windows native
- Nonstop

## The current targets: Terminals

These are expected to be used in their majority configuration - usually the defaults.
No switching $TERM required.

Tier 1:

- Konsole
- MS Terminal / Conhost
- Gnome Terminal
- tmux (used in our CI)
- foot
- other terms the core devs use

Tier 2:

- Ghostty
- Wezterm
- Alacritty
- Kitty
- xterm
- iTerm?
- Terminal.app?
- VSCode / xterm.js

Tier 3:

- putty?
- linux tty?
- freebsd tty?
- rxvt(-unicode)
- Midnight Commander?
- termux
- screen?
- st?
- zellij?

Tier 4:

- emacs ansi-term
- amazon web console
- dvtm
- cool-retro-term
