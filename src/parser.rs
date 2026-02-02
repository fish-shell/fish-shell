// The fish parser. Contains functions for parsing and evaluating code.

use crate::ast::{self, Node};
use crate::builtins::shared::STATUS_ILLEGAL_CMD;
use crate::common::{
    CancelChecker, EscapeFlags, EscapeStringStyle, FilenameRef, PROFILING_ACTIVE, ScopeGuarding,
    ScopedCell, ScopedRefCell, escape_string, wcs2bytes,
};
use crate::complete::CompletionList;
use crate::env::{
    EnvMode, EnvSetMode, EnvStack, EnvStackSetResult, Environment, FISH_TERMINAL_COLOR_THEME_VAR,
    Statuses,
};
use crate::event::{self, Event};
use crate::expand::{
    ExpandFlags, ExpandResultCode, expand_string, replace_home_directory_with_tilde,
};
use crate::fds::{BEST_O_SEARCH, open_dir};
use crate::global_safety::RelaxedAtomicBool;
use crate::input_common::TerminalQuery;
use crate::io::IoChain;
use crate::job_group::MaybeJobId;
use crate::operation_context::{EXPANSION_LIMIT_DEFAULT, OperationContext};
use crate::parse_constants::{
    FISH_MAX_EVAL_DEPTH, FISH_MAX_STACK_DEPTH, ParseError, ParseErrorList, ParseTreeFlags,
    SOURCE_LOCATION_UNKNOWN,
};
use crate::parse_execution::{EndExecutionReason, ExecutionContext};
use crate::parse_tree::{NodeRef, ParsedSourceRef, SourceLineCache, parse_source};
use crate::portable_atomic::AtomicU64;
use crate::prelude::*;
use crate::proc::{JobGroupRef, JobList, JobRef, Pid, ProcStatus, job_reap};
use crate::signal::{Signal, signal_check_cancel, signal_clear_cancel};
use crate::wait_handle::WaitHandleStore;
use crate::wutil::perror;
use crate::{flog, flogf, function};
use assert_matches::assert_matches;
use fish_util::get_time;
use fish_widestring::WExt;
use libc::c_int;
use std::cell::{Ref, RefCell, RefMut};
use std::ffi::OsStr;
use std::fs::File;
use std::io::Write;
use std::num::NonZeroU32;
use std::os::fd::OwnedFd;
use std::rc::Rc;
use std::sync::Arc;
use std::time::Duration;

pub enum BlockData {
    Function {
        /// Name of the function
        name: WString,
        /// Arguments passed to the function
        args: Vec<WString>,
    },
    Event(Rc<Event>),
    Source {
        /// The sourced file
        file: Arc<WString>,
    },
}

/// block_t represents a block of commands.
#[derive(Default)]
pub struct Block {
    /// Type of block.
    block_type: BlockType,

    /// [`BlockType`]-specific data.
    ///
    /// None of these data fields are accessed on a regular basis (only for shell introspection), so
    /// we store them in a `Box` to reduce the size of the `Block` itself.
    pub data: Option<Box<BlockData>>,

    /// Pseudo-counter of event blocks
    pub event_blocks: bool,

    /// Name of the file that created this block
    pub src_filename: Option<Arc<WString>>,

    /// The node containing this block, for lazy line number computation.
    src_node: Option<NodeRef<ast::JobPipeline>>,
}

impl Block {
    #[inline(always)]
    pub fn data(&self) -> Option<&BlockData> {
        self.data.as_deref()
    }

    #[inline(always)]
    pub fn wants_pop_env(&self) -> bool {
        self.typ() != BlockType::top
    }

    /// Return the 1-based line number of this block, using a cache.
    pub fn src_lineno(&self, cache: &mut SourceLineCache) -> Option<NonZeroU32> {
        self.src_node.as_ref()?.lineno_with_cache(cache)
    }
}

impl Block {
    /// Construct from a block type.
    pub fn new(block_type: BlockType) -> Self {
        Self {
            block_type,
            ..Default::default()
        }
    }

    /// Description of the block, for debugging.
    pub fn description(&self) -> WString {
        let mut result = match self.typ() {
            BlockType::while_block => L!("while"),
            BlockType::for_block => L!("for"),
            BlockType::if_block => L!("if"),
            BlockType::function_call { .. } => L!("function_call"),
            BlockType::switch_block => L!("switch"),
            BlockType::subst => L!("substitution"),
            BlockType::top => L!("top"),
            BlockType::begin => L!("begin"),
            BlockType::source => L!("source"),
            BlockType::event => L!("event"),
            BlockType::breakpoint => L!("breakpoint"),
            BlockType::variable_assignment => L!("variable_assignment"),
        }
        .to_owned();

        if let Some(src_lineno) = self.src_lineno(&mut SourceLineCache::default()) {
            result.push_utfstr(&sprintf!(" (line %d)", src_lineno.get()));
        }
        if let Some(src_filename) = &self.src_filename {
            result.push_utfstr(&sprintf!(" (file %s)", src_filename));
        }
        result
    }

    pub fn typ(&self) -> BlockType {
        self.block_type
    }

    /// Return if we are a function call (with or without shadowing).
    pub fn is_function_call(&self) -> bool {
        matches!(self.typ(), BlockType::function_call { .. })
    }

    /// Entry points for creating blocks.
    pub fn if_block() -> Block {
        Block::new(BlockType::if_block)
    }
    pub fn event_block(event: Event) -> Block {
        let mut b = Block::new(BlockType::event);
        b.data = Some(Box::new(BlockData::Event(Rc::new(event))));
        b
    }
    pub fn function_block(name: WString, args: Vec<WString>, shadows: bool) -> Block {
        let mut b = Block::new(BlockType::function_call { shadows });
        b.data = Some(Box::new(BlockData::Function { name, args }));
        b
    }
    pub fn source_block(src: FilenameRef) -> Block {
        let mut b = Block::new(BlockType::source);
        b.data = Some(Box::new(BlockData::Source { file: src }));
        b
    }
    pub fn for_block() -> Block {
        Block::new(BlockType::for_block)
    }
    pub fn while_block() -> Block {
        Block::new(BlockType::while_block)
    }
    pub fn switch_block() -> Block {
        Block::new(BlockType::switch_block)
    }
    pub fn scope_block(typ: BlockType) -> Block {
        assert!(
            [BlockType::begin, BlockType::top, BlockType::subst].contains(&typ),
            "Invalid scope type"
        );
        Block::new(typ)
    }
    pub fn breakpoint_block() -> Block {
        Block::new(BlockType::breakpoint)
    }
    pub fn variable_assignment_block() -> Block {
        Block::new(BlockType::variable_assignment)
    }
}

type Microseconds = i64;

#[derive(Default)]
pub struct ProfileItem {
    /// Time spent executing the command, including nested blocks.
    pub duration: Microseconds,

    /// The block level of the specified command. Nested blocks and command substitutions both
    /// increase the block level.
    pub level: isize,

    /// If the execution of this command was skipped.
    pub skipped: bool,

    /// The command string.
    pub cmd: WString,
}

impl ProfileItem {
    pub fn new() -> Self {
        Default::default()
    }
    /// Return the current time as a microsecond timestamp since the epoch.
    pub fn now() -> Microseconds {
        get_time()
    }
}

/// Data which is managed in a scoped fashion: is generally set for the duration of a block
/// of code. Note this is stored in a Cell and so must be Copy.
#[derive(Copy, Clone)]
pub struct ScopedData {
    /// The 'depth' of the fish call stack.
    /// -1 means nothing is executing. 0 means we are running a top-level command.
    /// Larger values indicate deeper nesting.
    pub eval_level: isize,

    /// Whether we are running a subshell command.
    pub is_subshell: bool,

    /// Whether we are running an event handler.
    pub is_event: bool,

    /// Whether we are currently interactive.
    pub is_interactive: bool,

    /// Whether the command line is closed for modification from fish script.
    pub readonly_commandline: bool,

    /// Whether to suppress fish_trace output. This occurs in the prompt, event handlers, and key
    /// bindings.
    pub suppress_fish_trace: bool,

    /// The read limit to apply to captured subshell output, or 0 for none.
    pub read_limit: usize,

    /// Whether we are currently cleaning processes.
    pub is_cleaning_procs: bool,

    /// The internal job ID of the job being populated, or 0 if none.
    /// This supports the '--on-job-exit caller' feature.
    pub caller_id: u64, // TODO should be InternalJobId
}

impl Default for ScopedData {
    fn default() -> Self {
        Self {
            eval_level: -1,
            is_subshell: false,
            is_event: false,
            readonly_commandline: false,
            is_interactive: false,
            suppress_fish_trace: false,
            read_limit: 0,
            is_cleaning_procs: false,
            caller_id: 0,
        }
    }
}

/// Miscellaneous data used to avoid recursion and others.
#[derive(Default)]
pub struct LibraryData {
    /// The current filename we are evaluating, either from builtin source or on the command line.
    pub current_filename: Option<FilenameRef>,

    /// A fake value to be returned by builtin_commandline. This is used by the completion
    /// machinery when wrapping: e.g. if `tig` wraps `git` then git completions need to see git on
    /// the command line.
    pub transient_commandline: Option<WString>,

    /// A file descriptor holding the current working directory, for use in openat().
    /// This is never null and never invalid.
    pub cwd_fd: Option<Arc<OwnedFd>>,

    /// Variables supporting the "status" builtin.
    pub status_vars: StatusVars,

    /// A counter incremented every time a command executes.
    pub exec_count: u64,

    /// A counter incremented every time a command produces a $status.
    pub status_count: u64,

    /// Last reader run count.
    pub last_exec_run_counter: u64,

