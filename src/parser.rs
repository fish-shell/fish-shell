// The fish parser. Contains functions for parsing and evaluating code.

use crate::ast::{self, Ast, List, Node};
use crate::builtins::shared::STATUS_ILLEGAL_CMD;
use crate::common::{
    escape_string, scoped_push_replacer, CancelChecker, EscapeFlags, EscapeStringStyle,
    FilenameRef, ScopeGuarding, PROFILING_ACTIVE,
};
use crate::complete::CompletionList;
use crate::env::{EnvMode, EnvStack, EnvStackSetResult, Environment, Statuses};
use crate::event::{self, Event};
use crate::expand::{
    expand_string, replace_home_directory_with_tilde, ExpandFlags, ExpandResultCode,
};
use crate::fds::{open_dir, BEST_O_SEARCH};
use crate::global_safety::RelaxedAtomicBool;
use crate::io::IoChain;
use crate::job_group::MaybeJobId;
use crate::operation_context::{OperationContext, EXPANSION_LIMIT_DEFAULT};
use crate::parse_constants::{
    ParseError, ParseErrorList, ParseTreeFlags, FISH_MAX_EVAL_DEPTH, FISH_MAX_STACK_DEPTH,
    SOURCE_LOCATION_UNKNOWN,
};
use crate::parse_execution::{EndExecutionReason, ExecutionContext};
use crate::parse_tree::{parse_source, LineCounter, ParsedSourceRef};
use crate::proc::{job_reap, JobGroupRef, JobList, JobRef, ProcStatus};
use crate::signal::{signal_check_cancel, signal_clear_cancel, Signal};
use crate::threads::assert_is_main_thread;
use crate::util::get_time;
use crate::wait_handle::WaitHandleStore;
use crate::wchar::{wstr, WString, L};
use crate::wutil::{perror, wgettext, wgettext_fmt};
use crate::{function, FLOG};
use fish_printf::sprintf;
use libc::c_int;
use std::cell::{Ref, RefCell, RefMut};
use std::ffi::{CStr, OsStr};
use std::os::fd::{AsRawFd, OwnedFd, RawFd};
use std::os::unix::prelude::OsStrExt;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicIsize, AtomicU64, Ordering},
    Arc,
};

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

    /// Line number where this block was created.
    pub src_lineno: Option<usize>,
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
}

