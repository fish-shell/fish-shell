//! Encapsulation of the reader's history search functionality.

use crate::history::{self, History, HistorySearch, SearchDirection, SearchFlags, SearchType};
use crate::parse_constants::SourceRange;
use crate::tokenizer::{TokenType, Tokenizer, TOK_ACCEPT_UNFINISHED};
use crate::wchar::prelude::*;
use crate::wcstringutil::ifind;
use std::collections::HashSet;
use std::ops::Range;
use std::sync::Arc;

// Make the search case-insensitive unless we have an uppercase character.
pub fn smartcase_flags(query: &wstr) -> history::SearchFlags {
    if query == query.to_lowercase() {
        history::SearchFlags::IGNORE_CASE
    } else {
        history::SearchFlags::default()
    }
}

struct SearchMatch {
    /// The text of the match.
    pub text: WString,
    /// The offset of the current search string in this match.
    offset: usize,
}

impl SearchMatch {
    fn new(text: WString, offset: usize) -> Self {
        Self { text, offset }
    }
}

#[derive(Clone, Copy, Eq, Default, PartialEq)]
pub enum SearchMode {
    #[default]
    /// no search
    Inactive,
    /// searching by line
    Line,
    /// searching by prefix
    Prefix,
    /// searching by token
    Token,
    /// search by the last token of the command
    LastToken,
}

/// Encapsulation of the reader's history search functionality.
#[derive(Default)]
pub struct ReaderHistorySearch {
    /// The type of search performed.
    mode: SearchMode,

    /// Our history search itself.
    search: Option<HistorySearch>,

    /// The ordered list of matches. This may grow long.
    matches: Vec<SearchMatch>,

    /// A set of new items to skip, corresponding to matches_ and anything added in skip().
    skips: HashSet<WString>,

    /// Index into our matches list.
    match_index: usize,

    /// The offset of the current token in the command line. Only non-zero for a token search.
    token_offset: usize,
}

impl ReaderHistorySearch {
    pub fn active(&self) -> bool {
        self.mode != SearchMode::Inactive
    }
    pub fn by_token(&self) -> bool {
        matches!(self.mode, SearchMode::Token | SearchMode::LastToken)
    }
    pub fn by_line(&self) -> bool {
        self.mode == SearchMode::Line
    }
    pub fn by_prefix(&self) -> bool {
        self.mode == SearchMode::Prefix
    }
    pub fn mode(&self) -> SearchMode {
        self.mode
    }

    /// Move the history search in the given direction `dir`.
    pub fn move_in_direction(&mut self, dir: SearchDirection) -> bool {
        match dir {
            SearchDirection::Forward => self.move_forwards(),
            SearchDirection::Backward => self.move_backwards(),
        }
    }

    /// Go to the oldest match (last match) of the search.
    pub fn go_to_oldest(&mut self) {
        if self.matches.is_empty() {
            return;
        }
        self.match_index = self.matches.len() - 1;
    }

    /// Go to the youngest match (original search string) of the search.
    pub fn go_to_present(&mut self) {
        self.match_index = 0;
    }

    /// Return the current search result.
    pub fn current_result(&self) -> &wstr {
        &self.matches[self.match_index].text
    }

    /// Return the string we are searching for.
    pub fn search_string(&self) -> &wstr {
        self.search().original_term()
    }

    /// Return the range of the current match in the command line.
    pub fn search_result_range(&self) -> Range<usize> {
        assert!(self.active());
        self.token_offset..self.token_offset + self.matches[self.match_index].text.len()
    }

    /// Return the range of the original search string in the new command line.
    pub fn search_range_if_active(&self) -> Option<SourceRange> {
        if !self.active() || self.is_at_present() {
            return None;
        }
        Some(SourceRange::new(
            self.token_offset + self.matches[self.match_index].offset,
            self.search_string().len(),
        ))
    }

    /// Return whether we are at the youngest match (original search string) in our search.
    pub fn is_at_present(&self) -> bool {
        self.match_index == 0
    }

    // Add an item to skip.
    // Return true if it was added, false if already present.
    pub fn add_skip(&mut self, s: WString) -> bool {
        self.skips.insert(s)
    }

    pub fn handle_deletion(&mut self) {
        assert!(!self.is_at_present());
        self.matches.remove(self.match_index);
        self.match_index -= 1;
        self.search_mut().prepare_to_search_after_deletion();
        self.move_backwards();
    }

