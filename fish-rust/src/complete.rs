use crate::{
    wchar::{wstr, WString},
    wcstringutil::StringFuzzyMatch,
};
use bitflags::bitflags;

pub struct Completion {
    pub completion: WString,
    pub flags: CompleteFlags,
}

bitflags! {
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

impl Completion {
    pub fn replaces_token(&self) -> bool {
        todo!()
    }
}

pub type CompletionList = Vec<Completion>;

pub struct CompletionReceiver {
    completion_list: CompletionList,
}

impl CompletionReceiver {
    pub fn new(v: CompletionList, limit: usize) -> Self {
        todo!()
    }
    pub fn add(&mut self, c: WString) -> bool {
        todo!()
    }
    pub fn add_list(&mut self, list: CompletionList) -> bool {
        todo!()
    }
    pub fn subreceiver(&mut self) -> Self {
        todo!()
    }
    pub fn swap(&mut self, list: &mut CompletionList) {
        todo!()
    }
    pub fn clear(&mut self) -> CompletionList {
        todo!()
    }
    pub fn take(&mut self) -> CompletionList {
        todo!()
    }
}

pub fn append_completion(completions: &mut CompletionList, comp: WString) {
    todo!()
}
pub fn append_completion_flags(
    completions: &mut CompletionList,
    comp: WString,
    desc: WString,
    flags: CompleteFlags,
    match_: StringFuzzyMatch,
) {
    todo!()
}

pub fn complete_get_wrap_targets(name: &wstr) -> Vec<WString> {
    todo!()
}
