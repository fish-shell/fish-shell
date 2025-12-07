# fish-shell Rust Development Guide

This describes how to get started building fish-shell in its partial Rust state, and how to contribute to the port.

## Overview

fish is in the process of transitioning from C++ to Rust. The fish project has a Rust crate embedded at path `fish-rust`. This crate builds a Rust library `libfish_rust.a` which is linked with the C++ `libfish.a`. Existing C++ code will be incrementally migrated to this crate; then CMake will be replaced with cargo and other Rust-native tooling.

Important tools used during this transition:

1. [Corrosion](https://github.com/corrosion-rs/corrosion) to invoke cargo from CMake.
2. [cxx](http://cxx.rs) for basic C++ <-> Rust interop.
3. [autocxx](https://google.github.io/autocxx/) for using C++ types in Rust.

We use forks of the last two - see the [FFI section](#ffi) below. No special action is required to obtain these packages. They're downloaded by cargo.

## Building

### Build Dependencies

fish-shell currently depends on Rust 1.85 or later. To install Rust, follow https://rustup.rs.

### Build via CMake

It is recommended to build inside `fish-shell/build`. This will make it easier for Rust to find the `config.h` file.

Build via CMake as normal (use any generator, here we use Ninja):

```shell
$ cd fish-shell
$ mkdir build && cd build
$ cmake -G Ninja ..
$ ninja
```

This will create the usual fish executables.

### Build just libfish_rust.a with Cargo

The directory `fish-rust` contains the Rust sources. These require that CMake has been run to produce `config.h` which is necessary for autocxx to succeed.

Follow the "Build from CMake" steps above, and then:

```shell
$ cd fish-shell/fish-rust
$ cargo build
```

This will build only the library, not a full working fish, but it allows faster iteration for Rust development. That is, after running `cmake` you can open the `fish-rust` as the root of a Rust crate, and tools like rust-analyzer will work.

## Development

The basic development loop for this port:

1. Pick a .cpp (or in some cases .h) file to port, say `util.cpp`.
2. Add the corresponding `util.rs` file to `fish-rust/`.
3. Reimplement it in Rust, along with its dependencies as needed. Match the existing C++ code where practical, including propagating any relevant comments.
   - Do this even if it results in less idiomatic Rust, but avoid being super-dogmatic either way.
   - One technique is to paste the C++ into the Rust code, commented out, and go line by line.
4. Decide whether any existing C++ callers should invoke the Rust implementation, or whether we should keep the C++ one.
   - Utility functions may have both a Rust and C++ implementation. An example is `FLOG` where interop is too hard.
   - Major components (e.g. builtin implementations) should _not_ be duplicated; instead the Rust should call C++ or vice-versa.
5. Remember to run `cargo fmt` and `cargo clippy` to keep the codebase somewhat clean (otherwise CI will fail). If you use rust-analyzer, you can run clippy automatically by setting `rust-analyzer.checkOnSave.command = "clippy"`.

You will likely run into limitations of [`autocxx`](https://google.github.io/autocxx/) and to a lesser extent [`cxx`](https://cxx.rs/). See the [FFI sections](#ffi) below.

## Type Mapping

### Constants & Type Aliases

The FFI does not support constants (`#define` or `static const`) or type aliases (`typedef`, `using`). Duplicate them using their Rust equivalent (`pub const` and `type`/`struct`/`enum`).

### Non-POD types

Many types cannot currently be passed across the language boundary by value or occur in shared structs. As a workaround, use references, raw pointers or smart pointers (`cxx` provides `SharedPtr` and `UniquePtr`). Try to keep workarounds on the C++ side and the FFI layer of the Rust code. This ensures we will get rid of the workarounds as we peel off the FFI layer.

### Strings

Fish will mostly _not_ use Rust's `String/&str` types as these cannot represent non-UTF8 data using the default encoding.

fish's primary string types will come from the [`widestring` crate](https://docs.rs/widestring). The two main string types are `WString` and `&wstr`, which are renamed [Utf32String](https://docs.rs/widestring/latest/widestring/utfstring/struct.Utf32String.html) and [Utf32Str](https://docs.rs/widestring/latest/widestring/utfstr/struct.Utf32Str.html). `WString` is an owned, heap-allocated UTF32 string, `&wstr` a borrowed UTF32 slice.

In general, follow this mapping when porting from C++:

- `wcstring` -> `WString`
- `const wcstring &` -> `&wstr`
- `const wchar_t *` -> `&wstr`

None of the Rust string types are nul-terminated. We're taking this opportunity to drop the nul-terminated aspect of wide string handling.

#### Creating strings

One may create a `&wstr` from a string literal using the `wchar::L!` macro:

```rust
use crate::wchar::prelude::*;
// This imports wstr, the L! macro, WString, a ToWString trait that supplies .to_wstring() along with other things

fn get_shell_name() -> &'static wstr {
    L!("fish")
}
```

There is also a `widestrs` proc-macro which enables L as a _suffix_, to reduce the noise. This can be applied to any block, including modules and individual functions:

```rust
use crate::wchar::{wstr, widestrs}
// also imported by the prelude

#[widestrs]
fn get_shell_name() -> &'static wstr {
    "fish"L // equivalent to L!("fish")
}
```

#### The wchar prelude

We have a prelude to make working with these string types a whole lot more ergonomic. In particular `WExt` supplies the null-terminated-compatible `.char_at(usize)`,
and a whole lot more methods that makes porting C++ code easier. It is also preferred to use char-based-methods like `.char_count()` and `.slice_{from,to}()`
of the `WExt` trait over directly calling `.len()` and `[usize..]/[..usize]`, as that makes the code compatible with a potential future change to UTF8-strings.

```rust
pub(crate) mod prelude {
    pub(crate) use crate::{
        wchar::{wstr, IntoCharIter, WString, L},
        wchar_ext::{ToWString, WExt},
        wutil::{sprintf, wgettext, wgettext_fmt, wgettext_str},
    };
    pub(crate) use widestring_suffix::widestrs;
}
```

### Strings for FFI

`WString` and `&wstr` are the common strings used by Rust components. At the FII boundary there are some additional strings for interop. _All of these are temporary for the duration of the port._

- `CxxWString` is the Rust binding of `std::wstring`. It is the wide-string analog to [`CxxString`](https://cxx.rs/binding/cxxstring.html) and is [added in our fork of cxx](https://github.com/ridiculousfish/cxx/blob/fish/src/cxx_wstring.rs). This is useful for functions which return e.g. `const wcstring &`.
- `W0String` is renamed [U32CString](https://docs.rs/widestring/latest/widestring/ucstring/struct.U32CString.html). This is basically `WString` except it _is_ nul-terminated. This is useful for getting a nul-terminated `const wchar_t *` to pass to C++ implementations.
- `wcharz_t` is an annoying C++ struct which merely wraps a `const wchar_t *`, used for passing these pointers from C++ to Rust. We would prefer to use `const wchar_t *` directly but `autocxx` refuses to generate bindings for types such as `std::vector<const wchar_t *>` so we wrap it in this silly struct.

Note C++ `wchar_t`, Rust `char`, and `u32` are effectively interchangeable: you can cast pointers to them back and forth (except we check upon u32->char conversion). However be aware of which types are nul-terminated.

These types should be confined to the FFI modules, in particular `wchar_ffi`. They should not "leak" into other modules. See the `wchar_ffi` module.

### Format strings

Rust's builtin `std::fmt` modules do not accept runtime-provided format strings, so we mostly won't use them, except perhaps for FLOG / other non-translated text.

Instead we'll continue to use printf-style strings, with a Rust printf implementation.

### Vectors

See [`Vec`](https://cxx.rs/binding/vec.html) and [`CxxVector`](https://cxx.rs/binding/cxxvector.html).

In many cases, `autocxx` refuses to allow vectors of certain types. For example, autocxx supports `std::vector` and `std::shared_ptr` but NOT `std::vector<std::shared_ptr<...>>`. To work around this one can create a helper (pointer, length) struct. Example:

```cpp
struct RustFFIJobList {
    std::shared_ptr<job_t> *jobs;
    size_t count;
};
```

This is just a POD (plain old data) so autocxx can generate bindings for it. Then it is trivial to convert it to a Rust slice:

```
pub fn get_jobs(ffi_jobs: &ffi::RustFFIJobList) -> &[SharedPtr<job_t>] {
    unsafe { slice::from_raw_parts(ffi_jobs.jobs, ffi_jobs.count) }
}
```

Another workaround is to define a struct that contains the shared pointer, and create a vector of that struct.

## Development Tooling

The [autocxx guidance](https://google.github.io/autocxx/workflow.html#how-can-i-see-what-bindings-autocxx-has-generated) is helpful:

1. Install cargo expand (`cargo install cargo-expand`). Then you can use `cargo expand` to see the generated Rust bindings for C++. In particular this is useful for seeing failed expansions for C++ types that autocxx cannot handle.
2. In rust-analyzer, enable Proc Macro and Proc Macro Attributes.

## FFI

The boundary between Rust and C++ is referred to as the Foreign Function Interface, or FFI.

`autocxx` and `cxx` both are designed for long-term interop: C++ and Rust coexisting for years. To this end, both emphasize safety: requiring lots of `unsafe`, `Pin`, etc.

fish plans to use them only temporarily, with a focus on getting things working. To this end, both cxx and autocxx have been forked to support fish:

1. Relax the requirement that all functions taking pointers are `unsafe` (this just added noise).
2. Add support for `wchar_t` as a recognized type, and `CxxWString` analogous to `CxxString`.

See the `Cargo.toml` file for the locations of the forks.
