use std::{
    cmp::Ordering,
    collections::{BTreeMap, HashMap, HashSet},
    mem,
    sync::{
        atomic::{self, AtomicUsize},
        Mutex,
    },
    time::{Duration, Instant},
};

use crate::{
    common::{charptr2wcstring, escape_string, EscapeFlags, EscapeStringStyle},
    reader::{get_quote, is_backslashed},
    util::wcsfilecmp,
};
use bitflags::bitflags;
use fish_printf::sprintf;
use once_cell::sync::Lazy;

use crate::{
    abbrs::with_abbrs,
    autoload::Autoload,
    builtins::shared::{builtin_exists, builtin_get_desc, builtin_get_names},
    common::{
        escape, unescape_string, valid_var_name_char, ScopeGuard, UnescapeFlags,
        UnescapeStringStyle,
    },
    env::{EnvMode, EnvStack, Environment},
    exec::exec_subshell,
    expand::{
        expand_escape_string, expand_escape_variable, expand_one, expand_string,
        expand_to_receiver, ExpandFlags, ExpandResultCode,
    },
    flog::{FLOG, FLOGF},
    function,
    history::{history_session_id, History},
    operation_context::OperationContext,
    parse_constants::SourceRange,
    parse_util::{
        parse_util_cmdsubst_extent, parse_util_process_extent, parse_util_unescape_wildcards,
    },
    parser::{Block, Parser},
    parser_keywords::parser_keywords_is_subcommand,
    path::{path_get_path, path_try_get_path},
    tokenizer::{variable_assignment_equals_pos, Tok, TokFlags, TokenType, Tokenizer},
    wchar::{wstr, WString, L},
    wchar_ext::WExt,
    wcstringutil::{
        string_fuzzy_match_string, string_prefixes_string, string_prefixes_string_case_insensitive,
        StringFuzzyMatch,
    },
    wildcard::{wildcard_complete, wildcard_has, wildcard_match},
    wutil::{gettext::wgettext_str, wgettext, wrealpath},
};

// Completion description strings, mostly for different types of files, such as sockets, block
// devices, etc.
//
// There are a few more completion description strings defined in expand.rs. Maybe all completion
// description strings should be defined in the same file?

/// Description for ~USER completion.
static COMPLETE_USER_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("Home for %ls"));

/// Description for short variables. The value is concatenated to this description.
static COMPLETE_VAR_DESC_VAL: Lazy<&wstr> = Lazy::new(|| wgettext!("Variable: %ls"));

/// Description for abbreviations.
static ABBR_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("Abbreviation: %ls"));

/// The special cased translation macro for completions. The empty string needs to be special cased,
/// since it can occur, and should not be translated. (Gettext returns the version information as
/// the response).
#[allow(non_snake_case)]
fn C_(s: &wstr) -> &'static wstr {
    if s.is_empty() {
        L!("")
    } else {
        wgettext_str(s)
    }
}

#[derive(Clone, Copy, Default, PartialEq, Eq, Debug)]
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
        /// This completes not just a token but replaces an entire line.
        const REPLACES_LINE = 1 << 7;
    }
}

/// Function which accepts a completion string and returns its description.
pub type DescriptionFunc = Box<dyn Fn(&wstr) -> WString>;

/// Helper to return a [`DescriptionFunc`] for a constant string.
pub fn const_desc(s: &wstr) -> DescriptionFunc {
    let s = s.to_owned();
    Box::new(move |_| s.clone())
}

pub type CompletionList = Vec<Completion>;

/// This is an individual completion entry, i.e. the result of an expansion of a completion rule.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Completion {
    /// The completion string.
    pub completion: WString,
    /// The description for this completion.
    pub description: WString,
    /// The type of fuzzy match.
    pub r#match: StringFuzzyMatch,
    /// Flags determining the completion behavior.
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
    pub fn new(
        completion: WString,
        description: WString,
        r#match: StringFuzzyMatch, /* = exact_match */
        flags: CompleteFlags,
    ) -> Self {
        let flags = resolve_auto_space(&completion, flags);
        Self {
            completion,
            description,
            r#match,
            flags,
        }
    }

    pub fn from_completion(completion: WString) -> Self {
        Self::with_desc(completion, WString::new())
    }

    pub fn with_desc(completion: WString, description: WString) -> Self {
        Self::new(
            completion,
            description,
            StringFuzzyMatch::exact_match(),
            CompleteFlags::empty(),
        )
    }

    /// Returns whether this replaces its token.
    pub fn replaces_token(&self) -> bool {
        self.flags.contains(CompleteFlags::REPLACES_TOKEN)
    }

    /// Returns whether this replaces the entire commandline.
    pub fn replaces_line(&self) -> bool {
        self.flags.contains(CompleteFlags::REPLACES_LINE)
    }

    /// Returns the completion's match rank. Lower ranks are better completions.
    pub fn rank(&self) -> u32 {
        self.r#match.rank()
    }

    /// If this completion replaces the entire token, prepend a prefix. Otherwise do nothing.
    pub fn prepend_token_prefix(&mut self, prefix: &wstr) {
        if self.flags.contains(CompleteFlags::REPLACES_TOKEN) {
            self.completion.insert_utfstr(0, prefix)
        }
    }
}

impl CompletionRequestOptions {
    /// Options for an autosuggestion.
    pub fn autosuggest() -> Self {
        Self {
            autosuggestion: true,
            descriptions: false,
            fuzzy_match: false,
        }
    }

    /// Options for a "normal" completion.
    pub fn normal() -> Self {
        Self {
            autosuggestion: false,
            descriptions: true,
            fuzzy_match: true,
        }
    }
}

/// A completion receiver accepts completions. It is essentially a wrapper around `Vec` with
/// some conveniences.
pub struct CompletionReceiver {
    /// Our list of completions.
    completions: Vec<Completion>,
    /// The maximum number of completions to add. If our list length exceeds this, then new
    /// completions are not added. Note 0 has no special significance here - use
    /// `usize::MAX` instead.
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
    /// Construct as empty, with a limit.
    pub fn new(limit: usize) -> Self {
        Self {
            completions: vec![],
            limit,
        }
    }

    /// Acquire an existing list, with a limit.
    pub fn from_list(completions: Vec<Completion>, limit: usize) -> Self {
        Self { completions, limit }
    }

    /// Add a completion.
    /// Return true on success, false if this would overflow the limit.
    #[must_use]
    pub fn add(&mut self, comp: impl Into<Completion>) -> bool {
        if self.completions.len() >= self.limit {
            return false;
        }
        self.completions.push(comp.into());
        return true;
    }

    /// Adds a completion with the given string, and default other properties. Returns `true` on
    /// success, `false` if this would overflow the limit.
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

    /// Clear the list of completions. This retains the storage inside `completions` which can be
    /// useful to prevent allocations.
    pub fn clear(&mut self) {
        self.completions.clear();
    }

    /// Returns whether our completion list is empty.
    pub fn empty(&self) -> bool {
        self.completions.is_empty()
    }

    /// Returns how many completions we have stored.
    pub fn size(&self) -> usize {
        self.completions.len()
    }

    /// Returns the list of completions.
    pub fn get_list(&self) -> &[Completion] {
        &self.completions
    }

    /// Returns the list of completions.
    pub fn get_list_mut(&mut self) -> &mut [Completion] {
        &mut self.completions
    }

    /// Returns the list of completions, clearing it.
    pub fn take(&mut self) -> Vec<Completion> {
        std::mem::take(&mut self.completions)
    }

    /// Returns a new, empty receiver whose limit is our remaining capacity.
    /// This is useful for e.g. recursive calls when you want to act on the result before adding it.
    pub fn subreceiver(&self) -> Self {
        let remaining_capacity = self
            .limit
            .checked_sub(self.completions.len())
            .expect("length should never be larger than limit");
        Self::new(remaining_capacity)
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum CompleteOptionType {
    /// no option
    ArgsOnly,
    /// `-x`
    Short,
    /// `-foo`
    SingleLong,
    /// `--foo`
    DoubleLong,
}

/// Struct describing a completion rule for options to a command.
///
/// If option is empty, the comp field must not be empty and contains a list of arguments to the
/// command.
///
/// The type field determines how the option is to be interpreted: either empty (args_only) or
/// short, single-long ("old") or double-long ("GNU"). An invariant is that the option is empty if
/// and only if the type is args_only.
///
/// If option is non-empty, it specifies a switch for the command. If \c comp is also not empty, it
/// contains a list of non-switch arguments that may only follow directly after the specified
/// switch.
#[derive(Clone, Debug)]
struct CompleteEntryOpt {
    /// Text of the option (like 'foo').
    option: WString,
    /// Arguments to the option; may be a subshell expression expanded at evaluation time.
    comp: WString,
    /// Description of the completion.
    desc: WString,
    /// Conditions under which to use the option, expanded and evaluated at completion time.
    conditions: Vec<WString>,
    /// Type of the option: `ArgsOnly`, `Short`, `SingleLong`, or `DoubleLong`.
    typ: CompleteOptionType,
    /// Determines how completions should be performed on the argument after the switch.
    result_mode: CompletionMode,
    /// Completion flags.
    flags: CompleteFlags,
}

impl CompleteEntryOpt {
    pub fn localized_desc(&self) -> &'static wstr {
        C_(&self.desc)
    }

    pub fn expected_dash_count(&self) -> usize {
        match self.typ {
            CompleteOptionType::ArgsOnly => 0,
            CompleteOptionType::Short | CompleteOptionType::SingleLong => 1,
            CompleteOptionType::DoubleLong => 2,
        }
    }
}

/// Last value used in the order field of [`CompletionEntry`].
static complete_order: AtomicUsize = AtomicUsize::new(0);

struct CompletionEntry {
    /// List of all options.
    options: Vec<CompleteEntryOpt>,
    /// Order for when this completion was created. This aids in outputting completions sorted by
    /// time.
    order: usize,
}

impl CompletionEntry {
    pub fn new() -> Self {
        Self {
            options: vec![],
            order: complete_order.fetch_add(1, atomic::Ordering::Relaxed),
        }
    }

    /// Getters for option list.
    pub fn get_options(&self) -> &[CompleteEntryOpt] {
        &self.options
    }