impl Default for BlockType {
    fn default() -> Self {
        BlockType::top
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

        if let Some(src_lineno) = self.src_lineno {
            result.push_utfstr(&sprintf!(" (line %d)", src_lineno));
        }
        if let Some(src_filename) = &self.src_filename {
            result.push_utfstr(&sprintf!(" (file %ls)", src_filename));
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

/// Miscellaneous data used to avoid recursion and others.
#[derive(Default)]
pub struct LibraryData {
    /// The current filename we are evaluating, either from builtin source or on the command line.
    pub current_filename: Option<FilenameRef>,

    /// A stack of fake values to be returned by builtin_commandline. This is used by the completion
    /// machinery when wrapping: e.g. if `tig` wraps `git` then git completions need to see git on
    /// the command line.
    pub transient_commandlines: Vec<WString>,

    /// A file descriptor holding the current working directory, for use in openat().
    /// This is never null and never invalid.
    pub cwd_fd: Option<Arc<OwnedFd>>,

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

    /// Whether we are currently cleaning processes.
    pub is_cleaning_procs: bool,

    /// The internal job id of the job being populated, or 0 if none.
    /// This supports the '--on-job-exit caller' feature.
    pub caller_id: u64, // TODO should be InternalJobId

    /// Whether we are running a subshell command.
    pub is_subshell: bool,

    /// Whether we are running an event handler. This is not a bool because we keep count of the
    /// event nesting level.
    pub is_event: i32,

    /// Whether we are currently interactive.
    pub is_interactive: bool,

    /// Whether to suppress fish_trace output. This occurs in the prompt, event handlers, and key
    /// bindings.
    pub suppress_fish_trace: bool,

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

    /// The read limit to apply to captured subshell output, or 0 for none.
    pub read_limit: usize,
}

impl LibraryData {
    pub fn new() -> Self {
        Self {
            last_exec_run_counter: u64::MAX,
            ..Default::default()
        }
    }
}

impl Default for LoopStatus {
    fn default() -> Self {
        LoopStatus::normals
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

pub type BlockId = usize;

// Controls the behavior when fish itself receives a signal and there are
// no blocks on the stack.
// The "outermost" parser is responsible for clearing the signal.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub enum CancelBehavior {
    #[default]
    Return, // Return the signal to the caller.
    Clear, // Clear the signal.
}

pub struct Parser {
    /// A shared line counter. This is handed out to each execution context
    /// so they can communicate the line number back to this Parser.
    line_counter: Rc<RefCell<LineCounter<ast::JobPipeline>>>,

    /// The jobs associated with this parser.
    job_list: RefCell<JobList>,

    /// Our store of recorded wait-handles. These are jobs that finished in the background,
    /// and have been reaped, but may still be wait'ed on.
    wait_handles: RefCell<WaitHandleStore>,

    /// The list of blocks.
    /// This is a stack; the topmost block is at the end. This is to avoid invalidating block
    /// indexes during recursive evaluation.
    block_list: RefCell<Vec<Block>>,

    /// The 'depth' of the fish call stack.
    pub eval_level: AtomicIsize,

    /// Set of variables for the parser.
    pub variables: Rc<EnvStack>,

    /// Miscellaneous library data.
    library_data: RefCell<LibraryData>,

    /// If set, we synchronize universal variables after external commands,
    /// including sending on-variable change events.
    syncs_uvars: RelaxedAtomicBool,

    /// The behavior when fish itself receives a signal and there are no blocks on the stack.
    cancel_behavior: CancelBehavior,

    /// List of profile items.
    profile_items: RefCell<Vec<ProfileItem>>,

    /// Global event blocks.
    pub global_event_blocks: AtomicU64,
}

impl Parser {
    /// Create a parser.
    pub fn new(variables: Rc<EnvStack>, cancel_behavior: CancelBehavior) -> Parser {
        let result = Self {
            line_counter: Rc::new(RefCell::new(LineCounter::empty())),
            job_list: RefCell::default(),
            wait_handles: RefCell::new(WaitHandleStore::new()),
            block_list: RefCell::default(),
            eval_level: AtomicIsize::new(-1),
            variables,
            library_data: RefCell::new(LibraryData::new()),
            syncs_uvars: RelaxedAtomicBool::new(false),
            cancel_behavior,
            profile_items: RefCell::default(),
            global_event_blocks: AtomicU64::new(0),
        };

        match open_dir(CStr::from_bytes_with_nul(b".\0").unwrap(), BEST_O_SEARCH) {
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
        self.blocks()
            .iter()
            .rev()
            // If a function sources a file, don't descend further.
            .take_while(|b| b.typ() != BlockType::source)
            .any(|b| b.is_function_call())
    }

    /// Return whether we are currently evaluating a command substitution.
    pub fn is_command_substitution(&self) -> bool {
        self.blocks()
            .iter()
            .rev()
            // If a function sources a file, don't descend further.
            .take_while(|b| b.typ() != BlockType::source)
            .any(|b| b.typ() == BlockType::subst)
    }

    /// Assert that this parser is allowed to execute on the current thread.
    pub fn assert_can_execute(&self) {
        assert_is_main_thread();
    }

    pub fn eval(&self, cmd: &wstr, io: &IoChain) -> EvalRes {
        self.eval_with(cmd, io, None, BlockType::top)
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
    ) -> EvalRes {
        // Parse the source into a tree, if we can.
        let mut error_list = ParseErrorList::new();
        if let Some(ps) = parse_source(
            cmd.to_owned(),
            ParseTreeFlags::empty(),
            Some(&mut error_list),
        ) {
            return self.eval_parsed_source(&ps, io, job_group, block_type);
        }

        // Get a backtrace. This includes the message.
        let backtrace_and_desc = self.get_backtrace(cmd, &error_list);

        // Print it.
        eprintf!("%s\n", backtrace_and_desc);

        // Set a valid status.
        self.set_last_statuses(Statuses::just(STATUS_ILLEGAL_CMD.unwrap()));
        let break_expand = true;
        EvalRes {
            status: ProcStatus::from_exit_code(STATUS_ILLEGAL_CMD.unwrap()),
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
    ) -> EvalRes {
        assert!([BlockType::top, BlockType::subst].contains(&block_type));
        let job_list = ps.ast.top().as_job_list().unwrap();
        if !job_list.is_empty() {
            // Execute the top job list.
            self.eval_node(ps, job_list, io, job_group, block_type)
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

    /// Evaluates a node.
    /// The node type must be ast::Statement or ast::JobList.
    pub fn eval_node<T: Node>(
        &self,
        ps: &ParsedSourceRef,
        node: &T,
        block_io: &IoChain,
        job_group: Option<&JobGroupRef>,
        block_type: BlockType,
    ) -> EvalRes {
        // Only certain blocks are allowed.
        assert!(
            [BlockType::top, BlockType::subst].contains(&block_type),
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

        job_reap(self, false); // not sure why we reap jobs here

        // Start it up
        let mut op_ctx = self.context();
        let scope_block = self.push_block(Block::scope_block(block_type));

        // Propagate our job group.
        op_ctx.job_group = job_group.cloned();

        // Replace the context's cancel checker with one that checks the job group's signal.
        let cancel_checker: CancelChecker = Box::new(move || check_cancel_signal().is_some());
        op_ctx.cancel_checker = cancel_checker;

        // Restore the line counter.
        let line_counter = Rc::clone(&self.line_counter);
        let scoped_line_counter =
            scoped_push_replacer(|v| line_counter.replace(v), ps.line_counter());

        // Create a new execution context.
        let mut execution_context =
            ExecutionContext::new(ps.clone(), block_io.clone(), Rc::clone(&line_counter));

        // Check the exec count so we know if anything got executed.
        let prev_exec_count = self.libdata().exec_count;
        let prev_status_count = self.libdata().status_count;
        let reason = execution_context.eval_node(&op_ctx, node, Some(scope_block));
        let new_exec_count = self.libdata().exec_count;
        let new_status_count = self.libdata().status_count;

        ScopeGuarding::commit(scoped_line_counter);
        self.pop_block(scope_block);

        job_reap(self, false); // reap again

        let sig = signal_check_cancel();
        if sig != 0 {
            EvalRes::new(ProcStatus::from_signal(Signal::new(sig)))
        } else {
            let status = ProcStatus::from_exit_code(self.get_last_status());
            let break_expand = reason == EndExecutionReason::error;
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
        let ast = Ast::parse_argument_list(arg_list_src, ParseTreeFlags::default(), None);
        if ast.errored() {
            // Failed to parse. Here we expect to have reported any errors in test_args.
            return vec![];
        }

        // Get the root argument list and extract arguments from it.
        let mut result = vec![];
        let list = ast.top().as_freestanding_argument_list().unwrap();
        for arg in &list.arguments {
            let arg_src = arg.source(arg_list_src);
            if expand_string(arg_src.to_owned(), &mut result, flags, ctx, None)
                == ExpandResultCode::error
            {
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
        let Some(source_offset) = self.line_counter.borrow_mut().source_offset_of_node() else {
            return WString::new();
        };

        let lineno = self.get_lineno().unwrap_or(0);
        let file = self.current_filename();

        let mut prefix = WString::new();

        // If we are not going to print a stack trace, at least print the line number and filename.
        if !self.is_interactive() || self.is_function() {
            if let Some(file) = file {
                prefix.push_utfstr(&wgettext_fmt!(
                    "%ls (line %d): ",
                    &user_presentable_path(&file, self.vars()),
                    lineno
                ));
            } else if self.libdata().within_fish_init {
                prefix.push_utfstr(&wgettext_fmt!("Startup (line %d): ", lineno));
            } else {
                prefix.push_utfstr(&wgettext_fmt!("Standard input (line %d): ", lineno));
            }
        }

        let skip_caret = self.is_interactive() && !self.is_function();

        // Use an error with empty text.
        let mut empty_error = ParseError::default();
        empty_error.source_start = source_offset;

        let mut line_info = empty_error.describe_with_prefix(
            self.line_counter.borrow().get_source(),
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
    pub fn get_lineno(&self) -> Option<usize> {
        // The offset is 0 based; the number is 1 based.
        self.line_counter
            .borrow_mut()
            .line_offset_of_node()
            .map(|offset| offset + 1)
    }

    /// Return whether we are currently evaluating a "block" such as an if statement.
    /// This supports 'status is-block'.
    pub fn is_block(&self) -> bool {
        // Note historically this has descended into 'source', unlike 'is_function'.
        self.blocks().iter().rev().any(|b| {
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
        self.blocks()
            .iter()
            .rev()
            .any(|b| b.typ() == BlockType::breakpoint)
    }

    /// Return the list of blocks. The first block is at the top.
    /// todo!("this RAII object should only be used for iterating over it (in reverse). Maybe enforce this")
    pub fn blocks(&self) -> Ref<'_, Vec<Block>> {
        self.block_list.borrow()
    }
    pub fn block_at_index(&self, index: usize) -> Option<Ref<'_, Block>> {
        let block_list = self.blocks();
        if index >= block_list.len() {
            None
        } else {
            Some(Ref::map(block_list, |bl| &bl[bl.len() - 1 - index]))
        }
    }
    pub fn block_at_index_mut(&self, index: usize) -> Option<RefMut<'_, Block>> {
        let block_list = self.block_list.borrow_mut();
        if index >= block_list.len() {
            None
        } else {
            Some(RefMut::map(block_list, |bl| {
                let len = bl.len();
                &mut bl[len - 1 - index]
            }))
        }
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

    /// Get the variables as an Arc.
    pub fn vars_ref(&self) -> Rc<EnvStack> {
        Rc::clone(&self.variables)
    }

    /// Get the library data.
    pub fn libdata(&self) -> Ref<'_, LibraryData> {
        self.library_data.borrow()
    }
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
        self.vars().set_last_statuses(s)
    }

    /// Cover of vars().set(), which also fires any returned event handlers.
    pub fn set_var_and_fire(
        &self,
        key: &wstr,
        mode: EnvMode,
        vals: Vec<WString>,
    ) -> EnvStackSetResult {
        let res = self.vars().set(key, mode, vals);
        if res == EnvStackSetResult::Ok {
            event::fire(self, Event::variable_set(key.to_owned()));
        }
        res
    }

    /// Cover of vars().set(), without firing events
    pub fn set_var(&self, key: &wstr, mode: EnvMode, vals: Vec<WString>) -> EnvStackSetResult {
        self.vars().set(key, mode, vals)
    }

    /// Update any universal variables and send event handlers.
    /// If `always` is set, then do it even if we have no pending changes (that is, look for
    /// changes from other fish instances); otherwise only sync if this instance has changed uvars.
    pub fn sync_uvars_and_fire(&self, always: bool) {
        if self.syncs_uvars.load() {
            let evts = self.vars().universal_sync(always);
            for evt in evts {
                event::fire(self, evt);
            }
        }
    }

    /// Pushes a new block. Returns a pointer to the block, stored in the parser.
    pub fn push_block(&self, mut block: Block) -> BlockId {
        block.src_lineno = self.get_lineno();
        block.src_filename = self.current_filename();
        if block.typ() != BlockType::top {
            let new_scope = block.typ() == BlockType::function_call { shadows: true };
            self.vars().push(new_scope);
        }

        let mut block_list = self.block_list.borrow_mut();
        block_list.push(block);
        block_list.len() - 1
    }

    /// Remove the outermost block, asserting it's the given one.
    pub fn pop_block(&self, expected: BlockId) {
        let block = {
            let mut block_list = self.block_list.borrow_mut();
            assert!(expected == block_list.len() - 1);
            block_list.pop().unwrap()
        };
        if block.wants_pop_env() {
            self.vars().pop();
        }
    }

    /// Return the function name for the specified stack frame. Default is one (current frame).
    pub fn get_function_name(&self, level: i32) -> Option<WString> {
        if level == 0 {
            // Return the function name for the level preceding the most recent breakpoint. If there
            // isn't one return the function name for the current level.
            // Walk until we find a breakpoint, then take the next function.
            return self
                .blocks()
                .iter()
                .rev()
                .skip_while(|b| b.typ() != BlockType::breakpoint)
                .find_map(|b| match b.data() {
                    Some(BlockData::Function { name, .. }) => Some(name.clone()),
                    _ => None,
                });
        }

        self.blocks()
            .iter()
            .rev()
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

    /// Return the job with the specified job id. If id is 0 or less, return the last job used.
    pub fn job_with_id(&self, job_id: MaybeJobId) -> Option<JobRef> {
        for job in self.jobs().iter() {
            if job_id.is_none() || job_id == job.job_id() {
                return Some(job.clone());
            }
        }
        None
    }

    /// Returns the job with the given pid.
    pub fn job_get_from_pid(&self, pid: libc::pid_t) -> Option<JobRef> {
        self.job_get_with_index_from_pid(pid).map(|t| t.1)
    }

    /// Returns the job and job index with the given pid.
    pub fn job_get_with_index_from_pid(&self, pid: libc::pid_t) -> Option<(usize, JobRef)> {
        for (i, job) in self.jobs().iter().enumerate() {
            for p in job.processes().iter() {
                if p.pid.load(Ordering::Relaxed) == pid {
                    return Some((i, job.clone()));
                }
            }
        }
        None
    }

    /// Returns a new profile item if profiling is active. The caller should fill it in.
    /// The Parser will deallocate it.
    /// If profiling is not active, this returns nullptr.
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
    pub fn emit_profiling(&self, path: &[u8]) {
        // Save profiling information. OK to not use CLO_EXEC here because this is called while fish is
        // exiting (and hence will not fork).
        let f = match std::fs::File::create(OsStr::from_bytes(path)) {
            Ok(f) => f,
            Err(err) => {
                FLOG!(
                    warning,
                    wgettext_fmt!(
                        "Could not write profiling information to file '%s': %s",
                        &String::from_utf8_lossy(path),
                        err.to_string()
                    )
                );
                return;
            }
        };
        fprintf!(f.as_raw_fd(), "Time\tSum\tCommand\n");
        print_profile(&self.profile_items.borrow(), f.as_raw_fd());
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
                wgettext_fmt!(
                    "%ls (line %lu): ",
                    user_presentable_path(&filename, self.vars()),
                    which_line
                )
            } else {
                sprintf!("%ls: ", user_presentable_path(&filename, self.vars()))
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
        self.blocks()
            .iter()
            .rev()
            .find_map(|b| match b.data() {
                Some(BlockData::Function { name, .. }) => {
                    function::get_props(name).and_then(|props| props.definition_file.clone())
                }
                Some(BlockData::Source { file }) => Some(file.clone()),
                _ => None,
            })
            .or_else(|| self.libdata().current_filename.clone())
    }

    /// Return if we are interactive, which means we are executing a command that the user typed in
    /// (and not, say, a prompt).
    pub fn is_interactive(&self) -> bool {
        self.libdata().is_interactive
    }

    /// Return a string representing the current stack trace.
    pub fn stack_trace(&self) -> WString {
        self.blocks()
            .iter()
            .rev()
            // Stop at event handler. No reason to believe that any other code is relevant.
            // It might make sense in the future to continue printing the stack trace of the code
            // that invoked the event, if this is a programmatic event, but we can't currently
            // detect that.
            .take_while(|b| b.typ() != BlockType::event)
            .fold(WString::new(), |mut trace, b| {
                append_block_description_to_stack_trace(self, b, &mut trace);
                trace
            })
    }

    /// Return whether the number of functions in the stack exceeds our stack depth limit.
    pub fn function_stack_is_overflowing(&self) -> bool {
        // We are interested in whether the count of functions on the stack exceeds
        // FISH_MAX_STACK_DEPTH. We don't separately track the number of functions, but we can have a
        // fast path through the eval_level. If the eval_level is in bounds, so must be the stack depth.
        if self.eval_level.load(Ordering::Relaxed) <= isize::try_from(FISH_MAX_STACK_DEPTH).unwrap()
        {
            return false;
        }
        // Count the functions.
        let depth = self
            .blocks()
            .iter()
            .rev()
            .filter(|b| b.is_function_call())
            .count();
        depth > FISH_MAX_STACK_DEPTH
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
        self.eval_level.load(Ordering::Relaxed) >= isize::try_from(FISH_MAX_EVAL_DEPTH).unwrap()
    }
}

// Given a file path, return something nicer. Currently we just "unexpand" tildes.
fn user_presentable_path(path: &wstr, vars: &dyn Environment) -> WString {
    replace_home_directory_with_tilde(path, vars)
}

/// Print profiling information to the specified stream.
fn print_profile(items: &[ProfileItem], out: RawFd) {
    for (idx, item) in items.iter().enumerate() {
        if item.skipped || item.cmd.is_empty() {
            continue;
        }

        let total_time = item.duration;

        // Compute the self time as the total time, minus the total time consumed by subsequent
        // items exactly one eval level deeper.
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

        fprintf!(out, "%lld\t%lld\t", self_time, total_time);
        for _i in 0..item.level {
            fprintf!(out, "-");
        }

        fprintf!(out, "> %ls\n", item.cmd);
    }
}

/// Append stack trace info for the block `b` to `trace`.
fn append_block_description_to_stack_trace(parser: &Parser, b: &Block, trace: &mut WString) {
    let mut print_call_site = false;
    match b.typ() {
        BlockType::function_call { .. } => {
            let Some(BlockData::Function { name, args, .. }) = b.data() else {
                unreachable!()
            };
            trace.push_utfstr(&wgettext_fmt!("in function '%ls'", name));
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
                    ))
                } else {
                    args_str.push_str("\"\"");
                }
            }
            if !args_str.is_empty() {
                // TODO: Escape these.
                trace.push_utfstr(&wgettext_fmt!(" with arguments '%ls'", args_str));
            }
            trace.push('\n');
            print_call_site = true;
        }
        BlockType::subst => {
            trace.push_utfstr(&wgettext!("in command substitution\n"));
            print_call_site = true;
        }
        BlockType::source => {
            let Some(BlockData::Source { file, .. }) = b.data() else {
                unreachable!()
            };
            let source_dest = file;
            trace.push_utfstr(&wgettext_fmt!(
                "from sourcing file %ls\n",
                &user_presentable_path(source_dest, parser.vars())
            ));
            print_call_site = true;
        }
        BlockType::event => {
            let Some(BlockData::Event(event)) = b.data() else {
                unreachable!()
            };
            let description = event::get_desc(parser, event);
            trace.push_utfstr(&wgettext_fmt!("in event handler: %ls\n", &description));
            print_call_site = true;
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

    if print_call_site {
        // Print where the function is called.
        if let Some(file) = b.src_filename.as_ref() {
            trace.push_utfstr(&sprintf!(
                "\tcalled on line %d of file %ls\n",
                b.src_lineno.unwrap_or(0),
                user_presentable_path(file, parser.vars())
            ));
        } else if parser.libdata().within_fish_init {
            trace.push_str("\tcalled during startup\n");
        }
    }
}

/// Types of blocks.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
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
#[derive(Clone, Copy, Eq, PartialEq)]
pub enum LoopStatus {
    /// current loop block executed as normal
    normals,
    /// current loop block should be removed
    breaks,
    /// current loop block should be skipped
    continues,
}