    /// Number of recursive calls to the internal completion function.
    pub complete_recursion_level: u32,

    /// If set, we are currently within fish's initialization routines.
    pub within_fish_init: bool,

    /// If we're currently repainting the commandline.
    /// Useful to stop infinite loops.
    pub is_repaint: bool,

    /// Whether we called builtin_complete -C without parameter.
    pub builtin_complete_current_commandline: bool,

    /// Whether we should break or continue the current loop.
    /// This is set by the 'break' and 'continue' commands.
    pub loop_status: LoopStatus,

    /// Whether we should return from the current function.
    /// This is set by the 'return' command.
    pub returning: bool,

    /// Whether we should stop executing.
    /// This is set by the 'exit' command, and unset after 'reader_read'.
    /// Note this only exits up to the "current script boundary." That is, a call to exit within a
    /// 'source' or 'read' command will only exit up to that command.
    pub exit_current_script: bool,
}

impl LibraryData {
    pub fn new() -> Self {
        Self {
            last_exec_run_counter: u64::MAX,
            ..Default::default()
        }
    }
}

/// Status variables set by the main thread as jobs are parsed and read by various consumers.
#[derive(Default)]
pub struct StatusVars {
    /// Used to get the head of the current job (not the current command, at least for now)
    /// for `status current-command`.
    pub command: WString,
    /// Used to get the full text of the current job for `status current-commandline`.
    pub commandline: WString,
}

/// The result of Parser::eval family.
#[derive(Default)]
pub struct EvalRes {
    /// The value for $status.
    pub status: ProcStatus,

    /// If set, there was an error that should be considered a failed expansion, such as
    /// command-not-found. For example, `touch (not-a-command)` will not invoke 'touch' because
    /// command-not-found will mark break_expand.
    pub break_expand: bool,

    /// If set, no commands were executed and there we no errors.
    pub was_empty: bool,

    /// If set, no commands produced a $status value.
    pub no_status: bool,
}

impl EvalRes {
    pub fn new(status: ProcStatus) -> Self {
        Self {
            status,
            ..Default::default()
        }
    }
}

pub enum ParserStatusVar {
    current_command,
    current_commandline,
    count_,
}

/// A newtype for the block index.
/// This is the naive position in the block list.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct BlockId(usize);

/// Controls the behavior when fish itself receives a signal and there are
/// no blocks on the stack.
/// The "outermost" parser is responsible for clearing the signal.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub enum CancelBehavior {
    #[default]
    /// Return the signal to the caller
    Return,
    /// Clear the signal
    Clear,
}

pub struct Parser {
    pub interactive_initialized: RelaxedAtomicBool,

    /// The currently executing Node.
    current_node: ScopedRefCell<Option<NodeRef<ast::JobPipeline>>>,

    /// The jobs associated with this parser.
    job_list: RefCell<JobList>,

    /// Our store of recorded wait-handles. These are jobs that finished in the background,
    /// and have been reaped, but may still be wait'ed on.
    wait_handles: RefCell<WaitHandleStore>,

    /// The list of blocks.
    /// This is a stack; the topmost block is at the end. This is to avoid invalidating block
    /// indexes during recursive evaluation.
    block_list: RefCell<Vec<Block>>,

    /// Set of variables for the parser.
    pub variables: EnvStack,

    /// Data managed in a scoped fashion.
    scoped_data: ScopedCell<ScopedData>,

    /// Miscellaneous library data.
    pub library_data: ScopedRefCell<LibraryData>,

    /// If set, we synchronize universal variables after external commands,
    /// including sending on-variable change events.
    syncs_uvars: RelaxedAtomicBool,

    /// The behavior when fish itself receives a signal and there are no blocks on the stack.
    cancel_behavior: CancelBehavior,

    /// List of profile items.
    profile_items: RefCell<Vec<ProfileItem>>,

    /// Global event blocks.
    pub global_event_blocks: AtomicU64,

    pub blocking_query: RefCell<Option<TerminalQuery>>,

    // Timeout for blocking terminal queries.
    pub blocking_query_timeout: RefCell<Option<Duration>>,
}

#[derive(Copy, Clone, Default)]
pub struct ParserEnvSetMode {
    pub mode: EnvMode,
    pub user: bool,
}

impl ParserEnvSetMode {
    pub fn new(mode: EnvMode) -> Self {
        Self { mode, user: false }
    }
    pub fn user(mode: EnvMode) -> Self {
        Self { mode, user: true }
    }
}

impl Parser {
    /// Create a parser.
    pub fn new(variables: EnvStack, cancel_behavior: CancelBehavior) -> Parser {
        let result = Self {
            interactive_initialized: RelaxedAtomicBool::new(false),
            current_node: ScopedRefCell::new(None),
            job_list: RefCell::default(),
            wait_handles: RefCell::default(),
            block_list: RefCell::default(),
            variables,
            scoped_data: ScopedCell::new(ScopedData::default()),
            library_data: ScopedRefCell::new(LibraryData::new()),
            syncs_uvars: RelaxedAtomicBool::new(false),
            cancel_behavior,
            profile_items: RefCell::default(),
            global_event_blocks: AtomicU64::new(0),
            blocking_query: RefCell::new(None),
            blocking_query_timeout: RefCell::new(None),
        };

        match open_dir(c".", BEST_O_SEARCH) {
            Ok(fd) => {
                result.libdata_mut().cwd_fd = Some(Arc::new(fd));
            }
            Err(_) => {
                perror("Unable to open the current working directory");
            }
        }

        result
    }

    /// Adds a job to the beginning of the job list.
    pub fn job_add(&self, job: JobRef) {
        assert!(!job.processes().is_empty());
        self.jobs_mut().insert(0, job);
    }

    /// Return whether we are currently evaluating a function.
    pub fn is_function(&self) -> bool {
        self.blocks_iter_rev()
            // If a function sources a file, don't descend further.
            .take_while(|b| b.typ() != BlockType::source)
            .any(|b| b.is_function_call())
    }

    /// Return whether we are currently evaluating a command substitution.
    pub fn is_command_substitution(&self) -> bool {
        self.blocks_iter_rev()
            // If a function sources a file, don't descend further.
            .take_while(|b| b.typ() != BlockType::source)
            .any(|b| b.typ() == BlockType::subst)
    }

    pub fn eval(&self, cmd: &wstr, io: &IoChain) -> EvalRes {
        self.eval_with(cmd, io, None, BlockType::top, false)
    }

    /// Evaluate the expressions contained in cmd.
    ///
    /// \param cmd the string to evaluate
    /// \param io io redirections to perform on all started jobs
    /// \param job_group if set, the job group to give to spawned jobs.
    /// \param block_type The type of block to push on the block stack, which must be either 'top'
    /// or 'subst'.
    /// Return the result of evaluation.
    pub fn eval_with(
        &self,
        cmd: &wstr,
        io: &IoChain,
        job_group: Option<&JobGroupRef>,
        block_type: BlockType,
        test_only_suppress_stderr: bool,
    ) -> EvalRes {
        // Parse the source into a tree, if we can.
        let mut error_list = ParseErrorList::new();
        if let Some(ps) = parse_source(
            cmd.to_owned(),
            ParseTreeFlags::default(),
            Some(&mut error_list),
        ) {
            return self.eval_parsed_source(
                &ps,
                io,
                job_group,
                block_type,
                test_only_suppress_stderr,
            );
        }

        // Get a backtrace. This includes the message.
        let backtrace_and_desc = self.get_backtrace(cmd, &error_list);

        if !test_only_suppress_stderr {
            // Print it.
            eprintf!("%s\n", backtrace_and_desc);
        }

        // Set a valid status.
        self.set_last_statuses(Statuses::just(STATUS_ILLEGAL_CMD));
        let break_expand = true;
        EvalRes {
            status: ProcStatus::from_exit_code(STATUS_ILLEGAL_CMD),
            break_expand,
            ..Default::default()
        }
    }

    /// Evaluate the parsed source ps.
    /// Because the source has been parsed, a syntax error is impossible.
    pub fn eval_parsed_source(
        &self,
        ps: &ParsedSourceRef,
        io: &IoChain,
        job_group: Option<&JobGroupRef>,
        block_type: BlockType,
        test_only_suppress_stderr: bool,
    ) -> EvalRes {
        assert_matches!(block_type, BlockType::top | BlockType::subst);
        let job_list = ps.top_job_list();
        if !job_list.is_empty() {
            // Execute the top job list.
            self.eval_node(
                &job_list,
                io,
                job_group,
                block_type,
                test_only_suppress_stderr,
            )
        } else {
            let status = ProcStatus::from_exit_code(self.get_last_status());
            EvalRes {
                status,
                break_expand: false,
                was_empty: true,
                no_status: true,
            }
        }
    }

    pub fn eval_wstr(
        &self,
        src: WString,
        io: &IoChain,
        job_group: Option<&JobGroupRef>,
        block_type: BlockType,
    ) -> Result<EvalRes, WString> {
        use crate::parse_tree::ParsedSource;
        use crate::parse_util::detect_parse_errors_in_ast;
        let mut errors = vec![];
        let ast = ast::parse(&src, ParseTreeFlags::default(), Some(&mut errors));
        let mut errored = ast.errored();
        if !errored {
            errored = detect_parse_errors_in_ast(&ast, &src, Some(&mut errors)).is_err();
        }
        if errored {
            let sb = self.get_backtrace(&src, &errors);
            return Err(sb);
        }

        // Construct a parsed source ref.
        // Be careful to transfer ownership, this could be a very large string.
        let ps = Arc::new(ParsedSource::new(src, ast));
        Ok(self.eval_parsed_source(&ps, io, job_group, block_type, false))
    }