    /// Adds an option.
    pub fn add_option(&mut self, opt: CompleteEntryOpt) {
        self.options.push(opt)
    }

    /// Remove all completion options in the specified entry that match the specified short / long
    /// option strings. Returns true if it is now empty and should be deleted, false if it's not
    /// empty.
    pub fn remove_option(&mut self, option: &wstr, typ: CompleteOptionType) -> bool {
        self.options
            .retain(|opt| opt.option != option || opt.typ != typ);
        self.options.is_empty()
    }
}

/// Set of all completion entries. Keyed by the command name, and whether it is a path.
#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
struct CompletionEntryIndex {
    name: WString,
    is_path: bool,
}
type CompletionEntryMap = BTreeMap<CompletionEntryIndex, CompletionEntry>;
static COMPLETION_MAP: Mutex<CompletionEntryMap> = Mutex::new(BTreeMap::new());

/// Completion "wrapper" support. The map goes from wrapping-command to wrapped-command-list.
type WrapperMap = HashMap<WString, Vec<WString>>;
static wrapper_map: Lazy<Mutex<WrapperMap>> = Lazy::new(|| Mutex::new(HashMap::new()));

/// Clear the [`CompleteFlags::AUTO_SPACE`] flag, and set [`CompleteFlags::NO_SPACE`] appropriately
/// depending on the suffix of the string.
fn resolve_auto_space(comp: &wstr, mut flags: CompleteFlags) -> CompleteFlags {
    if flags.contains(CompleteFlags::AUTO_SPACE) {
        flags -= CompleteFlags::AUTO_SPACE;
        if let Some('/' | '=' | '@' | ':' | '.' | ',' | '-') = comp.as_char_slice().last() {
            flags |= CompleteFlags::NO_SPACE;
        }
    }

    flags
}

// If these functions aren't force inlined, it is actually faster to call
// stable_sort twice rather than to iterate once performing all comparisons in one go!

#[inline(always)]
fn natural_compare_completions(a: &Completion, b: &Completion) -> Ordering {
    if (a.flags & b.flags).contains(CompleteFlags::DONT_SORT) {
        // Both completions are from a source with the --keep-order flag.
        return Ordering::Equal;
    }
    wcsfilecmp(&a.completion, &b.completion)
}

#[inline(always)]
fn compare_completions_by_duplicate_arguments(a: &Completion, b: &Completion) -> Ordering {
    let ad = a.flags.contains(CompleteFlags::DUPLICATES_ARGUMENT);
    let bd = b.flags.contains(CompleteFlags::DUPLICATES_ARGUMENT);

    ad.cmp(&bd)
}

#[inline(always)]
fn compare_completions_by_tilde(a: &Completion, b: &Completion) -> Ordering {
    if a.completion.is_empty() || b.completion.is_empty() {
        return Ordering::Equal;
    }

    let at = a.completion.ends_with('~');
    let bt = b.completion.ends_with('~');

    at.cmp(&bt)
}

/// Unique the list of completions, without perturbing their order.
fn unique_completions_retaining_order(comps: &mut Vec<Completion>) {
    let mut seen = HashSet::with_capacity(comps.len());

    let pred = |c: &Completion| {
        // Keep (return true) if insertion succeeds.
        // todo!("don't clone here");
        seen.insert(c.completion.to_owned())
    };

    comps.retain(pred);
}

/// Sorts and removes any duplicate completions in the completion list, then puts them in priority
/// order.
pub fn sort_and_prioritize(comps: &mut Vec<Completion>, flags: CompletionRequestOptions) {
    if comps.is_empty() {
        return;
    }

    // Find the best rank.
    let best_rank = comps.iter().map(Completion::rank).min().unwrap();

    // Throw out completions of worse ranks.
    comps.retain(|c| c.rank() == best_rank);

    // Deduplicate both sorted and unsorted results.
    unique_completions_retaining_order(comps);

    // Sort, provided DONT_SORT isn't set.
    // Here we do not pass suppress_exact, so that exact matches appear first.
    comps.sort_by(natural_compare_completions);

    // Lastly, if this is for an autosuggestion, prefer to avoid completions that duplicate
    // arguments, and penalize files that end in tilde - they're frequently autosave files from e.g.
    // emacs. Also prefer samecase to smartcase.
    if flags.autosuggestion {
        comps.sort_by(|a, b| {
            a.r#match
                .case_fold
                .cmp(&b.r#match.case_fold)
                .then_with(|| compare_completions_by_duplicate_arguments(a, b))
                .then_with(|| compare_completions_by_tilde(a, b))
        })
    }
}

/// Bag of data to support expanding a command's arguments using custom completions, including
/// the wrap chain.
struct CustomArgData<'a> {
    /// The unescaped argument before the argument which is being completed, or empty if none.
    previous_argument: WString,
    /// The unescaped argument which is being completed, or empty if none.
    current_argument: WString,
    /// Whether a -- has been encountered, which suppresses options.
    had_ddash: bool,
    /// Whether to perform file completions.
    /// This is an "out" parameter of the wrap chain walk: if any wrapped command suppresses file
    /// completions this gets set to false.
    do_file: bool,
    /// Depth in the wrap chain.
    wrap_depth: usize,
    /// The list of variable assignments: escaped strings of the form VAR=VAL.
    /// This may be temporarily appended to as we explore the wrap chain.
    /// When completing, variable assignments are really set in a local scope.
    var_assignments: &'a mut Vec<WString>,
    /// The set of wrapped commands which we have visited, and so should not be explored again.
    visited_wrapped_commands: HashSet<WString>,
}

impl<'a> CustomArgData<'a> {
    pub fn new(var_assignments: &'a mut Vec<WString>) -> Self {
        Self {
            previous_argument: WString::new(),
            current_argument: WString::new(),
            had_ddash: false,
            do_file: true,
            wrap_depth: 0,
            var_assignments,
            visited_wrapped_commands: HashSet::new(),
        }
    }
}

/// Class representing an attempt to compute completions.
struct Completer<'ctx> {
    /// The operation context for this completion.
    ctx: &'ctx OperationContext<'ctx>,
    /// Flags associated with the completion request.
    flags: CompletionRequestOptions,
    /// The output completions.
    completions: CompletionReceiver,
    /// Commands which we would have tried to load, if we had a parser.
    needs_load: Vec<WString>,
    /// Table of completions conditions that have already been tested and the corresponding test
    /// results.
    condition_cache: HashMap<WString, bool>,
}

static completion_autoloader: Lazy<Mutex<Autoload>> =
    Lazy::new(|| Mutex::new(Autoload::new(L!("fish_complete_path"))));

impl<'ctx> Completer<'ctx> {
    pub fn new(ctx: &'ctx OperationContext<'ctx>, flags: CompletionRequestOptions) -> Self {
        Self {
            ctx,
            flags,
            completions: CompletionReceiver::new(ctx.expansion_limit),
            needs_load: vec![],
            condition_cache: HashMap::new(),
        }
    }

    fn perform_for_commandline(&mut self, cmdline: WString) {
        // Limit recursion, in case a user-defined completion has cycles, or the completion for "x"
        // wraps "A=B x" (#3474, #7344).  No need to do that when there is no parser: this happens only
        // for autosuggestions where we don't evaluate command substitutions or variable assignments.
        if let Some(parser) = self.ctx.maybe_parser() {
            let level = &mut parser.libdata_mut().complete_recursion_level;
            if *level >= 24 {
                FLOG!(
                    error,
                    wgettext!("completion reached maximum recursion depth, possible cycle?"),
                );
                return;
            }
            *level += 1;
        }
        self.perform_for_commandline_impl(cmdline);
        if let Some(parser) = self.ctx.maybe_parser() {
            parser.libdata_mut().complete_recursion_level -= 1;
        }
    }

