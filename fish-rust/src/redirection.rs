//! This file supports specifying and applying redirections.

use crate::ffi::wcharz_t;
use crate::io::IoChain;
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharToFFI;
use crate::wutil::fish_wcstoi;
use cxx::{CxxVector, CxxWString, SharedPtr, UniquePtr};
use libc::{c_int, O_APPEND, O_CREAT, O_EXCL, O_RDONLY, O_TRUNC, O_WRONLY};
use std::os::fd::RawFd;

#[cxx::bridge]
mod redirection_ffi {
    extern "C++" {
        include!("wutil.h");
        type wcharz_t = super::wcharz_t;
    }

    enum RedirectionMode {
        overwrite, // normal redirection: > file.txt
        append,    // appending redirection: >> file.txt
        input,     // input redirection: < file.txt
        fd,        // fd redirection: 2>&1
        noclob,    // noclobber redirection: >? file.txt
    }

    extern "Rust" {
        type RedirectionSpec;

        fn is_close(self: &RedirectionSpec) -> bool;
        #[cxx_name = "get_target_as_fd"]
        fn get_target_as_fd_ffi(self: &RedirectionSpec) -> SharedPtr<i32>;
        fn oflags(self: &RedirectionSpec) -> i32;

        fn fd(self: &RedirectionSpec) -> i32;
        fn mode(self: &RedirectionSpec) -> RedirectionMode;
        fn target(self: &RedirectionSpec) -> UniquePtr<CxxWString>;
        fn new_redirection_spec(
            fd: i32,
            mode: RedirectionMode,
            target: wcharz_t,
        ) -> Box<RedirectionSpec>;

        type RedirectionSpecListFfi;
        fn new_redirection_spec_list() -> Box<RedirectionSpecListFfi>;
        fn size(self: &RedirectionSpecListFfi) -> usize;
        fn at(self: &RedirectionSpecListFfi, offset: usize) -> *const RedirectionSpec;
        fn push_back(self: &mut RedirectionSpecListFfi, spec: Box<RedirectionSpec>);
        fn clone(self: &RedirectionSpecListFfi) -> Box<RedirectionSpecListFfi>;
    }

    /// A type that represents the action dup2(src, target).
    /// If target is negative, this represents close(src).
    /// Note none of the fds here are considered 'owned'.
    #[derive(Clone, Copy)]
    struct Dup2Action {
        src: i32,
        target: i32,
    }

    /// A class representing a sequence of basic redirections.
    #[derive(Default)]
    struct Dup2List {
        /// The list of actions.
        actions: Vec<Dup2Action>,
    }

    extern "Rust" {
        fn get_actions(self: &Dup2List) -> &[Dup2Action];
        #[cxx_name = "dup2_list_resolve_chain"]
        fn dup2_list_resolve_chain_ffi(io_chain: &CxxVector<Dup2Action>) -> Dup2List;
        fn fd_for_target_fd(self: &Dup2List, target: i32) -> i32;
    }
}

pub use redirection_ffi::{Dup2Action, Dup2List, RedirectionMode};

impl RedirectionMode {
    /// The open flags for this redirection mode.
    pub fn oflags(self) -> Option<c_int> {
        match self {
            RedirectionMode::append => Some(O_CREAT | O_APPEND | O_WRONLY),
            RedirectionMode::overwrite => Some(O_CREAT | O_WRONLY | O_TRUNC),
            RedirectionMode::noclob => Some(O_CREAT | O_EXCL | O_WRONLY),
            RedirectionMode::input => Some(O_RDONLY),
            _ => None,
        }
    }
}

impl Dup2Action {
    pub fn new(src: RawFd, target: RawFd) -> Self {
        Self { src, target }
    }
}

/// A struct which represents a redirection specification from the user.
/// Here the file descriptors don't represent open files - it's purely textual.
#[derive(Clone)]
pub struct RedirectionSpec {
    /// The redirected fd, or -1 on overflow.
    /// In the common case of a pipe, this is 1 (STDOUT_FILENO).
    /// For example, in the case of "3>&1" this will be 3.
    pub fd: RawFd,

    /// The redirection mode.
    pub mode: RedirectionMode,

    /// The target of the redirection.
    /// For example in "3>&1", this will be "1".
    /// In "< file.txt" this will be "file.txt".
    pub target: WString,
}

impl RedirectionSpec {
    pub fn new(fd: RawFd, mode: RedirectionMode, target: WString) -> Self {
        Self { fd, mode, target }
    }
    /// \return if this is a close-type redirection.
    pub fn is_close(&self) -> bool {
        self.mode == RedirectionMode::fd && self.target == L!("-")
    }