    pub fn eval_file_wstr(
        &self,
        src: WString,
        filename: Arc<WString>,
        io: &IoChain,
        job_group: Option<&JobGroupRef>,
    ) -> Result<EvalRes, WString> {
        let _interactive_push = self.push_scope(|s| s.is_interactive = false);
        let sb = self.push_block(Block::source_block(filename.clone()));
        let _filename_push = self
            .library_data
            .scoped_set(Some(filename), |s| &mut s.current_filename);

        let ret = self.eval_wstr(src, io, job_group, BlockType::top);

        self.pop_block(sb);
        self.libdata_mut().exit_current_script = false;
        ret
    }

    /// Evaluates a node.
    /// The node type must be ast::Statement or ast::JobList.
    pub fn eval_node<T: Node>(
        &self,
        node: &NodeRef<T>,
        block_io: &IoChain,
        job_group: Option<&JobGroupRef>,
        block_type: BlockType,
        test_only_suppress_stderr: bool,
    ) -> EvalRes {
        // Only certain blocks are allowed.
        assert_matches!(
            block_type,
            BlockType::top | BlockType::subst,
            "Invalid block type"
        );

        // If fish itself got a cancel signal, then we want to unwind back to the parser which
        // has a Clear cancellation behavior.
        // Note this only happens in interactive sessions. In non-interactive sessions, SIGINT will
        // cause fish to exit.
        let sig = signal_check_cancel();
        if sig != 0 {
            if self.cancel_behavior == CancelBehavior::Clear && self.block_list.borrow().is_empty()
            {
                signal_clear_cancel();
            } else {
                return EvalRes::new(ProcStatus::from_signal(Signal::new(sig)));
            }
        }

        // A helper to detect if we got a signal.
        // This includes both signals sent to fish (user hit control-C while fish is foreground) and
        // signals from the job group (e.g. some external job terminated with SIGQUIT).
        let jg = job_group.cloned();
        let check_cancel_signal = move || {
            // Did fish itself get a signal?
            let sig = signal_check_cancel();
            if sig != 0 {
                return Some(Signal::new(sig));
            }
            // Has this job group been cancelled?
            jg.as_ref().and_then(|jg| jg.get_cancel_signal())
        };

        // If we have a job group which is cancelled, then do nothing.
        if let Some(sig) = check_cancel_signal() {
            return EvalRes::new(ProcStatus::from_signal(sig));
        }

        job_reap(self, false, Some(block_io)); // not sure why we reap jobs here

        // Start it up
        let mut op_ctx = self.context();
        let scope_block = self.push_block(Block::scope_block(block_type));

        // Propagate our job group.
        op_ctx.job_group = job_group.cloned();

        // Replace the context's cancel checker with one that checks the job group's signal.
        let cancel_checker: CancelChecker = Box::new(move || check_cancel_signal().is_some());
        op_ctx.cancel_checker = cancel_checker;

        // Restore the current pipeline node.
        let restore_current_node = self.current_node.scoped_replace(None);

        // Create a new execution context.
        let mut execution_context = ExecutionContext::new(
            node.parsed_source_ref(),
            block_io.clone(),
            &self.current_node,
            test_only_suppress_stderr,
        );

        // Check the exec count so we know if anything got executed.
        let prev_exec_count = self.libdata().exec_count;
        let prev_status_count = self.libdata().status_count;
        let reason = execution_context.eval_node(&op_ctx, &**node, Some(scope_block));
        let new_exec_count = self.libdata().exec_count;
        let new_status_count = self.libdata().status_count;

        ScopeGuarding::commit(restore_current_node);
        self.pop_block(scope_block);

        job_reap(self, false, Some(block_io)); // reap again

        let sig = signal_check_cancel();
        if sig != 0 {
            EvalRes::new(ProcStatus::from_signal(Signal::new(sig)))
        } else {
            let status = ProcStatus::from_exit_code(self.get_last_status());
            let break_expand = reason == EndExecutionReason::Error;
            EvalRes {
                status,
                break_expand,
                was_empty: !break_expand && prev_exec_count == new_exec_count,
                no_status: prev_status_count == new_status_count,
            }
        }
    }

    /// Evaluate line as a list of parameters, i.e. tokenize it and perform parameter expansion and
    /// cmdsubst execution on the tokens. Errors are ignored. If a parser is provided, it is used
    /// for command substitution expansion.
    pub fn expand_argument_list(
        arg_list_src: &wstr,
        flags: ExpandFlags,
        ctx: &OperationContext<'_>,
    ) -> CompletionList {
        // Parse the string as an argument list.
        let ast = ast::parse_argument_list(arg_list_src, ParseTreeFlags::default(), None);
        if ast.errored() {
            // Failed to parse. Here we expect to have reported any errors in test_args.
            return vec![];
        }

        // Get the root argument list and extract arguments from it.
        let mut result = vec![];
        for arg in &ast.top().arguments {
            let arg_src = arg.source(arg_list_src);
            if matches!(
                expand_string(arg_src.to_owned(), &mut result, flags, ctx, None).result,
                ExpandResultCode::error | ExpandResultCode::overflow
            ) {
                break; // failed to expand a string
            }
        }
        result
    }

    /// Returns a string describing the current parser position in the format 'FILENAME (line
    /// LINE_NUMBER): LINE'. Example:
    ///
    /// init.fish (line 127): ls|grep pancake
    pub fn current_line(&self) -> WString {
        let Some(node_ref) = self.current_node.borrow().clone() else {
            return WString::new();
        };
        let Some(source_offset) = node_ref.source_offset() else {
            return WString::new();
        };

        let lineno = self.get_lineno_for_display();
        let file = self.current_filename();

        let mut prefix = WString::new();

        // If we are not going to print a stack trace, at least print the line number and filename.
        if !self.is_interactive() || self.is_function() {
            if let Some(file) = file {
                prefix.push_utfstr(&wgettext_fmt!(
                    "%s (line %d):",
                    &user_presentable_path(&file, self.vars()),
                    lineno
                ));
            } else if self.libdata().within_fish_init {
                prefix.push_utfstr(&wgettext_fmt!("Startup (line %d):", lineno));
            } else {
                prefix.push_utfstr(&wgettext_fmt!("Standard input (line %d):", lineno));
            }
            prefix.push(' ');
        }

        let skip_caret = self.is_interactive() && !self.is_function();

        // Use an error with empty text.
        let empty_error = ParseError {
            source_start: source_offset,
            ..Default::default()
        };

        let mut line_info = empty_error.describe_with_prefix(
            node_ref.source_str(),
            &prefix,
            self.is_interactive(),
            skip_caret,
        );
        if !line_info.is_empty() {
            line_info.push('\n');
        }

        line_info.push_utfstr(&self.stack_trace());
        line_info
    }

    /// Returns the current line number, indexed from 1.
    pub fn get_lineno(&self) -> Option<NonZeroU32> {
        self.current_node.borrow().as_ref().and_then(|n| n.lineno())
    }

    /// Returns the current line number, indexed from 1, or zero if not sourced.
    pub fn get_lineno_for_display(&self) -> u32 {
        self.get_lineno().map_or(0, |n| n.get())
    }

    /// Returns a NodeRef to the current node being executed, if any.
    /// This can be used for lazy line number computation.
    pub fn current_node_ref(&self) -> Option<NodeRef<ast::JobPipeline>> {
        self.current_node.borrow().clone()
    }

    /// Return whether we are currently evaluating a "block" such as an if statement.
    /// This supports 'status is-block'.
    pub fn is_block(&self) -> bool {
        // Note historically this has descended into 'source', unlike 'is_function'.
        self.blocks_iter_rev().any(|b| {
            ![
                BlockType::top,
                BlockType::subst,
                BlockType::variable_assignment,
            ]
            .contains(&b.typ())
        })
    }

    /// Return whether we have a breakpoint block.
    pub fn is_breakpoint(&self) -> bool {
        self.blocks_iter_rev()
            .any(|b| b.typ() == BlockType::breakpoint)
    }