    fn perform_for_commandline_impl(&mut self, cmdline: WString) {
        let cursor_pos = cmdline.len();
        let is_autosuggest = self.flags.autosuggestion;

        // Find the process to operate on. The cursor may be past it (#1261), so backtrack
        // until we know we're no longer in a space. But the space may actually be part of the
        // argument (#2477).
        let mut position_in_statement = cursor_pos;
        while position_in_statement > 0 && cmdline.char_at(position_in_statement - 1) == ' ' {
            position_in_statement -= 1;
        }

        // Get all the arguments.
        let mut tokens = Vec::new();
        parse_util_process_extent(&cmdline, position_in_statement, Some(&mut tokens));
        let actual_token_count = tokens.len();

        // Hack: fix autosuggestion by removing prefixing "and"s #6249.
        if is_autosuggest {
            let prefixed_supercommand_count = tokens
                .iter()
                .take_while(|token| parser_keywords_is_subcommand(token.get_source(&cmdline)))
                .count();
            tokens.drain(..prefixed_supercommand_count);
        }

        // Consume variable assignments in tokens strictly before the cursor.
        // This is a list of (escaped) strings of the form VAR=VAL.
        // TODO: filter_drain
        let mut var_assignments = Vec::new();
        for tok in &tokens {
            if tok.location_in_or_at_end_of_source_range(cursor_pos) {
                break;
            }
            let tok_src = tok.get_source(&cmdline);
            if variable_assignment_equals_pos(tok_src).is_none() {
                break;
            }
            var_assignments.push(tok_src.to_owned());
        }
        tokens.drain(..var_assignments.len());

        // Empty process (cursor is after one of ;, &, |, \n, &&, || modulo whitespace).
        let [first_token, ..] = tokens.as_slice() else {
            // Don't autosuggest anything based on the empty string (generalizes #1631).
            if is_autosuggest {
                return;
            }
            self.complete_cmd(WString::new());
            self.complete_abbr(WString::new());
            return;
        };

        let effective_cmdline = if tokens.len() == actual_token_count {
            &cmdline
        } else {
            &cmdline[first_token.offset()..]
        };

        if tokens.last().unwrap().type_ == TokenType::comment {
            return;
        }
        tokens.retain(|tok| tok.type_ != TokenType::comment);
        assert!(!tokens.is_empty());

        let cmd_tok = tokens.first().unwrap();
        let cur_tok = tokens.last().unwrap();

        // Since fish does not currently support redirect in command position, we return here.
        if cmd_tok.type_ != TokenType::string {
            return;
        }
        if cur_tok.type_ == TokenType::error {
            return;
        }
        for tok in &tokens {
            // If there was an error, it was in the last token.
            assert!(matches!(tok.type_, TokenType::string | TokenType::redirect));
        }
        // If we are completing a variable name or a tilde expansion user name, we do that and
        // return. No need for any other completions.
        let current_token = cur_tok.get_source(&cmdline);
        if cur_tok.location_in_or_at_end_of_source_range(cursor_pos)
            && (self.try_complete_variable(current_token) || self.try_complete_user(current_token))
        {
            return;
        }

        if cmd_tok.location_in_or_at_end_of_source_range(cursor_pos) {
            let equals_sign_pos = variable_assignment_equals_pos(current_token);
            if let Some(pos) = equals_sign_pos {
                self.complete_param_expand(
                    &current_token[..pos + 1],
                    &current_token[pos + 1..],
                    /*do_file=*/ true,
                    /*handle_as_special_cd=*/ false,
                );
                return;
            }
            // Complete command filename.
            let current_token = current_token.to_owned();
            self.complete_cmd(current_token.clone());
            self.complete_abbr(current_token);
            return;
        }
        // See whether we are in an argument, in a redirection or in the whitespace in between.
        let mut in_redirection = cur_tok.type_ == TokenType::redirect;

        let mut had_ddash = false;
        let mut current_argument = L!("");
        let mut previous_argument = L!("");
        if cur_tok.type_ == TokenType::string
            && cur_tok.location_in_or_at_end_of_source_range(position_in_statement)
        {
            // If the cursor is in whitespace, then the "current" argument is empty and the
            // previous argument is the matching one. But if the cursor was in or at the end
            // of the argument, then the current argument is the matching one, and the
            // previous argument is the one before it.
            let cursor_in_whitespace = !cur_tok.location_in_or_at_end_of_source_range(cursor_pos);
            if cursor_in_whitespace {
                previous_argument = current_token;
            } else {
                current_argument = current_token;
                if tokens.len() >= 2 {
                    let prev_tok = &tokens[tokens.len() - 2];
                    if prev_tok.type_ == TokenType::string {
                        previous_argument = prev_tok.get_source(&cmdline);
                    }
                    in_redirection = prev_tok.type_ == TokenType::redirect;
                }
            }

            // Check to see if we have a preceding double-dash.
            for tok in &tokens[..tokens.len() - 1] {
                if tok.get_source(&cmdline) == "--" {
                    had_ddash = true;
                    break;
                }
            }
        }

        let mut do_file = false;
        let mut handle_as_special_cd = false;
        if in_redirection {
            do_file = true;
        } else {
            // Try completing as an argument.
            let mut arg_data = CustomArgData::new(&mut var_assignments);
            arg_data.had_ddash = had_ddash;

            let bias = cmdline.len() - effective_cmdline.len();
            let command_range = SourceRange::new(cmd_tok.offset() - bias, cmd_tok.length());

            let mut exp_command = cmd_tok.get_source(&cmdline).to_owned();
            let mut prev = None;
            let mut cur = None;
            if expand_command_token(self.ctx, &mut exp_command) {
                prev = unescape_string(previous_argument, UnescapeStringStyle::default());
                cur = unescape_string(
                    current_argument,
                    UnescapeStringStyle::Script(UnescapeFlags::INCOMPLETE),
                );
            }
            if let (Some(prev), Some(cur)) = (prev, cur) {
                arg_data.previous_argument = prev;
                arg_data.current_argument = cur;
                // Have to walk over the command and its entire wrap chain. If any command
                // disables do_file, then they all do.
                self.walk_wrap_chain(
                    &exp_command,
                    effective_cmdline,
                    command_range,
                    &mut arg_data,
                );
                do_file = arg_data.do_file;

                // If we're autosuggesting, and the token is empty, don't do file suggestions.
                if is_autosuggest && arg_data.current_argument.is_empty() {
                    do_file = false;
                }
            }

            // Hack. If we're cd, handle it specially (issue #1059, others).
            handle_as_special_cd =
                exp_command == "cd" || arg_data.visited_wrapped_commands.contains(L!("cd"));
        }

        // Maybe apply variable assignments.
        let _restore_vars = self.apply_var_assignments(&var_assignments);
        if self.ctx.check_cancel() {
            return;
        }

        // This function wants the unescaped string.
        self.complete_param_expand(L!(""), current_argument, do_file, handle_as_special_cd);

        // Lastly mark any completions that appear to already be present in arguments.
        self.mark_completions_duplicating_arguments(&cmdline, current_token, tokens);
    }

    pub fn acquire_completions(&mut self) -> Vec<Completion> {
        self.completions.take()
    }

    pub fn acquire_needs_load(&mut self) -> Vec<WString> {
        mem::take(&mut self.needs_load)
    }

    /// Test if the specified script returns zero. The result is cached, so that if multiple completions
    /// use the same condition, it needs only be evaluated once. condition_cache_clear must be called
    /// after a completion run to make sure that there are no stale completions.
    fn condition_test(&mut self, condition: &wstr) -> bool {
        if condition.is_empty() {
            return true;
        }
        let Some(parser) = self.ctx.maybe_parser() else {
            return false;
        };

        let cached_entry = self.condition_cache.get(condition);
        if let Some(&entry) = cached_entry {
            // Use the old value.
            entry
        } else {
            // Compute new value and reinsert it.
            let test_res = exec_subshell(
                condition, parser, None, false, /* don't apply exit status */
            ) == 0;
            self.condition_cache.insert(condition.to_owned(), test_res);
            test_res
        }
    }

    fn conditions_test(&mut self, conditions: &[WString]) -> bool {
        conditions.iter().all(|c| self.condition_test(c))
    }

    /// Copy any strings in `possible_comp` which have the specified prefix to the
    /// completer's completion array. The prefix may contain wildcards. The output
    /// will consist of [`Completion`] structs.
    ///
    /// There are three ways to specify descriptions for each completion. Firstly,
    /// if a description has already been added to the completion, it is _not_
    /// replaced. Secondly, if the `desc_func` function is specified, use it to
    /// determine a dynamic completion. Thirdly, if none of the above are available,
    /// the `desc` string is used as a description.
    ///
    /// - `wc_escaped`: the prefix, possibly containing wildcards. The wildcard should not have
    ///    been unescaped, i.e. '*' should be used for any string, not the `ANY_STRING` character.
    /// - `desc_func`: the function that generates a description for those completions without an
    ///    embedded description
    /// - `possible_comp`: the list of possible completions to iterate over
    /// - `flags`: The flags controlling completion
    /// - `extra_expand_flags`: Additional flags controlling expansion.
    fn complete_strings(
        &mut self,
        wc_escaped: &wstr,
        desc_func: &DescriptionFunc,
        possible_comp: &[Completion],
        flags: CompleteFlags,
        extra_expand_flags: ExpandFlags,
    ) {
        let mut tmp = wc_escaped.to_owned();
        if !expand_one(
            &mut tmp,
            self.expand_flags()
                | extra_expand_flags
                | ExpandFlags::FAIL_ON_CMDSUBST
                | ExpandFlags::SKIP_WILDCARDS,
            self.ctx,
            None,
        ) {
            return;
        }

        let wc = parse_util_unescape_wildcards(&tmp);
        for comp in possible_comp {
            let comp_str = &comp.completion;
            if !comp_str.is_empty() {
                let expand_flags = self.expand_flags() | extra_expand_flags;
                wildcard_complete(
                    comp_str,
                    &wc,
                    Some(desc_func),
                    Some(&mut self.completions),
                    expand_flags,
                    flags,
                );
            }
        }
    }

    fn expand_flags(&self) -> ExpandFlags {
        let mut result = ExpandFlags::empty();
        result.set(ExpandFlags::FAIL_ON_CMDSUBST, self.flags.autosuggestion);
        result.set(ExpandFlags::FUZZY_MATCH, self.flags.fuzzy_match);
        result.set(ExpandFlags::GEN_DESCRIPTIONS, self.flags.descriptions);
        result
    }

    /// If command to complete is short enough, substitute the description with the whatis information
    /// for the executable.
    fn complete_cmd_desc(&mut self, s: &wstr) {
        let Some(parser) = self.ctx.maybe_parser() else {
            return;
        };

        let cmd = if let Some(pos) = s.chars().rposition(|c| c == '/') {
            if pos + 1 > s.len() {
                return;
            }
            &s[pos + 1..]
        } else {
            s
        };

        // Using apropos with a single-character search term produces far too many results - require at
        // least two characters if we don't know the location of the whatis-database.
        if cmd.len() < 2 {
            return;
        }

        if wildcard_has(cmd) {
            return;
        }

        let keep_going =
            self.completions.get_list().iter().any(|c| {
                c.completion.is_empty() || c.completion.as_char_slice().last() != Some(&'/')
            });
        if !keep_going {
            return;
        }

        let lookup_cmd: WString = [L!("__fish_describe_command "), &escape(cmd)]
            .into_iter()
            .collect();

        // First locate a list of possible descriptions using a single call to apropos or a direct
        // search if we know the location of the whatis database. This can take some time on slower
        // systems with a large set of manuals, but it should be ok since apropos is only called once.
        let mut list = vec![];
        exec_subshell(
            &lookup_cmd,
            parser,
            Some(&mut list),
            false, /* don't apply exit status */
        );

        // Then discard anything that is not a possible completion and put the result into a
        // hashtable with the completion as key and the description as value.
        let mut lookup = HashMap::new();
        // A typical entry is the command name, followed by a tab, followed by a description.
        for elstr in &mut list {
            // Skip keys that are too short.
            if elstr.len() < cmd.len() {
                continue;
            }

            // Skip cases without a tab, or without a description, or bizarre cases where the tab is
            // part of the command.
            let Some(tab_idx) = elstr.find_char('\t') else {
                continue;
            };
            if tab_idx + 1 >= elstr.len() || tab_idx < cmd.len() {
                continue;
            }

            // Make the set components. This is the stuff after the command.
            // For example:
            //  elstr = lsmod\ta description
            //  cmd = ls
            //  key = mod
            //  val = A description
            // Note an empty key is common and natural, if 'cmd' were already valid.
            let parts = elstr.as_mut_utfstr().split_at_mut(tab_idx);
            let key = &parts.0[cmd.len()..tab_idx];
            let (_, val) = parts.1.split_at_mut(1);

            // And once again I make sure the first character is uppercased because I like it that
            // way, and I get to decide these things.
            let mut upper_chars = val.chars().next().unwrap().to_uppercase();
            if let (Some(c), None) = (upper_chars.next(), upper_chars.next()) {
                val.as_char_slice_mut()[0] = c;
            }
            lookup.insert(key, &*val);
        }

        // Then do a lookup on every completion and if a match is found, change to the new
        // description.
        for completion in self.completions.get_list_mut() {
            let el = &completion.completion;
            if let Some(&desc) = lookup.get(el.as_utfstr()) {
                completion.description = desc.to_owned();
            }
        }
    }

