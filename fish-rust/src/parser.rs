//! The fish parser.

pub struct ParseExecutionContext {}

use crate::ast::Node;
use crate::common::{CancelChecker, FilenameRef};
use crate::complete::CompletionList;
use crate::env::{EnvMode, EnvStack, Statuses};
use crate::event::Event;
use crate::expand::ExpandFlags;
use crate::fds::AutoCloseFd;
use crate::io::IoChain;
use crate::job_group::JobId;
use crate::operation_context::OperationContext;
use crate::parse_constants::{ParseErrorList, FISH_MAX_EVAL_DEPTH};
use crate::parse_tree::ParsedSourceRef;
use crate::proc::{Job, JobGroupRef, JobList, ProcStatus};
use crate::util::get_time;
use crate::wait_handle::WaitHandleStore;
use crate::wchar::{wstr, WString};
use std::collections::VecDeque;
use std::rc::Rc;
use std::sync::RwLock;

/// block_t represents a block of commands.
pub struct Block {
    /// If this is a function block, the function name. Otherwise empty.
    pub function_name: WString,

    /// List of event blocks.
    pub event_blocks: u64,

    /// If this is a function block, the function args. Otherwise empty.
    pub function_args: Vec<WString>,

    /// Name of file that created this block.
    pub src_filename: FilenameRef,

    // If this is an event block, the event. Otherwise ignored.
    pub event: Rc<Event>,

    // If this is a source block, the source'd file, interned.
    // Otherwise nothing.
    pub source_file: FilenameRef,

    /// Line number where this block was created.
    pub src_lineno: i32,

    /// Type of block.
    block_type: BlockType,

    /// Whether we should pop the environment variable stack when we're popped off of the block
    /// stack.
    pub wants_pop_env: bool,
}

impl Block {
    /// Construct from a block type.
    pub fn new(t: BlockType) -> Self {
        todo!()
    }

    /// Description of the block, for debugging.
    pub fn description(&self) -> WString {
        todo!()
    }

    pub fn typ(&self) -> BlockType {
        self.block_type
    }

    /// \return if we are a function call (with or without shadowing).
    pub fn is_function_call(&self) -> bool {
        todo!()
    }

    /// Entry points for creating blocks.
    pub fn if_block() -> Block {
        todo!()
    }
    pub fn event_block(event: Event) -> Block {
        todo!()
    }
    pub fn function_block(name: WString, args: Vec<WString>, shadows: bool) -> Block {
        todo!()
    }
    pub fn source_block(src: FilenameRef) -> Block {
        todo!()
    }
    pub fn for_block() -> Block {
        todo!()
    }
    pub fn while_block() -> Block {
        todo!()
    }
    pub fn switch_block() -> Block {
        todo!()
    }
    pub fn scope_block() -> Block {
        todo!()
    }
    pub fn breakpoint_block() -> Block {
        todo!()
    }
    pub fn variable_assignment_block() -> Block {
        todo!()
    }
}

type Microseconds = i64;

pub struct ProfileItem {
    /// Time spent executing the command, including nested blocks.
    pub duration: Microseconds,

    /// The block level of the specified command. Nested blocks and command substitutions both
    /// increase the block level.
    pub level: usize,

    /// If the execution of this command was skipped.
    pub skipped: bool,

    /// The command string.
    pub cmd: WString,
}

impl ProfileItem {
    /// \return the current time as a microsecond timestamp since the epoch.
    pub fn now() -> Microseconds {
        get_time()
    }
}

/// Miscellaneous data used to avoid recursion and others.
pub struct LibraryData {
    pub pods: library_data_pod_t,

    /// The current filename we are evaluating, either from builtin source or on the command line.
    pub current_filename: FilenameRef,

    /// A stack of fake values to be returned by builtin_commandline. This is used by the completion
    /// machinery when wrapping: e.g. if `tig` wraps `git` then git completions need to see git on
    /// the command line.
    pub transient_commandlines: Vec<WString>,

