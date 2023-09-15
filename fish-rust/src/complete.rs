/// Prototypes for functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
use crate::wchar::prelude::*;
use crate::wcstringutil::StringFuzzyMatch;
use bitflags::bitflags;

#[derive(Default, Debug)]
pub struct CompletionMode {
    /// If set, skip file completions.
    pub no_files: bool,
    pub force_files: bool,
    /// If set, require a parameter after completion.
    pub requires_param: bool,
}

/// Character that separates the completion and description on programmable completions.
pub const PROG_COMPLETE_SEP: char = '\t';

bitflags! {
    #[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
    pub struct CompleteFlags: u8 {
    /// Do not insert space afterwards if this is the only completion. (The default is to try insert
    /// a space).
    const NO_SPACE = 1 << 0;
    /// This is not the suffix of a token, but replaces it entirely.
    const REPLACES_TOKEN = 1 << 1;
    /// This completion may or may not want a space at the end - guess by checking the last
    /// character of the completion.
    const AUTO_SPACE = 1 << 2;
    /// This completion should be inserted as-is, without escaping.
    const DONT_ESCAPE = 1 << 3;
    /// If you do escape, don't escape tildes.
    const DONT_ESCAPE_TILDES = 1 << 4;
    /// Do not sort supplied completions
    const DONT_SORT = 1 << 5;
    /// This completion looks to have the same string as an existing argument.
    const DUPLICATES_ARGUMENT = 1 << 6;
    /// This completes not just a token but replaces the entire commandline.
    const REPLACES_COMMANDLINE = 1 << 7;
    }
}

#[derive(Debug)]
pub struct Completion {
    pub completion: WString,
    pub description: WString,
    pub r#match: StringFuzzyMatch,
    pub flags: CompleteFlags,
}

impl Default for Completion {
    fn default() -> Self {
        Self {
            completion: Default::default(),
            description: Default::default(),
            r#match: StringFuzzyMatch::exact_match(),
            flags: Default::default(),
        }
    }
}

impl From<WString> for Completion {
    fn from(completion: WString) -> Completion {
        Completion {
            completion,
            ..Default::default()
        }
    }
}

impl Completion {
    /// \return whether this replaces its token.
    pub fn replaces_token(&self) -> bool {
        self.flags.contains(CompleteFlags::REPLACES_TOKEN)
    }
    /// \return whether this replaces the entire commandline.
    pub fn replaces_commandline(&self) -> bool {
        self.flags.contains(CompleteFlags::REPLACES_COMMANDLINE)
    }

    /// \return the completion's match rank. Lower ranks are better completions.
    pub fn rank(&self) -> u32 {
        self.r#match.rank()
    }

    /// If this completion replaces the entire token, prepend a prefix. Otherwise do nothing.
    pub fn prepend_token_prefix(&mut self, prefix: impl AsRef<wstr>) {
        if self.flags.contains(CompleteFlags::REPLACES_TOKEN) {
            self.completion.insert_utfstr(0, prefix.as_ref());
        }
    }
}

/// A completion receiver accepts completions. It is essentially a wrapper around Vec with
/// some conveniences.
#[derive(Default, Debug)]
pub struct CompletionReceiver {
    /// Our list of completions.
    completions: Vec<Completion>,
    /// The maximum number of completions to add. If our list length exceeds this, then new
    /// completions are not added. Note 0 has no special significance here - use
    /// usize::MAX instead.
    limit: usize,
}

// We are only wrapping a `Vec<Completion>`, any non-mutable methods can be safely deferred to the
// Vec-impl
impl std::ops::Deref for CompletionReceiver {
    type Target = [Completion];

    fn deref(&self) -> &Self::Target {
        self.completions.as_slice()
    }
}

impl std::ops::DerefMut for CompletionReceiver {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.completions.as_mut_slice()
    }
}

impl CompletionReceiver {
    pub fn new(limit: usize) -> Self {
        Self {
            limit,
            ..Default::default()
        }
    }

    /// Add a completion.
    /// \return true on success, false if this would overflow the limit.
    #[must_use]
    pub fn add(&mut self, comp: impl Into<Completion>) -> bool {
        if self.completions.len() >= self.limit {
            return false;
        }
        self.completions.push(comp.into());
        return true;
    }

    /// Add a list of completions.
    /// \return true on success, false if this would overflow the limit.
    #[must_use]
    pub fn extend(
        &mut self,
        iter: impl IntoIterator<Item = Completion, IntoIter = impl ExactSizeIterator<Item = Completion>>,
    ) -> bool {
        let iter = iter.into_iter();
        if iter.len() > self.limit - self.completions.len() {
            return false;
        }
        self.completions.extend(iter);
        // this only fails if the ExactSizeIterator impl is bogus
        assert!(
            self.completions.len() <= self.limit,
            "ExactSizeIterator returned more items than it should"
        );
        true
    }

    /// Clear the list of completions. This retains the storage inside completions_ which can be
    /// useful to prevent allocations.
    pub fn clear(&mut self) {
        self.completions.clear();
    }

    /// \return the list of completions, clearing it.
    pub fn take(&mut self) -> Vec<Completion> {
        std::mem::take(&mut self.completions)
    }

    /// \return a new, empty receiver whose limit is our remaining capacity.
    /// This is useful for e.g. recursive calls when you want to act on the result before adding it.
    pub fn subreceiver(&self) -> Self {
        let remaining_capacity = self
            .limit
            .checked_sub(self.completions.len())
            .expect("length should never be larger than limit");
        Self::new(remaining_capacity)
    }
}