    /// Complete the specified command name. Search for executables in the path, executables defined
    /// using an absolute path, functions, builtins and directories for implicit cd commands.
    ///
    /// \param str_cmd the command string to find completions for
    fn complete_cmd(&mut self, str_cmd: WString) {
        // Append all possible executables
        let result = {
            let expand_flags = self.expand_flags()
                | ExpandFlags::SPECIAL_FOR_COMMAND
                | ExpandFlags::FOR_COMPLETIONS
                | ExpandFlags::PRESERVE_HOME_TILDES
                | ExpandFlags::EXECUTABLES_ONLY;
            expand_to_receiver(
                str_cmd.clone(),
                &mut self.completions,
                expand_flags,
                self.ctx,
                None,
            )
            .result
        };
        if result == ExpandResultCode::cancel {
            return;
        }
        if result == ExpandResultCode::ok && self.flags.descriptions {
            self.complete_cmd_desc(&str_cmd);
        }

        // We don't really care if this succeeds or fails. If it succeeds this->completions will be
        // updated with choices for the user.
        let _ = {
            // Append all matching directories
            let expand_flags = self.expand_flags()
                | ExpandFlags::FOR_COMPLETIONS
                | ExpandFlags::PRESERVE_HOME_TILDES
                | ExpandFlags::DIRECTORIES_ONLY;
            expand_to_receiver(
                str_cmd.clone(),
                &mut self.completions,
                expand_flags,
                self.ctx,
                None,
            )
        };

        if str_cmd.is_empty() || (!str_cmd.contains('/') && str_cmd.as_char_slice()[0] != '~') {
            let include_hidden = str_cmd.as_char_slice().first() == Some(&'_');
            // Append all known matching functions
            let possible_comp: Vec<_> = function::get_names(include_hidden, self.ctx.vars())
                .into_iter()
                .map(Completion::from_completion)
                .collect();

            self.complete_strings(
                &str_cmd,
                &{ Box::new(complete_function_desc) as DescriptionFunc },
                &possible_comp,
                CompleteFlags::empty(),
                ExpandFlags::empty(),
            );

            // Append all matching builtins
            let possible_comp: Vec<_> = builtin_get_names()
                .map(wstr::to_owned)
                .map(Completion::from_completion)
                .collect();

            self.complete_strings(
                &str_cmd,
                &{ Box::new(|name| builtin_get_desc(name).unwrap_or(L!("")).to_owned()) },
                &possible_comp,
                CompleteFlags::empty(),
                ExpandFlags::empty(),
            );
        }
    }

    /// Attempt to complete an abbreviation for the given string.
    fn complete_abbr(&mut self, cmd: WString) {
        // Copy the list of names and descriptions so as not to hold the lock across the call to
        // complete_strings.
        let mut possible_comp = Vec::new();
        let mut descs = HashMap::new();
        with_abbrs(|set| {
            for abbr in set.list() {
                if !abbr.is_regex() {
                    possible_comp.push(Completion::from_completion(abbr.key.clone()));
                    descs.insert(abbr.key.clone(), abbr.replacement.clone());
                }
            }
        });

        let desc_func = move |key: &wstr| {
            let replacement = descs.get(key).expect("Abbreviation not found");
            sprintf!(*ABBR_DESC, replacement)
        };
        self.complete_strings(
            &cmd,
            &{ Box::new(desc_func) as _ },
            &possible_comp,
            CompleteFlags::NO_SPACE,
            ExpandFlags::empty(),
        );
    }

    /// Evaluate the argument list (as supplied by `complete -a`) and insert any
    /// return matching completions. Matching is done using `copy_strings_with_prefix`,
    /// meaning the completion may contain wildcards.
    /// Logically, this is not always the right thing to do, but I have yet to come
    /// up with a case where this matters.
    ///
    /// - `str`: The string to complete.
    /// - `args`: The list of option arguments to be evaluated.
    /// - `desc`: Description of the completion
    /// - `flags`: The flags
    fn complete_from_args(&mut self, s: &wstr, args: &wstr, desc: &wstr, flags: CompleteFlags) {
        let is_autosuggest = self.flags.autosuggestion;

        let saved_state = if let Some(parser) = self.ctx.maybe_parser() {
            let saved_interactive = parser.libdata().is_interactive;
            parser.libdata_mut().is_interactive = false;

            Some((saved_interactive, parser.get_last_statuses()))
        } else {
            None
        };

        let eflags = if is_autosuggest {
            ExpandFlags::FAIL_ON_CMDSUBST
        } else {
            ExpandFlags::empty()
        };

        let possible_comp = Parser::expand_argument_list(args, eflags, self.ctx);

        if let Some(parser) = self.ctx.maybe_parser() {
            let (saved_interactive, status) = saved_state.unwrap();
            parser.libdata_mut().is_interactive = saved_interactive;
            parser.set_last_statuses(status);
        }

        // Allow leading dots - see #3707.
        self.complete_strings(
            &escape(s),
            &const_desc(desc),
            &possible_comp,
            flags,
            ExpandFlags::ALLOW_NONLITERAL_LEADING_DOT,
        );
    }