    /// Attempt to parse target as an fd.
    pub fn get_target_as_fd(&self) -> Option<RawFd> {
        fish_wcstoi(&self.target).ok()
    }
    fn get_target_as_fd_ffi(&self) -> SharedPtr<i32> {
        match self.get_target_as_fd() {
            Some(fd) => SharedPtr::new(fd),
            None => SharedPtr::null(),
        }
    }

    /// \return the open flags for this redirection.
    pub fn oflags(&self) -> c_int {
        match self.mode.oflags() {
            Some(flags) => flags,
            None => panic!("Not a file redirection"),
        }
    }

    fn fd(&self) -> RawFd {
        self.fd
    }

    fn mode(&self) -> RedirectionMode {
        self.mode
    }

    fn target(&self) -> UniquePtr<CxxWString> {
        self.target.to_ffi()
    }
}

fn new_redirection_spec(fd: i32, mode: RedirectionMode, target: wcharz_t) -> Box<RedirectionSpec> {
    Box::new(RedirectionSpec {
        fd,
        mode,
        target: target.into(),
    })
}

pub type RedirectionSpecList = Vec<RedirectionSpec>;

struct RedirectionSpecListFfi(RedirectionSpecList);

fn new_redirection_spec_list() -> Box<RedirectionSpecListFfi> {
    Box::new(RedirectionSpecListFfi(Vec::new()))
}

impl RedirectionSpecListFfi {
    fn size(&self) -> usize {
        self.0.len()
    }
    fn at(&self, offset: usize) -> *const RedirectionSpec {
        &self.0[offset]
    }
    #[allow(clippy::boxed_local)]
    fn push_back(&mut self, spec: Box<RedirectionSpec>) {
        self.0.push(*spec)
    }
    fn clone(&self) -> Box<RedirectionSpecListFfi> {
        Box::new(RedirectionSpecListFfi(self.0.clone()))
    }
}

/// Produce a dup_fd_list_t from an io_chain. This may not be called before fork().
/// The result contains the list of fd actions (dup2 and close), as well as the list
/// of fds opened.
pub fn dup2_list_resolve_chain(io_chain: &IoChain) -> Dup2List {
    let mut result = Dup2List { actions: vec![] };
    for io in &io_chain.0 {
        if io.source_fd() < 0 {
            result.add_close(io.fd())
        } else {
            result.add_dup2(io.source_fd(), io.fd())
        }
    }
    result
}

fn dup2_list_resolve_chain_ffi(io_chain: &CxxVector<Dup2Action>) -> Dup2List {
    let mut result = Dup2List { actions: vec![] };
    for io in io_chain {
        if io.src < 0 {
            result.add_close(io.target)
        } else {
            result.add_dup2(io.src, io.target)
        }
    }
    result
}

impl Dup2List {
    pub fn new() -> Self {
        Default::default()
    }
    /// \return the list of dup2 actions.
    pub fn get_actions(&self) -> &[Dup2Action] {
        &self.actions
    }

    /// \return the fd ultimately dup'd to a target fd, or -1 if the target is closed.
    /// For example, if target fd is 1, and we have a dup2 chain 5->3 and 3->1, then we will
    /// return 5. If the target is not referenced in the chain, returns target.
    pub fn fd_for_target_fd(&self, target: RawFd) -> RawFd {
        // Paranoia.
        if target < 0 {
            return target;
        }
        // Note we can simply walk our action list backwards, looking for src -> target dups.
        let mut cursor = target;
        for action in self.actions.iter().rev() {
            if action.target == cursor {
                // cursor is replaced by action.src
                cursor = action.src;
            } else if action.src == cursor && action.target < 0 {
                // cursor is closed.
                cursor = -1;
                break;
            }
        }
        cursor
    }

    /// Append a dup2 action.
    pub fn add_dup2(&mut self, src: RawFd, target: RawFd) {
        assert!(src >= 0 && target >= 0, "Invalid fd in add_dup2");
        // Note: record these even if src and target is the same.
        // This is a note that we must clear the CLO_EXEC bit.
        self.actions.push(Dup2Action { src, target });
    }

    /// Append a close action.
    pub fn add_close(&mut self, fd: RawFd) {
        assert!(fd >= 0, "Invalid fd in add_close");
        self.actions.push(Dup2Action {
            src: fd,
            target: -1,
        })
    }
}