    // Return an iterator over the blocks, in reverse order.
    // That is, the first block is the innermost block.
    pub fn blocks_iter_rev<'a>(&'a self) -> impl Iterator<Item = Ref<'a, Block>> {
        let blocks = self.block_list.borrow();
        let mut indices = (0..blocks.len()).rev();
        std::iter::from_fn(move || {
            let last = indices.next()?;
            // note this clone is cheap
            Some(Ref::map(Ref::clone(&blocks), |bl| &bl[last]))
        })
    }

    // Return the block at a given index, where 0 is the innermost block.
    pub fn block_at_index(&self, index: usize) -> Option<Ref<'_, Block>> {
        let block_list = self.block_list.borrow();
        let block_count = block_list.len();
        if index >= block_count {
            None
        } else {
            Some(Ref::map(block_list, |bl| &bl[block_count - 1 - index]))
        }
    }

    // Return the block at a given index, where 0 is the innermost block.
    pub fn block_at_index_mut(&self, index: usize) -> Option<RefMut<'_, Block>> {
        let block_list = self.block_list.borrow_mut();
        let block_count = block_list.len();
        if index >= block_count {
            None
        } else {
            Some(RefMut::map(block_list, |bl| {
                &mut bl[block_count - 1 - index]
            }))
        }
    }

    // Return the block with the given id, asserting it exists. Note ids are recycled.
    pub fn block_with_id(&self, id: BlockId) -> Ref<'_, Block> {
        Ref::map(self.block_list.borrow(), |bl| &bl[id.0])
    }

    pub fn blocks_size(&self) -> usize {
        self.block_list.borrow().len()
    }

    /// Get the list of jobs.
    pub fn jobs(&self) -> Ref<'_, JobList> {
        self.job_list.borrow()
    }
    pub fn jobs_mut(&self) -> RefMut<'_, JobList> {
        self.job_list.borrow_mut()
    }

    /// Get the variables.
    pub fn vars(&self) -> &EnvStack {
        &self.variables
    }

    /// Get a copy of the scoped data.
    #[inline(always)]
    pub fn scope(&self) -> ScopedData {
        self.scoped_data.get()
    }

    /// Modify the scoped values for the duration of the caller's scope (or whenever the ParserScope is dropped).
    /// This accepts a closure which modifies the ScopedData, and returns a ParserScope which restores the
    /// data when dropped.
    pub fn push_scope<'a, F: FnOnce(&mut ScopedData)>(
        &'a self,
        modifier: F,
    ) -> impl ScopeGuarding + 'a {
        self.scoped_data.scoped_mod(modifier)
    }

    /// Get the library data.
    pub fn libdata(&self) -> Ref<'_, LibraryData> {
        self.library_data.borrow()
    }

    /// Get the library data, mutably.
    pub fn libdata_mut(&self) -> RefMut<'_, LibraryData> {
        self.library_data.borrow_mut()
    }

    /// Get our wait handle store.
    pub fn get_wait_handles(&self) -> Ref<'_, WaitHandleStore> {
        self.wait_handles.borrow()
    }
    pub fn mut_wait_handles(&self) -> RefMut<'_, WaitHandleStore> {
        self.wait_handles.borrow_mut()
    }

    /// Get and set the last proc statuses.
    pub fn get_last_status(&self) -> c_int {
        self.vars().get_last_status()
    }
    pub fn get_last_statuses(&self) -> Statuses {
        self.vars().get_last_statuses()
    }
    pub fn set_last_statuses(&self, s: Statuses) {
        self.vars().set_last_statuses(s);
    }

    /// Cover of vars().set(), which also fires any returned event handlers.
    pub fn set_var_and_fire(
        &self,
        key: &wstr,
        mode: ParserEnvSetMode,
        vals: Vec<WString>,
    ) -> EnvStackSetResult {
        let res = self.set_var(key, mode, vals);
        if res == EnvStackSetResult::Ok {
            event::fire(self, Event::variable_set(key.to_owned()));
        }
        res
    }

    pub fn is_repainting(&self) -> bool {
        self.libdata().is_repaint
    }

    pub fn convert_env_set_mode(&self, mode: ParserEnvSetMode) -> EnvSetMode {
        EnvSetMode::new_with(mode.mode, mode.user, self.is_repainting())
    }

    /// Cover of vars().set(), without firing events
    pub fn set_var(
        &self,
        key: &wstr,
        mode: ParserEnvSetMode,
        vals: Vec<WString>,
    ) -> EnvStackSetResult {
        let mode = self.convert_env_set_mode(mode);
        self.vars().set(key, mode, vals)
    }

    /// Cover of vars().set_one(), without firing events
    pub fn set_one(&self, key: &wstr, mode: ParserEnvSetMode, val: WString) -> EnvStackSetResult {
        let mode = self.convert_env_set_mode(mode);
        self.vars().set_one(key, mode, val)
    }

    /// Cover of vars().set_empty(), without firing events
    pub fn set_empty(&self, key: &wstr, mode: ParserEnvSetMode) -> EnvStackSetResult {
        let mode = self.convert_env_set_mode(mode);
        self.vars().set_empty(key, mode)
    }

    /// Cover of vars().remove(), without firing events
    pub fn remove_var(&self, key: &wstr, mode: ParserEnvSetMode) -> EnvStackSetResult {
        let mode = self.convert_env_set_mode(mode);
        self.vars().remove(key, mode)
    }

    /// Update any universal variables and send event handlers.
    /// If `always` is set, then do it even if we have no pending changes (that is, look for
    /// changes from other fish instances); otherwise only sync if this instance has changed uvars.
    pub fn sync_uvars_and_fire(&self, always: bool) {
        if self.syncs_uvars.load() {
            let evts = self.vars().universal_sync(always, self.is_repainting());
            for evt in evts {
                event::fire(self, evt);
            }
        }
    }

    /// Pushes a new block. Returns an id (index) of the block, which is stored in the parser.
    pub fn push_block(&self, mut block: Block) -> BlockId {
        block.src_filename = self.current_filename();
        block.src_node.clone_from(&self.current_node.borrow());
        if block.typ() != BlockType::top {
            let new_scope = block.typ() == BlockType::function_call { shadows: true };
            self.vars().push(new_scope);
        }

        let mut block_list = self.block_list.borrow_mut();
        block_list.push(block);
        BlockId(block_list.len() - 1)
    }

    /// Remove the outermost block, asserting it's the given one.
    pub fn pop_block(&self, expected: BlockId) {
        let block = {
            let mut block_list = self.block_list.borrow_mut();
            assert_eq!(expected.0, block_list.len() - 1);
            block_list.pop().unwrap()
        };
        if block.wants_pop_env() {
            self.vars().pop(self.is_repainting());
        }
    }

    /// Return the function name for the specified stack frame. Default is one (current frame).
    pub fn get_function_name(&self, level: i32) -> Option<WString> {
        if level == 0 {
            // Return the function name for the level preceding the most recent breakpoint. If there
            // isn't one return the function name for the current level.
            // Walk until we find a breakpoint, then take the next function.
            return self
                .blocks_iter_rev()
                .skip_while(|b| b.typ() != BlockType::breakpoint)
                .find_map(|b| match b.data() {
                    Some(BlockData::Function { name, .. }) => Some(name.clone()),
                    _ => None,
                });
        }

        self.blocks_iter_rev()
            // Historical: If we want the topmost function, but we are really in a file sourced by a
            // function, don't consider ourselves to be in a function.
            .take_while(|b| !(level == 1 && b.typ() == BlockType::source))
            .map(|b| (b, 0))
            .map(|(b, level)| {
                if b.is_function_call() {
                    (b, level + 1)
                } else {
                    (b, level)
                }
            })
            .skip_while(|(_, l)| *l != level)
            .inspect(|(b, _)| debug_assert!(b.is_function_call()))
            .map(|(b, _)| {
                let Some(BlockData::Function { name, .. }) = b.data() else {
                    unreachable!()
                };
                name.clone()
            })
            .next()
    }

    /// Promotes a job to the front of the list.
    pub fn job_promote_at(&self, job_pos: usize) {
        // Move the job to the beginning.
        self.jobs_mut().rotate_left(job_pos);
    }

    /// Return the job with the specified job ID. If id is 0 or less, return the last job used.
    pub fn job_with_id(&self, job_id: MaybeJobId) -> Option<JobRef> {
        for job in self.jobs().iter() {
            if job_id.is_none() || job_id == job.job_id() {
                return Some(job.clone());
            }
        }
        None
    }

    /// Returns the job with the given pid.
    pub fn job_get_from_pid(&self, pid: Pid) -> Option<JobRef> {
        self.job_get_with_index_from_pid(pid).map(|t| t.1)
    }

    /// Returns the job and job index with the given pid.
    /// This assumes that all external jobs have a pid.
    pub fn job_get_with_index_from_pid(&self, pid: Pid) -> Option<(usize, JobRef)> {
        for (i, job) in self.jobs().iter().enumerate() {
            for p in job.external_procs() {
                if p.pid().unwrap() == pid {
                    return Some((i, job.clone()));
                }
            }
        }
        None
    }

    /// Returns a new profile item if profiling is active. The caller should fill it in.
    /// The Parser will deallocate it.
    /// If profiling is not active, this returns None.
    pub fn create_profile_item(&self) -> Option<usize> {
        if PROFILING_ACTIVE.load() {
            let mut profile_items = self.profile_items.borrow_mut();
            profile_items.push(ProfileItem::new());
            return Some(profile_items.len() - 1);
        }
        None
    }

    pub fn profile_items_mut(&self) -> RefMut<'_, Vec<ProfileItem>> {
        self.profile_items.borrow_mut()
    }

    /// Remove the profiling items.
    pub fn clear_profiling(&self) {
        self.profile_items.borrow_mut().clear();
    }

    /// Output profiling data to the given filename.
    pub fn emit_profiling(&self, path: &OsStr) {
        // Save profiling information. OK to not use CLO_EXEC here because this is called while fish is
        // exiting (and hence will not fork).
        let mut f = match std::fs::File::create(path) {
            Ok(f) => f,
            Err(err) => {
                flog!(
                    warning,
                    wgettext_fmt!(
                        "Could not write profiling information to file '%s': %s",
                        path.to_string_lossy(),
                        err.to_string()
                    )
                );
                return;
            }
        };
        print_profile(&self.profile_items.borrow(), &mut f);
    }

    pub fn get_backtrace(&self, src: &wstr, errors: &ParseErrorList) -> WString {
        let Some(err) = errors.first() else {
            return WString::new();
        };

        // Determine if we want to try to print a caret to point at the source error. The
        // err.source_start() <= src.size() check is due to the nasty way that slices work, which is
        // by rewriting the source.
        let mut which_line = 0;
        let mut skip_caret = true;
        if err.source_start != SOURCE_LOCATION_UNKNOWN && err.source_start <= src.len() {
            // Determine which line we're on.
            which_line = 1 + src[..err.source_start]
                .chars()
                .filter(|c| *c == '\n')
                .count();

            // Don't include the caret if we're interactive, this is the first line of text, and our
            // source is at its beginning, because then it's obvious.
            skip_caret = self.is_interactive() && which_line == 1 && err.source_start == 0;
        }

        let prefix = if let Some(filename) = self.current_filename() {
            if which_line > 0 {
                let mut prefix = wgettext_fmt!(
                    "%s (line %u):",
                    user_presentable_path(&filename, self.vars()),
                    which_line
                );
                prefix.push(' ');
                prefix
            } else {
                sprintf!("%s: ", user_presentable_path(&filename, self.vars()))
            }
        } else {
            L!("fish: ").to_owned()
        };

        let mut output = err.describe_with_prefix(src, &prefix, self.is_interactive(), skip_caret);
        if !output.is_empty() {
            output.push('\n');
        }
        output.push_utfstr(&self.stack_trace());
        output
    }

    /// Returns the file currently evaluated by the parser. This can be different than
    /// reader_current_filename, e.g. if we are evaluating a function defined in a different file
    /// than the one currently read.
    pub fn current_filename(&self) -> Option<FilenameRef> {
        self.blocks_iter_rev()
            .find_map(|b| match b.data() {
                Some(BlockData::Function { name, .. }) => {
                    function::get_props(name).and_then(|props| props.definition_file.clone())
                }
                Some(BlockData::Source { file, .. }) => Some(file.clone()),
                _ => None,
            })
            .or_else(|| self.libdata().current_filename.clone())
    }

    /// Return if we are interactive, which means we are executing a command that the user typed in
    /// (and not, say, a prompt).
    pub fn is_interactive(&self) -> bool {
        self.scope().is_interactive
    }

    /// Return a string representing the current stack trace.
    pub fn stack_trace(&self) -> WString {
        let mut line_cache = SourceLineCache::default();
        self.blocks_iter_rev()
            // Stop at event handler. No reason to believe that any other code is relevant.
            // It might make sense in the future to continue printing the stack trace of the code
            // that invoked the event, if this is a programmatic event, but we can't currently
            // detect that.
            .take_while(|b| b.typ() != BlockType::event)
            .fold(WString::new(), |mut trace, b| {
                append_block_description_to_stack_trace(self, &b, &mut trace, &mut line_cache);
                trace
            })
    }

    /// Return whether the number of functions in the stack exceeds our stack depth limit.
    pub fn function_stack_is_overflowing(&self) -> bool {
        // We are interested in whether the count of functions on the stack exceeds
        // FISH_MAX_STACK_DEPTH. We don't separately track the number of functions, but we can have a
        // fast path through the eval_level. If the eval_level is in bounds, so must be the stack depth.
        if self.scope().eval_level <= FISH_MAX_STACK_DEPTH {
            return false;
        }
        // Count the functions.
        let depth = self
            .blocks_iter_rev()
            .filter(|b| b.is_function_call())
            .count();
        depth > (FISH_MAX_STACK_DEPTH as usize)
    }

    /// Mark whether we should sync universal variables.
    pub fn set_syncs_uvars(&self, flag: bool) {
        self.syncs_uvars.store(flag);
    }

    /// Return the operation context for this parser.
    pub fn context(&self) -> OperationContext<'_> {
        OperationContext::foreground(
            self,
            Box::new(|| signal_check_cancel() != 0),
            EXPANSION_LIMIT_DEFAULT,
        )
    }

    /// Checks if the max eval depth has been exceeded
    pub fn is_eval_depth_exceeded(&self) -> bool {
        self.scope().eval_level >= FISH_MAX_EVAL_DEPTH
    }

    pub fn set_color_theme(&self, background_color: Option<&xterm_color::Color>) {
        let color_theme = match background_color.map(|c| c.perceived_lightness()) {
            Some(x) if x < 0.5 => L!("dark"),
            Some(_) => L!("light"),
            None => L!("unknown"),
        };
        if self
            .vars()
            .get(FISH_TERMINAL_COLOR_THEME_VAR)
            .is_some_and(|var| var.as_list() == [color_theme])
        {
            return;
        }
        flogf!(
            reader,
            "Setting %s to %s",
            FISH_TERMINAL_COLOR_THEME_VAR,
            color_theme
        );
        self.set_var_and_fire(
            FISH_TERMINAL_COLOR_THEME_VAR,
            ParserEnvSetMode::new(EnvMode::GLOBAL),
            vec![color_theme.to_owned()],
        );
    }
}