    /// complete_param: Given a command, find completions for the argument `s` of command `cmd_orig`
    /// with previous option `popt`. If file completions should be disabled, then mark
    /// `out_do_file` as `false`.
    ///
    /// Returns `true` if successful, `false` if there's an error.
    ///
    /// Examples in format (cmd, popt, str):
    ///
    /// ```text
    /// echo hello world <tab> -> ("echo", "world", "")
    /// echo hello world<tab> -> ("echo", "hello", "world")
    /// ```
    fn complete_param_for_command(
        &mut self,
        cmd_orig: &wstr,
        popt: &wstr,
        s: &wstr,
        use_switches: bool,
        out_do_file: &mut bool,
    ) -> bool {
        let mut use_files = true;
        let mut has_force = false;

        let CmdString { cmd, path } = parse_cmd_string(cmd_orig, self.ctx.vars());

        // Don't use cmd_orig here for paths. It's potentially pathed,
        // so that command might exist, but the completion script
        // won't be using it.
        let cmd_exists = builtin_exists(&cmd)
            || function::exists_no_autoload(&cmd)
            || path_get_path(&cmd, self.ctx.vars()).is_some();
        if !cmd_exists {
            // Do not load custom completions if the command does not exist
            // This prevents errors caused during the execution of completion providers for
            // tools that do not exist. Applies to both manual completions ("cm<TAB>", "cmd <TAB>")
            // and automatic completions ("gi" autosuggestion provider -> git)
            FLOG!(complete, "Skipping completions for non-existent command");
        } else if let Some(parser) = self.ctx.maybe_parser() {
            complete_load(&cmd, parser);
        } else if !completion_autoloader
            .lock()
            .unwrap()
            .has_attempted_autoload(&cmd)
        {
            self.needs_load.push(cmd.clone());
        }

        // Make a list of lists of all options that we care about.
        let all_options: Vec<Vec<CompleteEntryOpt>> = COMPLETION_MAP
            .lock()
            .unwrap()
            .iter()
            .filter_map(|(idx, completion)| {
                let r#match = if idx.is_path { &path } else { &cmd };
                if wildcard_match(r#match, &idx.name, false) {
                    // Copy all of their options into our list. Oof, this is a lot of copying.
                    let mut options = completion.get_options().to_vec();
                    // We have to copy them in reverse order to preserve legacy behavior (#9221).
                    options.reverse();
                    Some(options)
                } else {
                    None
                }
            })
            .collect();

        // Now release the lock and test each option that we captured above. We have to do this outside
        // the lock because callouts (like the condition) may add or remove completions. See issue #2.
        for options in all_options {
            let short_opt_pos = short_option_pos(s, &options);
            // We want last_option_requires_param to default to false but distinguish between when
            // a previous completion has set it to false and when it has its default value.
            let mut last_option_requires_param = None;
            let mut use_common = true;
            if use_switches {
                if s.char_at(0) == '-' {
                    // Check if we are entering a combined option and argument (like --color=auto or
                    // -I/usr/include).
                    for o in &options {
                        let arg = if o.typ == CompleteOptionType::Short {
                            let Some(short_opt_pos) = short_opt_pos else {
                                continue;
                            };
                            if o.option.char_at(0) != s.char_at(short_opt_pos) {
                                continue;
                            }
                            Some(s.slice_from(short_opt_pos + 1))
                        } else {
                            param_match2(o, s)
                        };

                        if self.conditions_test(&o.conditions) {
                            if o.typ == CompleteOptionType::Short {
                                // Only override a true last_option_requires_param value with a false
                                // one
                                *last_option_requires_param
                                    .get_or_insert(o.result_mode.requires_param) &=
                                    o.result_mode.requires_param;
                            }
                            if let Some(arg) = arg {
                                if o.result_mode.requires_param {
                                    use_common = false;
                                }
                                if o.result_mode.no_files {
                                    use_files = false;
                                }
                                if o.result_mode.force_files {
                                    has_force = true;
                                }
                                self.complete_from_args(arg, &o.comp, o.localized_desc(), o.flags);
                            }
                        }
                    }
                } else if popt.char_at(0) == '-' {
                    // Set to true if we found a matching old-style switch.
                    // Here we are testing the previous argument,
                    // to see how we should complete the current argument
                    let mut old_style_match = false;

                    // If we are using old style long options, check for them first.
                    for o in &options {
                        if o.typ == CompleteOptionType::SingleLong
                            && param_match(o, popt)
                            && self.conditions_test(&o.conditions)
                        {
                            old_style_match = false;
                            if o.result_mode.requires_param {
                                use_common = false;
                            }
                            if o.result_mode.no_files {
                                use_files = false;
                            }
                            if o.result_mode.force_files {
                                has_force = true;
                            }
                            self.complete_from_args(s, &o.comp, o.localized_desc(), o.flags);
                        }
                    }

                    // No old style option matched, or we are not using old style options. We check if
                    // any short (or gnu style) options do.
                    if !old_style_match {
                        let prev_short_opt_pos = short_option_pos(popt, &options);
                        for o in &options {
                            // Gnu-style options with _optional_ arguments must be specified as a single
                            // token, so that it can be differed from a regular argument.
                            // Here we are testing the previous argument for a GNU-style match,
                            // to see how we should complete the current argument
                            if !o.result_mode.requires_param {
                                continue;
                            }

                            let mut r#match = false;
                            if o.typ == CompleteOptionType::Short {
                                if let Some(prev_short_opt_pos) = prev_short_opt_pos {
                                    r#match = prev_short_opt_pos + 1 == popt.len()
                                        && o.option.char_at(0) == popt.char_at(prev_short_opt_pos);
                                }
                            } else if o.typ == CompleteOptionType::DoubleLong {
                                r#match = param_match(o, popt);
                            }
                            if r#match && self.conditions_test(&o.conditions) {
                                if o.result_mode.requires_param {
                                    use_common = false;
                                }
                                if o.result_mode.no_files {
                                    use_files = false;
                                }
                                if o.result_mode.force_files {
                                    has_force = true;
                                }
                                self.complete_from_args(s, &o.comp, o.localized_desc(), o.flags);
                            }
                        }
                    }
                }
            }

            if !use_common {
                continue;
            }

            // Set a default value for last_option_requires_param only if one hasn't been set
            let last_option_requires_param = last_option_requires_param.unwrap_or(false);

            // Now we try to complete an option itself
            for o in &options {
                // If this entry is for the base command, check if any of the arguments match.
                if !self.conditions_test(&o.conditions) {
                    continue;
                }
                if o.option.is_empty() {
                    use_files &= !o.result_mode.no_files;
                    has_force |= o.result_mode.force_files;
                    self.complete_from_args(s, &o.comp, o.localized_desc(), o.flags);
                }

                if !use_switches || s.is_empty() {
                    continue;
                }

                // Check if the short style option matches.
                if o.typ == CompleteOptionType::Short {
                    let optchar = o.option.char_at(0);
                    if let Some(short_opt_pos) = short_opt_pos {
                        // Only complete when the last short option has no parameter yet..
                        if short_opt_pos + 1 != s.len() {
                            continue;
                        }
                        // .. and it does not require one ..
                        if last_option_requires_param {
                            continue;
                        }
                        // .. and the option is not already there.
                        if s.contains(optchar) {
                            continue;
                        }
                    } else {
                        // str has no short option at all (but perhaps it is the
                        // prefix of a single long option).
                        // Only complete short options if there is no character after the dash.

                        if s != L!("-") {
                            continue;
                        }
                    }
                    // It's a match.
                    let desc = o.localized_desc();
                    // Append a short-style option
                    if !self
                        .completions
                        .add(Completion::with_desc(o.option.clone(), desc.to_owned()))
                    {
                        return false;
                    }
                }

                // Check if the long style option matches.
                if o.typ != CompleteOptionType::SingleLong
                    && o.typ != CompleteOptionType::DoubleLong
                {
                    continue;
                }

                let whole_opt = L!("-").repeat(o.expected_dash_count()) + o.option.as_utfstr();
                if whole_opt.len() < s.len() {
                    continue;
                }
                if !s.starts_with("-") {
                    continue;
                }
                let anchor_start = !self.flags.fuzzy_match;
                let Some(r#match) = string_fuzzy_match_string(s, &whole_opt, anchor_start) else {
                    continue;
                };

                let mut offset = 0;
                let mut flags = CompleteFlags::empty();

                if r#match.requires_full_replacement() {
                    flags = CompleteFlags::REPLACES_TOKEN;
                } else {
                    offset = s.len();
                }

                // does this switch have any known arguments
                let has_arg = !o.comp.is_empty();
                // does this switch _require_ an argument
                let req_arg = o.result_mode.requires_param;

                if o.typ == CompleteOptionType::DoubleLong && (has_arg && !req_arg) {
                    // Optional arguments to a switch can only be handled using the '=', so we add it as
                    // a completion. By default we avoid using '=' and instead rely on '--switch
                    // switch-arg', since it is more commonly supported by homebrew getopt-like
                    // functions.
                    let completion = sprintf!("%ls=", whole_opt.slice_from(offset));

                    // Append a long-style option with a mandatory trailing equal sign
                    if !self.completions.add(Completion::new(
                        completion,
                        o.localized_desc().to_owned(),
                        StringFuzzyMatch::exact_match(),
                        flags | CompleteFlags::NO_SPACE,
                    )) {
                        return false;
                    }
                }

                // Append a long-style option
                if !self.completions.add(Completion::new(
                    whole_opt.slice_from(offset).to_owned(),
                    o.localized_desc().to_owned(),
                    StringFuzzyMatch::exact_match(),
                    flags,
                )) {
                    return false;
                }
            }
        }

        if has_force {
            *out_do_file = true;
        } else if !use_files {
            *out_do_file = false;
        }

        true
    }

    /// Perform generic (not command-specific) expansions on the specified string.
    fn complete_param_expand(
        &mut self,
        variable_override_prefix: &wstr,
        s: &wstr,
        do_file: bool,
        handle_as_special_cd: bool,
    ) {
        if self.ctx.check_cancel() {
            return;
        }
        let mut flags = self.expand_flags()
            | ExpandFlags::FAIL_ON_CMDSUBST
            | ExpandFlags::FOR_COMPLETIONS
            | ExpandFlags::PRESERVE_HOME_TILDES;
        if !do_file {
            flags |= ExpandFlags::SKIP_WILDCARDS;
        }

        if handle_as_special_cd && do_file {
            if self.flags.autosuggestion {
                flags |= ExpandFlags::SPECIAL_FOR_CD_AUTOSUGGESTION;
            }
            flags |= ExpandFlags::DIRECTORIES_ONLY;
            flags |= ExpandFlags::SPECIAL_FOR_CD;
        }

        // Squelch file descriptions per issue #254.
        if self.flags.autosuggestion || do_file {
            flags -= ExpandFlags::GEN_DESCRIPTIONS;
        }

        // Expand words separated by '=' separately, unless '=' is escaped or quoted.
        // We have the following cases:
        //
        // --foo=bar => expand just bar
        // -foo=bar => expand just bar
        // foo=bar => expand the whole thing, and also just bar
        //
        // We also support colon separator (#2178). If there's more than one, prefer the last one.
        let quoted = get_quote(s, s.len()).is_some();
        let sep_index = if quoted {
            None
        } else {
            let mut end = s.len();
            loop {
                match s[..end].chars().rposition(|c| c == '=' || c == ':') {
                    Some(pos) => {
                        if !is_backslashed(s, pos) {
                            break Some(pos);
                        }
                        end = pos;
                    }
                    None => break None,
                }
            }
        };
        let complete_from_start = sep_index.is_none() || !string_prefixes_string(L!("-"), s);

        if let Some(sep_index) = sep_index {
            let sep_string = s.slice_from(sep_index + 1);
            let mut local_completions = Vec::new();
            if expand_string(
                sep_string.to_owned(),
                &mut local_completions,
                flags,
                self.ctx,
                None,
            )
            .result
                == ExpandResultCode::error
            {
                FLOGF!(complete, "Error while expanding string '%ls'", sep_string);
            }

            // Any COMPLETE_REPLACES_TOKEN will also stomp the separator. We need to "repair" them by
            // inserting our separator and prefix.
            Self::escape_opening_brackets(&mut local_completions, s);
            Self::escape_separators(
                &mut local_completions,
                variable_override_prefix,
                self.flags.autosuggestion,
                true,
                quoted,
            );
            let prefix_with_sep = s.as_char_slice()[..sep_index + 1].into();
            for comp in &mut local_completions {
                comp.prepend_token_prefix(prefix_with_sep);
            }
            if !self.completions.extend(local_completions) {
                return;
            }
        }

        if complete_from_start {
            // Don't do fuzzy matching for files if the string begins with a dash (issue #568). We could
            // consider relaxing this if there was a preceding double-dash argument.
            if string_prefixes_string(L!("-"), s) {
                flags -= ExpandFlags::FUZZY_MATCH;
            }

            let first = self.completions.len();
            if expand_to_receiver(s.to_owned(), &mut self.completions, flags, self.ctx, None).result
                == ExpandResultCode::error
            {
                FLOGF!(complete, "Error while expanding string '%ls'", s);
            }
            Self::escape_opening_brackets(&mut self.completions[first..], s);
            let have_token = !s.is_empty();
            Self::escape_separators(
                &mut self.completions[first..],
                variable_override_prefix,
                self.flags.autosuggestion,
                have_token,
                quoted,
            );
        }
    }

    fn escape_separators(
        completions: &mut [Completion],
        variable_override_prefix: &wstr,
        append_only: bool,
        have_token: bool,
        is_quoted: bool,
    ) {
        for c in completions {
            if is_quoted && !c.replaces_token() {
                continue;
            }
            // clone of completion_apply_to_command_line
            let add_space = !c.flags.contains(CompleteFlags::NO_SPACE);
            let no_tilde = c.flags.contains(CompleteFlags::DONT_ESCAPE_TILDES);
            let mut escape_flags = EscapeFlags::SEPARATORS;
            if append_only || !add_space || (!c.replaces_token() && have_token) {
                escape_flags.insert(EscapeFlags::NO_QUOTED);
            }
            if no_tilde {
                escape_flags.insert(EscapeFlags::NO_TILDE);
            }
            if c.replaces_token() {
                c.completion = variable_override_prefix.to_owned()
                    + &escape_string(&c.completion, EscapeStringStyle::Script(escape_flags))[..];
            } else {
                c.completion =
                    escape_string(&c.completion, EscapeStringStyle::Script(escape_flags));
            }
            assert!(!c.flags.contains(CompleteFlags::DONT_ESCAPE));
            c.flags |= CompleteFlags::DONT_ESCAPE;
        }
    }
    /// Complete the specified string as an environment variable.
    /// Returns `true` if this was a variable, so we should stop completion.
    fn complete_variable(&mut self, s: &wstr, start_offset: usize) -> bool {
        let whole_var = s;
        let var = whole_var.slice_from(start_offset);
        let varlen = s.len() - start_offset;
        let mut res = false;

        for env_name in self.ctx.vars().get_names(EnvMode::empty()) {
            let anchor_start = !self.flags.fuzzy_match;
            let Some(r#match) = string_fuzzy_match_string(var, &env_name, anchor_start) else {
                continue;
            };

            let (comp, flags) = if !r#match.requires_full_replacement() {
                // Take only the suffix.
                (
                    env_name.slice_from(varlen).to_owned(),
                    CompleteFlags::empty(),
                )
            } else {
                let comp = whole_var.slice_to(start_offset).to_owned() + env_name.as_utfstr();
                let flags = CompleteFlags::REPLACES_TOKEN | CompleteFlags::DONT_ESCAPE;
                (comp, flags)
            };

            let mut desc = WString::new();
            if self.flags.descriptions && self.flags.autosuggestion {
                // $history can be huge, don't put all of it in the completion description; see
                // #6288.
                if env_name == "history" {
                    let history = History::with_name(&history_session_id(self.ctx.vars()));
                    for i in 1..std::cmp::min(history.size(), 64) {
                        if i > 1 {
                            desc.push(' ');
                        }
                        desc.push_utfstr(&expand_escape_string(
                            history.item_at_index(i).unwrap().str(),
                        ));
                    }
                } else {
                    // Can't use ctx.vars() here, it could be any variable.
                    let Some(var) = self.ctx.vars().get(&env_name) else {
                        continue;
                    };

                    let value = expand_escape_variable(&var);
                    desc = sprintf!(*COMPLETE_VAR_DESC_VAL, value);
                }
            }

            // Append matching environment variables
            // TODO: need to propagate overflow here.
            let _ = self
                .completions
                .add(Completion::new(comp, desc, r#match, flags));

            res = true;
        }

        res
    }

    fn try_complete_variable(&mut self, s: &wstr) -> bool {
        #[derive(PartialEq, Eq)]
        enum Mode {
            Unquoted,
            SingleQuoted,
            DoubleQuoted,
        }
        use Mode::*;

        let mut mode = Unquoted;

        // Get the position of the dollar heading a (possibly empty) run of valid variable characters.
        let mut variable_start = None;

        let mut skip_next = false;
        for (in_pos, c) in s.chars().enumerate() {
            if skip_next {
                skip_next = false;
                continue;
            }

            if !valid_var_name_char(c) {
                // This character cannot be in a variable, reset the dollar.
                variable_start = None;
            }

            match c {
                '\\' => skip_next = true,
                '$' => {
                    if mode == Unquoted || mode == DoubleQuoted {
                        variable_start = Some(in_pos);
                    }
                }
                '\'' => {
                    if mode == SingleQuoted {
                        mode = Unquoted;
                    } else if mode == Unquoted {
                        mode = SingleQuoted;
                    }
                }
                '"' => {
                    if mode == DoubleQuoted {
                        mode = Unquoted;
                    } else if mode == Unquoted {
                        mode = DoubleQuoted;
                    }
                }
                _ => {
                    // all other chars ignored here
                }
            }
        }

        // Now complete if we have a variable start. Note the variable text may be empty; in that case
        // don't generate an autosuggestion, but do allow tab completion.
        let allow_empty = !self.flags.autosuggestion;
        let text_is_empty = variable_start == Some(s.len() - 1);
        if let Some(variable_start) = variable_start {
            if allow_empty || !text_is_empty {
                return self.complete_variable(s, variable_start + 1);
            }
        }

        false
    }

    /// Try to complete the specified string as a username. This is used by `~USER` type expansion.
    ///
    /// Returns `false` if unable to complete, `true` otherwise
    fn try_complete_user(&mut self, s: &wstr) -> bool {
        #[cfg(target_os = "android")]
        {
            // The getpwent() function does not exist on Android. A Linux user on Android isn't
            // really a user - each installed app gets an UID assigned. Listing all UID:s is not
            // possible without root access, and doing a ~USER type expansion does not make sense
            // since every app is sandboxed and can't access eachother.
            return false;
        }
        #[cfg(not(target_os = "android"))]
        {
            static s_setpwent_lock: Mutex<()> = Mutex::new(());

            if s.char_at(0) != '~' || s.contains('/') {
                return false;
            }

            let user_name = s.slice_from(1);
            if user_name.contains('~') {
                return false;
            }

            let start_time = Instant::now();
            let mut result = false;
            let name_len = s.len() - 1;

            fn getpwent_name() -> Option<WString> {
                let ptr = unsafe { libc::getpwent() };
                if ptr.is_null() {
                    return None;
                }
                let pw = unsafe { ptr.read() };
                Some(charptr2wcstring(pw.pw_name))
            }

            let _guard = s_setpwent_lock.lock().unwrap();

            unsafe { libc::setpwent() };
            while let Some(pw_name) = getpwent_name() {
                if self.ctx.check_cancel() {
                    break;
                }

                if string_prefixes_string(user_name, &pw_name) {
                    let desc = sprintf!(*COMPLETE_USER_DESC, &pw_name);
                    // Append a user name.
                    // TODO: propagate overflow?
                    let _ = self.completions.add(Completion::new(
                        pw_name.slice_from(name_len).to_owned(),
                        desc,
                        StringFuzzyMatch::exact_match(),
                        CompleteFlags::NO_SPACE,
                    ));
                    result = true;
                } else if string_prefixes_string_case_insensitive(user_name, &pw_name) {
                    let name = sprintf!("~%ls", &pw_name);
                    let desc = sprintf!(*COMPLETE_USER_DESC, &pw_name);

                    // Append a user name
                    // TODO: propagate overflow?
                    let _ = self.completions.add(Completion::new(
                        name,
                        desc,
                        StringFuzzyMatch::exact_match(),
                        CompleteFlags::REPLACES_TOKEN
                            | CompleteFlags::DONT_ESCAPE
                            | CompleteFlags::NO_SPACE,
                    ));
                    result = true;
                }

                // If we've spent too much time (more than 200 ms) doing this give up.
                if start_time.elapsed() > Duration::from_millis(200) {
                    break;
                }
            }
            unsafe { libc::endpwent() };

            result
        }
    }

    /// If we have variable assignments, attempt to apply them in our parser. As soon as the return
    /// value goes out of scope, the variables will be removed from the parser.
    fn apply_var_assignments<T: AsRef<wstr>>(
        &mut self,
        var_assignments: &[T],
    ) -> Option<ScopeGuard<(), impl FnOnce(&mut ()) + 'ctx>> {
        if !self.ctx.has_parser() || var_assignments.is_empty() {
            return None;
        }
        let parser = self.ctx.parser();

        let vars = parser.vars();
        assert_eq!(
            self.ctx.vars() as *const _ as *const (),
            vars as *const _ as *const (),
            "Don't know how to tab complete with a parser but a different variable set"
        );

        // clone of parse_execution_context_t::apply_variable_assignments.
        // Crucially do NOT expand subcommands:
        //   VAR=(launch_missiles) cmd<tab>
        // should not launch missiles.
        // Note we also do NOT send --on-variable events.
        let expand_flags = ExpandFlags::FAIL_ON_CMDSUBST;
        let block = parser.push_block(Block::variable_assignment_block());
        for var_assign in var_assignments {
            let var_assign: &wstr = var_assign.as_ref();
            let equals_pos = variable_assignment_equals_pos(var_assign)
                .expect("All variable assignments should have equals position");
            let variable_name = var_assign.as_char_slice()[..equals_pos].into();
            let expression = var_assign.slice_from(equals_pos + 1);

            let mut expression_expanded = Vec::new();
            let expand_ret = expand_string(
                expression.to_owned(),
                &mut expression_expanded,
                expand_flags,
                self.ctx,
                None,
            );
            // If expansion succeeds, set the value; if it fails (e.g. it has a cmdsub) set an empty
            // value anyways.
            let vals = if expand_ret.result == ExpandResultCode::ok {
                expression_expanded
                    .into_iter()
                    .map(|c| c.completion)
                    .collect()
            } else {
                Vec::new()
            };
            parser
                .vars()
                .set(variable_name, EnvMode::LOCAL | EnvMode::EXPORT, vals);
            if self.ctx.check_cancel() {
                break;
            }
        }

        let parser = self.ctx.parser();
        Some(ScopeGuard::new((), move |_| parser.pop_block(block)))
    }

    /// Complete a command by invoking user-specified completions.
    fn complete_custom(&mut self, cmd: &wstr, cmdline: &wstr, ad: &mut CustomArgData) {
        if self.ctx.check_cancel() {
            return;
        }

        let is_autosuggest = self.flags.autosuggestion;
        // Perhaps set a transient commandline so that custom completions
        // builtin_commandline will refer to the wrapped command. But not if
        // we're doing autosuggestions.
        let mut _remove_transient = None;
        let wants_transient =
            (ad.wrap_depth > 0 || !ad.var_assignments.is_empty()) && !is_autosuggest;
        if wants_transient {
            let parser = self.ctx.parser();
            parser
                .libdata_mut()
                .transient_commandlines
                .push(cmdline.to_owned());
            _remove_transient = Some(ScopeGuard::new((), move |_| {
                parser.libdata_mut().transient_commandlines.pop();
            }));
        }

        // Maybe apply variable assignments.
        let _restore_vars = self.apply_var_assignments(ad.var_assignments);
        if self.ctx.check_cancel() {
            return;
        }

        // Invoke any custom completions for this command.
        self.complete_param_for_command(
            cmd,
            &ad.previous_argument,
            &ad.current_argument,
            !ad.had_ddash,
            &mut ad.do_file,
        );
    }

    // Invoke command-specific completions given by `arg_data`.
    // Then, for each target wrapped by the given command, update the command
    // line with that target and invoke this recursively.
    // The command whose completions to use is given by `cmd`. The full command line is given by \p
    // cmdline and the command's range in it is given by `cmdrange`. Note: the command range
    // may have a different length than the command itself, because the command is unescaped (i.e.
    // quotes removed).
    fn walk_wrap_chain(
        &mut self,
        cmd: &wstr,
        cmdline: &wstr,
        cmdrange: SourceRange,
        ad: &mut CustomArgData,
    ) {
        // Limit our recursion depth. This prevents cycles in the wrap chain graph from overflowing.
        if ad.wrap_depth > 24 {
            return;
        }
        if self.ctx.check_cancel() {
            return;
        }

        // Extract command from the command line and invoke the receiver with it.
        self.complete_custom(cmd, cmdline, ad);

        let targets = complete_get_wrap_targets(cmd);
        let wrap_depth = ad.wrap_depth;
        let mut ad = ScopeGuard::new(ad, |ad| ad.wrap_depth = wrap_depth);
        ad.wrap_depth += 1;

        for wt in targets {
            // We may append to the variable assignment list; ensure we restore it.
            let saved_var_count = ad.var_assignments.len();
            let mut ad = ScopeGuard::new(&mut ad, |ad| {
                assert!(
                    ad.var_assignments.len() >= saved_var_count,
                    "Should not delete var assignments"
                );
                ad.var_assignments.truncate(saved_var_count);
            });

            // Separate the wrap target into any variable assignments VAR=... and the command itself.
            let mut wrapped_command = None;
            let mut wrapped_command_offset_in_wt = None;
            let tokenizer = Tokenizer::new(&wt, TokFlags(0));
            for tok in tokenizer {
                let mut tok_src = tok.get_source(&wt).to_owned();
                if variable_assignment_equals_pos(&tok_src).is_some() {
                    ad.var_assignments.push(tok_src);
                } else {
                    expand_command_token(self.ctx, &mut tok_src);

                    wrapped_command_offset_in_wt = Some(tok.offset());
                    wrapped_command = Some(tok_src);

                    break;
                }
            }

            // Skip this wrapped command if empty, or if we've seen it before.
            let Some((wrapped_command, wrapped_command_offset_in_wt)) =
                Option::zip(wrapped_command, wrapped_command_offset_in_wt)
            else {
                continue;
            };

            if !ad.visited_wrapped_commands.insert(wrapped_command.clone()) {
                continue;
            }

            // Construct a fake command line containing the wrap target.
            // https://github.com/starkat99/widestring-rs/issues/37
            let mut faux_commandline = cmdline.as_char_slice().to_vec();
            faux_commandline.splice(std::ops::Range::from(cmdrange), wt.chars());
            let faux_commandline = WString::from(faux_commandline);

            // Recurse with our new command and command line.
            let faux_source_range = SourceRange::new(
                cmdrange.start() + wrapped_command_offset_in_wt,
                wrapped_command.len(),
            );
            self.walk_wrap_chain(
                &wrapped_command,
                &faux_commandline,
                faux_source_range,
                ***ad,
            );
        }
    }

    /// If the argument contains a '[' typed by the user, completion by appending to the argument might
    /// produce an invalid token (#5831).
    ///
    /// Check if there is any unescaped, unquoted '['; if yes, make the completions replace the entire
    /// argument instead of appending, so '[' will be escaped.
    fn escape_opening_brackets(completions: &mut [Completion], argument: &wstr) {
        let mut have_unquoted_unescaped_bracket = false;
        let mut quote = None;
        let mut escaped = false;
        for c in argument.chars() {
            have_unquoted_unescaped_bracket |= c == '[' && quote.is_none() && !escaped;
            if escaped {
                escaped = false;
            } else if c == '\\' {
                escaped = true;
            } else if c == '\'' || c == '"' {
                if quote == Some(c) {
                    // Closing a quote.
                    quote = None;
                } else if quote.is_none() {
                    // Opening a quote.
                    quote = Some(c);
                }
            }
        }
        if !have_unquoted_unescaped_bracket {
            return;
        }

        // Since completion_apply_to_command_line will escape the completion, we need to provide an
        // unescaped version.
        let Some(unescaped_argument) = unescape_string(
            argument,
            UnescapeStringStyle::Script(UnescapeFlags::INCOMPLETE),
        ) else {
            return;
        };
        for comp in completions {
            if comp.flags.contains(CompleteFlags::REPLACES_TOKEN) {
                continue;
            }
            comp.flags |= CompleteFlags::REPLACES_TOKEN;
            comp.flags |= CompleteFlags::DONT_ESCAPE_TILDES; // See #9073.

            // We are grafting a completion that is expected to be escaped later. This will break
            // if the original completion doesn't want escaping.  Happily, this is only the case
            // for username completion and variable name completion. They shouldn't end up here
            // anyway because they won't contain '['.
            if comp.flags.contains(CompleteFlags::DONT_ESCAPE) {
                FLOG!(warning, "unexpected completion flag");
            }
            comp.completion.insert_utfstr(0, &unescaped_argument);
        }
    }

    /// Set the `DUPLICATES_ARG` flag in any completion that duplicates an argument.
    fn mark_completions_duplicating_arguments(
        &mut self,
        cmd: &wstr,
        prefix: &wstr,
        args: impl IntoIterator<Item = Tok>,
    ) {
        // Get all the arguments, unescaped, into an array that we're going to bsearch.
        let mut arg_strs: Vec<_> = args
            .into_iter()
            .map(|arg| arg.get_source(cmd))
            .filter_map(|argstr| unescape_string(argstr, UnescapeStringStyle::default()))
            .collect();
        arg_strs.sort();

        let mut comp_str;
        for comp in self.completions.get_list_mut() {
            comp_str = comp.completion.clone();
            if !comp.flags.contains(CompleteFlags::REPLACES_TOKEN) {
                comp_str.insert_utfstr(0, prefix);
            }
            if arg_strs.binary_search(&comp_str).is_ok() {
                comp.flags |= CompleteFlags::DUPLICATES_ARGUMENT;
            }
        }
    }
}

