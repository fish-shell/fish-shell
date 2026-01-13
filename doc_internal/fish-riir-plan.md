These is a proposed port of fish-shell from C++ to Rust, and from CMake to cargo or related. This document is high level - see the [Development Guide] for more details.

## Why Port

- Gain access to more contributors and enable easier contributions. C++ is becoming a legacy language.
- Free us from the annoyances of C++/CMake, and old toolchains.
- Ensure fish continues to be perceived as modern and relevant.
- Unlock concurrent mode (see below).

## Why Rust

- Rust is a systems programming language with broad platform support, a large community, and a relatively high probability of still being relevant in a decade.
- Rust has a unique strength in its thread safety features, which is the missing piece to enable concurrent mode - see below.
- Other languages considered:
  - Java, Python and the scripting family are ruled out for startup latency and memory usage reasons.
  - Go would be an awkward fit. fork is [quite the problem](https://stackoverflow.com/questions/28370646/how-do-i-fork-a-go-process/28371586#28371586) in Go.
  - Other system languages (D, Nim, Zig...) are too niche: fewer contributors, higher risk of the language becoming irrelevant.

## Risks

- Large amount of work with possible introduction of new bugs.
- Long period of complicated builds.
- Existing contributors will have to learn Rust.
- As of yet unknown compatibility story for Tier 2+ platforms (Cygwin, etc).

## Approach

We will do an **incremental port** in the span of one release. We will have a period of using both C++ and Rust, and both cargo and CMake, leveraging FFI tools (see below).

The work will **proceed on master**: no long-lived branches. Tests and CI continue to pass at every commit for recent Linux and Mac. Centos7, \*BSD, etc may be temporarily disabled if they prove problematic.

The Rust code will initially resemble the replaced C++. Fidelity to existing code is more important than Rust idiomaticity, to aid review and bisecting. But don't take this to extremes - use judgement.

The port will proceed "outside in." We'll start with leaf components (e.g. builtins) and proceed towards the core. Some components will have both a Rust and C++ implementation (e.g. flog), in other cases we'll change the existing C++ to invoke the new Rust implementations (builtins).

After porting the C++, we'll replace CMake.

We will continue to use wide chars, locales, gettext, printf format strings, and PCRE2. We will not change the fish scripting language at all. We will _not_ use this as an opportunity to fix existing design flaws, with a few carefully chosen exceptions. See [Strings](#strings).

We will not use tokio, serde, async, or other fancy Rust frameworks initially.

### FFI

Rust/C++ interop will use [autocxx](https://github.com/google/autocxx), [Cxx](https://cxx.rs), and possibly [bindgen](https://rust-lang.github.io/rust-bindgen/). I've forked these for fish (see the [Development Guide]). Once the port is done, we will stop using them, except perhaps bindgen for PCRE2.

We will use [corrosion](https://github.com/corrosion-rs/corrosion) for CMake integration.

Inefficiencies (e.g. extra string copying) at the FFI layer are fine, since it will all get thrown away.

Tests can stay in fish_tests.cpp or be moved into Rust .rs files; either is fine.

### Strings

Rust's `String` / `&str` types cannot represent non-UTF8 filenames or data using the default encoding scheme. That's why all string conversions must go through fish's encoding scheme (using the private-use area to encode invalid sequences). For example, fish cannot use `File::open` with a `&str` because the decoding will be incorrect.

So instead of `String`, fish will use its own string type, and manage encoding and decoding as it does today. However we will make some specific changes:

1. Drop the nul-terminated requirement. When passing `const wchar_t*` back to C++, we will allocate and copy into a nul-terminated buffer.
2. Drop support for 16-bit wchar. fish will use UTF32 on all platforms, and manage conversions itself.

After the port we can consider moving to UTF-8, for memory usage reasons.

See the [Rust Development Guide][Development Guide] for more on strings.

### Thread Safety

Allowing [background functions](https://github.com/fish-shell/fish-shell/issues/238) and concurrent functions has been a goal for many years. I have been nursing [a long-lived branch](https://github.com/ridiculousfish/fish-shell/tree/concurrent_even_simpler) which allows full threaded execution. But though the changes are small, I have been reluctant to propose them, because they will make reasoning about the shell internals too complex: it is difficult in C++ to check and enforce what crosses thread boundaries.

This is Rust's bread and butter: we will encode thread requirements into our types, making it explicit and compiler-checked, via Send and Sync. Rust will allow turning on concurrent mode in a safe way, with a manageable increase in complexity, finally enabling this feature.

## Timeline

Handwaving, 6 months? Frankly unknown - there's 102 remaining .cpp files of various lengths. It'll go faster as we get better at it. Peter (ridiculous_fish) is motivated to work on this, other current contributors have some Rust as well, and we may also get new contributors from the Rust community. Part of the point is to make contribution easier.

## Links

- [Packaging Rust projects](https://wiki.archlinux.org/title/Rust_package_guidelines) from Arch Linux

[Development Guide]: rust-devel.md