    /// A file descriptor holding the current working directory, for use in openat().
    /// This is never null and never invalid.
    pub cwd_fd: Rc<AutoCloseFd>,

    pub status_vars: StatusVars,
}

/// Status variables set by the main thread as jobs are parsed and read by various consumers.
pub struct StatusVars {
    /// Used to get the head of the current job (not the current command, at least for now)
    /// for `status current-command`.
    command: WString,
    /// Used to get the full text of the current job for `status current-commandline`.
    commandline: WString,
}

/// The result of parser_t::eval family.
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

pub struct Parser {
    /// The current execution context.
    execution_context: Box<ParseExecutionContext>,

    /// The jobs associated with this parser.
    job_list: JobList,

    /// Our store of recorded wait-handles. These are jobs that finished in the background,
    /// and have been reaped, but may still be wait'ed on.
    wait_handles: WaitHandleStore,

    /// The list of blocks. This is a deque because we give out raw pointers to callers, who hold
    /// them across manipulating this stack.
    /// This is in "reverse" order: the topmost block is at the front. This enables iteration from
    /// top down using range-based for loops.
    block_list: VecDeque<Block>,

    /// The 'depth' of the fish call stack.
    eval_level: i32,

    /// Set of variables for the parser.
    variables: Rc<EnvStack>,

    /// Miscellaneous library data.
    library_data: LibraryData,

    /// If set, we synchronize universal variables after external commands,
    /// including sending on-variable change events.
    syncs_uvars: bool,

    /// If set, we are the principal parser.
    is_principal: bool,

    /// List of profile items.
    /// This must be a deque because we return pointers to them to callers,
    /// who may hold them across blocks (which would cause reallocations internal
    /// to profile_items). deque does not move items on reallocation.
    profile_items: VecDeque<ProfileItem>,

    /// Global event blocks.
    pub global_event_blocks: u64,
}

impl Parser {
    /// Adds a job to the beginning of the job list.
    fn job_add(&mut self, job: Rc<Job>) {
        todo!()
    }

    /// \return whether we are currently evaluating a function.
    fn is_function(&self) -> bool {
        todo!()
    }

    /// \return whether we are currently evaluating a command substitution.
    fn is_command_substitution(&self) -> bool {
        todo!()
    }

    /// Create a parser
    fn new(vars: Rc<EnvStack>, is_principal: bool) -> Self {
        todo!()
    }