struct CmdString {
    cmd: WString,
    path: WString,
}

/// Find the full path and commandname from a command string `s`.
fn parse_cmd_string(s: &wstr, vars: &dyn Environment) -> CmdString {
    let path_result = path_try_get_path(s, vars);
    let found = path_result.err.is_none();
    let mut path = path_result.path;
    // Resolve commands that use relative paths because we compare full paths with "complete -p".
    if found && !path.is_empty() && path.as_char_slice().first() != Some(&'/') {
        if let Some(full_path) = wrealpath(&path) {
            path = full_path;
        }
    }

    // Make sure the path is not included in the command.
    let cmd = if let Some(last_slash) = s.chars().rposition(|c| c == '/') {
        &s[last_slash + 1..]
    } else {
        s
    }
    .to_owned();

    CmdString { cmd, path }
}

/// Returns a description for the specified function, or an empty string if none.
fn complete_function_desc(f: &wstr) -> WString {
    if let Some(props) = function::get_props(f) {
        props.description.clone()
    } else {
        WString::new()
    }
}

fn leading_dash_count(s: &wstr) -> usize {
    s.chars().take_while(|&c| c == '-').count()
}

/// Match a parameter.
fn param_match(e: &CompleteEntryOpt, optstr: &wstr) -> bool {
    if e.typ == CompleteOptionType::ArgsOnly {
        false
    } else {
        let dashes = leading_dash_count(optstr);
        dashes == e.expected_dash_count() && e.option == optstr[dashes..]
    }
}

