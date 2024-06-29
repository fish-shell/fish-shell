//! This file supports specifying and applying redirections.

use crate::io::IoChain;
use crate::wchar::prelude::*;
use crate::wutil::fish_wcstoi;
use nix::fcntl::OFlag;
use std::os::fd::RawFd;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RedirectionMode {
    overwrite, // normal redirection: > file.txt
    append,    // appending redirection: >> file.txt
    input,     // input redirection: < file.txt
    try_input, // try-input redirection: <? file.txt
    fd,        // fd redirection: 2>&1
    noclob,    // noclobber redirection: >? file.txt
}

/// A type that represents the action dup2(src, target).
/// If target is negative, this represents close(src).
/// Note none of the fds here are considered 'owned'.
#[derive(Clone, Copy)]
pub struct Dup2Action {
    pub src: i32,
    pub target: i32,
}

/// A class representing a sequence of basic redirections.
#[derive(Default)]
pub struct Dup2List {
    /// The list of actions.
    pub actions: Vec<Dup2Action>,
}

impl RedirectionMode {
    /// The open flags for this redirection mode.
    pub fn oflags(self) -> Option<OFlag> {
        match self {
            RedirectionMode::append => Some(OFlag::O_CREAT | OFlag::O_APPEND | OFlag::O_WRONLY),
            RedirectionMode::overwrite => Some(OFlag::O_CREAT | OFlag::O_WRONLY | OFlag::O_TRUNC),
            RedirectionMode::noclob => Some(OFlag::O_CREAT | OFlag::O_EXCL | OFlag::O_WRONLY),
            RedirectionMode::input | RedirectionMode::try_input => Some(OFlag::O_RDONLY),
            _ => None,
        }
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
    /// Return if this is a close-type redirection.
    pub fn is_close(&self) -> bool {
        self.mode == RedirectionMode::fd && self.target == "-"
    }

    /// Attempt to parse target as an fd.
    pub fn get_target_as_fd(&self) -> Option<RawFd> {
        fish_wcstoi(&self.target).ok()
    }

    /// Return the open flags for this redirection.
    pub fn oflags(&self) -> OFlag {
        match self.mode.oflags() {
            Some(flags) => flags,
            None => panic!("Not a file redirection"),
        }
    }
}

pub type RedirectionSpecList = Vec<RedirectionSpec>;

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

impl Dup2List {
    pub fn new() -> Self {
        Default::default()
    }
    /// Return the list of dup2 actions.
    pub fn get_actions(&self) -> &[Dup2Action] {
        &self.actions
    }

    /// Return the fd ultimately dup'd to a target fd, or -1 if the target is closed.
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