    /// Reset, beginning a new line or token mode search.
    pub fn reset_to_mode(
        &mut self,
        text: WString,
        hist: Arc<History>,
        mode: SearchMode,
        token_offset: usize,
    ) {
        assert!(
            mode != SearchMode::Inactive,
            "mode cannot be inactive in this setter"
        );
        self.skips = HashSet::from([text.clone()]);
        self.matches = vec![SearchMatch::new(text.clone(), 0)];
        self.match_index = 0;
        self.mode = mode;
        self.token_offset = token_offset;
        let flags = SearchFlags::NO_DEDUP | smartcase_flags(&text);
        // We can skip dedup in history_search_t because we do it ourselves in skips_.
        self.search = Some(HistorySearch::new_with(
            hist,
            text,
            if self.by_prefix() {
                SearchType::Prefix
            } else {
                SearchType::Contains
            },
            flags,
            0,
        ));
    }

    /// Reset to inactive search.
    pub fn reset(&mut self) {
        self.matches.clear();
        self.skips.clear();
        self.match_index = 0;
        self.mode = SearchMode::Inactive;
        self.token_offset = 0;
        self.search = None;
    }

    /// Adds the given match if we haven't seen it before.
    fn add_if_new(&mut self, search_match: SearchMatch) {
        if self.add_skip(search_match.text.clone()) {
            self.matches.push(search_match);
        }
    }

    /// Attempt to append matches from the current history item.
    /// Return true if something was appended.
    fn append_matches_from_search(&mut self) -> bool {
        fn find(zelf: &ReaderHistorySearch, haystack: &wstr, needle: &wstr) -> Option<usize> {
            if zelf.search().ignores_case() {
                return ifind(haystack, needle, false);
            }
            haystack.find(needle)
        }
        let before = self.matches.len();
        let text = self.search().current_string();
        let needle = self.search_string();
        if matches!(self.mode, SearchMode::Line | SearchMode::Prefix) {
            // FIXME: Previous versions asserted out if this wasn't true.
            // This could be hit with a needle of "ö" and haystack of "echo Ö"
            // I'm not sure why - this points to a bug in ifind (probably wrong locale?)
            // However, because the user experience of having it crash is horrible,
            // and the worst thing that can otherwise happen here is that a search is unsuccessful,
            // we just check it instead.
            if let Some(offset) = find(self, text, needle) {
                self.add_if_new(SearchMatch::new(text.to_owned(), offset));
            }
        } else if matches!(self.mode, SearchMode::Token | SearchMode::LastToken) {
            let mut tok = Tokenizer::new(text, TOK_ACCEPT_UNFINISHED);

            let mut local_tokens = vec![];
            while let Some(token) = tok.next() {
                if token.type_ != TokenType::string {
                    continue;
                }
                let text = tok.text_of(&token);
                if let Some(offset) = find(self, text, needle) {
                    local_tokens.push(SearchMatch::new(text.to_owned(), offset));
                }
            }

            // Make sure tokens are added in reverse order. See #5150
            for tok in local_tokens.into_iter().rev() {
                self.add_if_new(tok);
                if self.mode == SearchMode::LastToken {
                    break;
                }
            }
        }
        self.matches.len() > before
    }

    fn move_forwards(&mut self) -> bool {
        // Try to move within our previously discovered matches.
        if self.match_index > 0 {
            self.match_index -= 1;
            true
        } else {
            false
        }
    }

    fn move_backwards(&mut self) -> bool {
        // Try to move backwards within our previously discovered matches.
        if self.match_index + 1 < self.matches.len() {
            self.match_index += 1;
            return true;
        }

        // Add more items from our search.
        while self
            .search_mut()
            .go_to_next_match(SearchDirection::Backward)
        {
            if self.append_matches_from_search() {
                self.match_index += 1;
                assert!(
                    self.match_index < self.matches.len(),
                    "Should have found more matches"
                );
                return true;
            }
        }

        // Here we failed to go backwards past the last history item.
        false
    }

    fn search(&self) -> &HistorySearch {
        self.search.as_ref().unwrap()
    }

    fn search_mut(&mut self) -> &mut HistorySearch {
        self.search.as_mut().unwrap()
    }
}