// Given a file path, return something nicer. Currently we just "unexpand" tildes.
fn user_presentable_path(path: &wstr, vars: &dyn Environment) -> WString {
    replace_home_directory_with_tilde(path, vars)
}

/// Print profiling information to the specified stream.
fn print_profile(items: &[ProfileItem], out: &mut File) {
    let col_width = 10;
    let _ = out.write_all(
        format!(
            "{:^col_width$} {:^col_width$} Command\n",
            "Time (μs)", "Sum (μs)",
        )
        .as_bytes(),
    );
    for (idx, item) in items.iter().enumerate() {
        if item.skipped || item.cmd.is_empty() {
            continue;
        }

        let total_time = item.duration;

        // Compute the self time as the total time, minus the total time consumed by subsequent
        // items exactly one eval level deeper.
        let mut self_time = item.duration;
        for nested_item in items[idx + 1..].iter() {
            if nested_item.skipped {
                continue;
            }

            // If the eval level is not larger, then we have exhausted nested items.
            if nested_item.level <= item.level {
                break;
            }

            // If the eval level is exactly one more than our level, it is a directly nested item.
            if nested_item.level == item.level + 1 {
                self_time -= nested_item.duration;
            }
        }

        let level = item.level.unsigned_abs().saturating_add(1);
        let _ = out.write_all(
            format!(
                "{:>col_width$} {:>col_width$} {:->level$} ",
                self_time, total_time, '>'
            )
            .as_bytes(),
        );
        let indentation_level = col_width + 1 + col_width + 1 + level + 1;
        let indented_cmd = item.cmd.replace(
            L!("\n"),
            &(WString::from("\n") + &wstr::repeat(L!(" "), indentation_level)[..]),
        );
        let _ = out.write_all(&wcs2bytes(&indented_cmd));
        let _ = out.write_all(b"\n");
    }
}

/// Append stack trace info for the block `b` to `trace`.
fn append_block_description_to_stack_trace(
    parser: &Parser,
    b: &Block,
    trace: &mut WString,
    line_cache: &mut SourceLineCache,
) {
    let mut print_source_location = false;
    match b.typ() {
        BlockType::function_call { .. } => {
            let Some(BlockData::Function { name, args, .. }) = b.data() else {
                unreachable!()
            };
            trace.push_utfstr(&wgettext_fmt!("in function '%s'", name));
            // Print arguments on the same line.
            let mut args_str = WString::new();
            for arg in args {
                if !args_str.is_empty() {
                    args_str.push(' ');
                }
                // We can't quote the arguments because we print this in quotes.
                // As a special-case, add the empty argument as "".
                if !arg.is_empty() {
                    args_str.push_utfstr(&escape_string(
                        arg,
                        EscapeStringStyle::Script(EscapeFlags::NO_QUOTED),
                    ));
                } else {
                    args_str.push_str("\"\"");
                }
            }
            if !args_str.is_empty() {
                trace.push(' ');
                // TODO: Escape these.
                trace.push_utfstr(&wgettext_fmt!("with arguments '%s'", args_str));
            }
            trace.push('\n');
            print_source_location = true;
        }
        BlockType::subst => {
            trace.push_utfstr(&wgettext!("in command substitution"));
            trace.push('\n');
            print_source_location = true;
        }
        BlockType::source => {
            let Some(BlockData::Source { file, .. }) = b.data() else {
                unreachable!()
            };
            let source_dest = file;
            trace.push_utfstr(&wgettext_fmt!(
                "from sourcing file %s",
                &user_presentable_path(source_dest, parser.vars())
            ));
            trace.push('\n');
            print_source_location = true;
        }
        BlockType::event => {
            let Some(BlockData::Event(event)) = b.data() else {
                unreachable!()
            };
            let description = event::get_desc(parser, event);
            trace.push_utfstr(&wgettext_fmt!("in event handler: %s", &description));
            trace.push('\n');
            print_source_location = true;
        }
        BlockType::top
        | BlockType::begin
        | BlockType::switch_block
        | BlockType::while_block
        | BlockType::for_block
        | BlockType::if_block
        | BlockType::breakpoint
        | BlockType::variable_assignment => {}
    }

    if print_source_location {
        if let Some(file) = b.src_filename.as_ref() {
            trace.push_utfstr(&sprintf!(
                "\tcalled on line %d of file %s\n",
                b.src_lineno(line_cache).map_or(0, |n| n.get()),
                user_presentable_path(file, parser.vars())
            ));
        } else if parser.libdata().within_fish_init {
            trace.push_str("\tcalled during startup\n");
        }
    }
}

/// Types of blocks.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum BlockType {
    /// While loop block
    while_block,
    /// For loop block
    for_block,
    /// If block
    if_block,
    /// Function invocation block
    function_call { shadows: bool },
    /// Switch block
    switch_block,
    /// Command substitution scope
    subst,
    /// Outermost block
    #[default]
    top,
    /// Unconditional block
    begin,
    /// Block created by the . (source) builtin
    source,
    /// Block created on event notifier invocation
    event,
    /// Breakpoint block
    breakpoint,
    /// Variable assignment before a command
    variable_assignment,
}

/// Possible states for a loop.
#[derive(Clone, Copy, Default, Eq, PartialEq)]
pub enum LoopStatus {
    /// current loop block executed as normal
    #[default]
    normals,
    /// current loop block should be removed
    breaks,
    /// current loop block should be skipped
    continues,
}