/// Test if a string is an option with an argument, like --color=auto or -I/usr/include.
fn param_match2<'s>(e: &CompleteEntryOpt, optstr: &'s wstr) -> Option<&'s wstr> {
    // We may get a complete_entry_opt_t with no options if it's just arguments.
    if e.option.is_empty() {
        return None;
    }

    // Verify leading dashes.
    let mut cursor = leading_dash_count(optstr);
    if cursor != e.expected_dash_count() {
        return None;
    }

    // Verify options match.
    if !optstr.slice_from(cursor).starts_with(&e.option) {
        return None;
    }
    cursor += e.option.len();

    // Short options are like -DNDEBUG. Long options are like --color=auto. So check for an equal
    // sign for long options.
    assert!(e.typ != CompleteOptionType::Short);
    if optstr.char_at(cursor) != '=' {
        return None;
    }
    cursor += 1;
    Some(optstr.slice_from(cursor))
}

/// Parses a token of short options plus one optional parameter like
/// '-xzPARAM', where x and z are short options.
///
/// Returns the position of the last option character (e.g. the position of z which is 2).
/// Everything after that is assumed to be part of the parameter.
/// Returns wcstring::npos if there is no valid short option.
fn short_option_pos(arg: &wstr, options: &[CompleteEntryOpt]) -> Option<usize> {
    if arg.len() <= 1 || leading_dash_count(arg) != 1 {
        return None;
    }
    for (pos, arg_char) in arg.chars().enumerate().skip(1) {
        let r#match = options
            .iter()
            .find(|o| o.typ == CompleteOptionType::Short && o.option.char_at(0) == arg_char);
        if let Some(r#match) = r#match {
            if r#match.result_mode.requires_param {
                return Some(pos);
            }
        } else {
            // The first character after the dash is not a valid option.
            if pos == 1 {
                return None;
            }
            return Some(pos - 1);
        }
    }

    Some(arg.len() - 1)
}

fn expand_command_token(ctx: &OperationContext<'_>, cmd_tok: &mut WString) -> bool {
    // TODO: we give up if the first token expands to more than one argument. We could handle
    // that case by propagating arguments.
    // Also we could expand wildcards.
    expand_one(
        cmd_tok,
        ExpandFlags::FAIL_ON_CMDSUBST | ExpandFlags::SKIP_WILDCARDS,
        ctx,
        None,
    )
}