    /// Get the "principal" parser, whatever that is.
    pub fn principal_parser() -> &'static mut Parser {
        todo!()
    }

    /// Assert that this parser is allowed to execute on the current thread.
    pub fn assert_can_execute(&self) {
        todo!()
    }

    pub fn eval(&mut self, cmd: &wstr, io: &IoChain) -> EvalRes {
        todo!()
    }

    /// Evaluate the expressions contained in cmd.
    ///
    /// \param cmd the string to evaluate
    /// \param io io redirections to perform on all started jobs
    /// \param job_group if set, the job group to give to spawned jobs.
    /// \param block_type The type of block to push on the block stack, which must be either 'top'
    /// or 'subst'.
    /// \return the result of evaluation.
    pub fn eval_with(
        &mut self,
        cmd: &wstr,
        io: &IoChain,
        job_group: &Option<JobGroupRef>,
        block_type: BlockType,
    ) -> EvalRes {
        todo!()
    }

    /// Evaluate the parsed source ps.
    /// Because the source has been parsed, a syntax error is impossible.
    pub fn eval_parsed_source(
        &mut self,
        ps: &ParsedSourceRef,
        io: IoChain,
        job_group: Option<JobGroupRef>,
        block_type: BlockType,
    ) -> EvalRes {
        todo!()
    }

    /// Evaluates a node.
    /// The node type must be ast_t::statement_t or ast::job_list_t.
    pub fn eval_node<T: Node>(
        &mut self,
        ps: &ParsedSourceRef,
        node: &T,
        block_io: &IoChain,
        job_group: &Option<JobGroupRef>,
        block_type: BlockType,
    ) -> EvalRes {
        todo!()
    }

    /// Evaluate line as a list of parameters, i.e. tokenize it and perform parameter expansion and
    /// cmdsubst execution on the tokens. Errors are ignored. If a parser is provided, it is used
    /// for command substitution expansion.
    pub fn expand_argument_list(
        arg_list_src: &wstr,
        flags: ExpandFlags,
        ctx: OperationContext<'_>,
    ) -> CompletionList {
        todo!()
    }

    /// Returns a string describing the current parser position in the format 'FILENAME (line
    /// LINE_NUMBER): LINE'. Example:
    ///
    /// init.fish (line 127): ls|grep pancake
    pub fn current_line(&mut self) -> WString {
        todo!()
    }

    /// Returns the current line number.
    pub fn get_lineno(&self) -> usize {
        todo!()
    }

    /// \return whether we are currently evaluating a "block" such as an if statement.
    /// This supports 'status is-block'.
    pub fn is_block(&self) -> bool {
        todo!()
    }

    /// \return whether we have a breakpoint block.
    pub fn is_breakpoint(&self) -> bool {
        todo!()
    }

    /// Returns the block at the given index. 0 corresponds to the innermost block. Returns nullptr
    /// when idx is at or equal to the number of blocks.
    pub fn block_at_index(&self, idx: usize) -> Option<&Block> {
        todo!()
    }
    pub fn block_at_index_mut(&mut self, idx: usize) -> &mut Block {
        todo!()
    }

    /// Return the list of blocks. The first block is at the top.
    pub fn blocks(&self) -> &VecDeque<Block> {
        &self.block_list
    }

    pub fn blocks_size(&self) -> usize {
        self.block_list.len()
    }

    /// Get the list of jobs.
    pub fn jobs(&self) -> &JobList {
        &self.job_list
    }
    pub fn jobs_mut(&mut self) -> &mut JobList {
        &mut self.job_list
    }

    /// Get the variables.
    pub fn vars(&self) -> &EnvStack {
        &self.variables
    }
    pub fn vars_mut(&mut self) -> &mut EnvStack {
        todo!()
    }

    /// Get the library data.
    pub fn libdata(&self) -> &LibraryData {
        &self.library_data
    }
    pub fn libdata_mut(&mut self) -> &mut LibraryData {
        &mut self.library_data
    }

    /// Get our wait handle store.
    pub fn get_wait_handles(&self) -> &WaitHandleStore {
        &self.wait_handles
    }
    pub fn mut_wait_handles(&mut self) -> &mut WaitHandleStore {
        &mut self.wait_handles
    }

    /// Get and set the last proc statuses.
    pub fn get_last_status(&self) -> i32 {
        todo!()
    }
    pub fn get_last_statuses(&self) -> Statuses {
        todo!()
    }
    pub fn set_last_statuses(&mut self, s: Statuses) {
        todo!()
    }

    /// Cover of vars().set(), which also fires any returned event handlers.
    /// \return a value like ENV_OK.
    pub fn set_var_and_fire(&mut self, key: &wstr, mode: EnvMode, val: Vec<WString>) -> i32 {
        todo!()
    }

    /// Update any universal variables and send event handlers.
    /// If \p always is set, then do it even if we have no pending changes (that is, look for
    /// changes from other fish instances); otherwise only sync if this instance has changed uvars.
    pub fn sync_uvars_and_fire(&mut self, always: bool) {
        todo!()
    }

    /// Pushes a new block. Returns a pointer to the block, stored in the parser. The pointer is
    /// valid until the call to pop_block().
    pub fn push_block(&mut self, b: Block) -> BlockId {
        todo!()
    }

    /// Remove the outermost block, asserting it's the given one.
    pub fn pop_block(&mut self, expected: BlockId) {
        todo!()
    }

    /// Return the function name for the specified stack frame. Default is one (current frame).
    pub fn get_function_name(&self, level: i32) -> Option<WString> {
        todo!()
    }

    /// Promotes a job to the front of the list.
    pub fn job_promote(&mut self, job: &Job) {
        todo!()
    }
    pub fn job_promote_at(&mut self, job_pos: usize) {
        todo!()
    }

    /// Return the job with the specified job id. If id is 0 or less, return the last job used.
    pub fn job_with_id(&self, job_id: JobId) -> &Job {
        todo!()
    }

    /// Returns the job with the given pid.
    pub fn job_get_from_pid(&self, pid: libc::pid_t) -> &Job {
        todo!()
    }

    /// Returns the job and position with the given pid.
    pub fn job_get_from_pid_at(&self, pid: i64, job_pos: &mut usize) -> &Job {
        todo!()
    }

    /// Returns a new profile item if profiling is active. The caller should fill it in.
    /// The parser_t will deallocate it.
    /// If profiling is not active, this returns nullptr.
    pub fn create_profile_item(&mut self) -> ProfileItem {
        todo!()
    }

    /// Remove the profiling items.
    pub fn clear_profiling(&mut self) {
        todo!()
    }

    /// Output profiling data to the given filename.
    pub fn emit_profiling(&self, path: &str) {
        todo!()
    }

    pub fn get_backtrace(src: &wstr, errors: &ParseErrorList) -> WString {
        todo!()
    }

    /// Returns the file currently evaluated by the parser. This can be different than
    /// reader_current_filename, e.g. if we are evaluating a function defined in a different file
    /// than the one currently read.
    pub fn current_filename(&self) -> FilenameRef {
        todo!()
    }

    /// Return if we are interactive, which means we are executing a command that the user typed in
    /// (and not, say, a prompt).
    pub fn is_interactive(&self) -> bool {
        self.libdata().pods.is_interactive
    }

    /// Return a string representing the current stack trace.
    pub fn stack_trace(&self) -> WString {
        todo!()
    }

    /// \return whether the number of functions in the stack exceeds our stack depth limit.
    pub fn function_stack_is_overflowing(&self) -> bool {
        todo!()
    }

    /// Mark whether we should sync universal variables.
    pub fn set_syncs_uvars(&mut self, flag: bool) {
        self.syncs_uvars = flag;
    }

    /// \return a shared pointer reference to this parser.
    // TODO this probably needs to be a free function in rust
    pub fn shared(&mut self) -> Rc<RwLock<Parser>> {
        todo!()
    }

    /// \return a cancel poller for checking if this parser has been signalled.
    /// autocxx falls over with this so hide it.
    pub fn cancel_checker(&self) -> &CancelChecker {
        todo!()
    }

    /// \return the operation context for this parser.
    pub fn context(&mut self) -> OperationContext<'_> {
        todo!()
    }

    /// Checks if the max eval depth has been exceeded
    pub fn is_eval_depth_exceeded(&self) -> bool {
        self.eval_level as usize >= FISH_MAX_EVAL_DEPTH
    }
}

pub use parser_ffi::{library_data_pod_t, BlockType, LoopStatus};

#[cxx::bridge]
mod parser_ffi {
    extern "C++" {}

    /// Types of blocks.
    #[cxx_name = "block_type_t"]
    pub enum BlockType {
        /// While loop block
        while_block,
        /// For loop block
        for_block,
        /// If block
        if_block,
        /// Function invocation block
        function_call,
        /// Function invocation block with no variable shadowing
        function_call_no_shadow,
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
    pub enum LoopStatus {
        /// current loop block executed as normal
        normals,
        /// current loop block should be removed
        breaks,
        /// current loop block should be skipped
        continues,
    }

    /// Plain-Old-Data components of `struct library_data_t` that can be shared over FFI
    pub struct library_data_pod_t {
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
}