#[cfg(test)]
mod tests {
    use super::{CancelBehavior, Parser};
    use crate::ast::{self, Ast, JobList, Kind, Node, Traversal, is_same_node};
    use crate::common::str2wcstring;
    use crate::env::EnvStack;
    use crate::expand::ExpandFlags;
    use crate::io::{IoBufferfill, IoChain};
    use crate::parse_constants::{
        ParseErrorCode, ParseTokenType, ParseTreeFlags, ParserTestErrorBits, StatementDecoration,
    };
    use crate::parse_util::{detect_errors_in_argument, detect_parse_errors};
    use crate::prelude::*;
    use crate::reader::{fake_scoped_reader, reader_reset_interrupted};
    use crate::signal::{signal_clear_cancel, signal_reset_handlers, signal_set_handlers};
    use crate::tests::prelude::*;
    use fish_wcstringutil::join_strings;
    use libc::SIGINT;
    use std::time::Duration;
    #[test]
    #[serial]
    fn test_parser() {
        let _cleanup = test_init();
        macro_rules! detect_errors {
            ($src:literal) => {
                detect_parse_errors(L!($src), None, true /* accept incomplete */)
            };
        }

        fn detect_argument_errors(src: &str) -> Result<(), ParserTestErrorBits> {
            let src = str2wcstring(src);
            let ast = ast::parse_argument_list(&src, ParseTreeFlags::default(), None);
            if ast.errored() {
                return Err(ParserTestErrorBits::ERROR);
            }
            let args = &ast.top().arguments;
            let first_arg = args.first().expect("Failed to parse an argument");
            let mut errors = None;
            detect_errors_in_argument(first_arg, first_arg.source(&src), &mut errors)
        }

        // Testing block nesting
        assert!(
            detect_errors!("if; end").is_err(),
            "Incomplete if statement undetected"
        );
        assert!(
            detect_errors!("if test; echo").is_err(),
            "Missing end undetected"
        );
        assert!(
            detect_errors!("if test; end; end").is_err(),
            "Unbalanced end undetected"
        );

        // Testing detection of invalid use of builtin commands
        assert!(
            detect_errors!("case foo").is_err(),
            "'case' command outside of block context undetected"
        );
        assert!(
            detect_errors!("switch ggg; if true; case foo;end;end").is_err(),
            "'case' command outside of switch block context undetected"
        );
        assert!(
            detect_errors!("else").is_err(),
            "'else' command outside of conditional block context undetected"
        );
        assert!(
            detect_errors!("else if").is_err(),
            "'else if' command outside of conditional block context undetected"
        );
        assert!(
            detect_errors!("if false; else if; end").is_err(),
            "'else if' missing command undetected"
        );

        assert!(
            detect_errors!("break").is_err(),
            "'break' command outside of loop block context undetected"
        );

        assert!(
            detect_errors!("break --help").is_ok(),
            "'break --help' incorrectly marked as error"
        );

        assert!(
            detect_errors!("while false ; function foo ; break ; end ; end ").is_err(),
            "'break' command inside function allowed to break from loop outside it"
        );

        assert!(
            detect_errors!("exec ls|less").is_err() && detect_errors!("echo|return").is_err(),
            "Invalid pipe command undetected"
        );

        assert!(
            detect_errors!("for i in foo ; switch $i ; case blah ; break; end; end ").is_ok(),
            "'break' command inside switch falsely reported as error"
        );

        assert!(
            detect_errors!("or cat | cat").is_ok() && detect_errors!("and cat | cat").is_ok(),
            "boolean command at beginning of pipeline falsely reported as error"
        );

        assert!(
            detect_errors!("cat | and cat").is_err(),
            "'and' command in pipeline not reported as error"
        );

        assert!(
            detect_errors!("cat | or cat").is_err(),
            "'or' command in pipeline not reported as error"
        );

        assert!(
            detect_errors!("cat | exec").is_err() && detect_errors!("exec | cat").is_err(),
            "'exec' command in pipeline not reported as error"
        );

        assert!(
            detect_errors!("begin ; end arg").is_err(),
            "argument to 'end' not reported as error"
        );

        assert!(
            detect_errors!("switch foo ; end arg").is_err(),
            "argument to 'end' not reported as error"
        );

        assert!(
            detect_errors!("if true; else if false ; end arg").is_err(),
            "argument to 'end' not reported as error"
        );

        assert!(
            detect_errors!("if true; else ; end arg").is_err(),
            "argument to 'end' not reported as error"
        );

        assert!(
            detect_errors!("begin ; end 2> /dev/null").is_ok(),
            "redirection after 'end' wrongly reported as error"
        );

        assert_eq!(
            detect_errors!("true | "),
            Err(ParserTestErrorBits::INCOMPLETE),
            "unterminated pipe not reported properly"
        );

        assert_eq!(
            detect_errors!("echo (\nfoo\n  bar"),
            Err(ParserTestErrorBits::INCOMPLETE),
            "unterminated multiline subshell not reported properly"
        );

        assert_eq!(
            detect_errors!("begin ; true ; end | "),
            Err(ParserTestErrorBits::INCOMPLETE),
            "unterminated pipe not reported properly"
        );

        assert_eq!(
            detect_errors!(" | true "),
            Err(ParserTestErrorBits::ERROR),
            "leading pipe not reported properly"
        );

        assert_eq!(
            detect_errors!("true | # comment"),
            Err(ParserTestErrorBits::INCOMPLETE),
            "comment after pipe not reported as incomplete"
        );

        assert!(
            detect_errors!("true | # comment \n false ").is_ok(),
            "comment and newline after pipe wrongly reported as error"
        );

        assert_eq!(
            detect_errors!("true | ; false "),
            Err(ParserTestErrorBits::ERROR),
            "semicolon after pipe not detected as error"
        );

        assert!(
            detect_argument_errors("foo").is_ok(),
            "simple argument reported as error"
        );

        assert!(
            detect_argument_errors("''").is_ok(),
            "Empty string reported as error"
        );

        assert!(
            detect_argument_errors("foo$$")
                .unwrap_err()
                .contains(ParserTestErrorBits::ERROR),
            "Bad variable expansion not reported as error"
        );

        assert!(
            detect_argument_errors("foo$@")
                .unwrap_err()
                .contains(ParserTestErrorBits::ERROR),
            "Bad variable expansion not reported as error"
        );

        // Within command substitutions, we should be able to detect everything that
        // detect_errors! can detect.
        assert!(
            detect_argument_errors("foo(cat | or cat)")
                .unwrap_err()
                .contains(ParserTestErrorBits::ERROR),
            "Bad command substitution not reported as error"
        );

        assert!(
            detect_errors!("false & ; and cat").is_err(),
            "'and' command after background not reported as error"
        );

        assert!(
            detect_errors!("true & ; or cat").is_err(),
            "'or' command after background not reported as error"
        );

        assert!(
            detect_errors!("true & ; not cat").is_ok(),
            "'not' command after background falsely reported as error"
        );

        assert!(
            detect_errors!("if true & ; end").is_err(),
            "backgrounded 'if' conditional not reported as error"
        );

        assert!(
            detect_errors!("if false; else if true & ; end").is_err(),
            "backgrounded 'else if' conditional not reported as error"
        );

        assert!(
            detect_errors!("while true & ; end").is_err(),
            "backgrounded 'while' conditional not reported as error"
        );

        assert!(
            detect_errors!("true | || false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("|| false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("&& false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("true ; && false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("true ; || false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("true || && false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("true && || false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert!(
            detect_errors!("true && && false").is_err(),
            "bogus boolean statement error not detected"
        );

        assert_eq!(
            detect_errors!("true && "),
            Err(ParserTestErrorBits::INCOMPLETE),
            "unterminated conjunction not reported properly"
        );

        assert!(
            detect_errors!("true && \n true").is_ok(),
            "newline after && reported as error"
        );

        assert_eq!(
            detect_errors!("true || \n"),
            Err(ParserTestErrorBits::INCOMPLETE),
            "unterminated conjunction not reported properly"
        );

        assert_eq!(
            detect_errors!("begin ; echo hi; }"),
            Err(ParserTestErrorBits::ERROR),
            "closing of unopened brace statement not reported properly"
        );

        assert_eq!(
            detect_errors!("begin {"), // }
            Err(ParserTestErrorBits::INCOMPLETE),
            "brace after begin not reported properly"
        );
        assert_eq!(
            detect_errors!("a=b {"), // }
            Err(ParserTestErrorBits::INCOMPLETE),
            "brace after variable override not reported properly"
        );
    }

    #[test]
    #[serial]
    fn test_new_parser_correctness() {
        let _cleanup = test_init();
        macro_rules! validate {
            ($src:expr, $ok:expr) => {
                let ast = ast::parse(L!($src), ParseTreeFlags::default(), None);
                assert_eq!(ast.errored(), !$ok);
            };
        }
        validate!("; ; ; ", true);
        validate!("if ; end", false);
        validate!("if true ; end", true);
        validate!("if true; end ; end", false);
        validate!("if end; end ; end", false);
        validate!("if end", false);
        validate!("end", false);
        validate!("for i i", false);
        validate!("for i in a b c ; end", true);
        validate!("begin end", true);
        validate!("begin; end", true);
        validate!("begin if true; end; end;", true);
        validate!("begin if true ; echo hi ; end; end", true);
        validate!("true && false || false", true);
        validate!("true || false; and true", true);
        validate!("true || ||", false);
        validate!("|| true", false);
        validate!("true || \n\n false", true);
    }

    #[test]
    #[serial]
    fn test_new_parser_correctness_by_fuzzing() {
        let _cleanup = test_init();
        let fuzzes = [
            L!("if"),
            L!("else"),
            L!("for"),
            L!("in"),
            L!("while"),
            L!("begin"),
            L!("function"),
            L!("switch"),
            L!("case"),
            L!("end"),
            L!("and"),
            L!("or"),
            L!("not"),
            L!("command"),
            L!("builtin"),
            L!("foo"),
            L!("|"),
            L!("^"),
            L!("&"),
            L!(";"),
        ];

        // Generate a list of strings of all keyword / token combinations.
        let mut src = WString::new();
        src.reserve(128);

        // Given that we have an array of 'fuzz_count' strings, we wish to enumerate all permutations of
        // 'len' values. We do this by incrementing an integer, interpreting it as "base fuzz_count".
        fn string_for_permutation(
            fuzzes: &[&wstr],
            len: usize,
            permutation: usize,
        ) -> Option<WString> {
            let mut remaining_permutation = permutation;
            let mut out_str = WString::new();
            for _i in 0..len {
                let idx = remaining_permutation % fuzzes.len();
                remaining_permutation /= fuzzes.len();
                out_str.push_utfstr(fuzzes[idx]);
                out_str.push(' ');
            }
            // Return false if we wrapped.
            (remaining_permutation == 0).then_some(out_str)
        }

        let max_len = 5;
        for len in 0..max_len {
            // We wish to look at all permutations of 4 elements of 'fuzzes' (with replacement).
            // Construct an int and keep incrementing it.
            let mut permutation = 0;
            while let Some(src) = string_for_permutation(&fuzzes, len, permutation) {
                permutation += 1;
                ast::parse(&src, ParseTreeFlags::default(), None);
            }
        }
    }

    // Test the LL2 (two token lookahead) nature of the parser by exercising the special builtin and
    // command handling. In particular, 'command foo' should be a decorated statement 'foo' but 'command
    // -help' should be an undecorated statement 'command' with argument '--help', and NOT attempt to
    // run a command called '--help'.
    #[test]
    #[serial]
    fn test_new_parser_ll2() {
        let _cleanup = test_init();
        // Parse a statement, returning the command, args (joined by spaces), and the decoration. Returns
        // true if successful.
        fn test_1_parse_ll2(src: &wstr) -> Option<(WString, WString, StatementDecoration)> {
            let ast = ast::parse(src, ParseTreeFlags::default(), None);
            if ast.errored() {
                return None;
            }

            // Get the statement. Should only have one.
            let mut statement = None;
            for n in Traversal::new(ast.top()) {
                if let Kind::DecoratedStatement(tmp) = n.kind() {
                    assert!(
                        statement.is_none(),
                        "More than one decorated statement found in '{}'",
                        src
                    );
                    statement = Some(tmp);
                }
            }
            let statement = statement.expect("No decorated statement found");

            // Return its decoration and command.
            let out_deco = statement.decoration();
            let out_cmd = statement.command.source(src).to_owned();

            // Return arguments separated by spaces.
            let out_joined_args = join_strings(
                &statement
                    .args_or_redirs
                    .iter()
                    .filter(|a| a.is_argument())
                    .map(|a| a.source(src))
                    .collect::<Vec<_>>(),
                ' ',
            );

            Some((out_cmd, out_joined_args, out_deco))
        }
        macro_rules! validate {
            ($src:expr, $cmd:expr, $args:expr, $deco:expr) => {
                let (cmd, args, deco) = test_1_parse_ll2(L!($src)).unwrap();
                assert_eq!(cmd, L!($cmd));
                assert_eq!(args, L!($args));
                assert_eq!(deco, $deco);
            };
        }

        validate!("echo hello", "echo", "hello", StatementDecoration::None);
        validate!(
            "command echo hello",
            "echo",
            "hello",
            StatementDecoration::Command
        );
        validate!(
            "exec echo hello",
            "echo",
            "hello",
            StatementDecoration::Exec
        );
        validate!(
            "command command hello",
            "command",
            "hello",
            StatementDecoration::Command
        );
        validate!(
            "builtin command hello",
            "command",
            "hello",
            StatementDecoration::Builtin
        );
        validate!(
            "command --help",
            "command",
            "--help",
            StatementDecoration::None
        );
        validate!("command -h", "command", "-h", StatementDecoration::None);
        validate!("command", "command", "", StatementDecoration::None);
        validate!("command -", "command", "-", StatementDecoration::None);
        validate!("command --", "command", "--", StatementDecoration::None);
        validate!(
            "builtin --names",
            "builtin",
            "--names",
            StatementDecoration::None
        );
        validate!("function", "function", "", StatementDecoration::None);
        validate!(
            "function --help",
            "function",
            "--help",
            StatementDecoration::None
        );

        // Verify that 'function -h' and 'function --help' are plain statements but 'function --foo' is
        // not (issue #1240).
        macro_rules! check_function_help {
            ($src:expr, $kind:pat) => {
                let ast = ast::parse(L!($src), ParseTreeFlags::default(), None);
                assert!(!ast.errored());
                assert_eq!(
                    Traversal::new(ast.top())
                        .filter(|n| matches!(n.kind(), $kind))
                        .count(),
                    1
                );
            };
        }
        check_function_help!("function -h", ast::Kind::DecoratedStatement(_));
        check_function_help!("function --help", ast::Kind::DecoratedStatement(_));
        check_function_help!("function --foo; end", ast::Kind::FunctionHeader(_));
        check_function_help!("function foo; end", ast::Kind::FunctionHeader(_));
    }

    #[test]
    #[serial]
    fn test_new_parser_ad_hoc() {
        let _cleanup = test_init();
        // Very ad-hoc tests for issues encountered.

        // Ensure that 'case' terminates a job list.
        let src = L!("switch foo ; case bar; case baz; end");
        let ast = ast::parse(src, ParseTreeFlags::default(), None);
        assert!(!ast.errored());
        // Expect two CaseItems. The bug was that we'd
        // try to run a command 'case'.
        assert_eq!(
            Traversal::new(ast.top())
                .filter(|n| matches!(n.kind(), ast::Kind::CaseItem(_)))
                .count(),
            2
        );

        // Ensure that naked variable assignments don't hang.
        // The bug was that "a=" would produce an error but not be consumed,
        // leading to an infinite loop.

        // By itself it should produce an error.
        let ast = ast::parse(L!("a="), ParseTreeFlags::default(), None);
        assert!(ast.errored());

        let flags = ParseTreeFlags {
            leave_unterminated: true,
            ..ParseTreeFlags::default()
        };

        // If we are leaving things unterminated, this should not produce an error.
        // i.e. when typing "a=" at the command line, it should be treated as valid
        // because we don't want to color it as an error.
        let ast = ast::parse(L!("a="), flags, None);
        assert!(!ast.errored());

        let mut errors = vec![];
        ast::parse(L!("begin; echo ("), flags, Some(&mut errors));
        assert_eq!(errors.len(), 1);
        assert_eq!(
            errors[0].code,
            ParseErrorCode::TokenizerUnterminatedSubshell
        );

        errors.clear();
        ast::parse(L!("for x in ("), flags, Some(&mut errors));
        assert_eq!(errors.len(), 1);
        assert_eq!(
            errors[0].code,
            ParseErrorCode::TokenizerUnterminatedSubshell
        );

        errors.clear();
        ast::parse(L!("begin; echo '"), flags, Some(&mut errors));
        assert_eq!(errors.len(), 1);
        assert_eq!(errors[0].code, ParseErrorCode::TokenizerUnterminatedQuote);
    }

    #[test]
    #[serial]
    fn test_new_parser_errors() {
        let _cleanup = test_init();
        macro_rules! validate {
            ($src:expr, $expected_code:expr) => {
                let mut errors = vec![];
                let ast = ast::parse(L!($src), ParseTreeFlags::default(), Some(&mut errors));
                assert!(ast.errored());
                assert_eq!(
                    errors.into_iter().map(|e| e.code).collect::<Vec<_>>(),
                    vec![$expected_code],
                );
            };
        }

        validate!("echo 'abc", ParseErrorCode::TokenizerUnterminatedQuote);
        validate!("'", ParseErrorCode::TokenizerUnterminatedQuote);
        validate!("echo (abc", ParseErrorCode::TokenizerUnterminatedSubshell);

        validate!("end", ParseErrorCode::UnbalancingEnd);
        validate!("echo hi ; end", ParseErrorCode::UnbalancingEnd);

        validate!("else", ParseErrorCode::UnbalancingElse);
        validate!("if true ; end ; else", ParseErrorCode::UnbalancingElse);

        validate!("case", ParseErrorCode::UnbalancingCase);
        validate!("if true ; case ; end", ParseErrorCode::UnbalancingCase);

        validate!("begin ; }", ParseErrorCode::UnbalancingBrace);

        validate!("true | and", ParseErrorCode::AndOrInPipeline);

        validate!("a=", ParseErrorCode::BareVariableAssignment);
    }

    #[test]
    #[serial]
    fn test_eval_recursion_detection() {
        let _cleanup = test_init();
        // Ensure that we don't crash on infinite self recursion and mutual recursion.
        let parser = TestParser::new();
        parser.eval(
            L!("function recursive ; recursive ; end ; recursive; "),
            &IoChain::new(),
        );

        parser.eval(
            L!(concat!(
                "function recursive1 ; recursive2 ; end ; ",
                "function recursive2 ; recursive1 ; end ; recursive1; ",
            )),
            &IoChain::new(),
        );
    }

    #[test]
    #[serial]
    fn test_eval_illegal_exit_code() {
        let _cleanup = test_init();
        let parser = TestParser::new();
        macro_rules! validate {
            ($cmd:expr, $result:expr) => {
                parser.eval($cmd, &IoChain::new());
                let exit_status = parser.get_last_status();
                assert_eq!(exit_status, parser.get_last_status());
            };
        }

        // We need to be in an empty directory so that none of the wildcards match a file that might be
        // in the fish source tree. In particular we need to ensure that "?" doesn't match a file
        // named by a single character. See issue #3852.
        parser.pushd("test/temp");
        validate!(L!("echo -n"), STATUS_CMD_OK.unwrap());
        validate!(L!("pwd"), STATUS_CMD_OK.unwrap());
        validate!(L!("UNMATCHABLE_WILDCARD*"), STATUS_UNMATCHED_WILDCARD);
        validate!(L!("UNMATCHABLE_WILDCARD**"), STATUS_UNMATCHED_WILDCARD);
        validate!(L!("?"), STATUS_UNMATCHED_WILDCARD);
        validate!(L!("abc?def"), STATUS_UNMATCHED_WILDCARD);
        parser.popd();
    }

    #[test]
    #[serial]
    fn test_eval_empty_function_name() {
        let _cleanup = test_init();
        let parser = TestParser::new();
        parser.eval(
            L!("function '' ; echo fail; exit 42 ; end ; ''"),
            &IoChain::new(),
        );
    }

    #[test]
    #[serial]
    fn test_expand_argument_list() {
        let _cleanup = test_init();
        let parser = TestParser::new();
        let comps: Vec<WString> = Parser::expand_argument_list(
            L!("alpha 'beta gamma' delta"),
            ExpandFlags::default(),
            &parser.context(),
        )
        .into_iter()
        .map(|c| c.completion)
        .collect();
        assert_eq!(comps, &[L!("alpha"), L!("beta gamma"), L!("delta"),]);
    }

    fn test_1_cancellation(parser: &Parser, src: &wstr) {
        let filler = IoBufferfill::create().unwrap();
        let delay = Duration::from_millis(100);
        #[allow(clippy::unnecessary_cast)]
        let thread = unsafe { libc::pthread_self() } as usize;
        std::thread::spawn(move || {
            // Wait a while and then SIGINT the main thread.
            std::thread::sleep(delay);
            unsafe {
                libc::pthread_kill(thread as libc::pthread_t, SIGINT);
            }
        });
        let mut io = IoChain::new();
        io.push(filler.clone());
        let res = parser.eval(src, &io);
        let buffer = IoBufferfill::finish(filler);
        assert_eq!(
            buffer.len(),
            0,
            "Expected 0 bytes in out_buff, but instead found {} bytes, for command {}",
            buffer.len(),
            src
        );
        assert!(res.status.signal_exited() && res.status.signal_code() == SIGINT);
    }

    #[test]
    #[serial]
    fn test_cancellation() {
        let _cleanup = test_init();
        let parser = Parser::new(EnvStack::new(), CancelBehavior::Clear);
        let _pop = fake_scoped_reader(&parser);

        printf!("Testing Ctrl-C cancellation. If this hangs, that's a bug!\n");

        // Enable fish's signal handling here.
        signal_set_handlers(true);

        // This tests that we can correctly ctrl-C out of certain loop constructs, and that nothing gets
        // printed if we do.

        // Here the command substitution is an infinite loop. echo never even gets its argument, so when
        // we cancel we expect no output.
        test_1_cancellation(&parser, L!("echo (while true ; echo blah ; end)"));

        // Nasty infinite loop that doesn't actually execute anything.
        test_1_cancellation(
            &parser,
            L!("echo (while true ; end) (while true ; end) (while true ; end)"),
        );
        test_1_cancellation(&parser, L!("while true ; end"));
        test_1_cancellation(&parser, L!("while true ; echo nothing > /dev/null; end"));
        test_1_cancellation(&parser, L!("for i in (while true ; end) ; end"));

        signal_reset_handlers();

        // Ensure that we don't think we should cancel.
        reader_reset_interrupted();
        signal_clear_cancel();
    }

    // Helper for testing a simple ast traversal.
    // The ast is always for the command 'true;'.
    struct TrueSemiAstTester<'a> {
        // The AST we are testing.
        ast: &'a Ast,

        // Expected parent-child relationships, in the order we expect to encounter them.
        parent_child: Box<[(&'a dyn Node, &'a dyn Node)]>,
    }

    impl<'a> TrueSemiAstTester<'a> {
        const TRUE_SEMI: &'static wstr = L!("true;");
        fn new(ast: &'a Ast) -> Self {
            let job_list: &JobList = ast.top();
            let job_conjunction = &job_list[0];
            let job_pipeline = &job_conjunction.job;
            let variable_assignment_list = &job_pipeline.variables;
            let statement = &job_pipeline.statement;

            let decorated_statement = statement
                .as_decorated_statement()
                .expect("Expected decorated_statement");
            let command = &decorated_statement.command;
            let args_or_redirs = &decorated_statement.args_or_redirs;
            let job_continuation = &job_pipeline.continuation;
            let job_conjunction_continuation = &job_conjunction.continuations;
            let semi_nl = job_conjunction.semi_nl.as_ref().expect("Expected semi_nl");

            // Helpful parent-child map, such that the children are in the order that we expect to encounter them
            // in the AST.
            let parent_child: &[(&'a dyn Node, &'a dyn Node)] = &[
                (job_list, job_conjunction),
                (job_conjunction, job_pipeline),
                (job_pipeline, variable_assignment_list),
                (job_pipeline, statement),
                (statement, decorated_statement),
                (decorated_statement, command),
                (decorated_statement, args_or_redirs),
                (job_pipeline, job_continuation),
                (job_conjunction, job_conjunction_continuation),
                (job_conjunction, semi_nl),
            ];
            Self {
                ast,
                parent_child: Box::from(parent_child),
            }
        }

        // Expected nodes, in-order.
        fn expected_nodes(&self) -> Vec<&'a dyn Node> {
            let mut expected: Vec<&dyn Node> = vec![self.ast.top()];
            expected.extend(self.parent_child.iter().map(|&(_p, c)| c));
            expected
        }

        // Helper function to construct the parent list of a given node, such at the first entry is
        // the node itself, and the last entry is the root node.
        fn get_parents<'s>(
            &'s self,
            node: &'a dyn Node,
        ) -> impl Iterator<Item = &'a dyn Node> + 's {
            let mut next = Some(node);
            std::iter::from_fn(move || {
                let out = next?;
                next = self
                    .parent_child
                    .iter()
                    .find_map(|&(p, c)| is_same_node(c, out).then_some(p));
                Some(out)
            })
        }
    }

    #[test]
    fn test_ast() {
        // Light testing of the AST and traversals.
        let ast = ast::parse(
            TrueSemiAstTester::TRUE_SEMI,
            ParseTreeFlags::default(),
            None,
        );
        let tester = TrueSemiAstTester::new(&ast);

        // Walk the AST and collect all nodes.
        // See is_same_node comments for why we can't use assert_eq! here.
        let found = ast.walk().collect::<Vec<_>>();
        let expected = tester.expected_nodes();
        assert_eq!(found.len(), expected.len());
        for idx in 0..found.len() {
            assert!(is_same_node(found[idx], expected[idx]));
        }

        // Walk and check parents.
        let mut traversal = ast.walk();
        while let Some(node) = traversal.next() {
            let expected_parents = tester.get_parents(node).collect::<Vec<_>>();
            let found_parents = traversal.parent_nodes().collect::<Vec<_>>();
            assert_eq!(found_parents.len(), expected_parents.len());
            for idx in 0..found_parents.len() {
                assert!(is_same_node(found_parents[idx], expected_parents[idx]));
            }
        }

        // Find the decorated statement.
        let decorated_statement = ast
            .walk()
            .find(|n| matches!(n.kind(), ast::Kind::DecoratedStatement(_)))
            .expect("Expected decorated statement");

        // Test the skip feature. Don't descend into the decorated_statement.
        let expected_skip: Vec<&dyn Node> = expected
            .iter()
            .copied()
            .filter(|&n| {
                // Discard nodes who have the decorated_statement as a parent,
                // excepting the decorated_statement itself.
                tester
                    .get_parents(n)
                    .skip(1)
                    .all(|p| !is_same_node(p, decorated_statement))
            })
            .collect();

        let mut found = vec![];
        let mut traversal = ast.walk();
        while let Some(node) = traversal.next() {
            if is_same_node(node, decorated_statement) {
                traversal.skip_children(node);
            }
            found.push(node);
        }
        assert_eq!(found.len(), expected_skip.len());
        for idx in 0..found.len() {
            assert!(is_same_node(found[idx], expected_skip[idx]));
        }
    }

    #[test]
    #[should_panic]
    fn test_traversal_skip_children_panics() {
        // Test that we panic if we try to skip children of a node that is not the current node.
        let ast = ast::parse(L!("true;"), ParseTreeFlags::default(), None);
        let mut traversal = ast.walk();
        while let Some(node) = traversal.next() {
            if matches!(node.kind(), ast::Kind::DecoratedStatement(_)) {
                // Should panic as we can only skip the current node.
                traversal.skip_children(ast.top());
            }
        }
    }

    #[test]
    #[should_panic]
    fn test_traversal_parent_panics() {
        // Can only get the parent of nodes still on the stack.
        let ast = ast::parse(L!("true;"), ParseTreeFlags::default(), None);
        let mut traversal = ast.walk();
        let mut decorated_statement = None;
        while let Some(node) = traversal.next() {
            if let Kind::DecoratedStatement(_) = node.kind() {
                decorated_statement = Some(node);
            } else if node.as_token().map(|t| t.token_type()) == Some(ParseTokenType::End) {
                // should panic as the decorated_statement is not on the stack.
                let _ = traversal.parent(decorated_statement.unwrap());
            }
        }
    }
}