/// Add an unexpanded completion "rule" to generate completions from for a command.
///
/// # Examples
///
/// The command 'gcc -o' requires that a file follows it, so the `requires_param` mode is suitable.
/// This can be done using the following line:
///
/// complete -c gcc -s o -r
///
/// The command 'grep -d' required that one of the strings 'read', 'skip' or 'recurse' is used. As
/// such, it is suitable to specify that a completion requires one of them. This can be done using
/// the following line:
///
/// complete -c grep -s d -x -a "read skip recurse"
///
/// - `cmd`: Command to complete.
/// - `cmd_is_path`: If `true`, cmd will be interpreted as the absolute
///   path of the program (optionally containing wildcards), otherwise it
///   will be interpreted as the command name.
/// - `option`: The name of an option.
/// - `option_type`: The type of option: can be option_type_short (-x),
///   option_type_single_long (-foo), option_type_double_long (--bar).
/// - `result_mode`: Controls how to search further completions when this completion has been
///   successfully matched.
/// - `comp`: A space separated list of completions which may contain subshells.
/// - `desc`: A description of the completion.
/// - `condition`: a command to be run to check it this completion should be used. If `condition`
///   is empty, the completion is always used.
/// - `flags`: A set of completion flags
#[allow(clippy::too_many_arguments)]
pub fn complete_add(
    cmd: WString,
    cmd_is_path: bool,
    option: WString,
    option_type: CompleteOptionType,
    result_mode: CompletionMode,
    condition: Vec<WString>,
    comp: WString,
    desc: WString,
    flags: CompleteFlags,
) {
    // option should be empty iff the option type is arguments only.
    assert!(option.is_empty() == (option_type == CompleteOptionType::ArgsOnly));

    // Lock the lock that allows us to edit the completion entry list.
    let mut completion_map = COMPLETION_MAP.lock().expect("mutex poisoned");
    let c = &mut completion_map
        .entry(CompletionEntryIndex {
            name: cmd,
            is_path: cmd_is_path,
        })
        .or_insert_with(CompletionEntry::new);

    // Create our new option.
    let opt = CompleteEntryOpt {
        option,
        typ: option_type,
        result_mode,
        comp,
        desc,
        conditions: condition,
        flags,
    };
    c.add_option(opt);
}

/// Remove a previously defined completion.
pub fn complete_remove(cmd: WString, cmd_is_path: bool, option: &wstr, typ: CompleteOptionType) {
    let mut completion_map = COMPLETION_MAP.lock().expect("mutex poisoned");
    let idx = CompletionEntryIndex {
        name: cmd,
        is_path: cmd_is_path,
    };
    if let Some(c) = completion_map.get_mut(&idx) {
        let delete_it = c.remove_option(option, typ);
        if delete_it {
            completion_map.remove(&idx);
        }
    }
}

/// Removes all completions for a given command.
pub fn complete_remove_all(cmd: WString, cmd_is_path: bool) {
    let mut completion_map = COMPLETION_MAP.lock().expect("mutex poisoned");
    completion_map.remove(&CompletionEntryIndex {
        name: cmd,
        is_path: cmd_is_path,
    });
}

/// Returns all completions of the command cmd.
/// If `ctx` contains a parser, this will autoload functions and completions as needed.
/// If it does not contain a parser, then any completions which need autoloading will be returned.
pub fn complete(
    cmd_with_subcmds: &wstr,
    flags: CompletionRequestOptions,
    ctx: &OperationContext,
) -> (Vec<Completion>, Vec<WString>) {
    // Determine the innermost subcommand.
    let cmdsubst = parse_util_cmdsubst_extent(cmd_with_subcmds, cmd_with_subcmds.len());
    let cmd = cmd_with_subcmds[cmdsubst].to_owned();
    let mut completer = Completer::new(ctx, flags);
    completer.perform_for_commandline(cmd);

    (
        completer.acquire_completions(),
        completer.acquire_needs_load(),
    )
}

/// Print the short switch `opt`, and the argument `arg` to the specified
/// [`WString`], but only if `argument` isn't an empty string.
fn append_switch_short_arg(out: &mut WString, opt: char, arg: &wstr) {
    if arg.is_empty() {
        return;
    }

    sprintf!(=> out, " -%lc %ls", opt, escape(arg));
}
fn append_switch_long_arg(out: &mut WString, opt: &wstr, arg: &wstr) {
    if arg.is_empty() {
        return;
    }

    sprintf!(=> out, " --%ls %ls", opt, escape(arg));
}
fn append_switch_short(out: &mut WString, opt: char) {
    sprintf!(=> out, " -%lc", opt);
}
fn append_switch_long(out: &mut WString, opt: &wstr) {
    sprintf!(=> out, " --%ls", opt);
}

fn completion2string(index: &CompletionEntryIndex, o: &CompleteEntryOpt) -> WString {
    let mut out = WString::from(L!("complete"));

    if o.flags.contains(CompleteFlags::DONT_SORT) {
        append_switch_short(&mut out, 'k');
    }

    if o.result_mode.no_files && o.result_mode.requires_param {
        append_switch_long(&mut out, L!("exclusive"));
    } else if o.result_mode.no_files {
        append_switch_long(&mut out, L!("no-files"));
    } else if o.result_mode.force_files {
        append_switch_long(&mut out, L!("force-files"));
    } else if o.result_mode.requires_param {
        append_switch_long(&mut out, L!("require-parameter"));
    }

    if index.is_path {
        append_switch_short_arg(&mut out, 'p', &index.name);
    } else {
        out.push(' ');
        out.push_utfstr(&escape(&index.name));
    }

    match o.typ {
        CompleteOptionType::ArgsOnly => {}
        CompleteOptionType::Short => append_switch_short_arg(&mut out, 's', &o.option[..1]),
        CompleteOptionType::SingleLong => append_switch_short_arg(&mut out, 'o', &o.option),
        CompleteOptionType::DoubleLong => append_switch_short_arg(&mut out, 'l', &o.option),
    }

    append_switch_short_arg(&mut out, 'd', o.localized_desc());
    append_switch_short_arg(&mut out, 'a', &o.comp);
    for c in &o.conditions {
        append_switch_short_arg(&mut out, 'n', c);
    }
    out.push('\n');

    out
}

/// Load command-specific completions for the specified command.
/// Returns `true` if something new was loaded, `false` if not.
pub fn complete_load(cmd: &wstr, parser: &Parser) -> bool {
    let mut loaded_new = false;

    // We have to load this as a function, since it may define a --wraps or signature.
    // See issue #2466.
    if function::load(cmd, parser) {
        // We autoloaded something; check if we have a --wraps.
        loaded_new |= !complete_get_wrap_targets(cmd).is_empty();
    }

    // It's important to NOT hold the lock around completion loading.
    // We need to take the lock to decide what to load, drop it to perform the load, then reacquire
    // it.
    // Note we only look at the global fish_function_path and fish_complete_path.
    let path_to_load = completion_autoloader
        .lock()
        .expect("mutex poisoned")
        .resolve_command(cmd, EnvStack::globals());
    if let Some(path_to_load) = path_to_load {
        Autoload::perform_autoload(&path_to_load, parser);
        completion_autoloader
            .lock()
            .expect("mutex poisoned")
            .mark_autoload_finished(cmd);
        loaded_new = true;
    }
    loaded_new
}

/// Return a list of all current completions.
/// Used by the bare `complete`, loaded completions are printed out as commands
pub fn complete_print(cmd: &wstr) -> WString {
    let mut out = WString::new();

    // Get references to our completions and sort them by order.
    let completions = COMPLETION_MAP.lock().expect("poisoned mutex");
    let mut completion_refs: Vec<_> = completions.iter().collect();
    completion_refs.sort_by_key(|(_, c)| c.order);

    for (key, entry) in completion_refs {
        if !cmd.is_empty() && key.name != cmd {
            continue;
        }

        // Output in reverse order to preserve legacy behavior (see #9221).
        for o in entry.get_options().iter().rev() {
            out.push_utfstr(&completion2string(key, o));
        }
    }

    // Append wraps.
    let wrappers = wrapper_map.lock().expect("poisoned mutex");
    for (src, targets) in wrappers.iter() {
        if !cmd.is_empty() && src != cmd {
            continue;
        }
        for target in targets {
            out.push_utfstr(L!("complete "));
            out.push_utfstr(&escape(src));
            append_switch_long_arg(&mut out, L!("wraps"), target);
            out.push_utfstr(L!("\n"));
        }
    }

    out
}

/// Observes that fish_complete_path has changed.
pub fn complete_invalidate_path() {
    // TODO: here we unload all completions for commands that are loaded by the autoloader. We also
    // unload any completions that the user may specified on the command line. We should in
    // principle track those completions loaded by the autoloader alone.

    let cmds = completion_autoloader
        .lock()
        .expect("mutex poisoned")
        .get_autoloaded_commands();
    for cmd in cmds {
        complete_remove_all(cmd, false /* not a path */);
    }
}

/// Adds a "wrap target." A wrap target is a command that completes like another command.
pub fn complete_add_wrapper(command: WString, new_target: WString) -> bool {
    if command.is_empty() || new_target.is_empty() {
        return false;
    }

    // If the command and the target are the same,
    // there's no point in following the wrap-chain because we'd only complete the same thing.
    // TODO: This should maybe include full cycle detection.
    if command == new_target {
        return false;
    }

    let mut wrappers = wrapper_map.lock().expect("poisoned mutex");
    let targets = wrappers.entry(command).or_default();
    // If it's already present, we do nothing.
    if !targets.contains(&new_target) {
        targets.push(new_target);
    }

    true
}

/// Removes a wrap target.
pub fn complete_remove_wrapper(command: WString, target_to_remove: &wstr) -> bool {
    if command.is_empty() || target_to_remove.is_empty() {
        return false;
    }

    let mut wrappers = wrapper_map.lock().expect("poisoned mutex");
    let mut result = false;
    for targets in wrappers.values_mut() {
        if let Some(pos) = targets.iter().position(|t| t == target_to_remove) {
            targets.remove(pos);
            result = true;
        }
    }

    result
}

/// Returns a list of wrap targets for a given command.
pub fn complete_get_wrap_targets(command: &wstr) -> Vec<WString> {
    if command.is_empty() {
        return vec![];
    }

    let wrappers = wrapper_map.lock().expect("poisoned mutex");
    wrappers.get(command).cloned().unwrap_or_default()
}

#[derive(Clone, Copy)]
pub struct CompletionRequestOptions {
    /// Requesting autosuggestion
    pub autosuggestion: bool,
    /// Make descriptions
    pub descriptions: bool,
    /// If set, we do not require a prefix match
    pub fuzzy_match: bool,
}

impl Default for CompletionRequestOptions {
    fn default() -> Self {
        Self {
            autosuggestion: false,
            descriptions: false,
            fuzzy_match: false,
        }
    }
}
