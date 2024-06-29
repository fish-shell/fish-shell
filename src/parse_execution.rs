//! Provides the "linkage" between an ast and actual execution structures (job_t, etc.).

use crate::ast::{
    self, BlockStatementHeaderVariant, Keyword, Leaf, List, Node, StatementVariant, Token,
};
use crate::builtins;
use crate::builtins::shared::{
    builtin_exists, BUILTIN_ERR_VARNAME, STATUS_CMD_ERROR, STATUS_CMD_OK, STATUS_CMD_UNKNOWN,
    STATUS_EXPAND_ERROR, STATUS_ILLEGAL_CMD, STATUS_INVALID_ARGS, STATUS_NOT_EXECUTABLE,
    STATUS_UNMATCHED_WILDCARD,
};
use crate::common::{
    escape, scoped_push_replacer, should_suppress_stderr_for_tests, truncate_at_nul,
    valid_var_name, ScopeGuard, ScopeGuarding,
};
use crate::complete::CompletionList;
use crate::env::{EnvMode, EnvStackSetResult, EnvVar, EnvVarFlags, Environment, Statuses};
use crate::event::{self, Event};
use crate::exec::exec_job;
use crate::expand::{
    expand_one, expand_string, expand_to_command_and_args, ExpandFlags, ExpandResultCode,
};
use crate::flog::FLOG;
use crate::function;
use crate::io::{IoChain, IoStreams, OutputStream, StringOutputStream};
use crate::job_group::JobGroup;
use crate::operation_context::OperationContext;
use crate::parse_constants::{
    parse_error_offset_source_start, ParseError, ParseErrorCode, ParseErrorList, ParseKeyword,
    ParseTokenType, StatementDecoration, CALL_STACK_LIMIT_EXCEEDED_ERR_MSG,
    ERROR_NO_BRACE_GROUPING, ERROR_TIME_BACKGROUND, FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG,
    ILLEGAL_FD_ERR_MSG, INFINITE_FUNC_RECURSION_ERR_MSG, WILDCARD_ERR_MSG,
};
use crate::parse_tree::{LineCounter, NodeRef, ParsedSourceRef};
use crate::parse_util::parse_util_unescape_wildcards;
use crate::parser::{Block, BlockData, BlockId, BlockType, LoopStatus, Parser, ProfileItem};
use crate::parser_keywords::parser_keywords_is_subcommand;
use crate::path::{path_as_implicit_cd, path_try_get_path};
use crate::proc::{
    get_job_control_mode, job_reap, no_exec, ConcreteAssignment, Job, JobControl, JobProperties,
    JobRef, Process, ProcessList, ProcessType,
};
use crate::reader::fish_is_unwinding_for_exit;
use crate::redirection::{RedirectionMode, RedirectionSpec, RedirectionSpecList};
use crate::signal::Signal;
use crate::timer::push_timer;
use crate::tokenizer::{variable_assignment_equals_pos, PipeOrRedir};
use crate::trace::{trace_if_enabled, trace_if_enabled_with_args};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wildcard::wildcard_match;
use crate::wutil::{wgettext, wgettext_maybe_fmt};
use libc::{c_int, ENOTDIR, EXIT_SUCCESS, STDERR_FILENO, STDOUT_FILENO};
use std::cell::RefCell;
use std::io::ErrorKind;
use std::rc::Rc;
use std::sync::{atomic::Ordering, Arc};

/// An eval_result represents evaluation errors including wildcards which failed to match, syntax
/// errors, or other expansion errors. It also tracks when evaluation was skipped due to signal
/// cancellation. Note it does not track the exit status of commands.
#[derive(Eq, PartialEq)]
pub enum EndExecutionReason {
    /// Evaluation was successfull.
    ok,

    /// Evaluation was skipped due to control flow (break or return).
    control_flow,

    /// Evaluation was cancelled, e.g. because of a signal or exit.
    cancelled,

    /// A parse error or failed expansion (but not an error exit status from a command).
    error,
}

pub struct ExecutionContext {
    // The parsed source and its AST.
    pstree: ParsedSourceRef,

    // If set, one of our processes received a cancellation signal (INT or QUIT) so we are
    // unwinding.
    cancel_signal: Option<Signal>,

    // Helper to count lines.
    // This is shared with the Parser so that the Parser can access the current line.
    line_counter: Rc<RefCell<LineCounter<ast::JobPipeline>>>,

    /// The block IO chain.
    /// For example, in `begin; foo ; end < file.txt` this would have the 'file.txt' IO.
    block_io: IoChain,
}

// Report an error, setting $status to `status`. Always returns
// 'end_execution_reason_t::error'.
macro_rules! report_error {
    ( $self:ident, $ctx:expr, $status:expr, $node:expr, $fmt:expr $(, $arg:expr )* $(,)? ) => {
        report_error_formatted!($self, $ctx, $status, $node, wgettext_maybe_fmt!($fmt $(, $arg )*))
    };
}
macro_rules! report_error_formatted {
    ( $self:ident, $ctx:expr, $status:expr, $node:expr, $text:expr $(,)? ) => {{
        let r = $node.source_range();
        // Create an error.
        let mut error = ParseError::default();
        error.source_start = r.start();
        error.source_length = r.length();
        error.code = ParseErrorCode::syntax; // hackish
        error.text = $text;
        $self.report_errors($ctx, $status, &vec![error])
    }};
}

impl<'a> ExecutionContext {
    /// Construct a context in preparation for evaluating a node in a tree, with the given block_io.
    /// The execution context may access the parser and parent job group (if any) through ctx.
    pub fn new(
        pstree: ParsedSourceRef,
        block_io: IoChain,
        line_counter: Rc<RefCell<LineCounter<ast::JobPipeline>>>,
    ) -> Self {
        Self {
            pstree,
            cancel_signal: None,
            line_counter,
            block_io,
        }
    }

    pub fn pstree(&self) -> &ParsedSourceRef {
        &self.pstree
    }

    pub fn eval_node(
        &mut self,
        ctx: &OperationContext<'_>,
        node: &dyn Node,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        match node.typ() {
            ast::Type::statement => {
                self.eval_statement(ctx, node.as_statement().unwrap(), associated_block)
            }
            ast::Type::job_list => {
                self.eval_job_list(ctx, node.as_job_list().unwrap(), associated_block.unwrap())
            }
            _ => unreachable!(),
        }
    }

    /// Start executing at the given node. Returns 0 if there was no error, 1 if there was an
    /// error.
    fn eval_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        statement: &'a ast::Statement,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        // Note we only expect block-style statements here. No not statements.
        let contents = &statement.contents;
        match &**contents {
            StatementVariant::BlockStatement(block) => {
                self.run_block_statement(ctx, block, associated_block)
            }
            StatementVariant::IfStatement(ifstat) => {
                self.run_if_statement(ctx, ifstat, associated_block)
            }
            StatementVariant::SwitchStatement(switchstat) => {
                self.run_switch_statement(ctx, switchstat)
            }
            StatementVariant::DecoratedStatement(_)
            | StatementVariant::NotStatement(_)
            | StatementVariant::None => panic!(),
        }
    }

    fn eval_job_list(
        &mut self,
        ctx: &OperationContext<'_>,
        job_list: &'a ast::JobList,
        associated_block: BlockId,
    ) -> EndExecutionReason {
        // Check for infinite recursion: a function which immediately calls itself..
        let mut func_name = WString::new();
        if let Some(infinite_recursive_node) =
            self.infinite_recursive_statement_in_job_list(ctx, job_list, &mut func_name)
        {
            // We have an infinite recursion.
            return report_error!(
                self,
                ctx,
                STATUS_CMD_ERROR.unwrap(),
                infinite_recursive_node,
                INFINITE_FUNC_RECURSION_ERR_MSG,
                func_name
            );
        }

        // Check for stack overflow in case of function calls (regular stack overflow) or string
        // substitution blocks, which can be recursively called with eval (issue #9302).
        let block_type = {
            let blocks = ctx.parser().blocks();
            blocks.get(associated_block).unwrap().typ()
        };
        if (block_type == BlockType::top && ctx.parser().function_stack_is_overflowing())
            || (block_type == BlockType::subst && ctx.parser().is_eval_depth_exceeded())
        {
            return report_error!(
                self,
                ctx,
                STATUS_CMD_ERROR.unwrap(),
                job_list,
                CALL_STACK_LIMIT_EXCEEDED_ERR_MSG
            );
        }
        self.run_job_list(ctx, job_list, Some(associated_block))
    }

    // Check to see if we should end execution.
    // Return the eval result to end with, or none() to continue on.
    // This will never return end_execution_reason_t::ok.
    fn check_end_execution(&self, ctx: &OperationContext<'_>) -> Option<EndExecutionReason> {
        // If one of our jobs ended with SIGINT, we stop execution.
        // Likewise if fish itself got a SIGINT, or if something ran exit, etc.
        if self.cancel_signal.is_some() || ctx.check_cancel() || fish_is_unwinding_for_exit() {
            return Some(EndExecutionReason::cancelled);
        }
        let parser = ctx.parser();
        let ld = &parser.libdata();
        if ld.exit_current_script {
            return Some(EndExecutionReason::cancelled);
        }
        if ld.returning {
            return Some(EndExecutionReason::control_flow);
        }
        if ld.loop_status != LoopStatus::normals {
            return Some(EndExecutionReason::control_flow);
        }
        None
    }

    fn report_errors(
        &self,
        ctx: &OperationContext<'_>,
        status: c_int,
        error_list: &ParseErrorList,
    ) -> EndExecutionReason {
        if !ctx.check_cancel() {
            if error_list.is_empty() {
                FLOG!(error, "Error reported but no error text found.");
            }

            // Get a backtrace.
            let backtrace_and_desc = ctx.parser().get_backtrace(&self.pstree().src, error_list);

            // Print it.
            if !should_suppress_stderr_for_tests() {
                eprintf!("%s", backtrace_and_desc);
            }

            // Mark status.
            ctx.parser().set_last_statuses(Statuses::just(status));
        }
        EndExecutionReason::error
    }

    /// Command not found support.
    fn handle_command_not_found(
        &mut self,
        ctx: &OperationContext<'_>,
        cmd: &wstr,
        statement: &ast::DecoratedStatement,
        err: std::io::Error,
    ) -> EndExecutionReason {
        // We couldn't find the specified command. This is a non-fatal error. We want to set the exit
        // status to 127, which is the standard number used by other shells like bash and zsh.

        if err.kind() != ErrorKind::NotFound {
            // TODO: We currently handle all errors here the same,
            // but this mainly applies to EACCES. We could also feasibly get:
            // ELOOP
            // ENAMETOOLONG
            if err.raw_os_error() == Some(ENOTDIR) {
                // If the original command did not include a "/", assume we found it via $PATH.
                let src = self.node_source(&statement.command);
                if !src.contains('/') {
                    return report_error!(
                        self,
                        ctx,
                        STATUS_NOT_EXECUTABLE.unwrap(),
                        &statement.command,
                        concat!(
                            "Unknown command. A component of '%ls' is not a ",
                            "directory. Check your $PATH."
                        ),
                        cmd
                    );
                } else {
                    return report_error!(
                        self,
                        ctx,
                        STATUS_NOT_EXECUTABLE.unwrap(),
                        &statement.command,
                        "Unknown command. A component of '%ls' is not a directory.",
                        cmd
                    );
                }
            }

            return report_error!(
                self,
                ctx,
                STATUS_NOT_EXECUTABLE.unwrap(),
                &statement.command,
                "Unknown command. '%ls' exists but is not an executable file.",
                cmd
            );
        }

        // Handle unrecognized commands with standard command not found handler that can make better
        // error messages.
        let mut event_args = vec![];
        {
            let args = Self::get_argument_nodes_no_redirs(&statement.args_or_redirs);
            let arg_result =
                self.expand_arguments_from_nodes(ctx, &args, &mut event_args, Globspec::failglob);
            if arg_result != EndExecutionReason::ok {
                return arg_result;
            }

            event_args.insert(0, cmd.to_owned());
        }

        let mut error = WString::new();

        // Redirect to stderr
        let mut io = IoChain::new();
        let mut list = RedirectionSpecList::new();
        list.push(RedirectionSpec::new(
            STDOUT_FILENO,
            RedirectionMode::fd,
            L!("2").to_owned(),
        ));
        io.append_from_specs(&list, L!(""));

        if function::exists(L!("fish_command_not_found"), ctx.parser()) {
            let mut buffer = L!("fish_command_not_found").to_owned();
            for arg in &event_args {
                buffer.push(' ');
                buffer.push_utfstr(&escape(arg));
            }
            let parser = ctx.parser();
            let prev_statuses = parser.get_last_statuses();

            let event = Event::generic(L!("fish_command_not_found").to_owned());
            let b = parser.push_block(Block::event_block(event));
            parser.eval(&buffer, &io);
            parser.pop_block(b);
            parser.set_last_statuses(prev_statuses);
        } else {
            // If we have no handler, just print it as a normal error.
            error = wgettext!("Unknown command:").to_owned();
            if !event_args.is_empty() {
                error.push(' ');
                error.push_utfstr(&escape(&event_args[0]));
            }
        }

        if cmd.as_char_slice().first() == Some(&'{' /*}*/) {
            error.push_utfstr(&wgettext!(ERROR_NO_BRACE_GROUPING));
        }

        // Here we want to report an error (so it shows a backtrace).
        // If the handler printed text, that's already shown, so error will be empty.
        report_error_formatted!(
            self,
            ctx,
            STATUS_CMD_UNKNOWN.unwrap(),
            &statement.command,
            error
        )
    }

    // Utilities
    fn node_source(&self, node: &dyn ast::Node) -> WString {
        // todo!("maybe don't copy")
        node.source(&self.pstree().src).to_owned()
    }

    fn infinite_recursive_statement_in_job_list<'b>(
        &self,
        ctx: &OperationContext<'_>,
        jobs: &'b ast::JobList,
        out_func_name: &mut WString,
    ) -> Option<&'b ast::DecoratedStatement> {
        // This is a bit fragile. It is a test to see if we are inside of function call, but
        // not inside a block in that function call. If, in the future, the rules for what
        // block scopes are pushed on function invocation changes, then this check will break.
        let parser = ctx.parser();
        let parent;
        let parent_fn_name = {
            match (parser.block_at_index(0), parser.block_at_index(1)) {
                (Some(current), Some(p)) if current.typ() == BlockType::top => {
                    parent = p;
                    match parent.data() {
                        Some(BlockData::Function { name, .. }) => name,
                        _ => return None,
                    }
                }
                _ => return None, // Not within function call.
            }
        };

        // Get the function name of the immediate block.
        let forbidden_function_name = parent_fn_name;

        // Get the first job in the job list.
        let jc = &jobs.get(0)?;
        let job = &jc.job;

        // Helper to return if a statement is infinitely recursive in this function.
        let statement_recurses = |stat: &'b ast::Statement| -> Option<&'b ast::DecoratedStatement> {
            // Ignore non-decorated statements like `if`, etc.
            let StatementVariant::DecoratedStatement(dc) = &*stat.contents else {
                return None;
            };

            // Ignore statements with decorations like 'builtin' or 'command', since those
            // are not infinite recursion. In particular that is what enables 'wrapper functions'.
            if dc.decoration() != StatementDecoration::none {
                return None;
            }

            // Check the command.
            let mut cmd = self.node_source(&dc.command);
            let forbidden = !cmd.is_empty()
                && expand_one(
                    &mut cmd,
                    ExpandFlags::FAIL_ON_CMDSUBST | ExpandFlags::SKIP_VARIABLES,
                    ctx,
                    None,
                )
                && &cmd == forbidden_function_name;
            if forbidden {
                Some(dc)
            } else {
                None
            }
        };

        // Check main statement.
        let infinite_recursive_statement = statement_recurses(&jc.job.statement)
            // Check piped remainder.
            .or_else(|| {
                for c in &job.continuation {
                    let s = statement_recurses(&c.statement);
                    if s.is_some() {
                        return s;
                    }
                }
                None
            });

        if infinite_recursive_statement.is_some() {
            *out_func_name = forbidden_function_name.to_owned();
        }

        // may be none
        infinite_recursive_statement
    }

    // Expand a command which may contain variables, producing an expand command and possibly
    // arguments. Prints an error message on error.
    fn expand_command(
        &mut self,
        ctx: &OperationContext<'_>,
        statement: &ast::DecoratedStatement,
        out_cmd: &mut WString,
        out_args: &mut Vec<WString>,
    ) -> EndExecutionReason {
        // Here we're expanding a command, for example $HOME/bin/stuff or $randomthing. The first
        // completion becomes the command itself, everything after becomes arguments. Command
        // substitutions are not supported.
        let mut errors = ParseErrorList::new();

        // Get the unexpanded command string. We expect to always get it here.
        // todo!("remove clone")
        let unexp_cmd = self.node_source(&statement.command);
        let pos_of_command_token = statement.command.range().unwrap().start();

        // Expand the string to produce completions, and report errors.
        let expand_err = expand_to_command_and_args(
            &unexp_cmd,
            ctx,
            out_cmd,
            Some(out_args),
            Some(&mut errors),
            false,
        );
        match expand_err.result {
            ExpandResultCode::error => {
                // Issue #5812 - the expansions were done on the command token,
                // excluding prefixes such as " " or "if ".
                // This means that the error positions are relative to the beginning
                // of the token; we need to make them relative to the original source.
                parse_error_offset_source_start(&mut errors, pos_of_command_token);
                return self.report_errors(ctx, STATUS_ILLEGAL_CMD.unwrap(), &errors);
            }
            ExpandResultCode::wildcard_no_match => {
                return report_error!(
                    self,
                    ctx,
                    STATUS_UNMATCHED_WILDCARD.unwrap(),
                    statement,
                    WILDCARD_ERR_MSG,
                    &self.node_source(statement)
                );
            }
            ExpandResultCode::cancel => {
                return EndExecutionReason::cancelled;
            }
            ExpandResultCode::ok => {}
        }

        // Complain if the resulting expansion was empty, or expanded to an empty string.
        // For no-exec it's okay, as we can't really perform the expansion.
        if out_cmd.is_empty() && !no_exec() {
            return report_error!(
                self,
                ctx,
                STATUS_ILLEGAL_CMD.unwrap(),
                &statement.command,
                "The expanded command was empty."
            );
        }
        // Complain if we've expanded to a subcommand keyword like "command" or "if",
        // because these will call the corresponding *builtin*,
        // which won't be doing what the user asks for
        //
        // (skipping in no-exec because we don't have the actual variable value)
        if !no_exec() && parser_keywords_is_subcommand(out_cmd) && &unexp_cmd != out_cmd {
            return report_error!(
                self,
                ctx,
                STATUS_ILLEGAL_CMD.unwrap(),
                &statement.command,
                "The expanded command is a keyword."
            );
        }
        EndExecutionReason::ok
    }

    /// Indicates whether a job is a simple block (one block, no redirections).
    fn job_is_simple_block(&self, job: &ast::JobPipeline) -> bool {
        // Must be no pipes.
        if !job.continuation.is_empty() {
            return false;
        }

        // Helper to check if an argument_or_redirection_list_t has no redirections.
        let no_redirs =
            |list: &ast::ArgumentOrRedirectionList| !list.iter().any(|val| val.is_redirection());

        // Check if we're a block statement with redirections. We do it this obnoxious way to preserve
        // type safety (in case we add more specific statement types).
        match &*job.statement.contents {
            StatementVariant::BlockStatement(stmt) => no_redirs(&stmt.args_or_redirs),
            StatementVariant::SwitchStatement(stmt) => no_redirs(&stmt.args_or_redirs),
            StatementVariant::IfStatement(stmt) => no_redirs(&stmt.args_or_redirs),
            StatementVariant::NotStatement(_) | StatementVariant::DecoratedStatement(_) => {
                // not block statements
                false
            }
            StatementVariant::None => panic!(),
        }
    }

    fn process_type_for_command(
        &self,
        ctx: &OperationContext<'_>,
        statement: &ast::DecoratedStatement,
        cmd: &wstr,
    ) -> ProcessType {
        // Determine the process type, which depends on the statement decoration (command, builtin,
        // etc).
        match statement.decoration() {
            StatementDecoration::exec => ProcessType::exec,
            StatementDecoration::command => ProcessType::external,
            StatementDecoration::builtin => ProcessType::builtin,
            StatementDecoration::none => {
                if function::exists(cmd, ctx.parser()) {
                    ProcessType::function
                } else if builtin_exists(cmd) {
                    ProcessType::builtin
                } else {
                    ProcessType::external
                }
            }
        }
    }

    fn apply_variable_assignments(
        &mut self,
        ctx: &OperationContext<'_>,
        mut proc: Option<&mut Process>,
        variable_assignment_list: &ast::VariableAssignmentList,
        block: &mut Option<BlockId>,
    ) -> EndExecutionReason {
        if variable_assignment_list.is_empty() {
            return EndExecutionReason::ok;
        }
        *block = Some(ctx.parser().push_block(Block::variable_assignment_block()));
        for variable_assignment in variable_assignment_list {
            let source = self.node_source(&**variable_assignment);
            let equals_pos = variable_assignment_equals_pos(&source).unwrap();
            let variable_name = &source[..equals_pos];
            let expression = &source[equals_pos + 1..];
            let mut expression_expanded = vec![];
            let mut errors = ParseErrorList::new();
            // TODO this is mostly copied from expand_arguments_from_nodes, maybe extract to function
            let expand_ret = expand_string(
                expression.to_owned(),
                &mut expression_expanded,
                ExpandFlags::default(),
                ctx,
                Some(&mut errors),
            );
            parse_error_offset_source_start(
                &mut errors,
                variable_assignment.range().unwrap().start() + equals_pos + 1,
            );
            match expand_ret.result {
                ExpandResultCode::error => {
                    return self.report_errors(ctx, expand_ret.status, &errors);
                }
                ExpandResultCode::cancel => {
                    return EndExecutionReason::cancelled;
                }
                ExpandResultCode::wildcard_no_match // nullglob (equivalent to set)
                    | ExpandResultCode::ok => {}
            }
            let vals: Vec<_> = expression_expanded
                .into_iter()
                .map(|comp| comp.completion)
                .collect();
            if let Some(proc) = &mut proc {
                proc.variable_assignments.push(ConcreteAssignment::new(
                    variable_name.to_owned(),
                    vals.clone(),
                ));
            }
            ctx.parser()
                .set_var_and_fire(variable_name, EnvMode::LOCAL | EnvMode::EXPORT, vals);
        }
        EndExecutionReason::ok
    }

    // These create process_t structures from statements.
    fn populate_job_process(
        &mut self,
        ctx: &OperationContext<'_>,
        job: &mut Job,
        proc: &mut Process,
        statement: &ast::Statement,
        variable_assignments: &ast::VariableAssignmentList,
    ) -> EndExecutionReason {
        // Get the "specific statement" which is boolean / block / if / switch / decorated.
        let specific_statement = &statement.contents;

        let mut block = None;
        let result =
            self.apply_variable_assignments(ctx, Some(proc), variable_assignments, &mut block);
        let _scope = ScopeGuard::new((), |()| {
            if let Some(block) = block {
                ctx.parser().pop_block(block);
            }
        });
        if result != EndExecutionReason::ok {
            return result;
        }

        match &**specific_statement {
            StatementVariant::NotStatement(not_statement) => {
                self.populate_not_process(ctx, job, proc, not_statement)
            }
            StatementVariant::BlockStatement(_)
            | StatementVariant::IfStatement(_)
            | StatementVariant::SwitchStatement(_) => {
                self.populate_block_process(ctx, proc, statement, specific_statement)
            }
            StatementVariant::DecoratedStatement(decorated_statement) => {
                self.populate_plain_process(ctx, proc, decorated_statement)
            }
            StatementVariant::None => panic!(),
        }
    }

    fn populate_not_process(
        &mut self,
        ctx: &OperationContext<'_>,
        job: &mut Job,
        proc: &mut Process,
        not_statement: &ast::NotStatement,
    ) -> EndExecutionReason {
        {
            let mut flags = job.mut_flags();
            flags.negate = !flags.negate;
        }
        self.populate_job_process(
            ctx,
            job,
            proc,
            &not_statement.contents,
            &not_statement.variables,
        )
    }

    /// Creates a 'normal' (non-block) process.
    fn populate_plain_process(
        &mut self,
        ctx: &OperationContext<'_>,
        proc: &mut Process,
        statement: &ast::DecoratedStatement,
    ) -> EndExecutionReason {
        // We may decide that a command should be an implicit cd.
        let mut use_implicit_cd = false;

        // Get the command and any arguments due to expanding the command.
        let mut cmd = WString::new();
        let mut args_from_cmd_expansion = vec![];
        let ret = self.expand_command(ctx, statement, &mut cmd, &mut args_from_cmd_expansion);
        if ret != EndExecutionReason::ok {
            return ret;
        }

        // For no-exec, having an empty command is okay. We can't do anything more with it tho.
        if no_exec() {
            return EndExecutionReason::ok;
        }

        assert!(
            !cmd.is_empty(),
            "expand_command should not produce an empty command",
        );

        // Determine the process type.
        let mut process_type = self.process_type_for_command(ctx, statement, &cmd);
        let external_cmd = if [ProcessType::external, ProcessType::exec].contains(&process_type) {
            let parser = ctx.parser();
            // Determine the actual command. This may be an implicit cd.
            let external_cmd = path_try_get_path(&cmd, parser.vars());
            let has_command = external_cmd.err.is_none();

            let mut path = WString::new();
            if has_command {
                path = external_cmd.path;
            } else {
                // If the specified command does not exist, and is undecorated, try using an implicit cd.
                if statement.decoration() == StatementDecoration::none {
                    // Implicit cd requires an empty argument and redirection list.
                    if statement.args_or_redirs.is_empty() {
                        // Ok, no arguments or redirections; check to see if the command is a directory.
                        use_implicit_cd = path_as_implicit_cd(
                            &cmd,
                            &parser.vars().get_pwd_slash(),
                            parser.vars(),
                        )
                        .is_some();
                    }
                }

                if !use_implicit_cd {
                    return self.handle_command_not_found(
                        ctx,
                        if external_cmd.path.is_empty() {
                            &cmd
                        } else {
                            &external_cmd.path
                        },
                        statement,
                        std::io::Error::from_raw_os_error(external_cmd.err.unwrap().into()),
                    );
                }
            };
            path
        } else {
            WString::new()
        };

        // Produce the full argument list and the set of IO redirections.
        let mut cmd_args = vec![];
        let mut redirections = RedirectionSpecList::new();
        if use_implicit_cd {
            // Implicit cd is simple.
            cmd_args = vec![L!("cd").to_owned(), cmd];

            // If we have defined a wrapper around cd, use it, otherwise use the cd builtin.
            process_type = if function::exists(L!("cd"), ctx.parser()) {
                ProcessType::function
            } else {
                ProcessType::builtin
            };
        } else {
            // Not implicit cd.
            let glob_behavior = if [L!("set"), L!("count"), L!("path")].contains(&&cmd[..]) {
                Globspec::nullglob
            } else {
                Globspec::failglob
            };
            // Form the list of arguments. The command is the first argument, followed by any arguments
            // from expanding the command, followed by the argument nodes themselves. E.g. if the
            // command is '$gco foo' and $gco is git checkout.
            cmd_args.push(cmd);
            cmd_args.extend_from_slice(&args_from_cmd_expansion);
            let arg_nodes = Self::get_argument_nodes_no_redirs(&statement.args_or_redirs);
            let arg_result =
                self.expand_arguments_from_nodes(ctx, &arg_nodes, &mut cmd_args, glob_behavior);
            if arg_result != EndExecutionReason::ok {
                return arg_result;
            }

            // The set of IO redirections that we construct for the process.
            let reason =
                self.determine_redirections(ctx, &statement.args_or_redirs, &mut redirections);
            if reason != EndExecutionReason::ok {
                return reason;
            }
        }

        // Populate the process.
        proc.typ = process_type;
        proc.set_argv(cmd_args);
        proc.set_redirection_specs(redirections);
        proc.actual_cmd = external_cmd;
        EndExecutionReason::ok
    }

    fn populate_block_process(
        &mut self,
        ctx: &OperationContext<'_>,
        proc: &mut Process,
        statement: &ast::Statement,
        specific_statement: &ast::StatementVariant,
    ) -> EndExecutionReason {
        // We handle block statements by creating process_type_t::block_node, that will bounce back to
        // us when it's time to execute them.
        // Get the argument or redirections list.
        // TODO: args_or_redirs should be available without resolving the statement type.
        let args_or_redirs = match specific_statement {
            StatementVariant::BlockStatement(block_statement) => &block_statement.args_or_redirs,
            StatementVariant::IfStatement(if_statement) => &if_statement.args_or_redirs,
            StatementVariant::SwitchStatement(switch_statement) => &switch_statement.args_or_redirs,
            _ => panic!("Unexpected block node type"),
        };

        let mut redirections = RedirectionSpecList::new();
        let reason = self.determine_redirections(ctx, args_or_redirs, &mut redirections);
        if reason == EndExecutionReason::ok {
            proc.typ = ProcessType::block_node;
            proc.block_node_source = Some(Arc::clone(self.pstree()));
            proc.internal_block_node = Some(statement.into());
            proc.set_redirection_specs(redirections);
        }
        reason
    }

    // These encapsulate the actual logic of various (block) statements.
    fn run_block_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        statement: &'a ast::BlockStatement,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        let bh = &statement.header;
        let contents = &statement.jobs;
        match &**bh {
            BlockStatementHeaderVariant::ForHeader(fh) => self.run_for_statement(ctx, fh, contents),
            BlockStatementHeaderVariant::WhileHeader(wh) => {
                self.run_while_statement(ctx, wh, contents, associated_block)
            }
            BlockStatementHeaderVariant::FunctionHeader(fh) => {
                self.run_function_statement(ctx, statement, fh)
            }
            BlockStatementHeaderVariant::BeginHeader(_bh) => {
                self.run_begin_statement(ctx, contents)
            }
            BlockStatementHeaderVariant::None => panic!(),
        }
    }

    fn run_for_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        header: &'a ast::ForHeader,
        block_contents: &'a ast::JobList,
    ) -> EndExecutionReason {
        // Get the variable name: `for var_name in ...`. We expand the variable name. It better result
        // in just one.
        let mut for_var_name = self.node_source(&header.var_name);
        if !expand_one(&mut for_var_name, ExpandFlags::default(), ctx, None) {
            return report_error!(
                self,
                ctx,
                STATUS_EXPAND_ERROR.unwrap(),
                &header.var_name,
                FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG,
                for_var_name
            );
        }

        if !valid_var_name(&for_var_name) {
            return report_error!(
                self,
                ctx,
                STATUS_INVALID_ARGS.unwrap(),
                header.var_name,
                BUILTIN_ERR_VARNAME,
                "for",
                for_var_name
            );
        }

        // Get the contents to iterate over.
        let mut arguments = vec![];
        let arg_nodes = Self::get_argument_nodes(&header.args);
        let ret =
            self.expand_arguments_from_nodes(ctx, &arg_nodes, &mut arguments, Globspec::nullglob);
        if ret != EndExecutionReason::ok {
            return ret;
        }
        let var = ctx.parser().vars().get(&for_var_name);
        if EnvVar::flags_for(&for_var_name).contains(EnvVarFlags::READ_ONLY) {
            return report_error!(
                self,
                ctx,
                STATUS_INVALID_ARGS.unwrap(),
                header.var_name,
                "%ls: %ls: cannot overwrite read-only variable",
                "for",
                for_var_name
            );
        }

        let retval = ctx.parser().vars().set(
            &for_var_name,
            EnvMode::LOCAL | EnvMode::USER,
            var.map_or(vec![], |var| var.as_list().to_owned()),
        );
        assert!(retval == EnvStackSetResult::Ok);

        trace_if_enabled_with_args(ctx.parser(), L!("for"), &arguments);

        // We fire the same event over and over again, just construct it once.
        let evt = Event::variable_set(for_var_name.clone());

        // Now drive the for loop.
        let mut ret = EndExecutionReason::ok;
        for val in arguments {
            if let Some(reason) = self.check_end_execution(ctx) {
                ret = reason;
                break;
            }

            let retval = ctx
                .parser()
                .vars()
                .set(&for_var_name, EnvMode::USER, vec![val]);
            assert!(
                retval == EnvStackSetResult::Ok,
                "for loop variable should have been successfully set"
            );
            event::fire(ctx.parser(), evt.clone());

            ctx.parser().libdata_mut().loop_status = LoopStatus::normals;

            // Push and pop the block again and again to clear variables
            let fb = ctx.parser().push_block(Block::for_block());
            self.run_job_list(ctx, block_contents, Some(fb));
            ctx.parser().pop_block(fb);

            if self.check_end_execution(ctx) == Some(EndExecutionReason::control_flow) {
                // Handle break or continue.
                let do_break = ctx.parser().libdata().loop_status == LoopStatus::breaks;
                ctx.parser().libdata_mut().loop_status = LoopStatus::normals;
                if do_break {
                    break;
                }
            }
        }

        trace_if_enabled(ctx.parser(), L!("end for"));
        ret
    }

    fn run_if_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        statement: &'a ast::IfStatement,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        let mut result = EndExecutionReason::ok;

        // We have a sequence of if clauses, with a final else, resulting in a single job list that we
        // execute.
        let mut job_list_to_execute = None;
        let mut if_clause = &statement.if_clause;

        // Index of the *next* elseif_clause to test.
        let elseif_clauses = &statement.elseif_clauses;
        let mut next_elseif_idx = 0;

        // We start with the 'if'.
        trace_if_enabled(ctx.parser(), L!("if"));

        loop {
            if let Some(ret) = self.check_end_execution(ctx) {
                result = ret;
                break;
            }

            // An if condition has a job and a "tail" of andor jobs, e.g. "foo ; and bar; or baz".
            // Check the condition and the tail. We treat end_execution_reason_t::error here as failure,
            // in accordance with historic behavior.
            let mut cond_ret =
                self.run_job_conjunction(ctx, &if_clause.condition, associated_block);
            if cond_ret == EndExecutionReason::ok {
                cond_ret = self.run_andor_job_list(ctx, &if_clause.andor_tail, associated_block);
            }
            let take_branch = cond_ret == EndExecutionReason::ok
                && ctx.parser().get_last_status() == EXIT_SUCCESS;

            if take_branch {
                // Condition succeeded.
                job_list_to_execute = Some(&if_clause.body);
                break;
            }

            // See if we have an elseif.
            next_elseif_idx += 1;
            if let Some(elseif_clause) = elseif_clauses.get(next_elseif_idx - 1) {
                trace_if_enabled(ctx.parser(), L!("else if"));
                if_clause = &elseif_clause.if_clause;
            } else {
                break;
            }
        }

        if job_list_to_execute.is_none() {
            // our ifs and elseifs failed.
            // Check our else body.
            if let Some(else_clause) = statement.else_clause.as_ref() {
                trace_if_enabled(ctx.parser(), L!("else"));
                job_list_to_execute = Some(&else_clause.body);
            }
        }

        match job_list_to_execute {
            None => {
                // 'if' condition failed, no else clause, return 0, we're done.
                // No job list means no successful conditions, so return 0 (issue #1443).
                ctx.parser()
                    .set_last_statuses(Statuses::just(STATUS_CMD_OK.unwrap()));
            }
            Some(job_list_to_execute) => {
                // Execute the job list we got.
                let ib = ctx.parser().push_block(Block::if_block());
                self.run_job_list(ctx, job_list_to_execute, Some(ib));
                if let Some(ret) = self.check_end_execution(ctx) {
                    result = ret;
                }
                ctx.parser().pop_block(ib);
            }
        }
        trace_if_enabled(ctx.parser(), L!("end if"));

        // It's possible there's a last-minute cancellation (issue #1297).
        if let Some(ret) = self.check_end_execution(ctx) {
            result = ret;
        }

        // Otherwise, take the exit status of the job list. Reversal of issue #1061.
        result
    }

    fn run_switch_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        statement: &'a ast::SwitchStatement,
    ) -> EndExecutionReason {
        // Get the switch variable.
        let switch_value = self.node_source(&statement.argument);

        // Expand it. We need to offset any errors by the position of the string.
        let mut switch_values_expanded = vec![];
        let mut errors = ParseErrorList::new();
        let expand_ret = expand_string(
            switch_value,
            &mut switch_values_expanded,
            ExpandFlags::default(),
            ctx,
            Some(&mut errors),
        );
        parse_error_offset_source_start(&mut errors, statement.argument.range().unwrap().start());

        match expand_ret.result {
            ExpandResultCode::error => {
                return self.report_errors(ctx, expand_ret.status, &errors);
            }
            ExpandResultCode::cancel => {
                return EndExecutionReason::cancelled;
            }
            ExpandResultCode::wildcard_no_match => {
                return report_error!(
                    self,
                    ctx,
                    STATUS_UNMATCHED_WILDCARD.unwrap(),
                    &statement.argument,
                    WILDCARD_ERR_MSG,
                    &self.node_source(&statement.argument)
                );
            }
            ExpandResultCode::ok => {
                if switch_values_expanded.len() > 1 {
                    return report_error!(
                        self,
                        ctx,
                        STATUS_INVALID_ARGS.unwrap(),
                        &statement.argument,
                        "switch: Expected at most one argument, got %lu\n",
                        switch_values_expanded.len()
                    );
                }
            }
        }

        // If we expanded to nothing, match the empty string.
        assert!(
            switch_values_expanded.len() <= 1,
            "Should have at most one expansion"
        );
        let switch_value_expanded = if switch_values_expanded.is_empty() {
            WString::new()
        } else {
            switch_values_expanded.remove(0).completion
        };

        let mut result = EndExecutionReason::ok;

        trace_if_enabled_with_args(ctx.parser(), L!("switch"), &[&switch_value_expanded]);
        let sb = ctx.parser().push_block(Block::switch_block());

        // Expand case statements.
        let mut matching_case_item = None;
        for case_item in &statement.cases {
            if let Some(ret) = self.check_end_execution(ctx) {
                result = ret;
                break;
            }

            // Expand arguments. A case item list may have a wildcard that fails to expand to
            // anything. We also report case errors, but don't stop execution; i.e. a case item that
            // contains an unexpandable process will report and then fail to match.
            let arg_nodes = Self::get_argument_nodes(&case_item.arguments);
            let mut case_args = vec![];
            let case_result = self.expand_arguments_from_nodes(
                ctx,
                &arg_nodes,
                &mut case_args,
                Globspec::failglob,
            );
            if case_result == EndExecutionReason::ok {
                for arg in case_args {
                    // Unescape wildcards so they can be expanded again.
                    let unescaped_arg = parse_util_unescape_wildcards(&arg);
                    if wildcard_match(&switch_value_expanded, &unescaped_arg, false) {
                        // If this matched, we're done.
                        matching_case_item = Some(case_item);
                        break;
                    }
                }
            }
            if matching_case_item.is_some() {
                break;
            }
        }

        if let Some(case_item) = matching_case_item {
            // Success, evaluate the job list.
            assert!(result == EndExecutionReason::ok, "Expected success");
            result = self.run_job_list(ctx, &case_item.body, Some(sb));
        }

        ctx.parser().pop_block(sb);
        trace_if_enabled(ctx.parser(), L!("end switch"));
        result
    }

    fn run_while_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        header: &'a ast::WhileHeader,
        contents: &'a ast::JobList,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        let mut ret = EndExecutionReason::ok;

        // "The exit status of the while loop shall be the exit status of the last compound-list-2
        // executed, or zero if none was executed."
        // Here are more detailed requirements:
        // - If we execute the loop body zero times, or the loop body is empty, the status is success.
        // - An empty loop body is treated as true, both in the loop condition and after loop exit.
        // - The exit status of the last command is visible in the loop condition. (i.e. do not set the
        // exit status to true BEFORE executing the loop condition).
        // We achieve this by restoring the status if the loop condition fails, plus a special
        // affordance for the first condition.
        let mut first_cond_check = true;

        trace_if_enabled(ctx.parser(), L!("while"));

        // Run while the condition is true.
        loop {
            // Save off the exit status if it came from the loop body. We'll restore it if the condition
            // is false.
            let cond_saved_status = if first_cond_check {
                Statuses::just(EXIT_SUCCESS)
            } else {
                ctx.parser().get_last_statuses()
            };
            first_cond_check = false;

            // Check the condition.
            let mut cond_ret = self.run_job_conjunction(ctx, &header.condition, associated_block);
            if cond_ret == EndExecutionReason::ok {
                cond_ret = self.run_andor_job_list(ctx, &header.andor_tail, associated_block);
            }

            // If the loop condition failed to execute, then exit the loop without modifying the exit
            // status. If the loop condition executed with a failure status, restore the status and then
            // exit the loop.
            if cond_ret != EndExecutionReason::ok {
                break;
            } else if ctx.parser().get_last_status() != EXIT_SUCCESS {
                ctx.parser().set_last_statuses(cond_saved_status);
                break;
            }

            // Check cancellation.
            if let Some(reason) = self.check_end_execution(ctx) {
                ret = reason;
                break;
            }

            // Push a while block and then check its cancellation reason.
            ctx.parser().libdata_mut().loop_status = LoopStatus::normals;

            let wb = ctx.parser().push_block(Block::while_block());
            self.run_job_list(ctx, contents, Some(wb));
            let cancel_reason = self.check_end_execution(ctx);
            ctx.parser().pop_block(wb);

            if cancel_reason == Some(EndExecutionReason::control_flow) {
                // Handle break or continue.
                let do_break = ctx.parser().libdata().loop_status == LoopStatus::breaks;
                ctx.parser().libdata_mut().loop_status = LoopStatus::normals;
                if do_break {
                    break;
                } else {
                    continue;
                }
            }

            // no_exec means that fish was invoked with -n or --no-execute. If set, we allow the loop to
            // not-execute once so its contents can be checked, and then break.
            if no_exec() {
                break;
            }
        }
        trace_if_enabled(ctx.parser(), L!("end while"));
        ret
    }

    // Define a function.
    fn run_function_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        statement: &ast::BlockStatement,
        header: &ast::FunctionHeader,
    ) -> EndExecutionReason {
        // Get arguments.
        let mut arguments = vec![];
        let mut arg_nodes = Self::get_argument_nodes(&header.args);
        arg_nodes.insert(0, &header.first_arg);
        let result =
            self.expand_arguments_from_nodes(ctx, &arg_nodes, &mut arguments, Globspec::failglob);

        if result != EndExecutionReason::ok {
            return result;
        }

        trace_if_enabled_with_args(ctx.parser(), L!("function"), &arguments);
        let mut outs = OutputStream::Null;
        let mut errs = OutputStream::String(StringOutputStream::new());
        let io_chain = IoChain::new();
        let mut streams = IoStreams::new(&mut outs, &mut errs, &io_chain);
        let mut shim_arguments: Vec<&wstr> = arguments
            .iter()
            .map(|s| truncate_at_nul(s.as_ref()))
            .collect();
        let err_code = builtins::function::function(
            ctx.parser(),
            &mut streams,
            &mut shim_arguments,
            NodeRef::new(
                Arc::clone(self.pstree()),
                statement as *const ast::BlockStatement,
            ),
        );
        let err_code = err_code.unwrap();
        ctx.parser().libdata_mut().status_count += 1;
        ctx.parser().set_last_statuses(Statuses::just(err_code));

        let errtext = errs.contents();
        if !errtext.is_empty() {
            report_error!(self, ctx, err_code, header, "%ls", errtext);
        }
        result
    }

    fn run_begin_statement(
        &mut self,
        ctx: &OperationContext<'_>,
        contents: &'a ast::JobList,
    ) -> EndExecutionReason {
        // Basic begin/end block. Push a scope block, run jobs, pop it
        trace_if_enabled(ctx.parser(), L!("begin"));
        let sb = ctx
            .parser()
            .push_block(Block::scope_block(BlockType::begin));
        let ret = self.run_job_list(ctx, contents, Some(sb));
        ctx.parser().pop_block(sb);
        trace_if_enabled(ctx.parser(), L!("end begin"));
        ret
    }

    fn get_argument_nodes(args: &ast::ArgumentList) -> AstArgsList<'_> {
        let mut result = AstArgsList::new();
        for arg in args {
            result.push(&**arg);
        }
        result
    }

    fn get_argument_nodes_no_redirs(args: &ast::ArgumentOrRedirectionList) -> AstArgsList<'_> {
        let mut result = AstArgsList::new();
        for arg in args {
            if arg.is_argument() {
                result.push(arg.argument());
            }
        }
        result
    }

    fn expand_arguments_from_nodes(
        &mut self,
        ctx: &OperationContext<'_>,
        argument_nodes: &AstArgsList<'_>,
        out_arguments: &mut Vec<WString>,
        glob_behavior: Globspec,
    ) -> EndExecutionReason {
        // Get all argument nodes underneath the statement. We guess we'll have that many arguments (but
        // may have more or fewer, if there are wildcards involved).
        out_arguments.reserve(argument_nodes.len());
        for arg_node in argument_nodes {
            // Expect all arguments to have source.
            assert!(arg_node.has_source(), "Argument should have source");

            // Expand this string.
            let mut errors = ParseErrorList::new();
            let mut arg_expanded = CompletionList::new();
            let expand_ret = expand_string(
                self.node_source(*arg_node),
                &mut arg_expanded,
                ExpandFlags::default(),
                ctx,
                Some(&mut errors),
            );
            parse_error_offset_source_start(&mut errors, arg_node.range().unwrap().start());
            match expand_ret.result {
                ExpandResultCode::error => {
                    return self.report_errors(ctx, expand_ret.status, &errors);
                }
                ExpandResultCode::cancel => {
                    return EndExecutionReason::cancelled;
                }
                ExpandResultCode::wildcard_no_match => {
                    if glob_behavior == Globspec::failglob {
                        // For no_exec, ignore the error - this might work at runtime.
                        if no_exec() {
                            return EndExecutionReason::ok;
                        }
                        // Report the unmatched wildcard error and stop processing.
                        return report_error!(
                            self,
                            ctx,
                            STATUS_UNMATCHED_WILDCARD.unwrap(),
                            arg_node,
                            WILDCARD_ERR_MSG,
                            &self.node_source(*arg_node)
                        );
                    }
                }
                ExpandResultCode::ok => {}
            }

            // Now copy over any expanded arguments. Use std::move() to avoid extra allocations; this
            // is called very frequently.
            if let Some(additional) =
                (out_arguments.len() + arg_expanded.len()).checked_sub(out_arguments.capacity())
            {
                out_arguments.reserve(additional);
            }
            for new_arg in arg_expanded {
                out_arguments.push(new_arg.completion);
            }
        }

        // We may have received a cancellation during this expansion.
        if let Some(ret) = self.check_end_execution(ctx) {
            return ret;
        }

        EndExecutionReason::ok
    }

    // Determines the list of redirections for a node.
    fn determine_redirections(
        &self,
        ctx: &OperationContext<'_>,
        list: &ast::ArgumentOrRedirectionList,
        out_redirections: &mut RedirectionSpecList,
    ) -> EndExecutionReason {
        // Get all redirection nodes underneath the statement.
        for arg_or_redir in list {
            if !arg_or_redir.is_redirection() {
                continue;
            }
            let redir_node = arg_or_redir.redirection();

            let oper = match PipeOrRedir::try_from(&self.node_source(&redir_node.oper)[..]) {
                Ok(oper) if oper.is_valid() => oper,
                _ => {
                    // TODO: figure out if this can ever happen. If so, improve this error message.
                    return report_error!(
                        self,
                        ctx,
                        STATUS_INVALID_ARGS.unwrap(),
                        redir_node,
                        "Invalid redirection: %ls",
                        &self.node_source(redir_node)
                    );
                }
            };

            // PCA: I can't justify this skip_variables flag. It was like this when I got here.
            let mut target = self.node_source(&redir_node.target);
            let target_expanded = expand_one(
                &mut target,
                if no_exec() {
                    ExpandFlags::SKIP_VARIABLES
                } else {
                    ExpandFlags::default()
                },
                ctx,
                None,
            );
            if !target_expanded || target.is_empty() {
                // TODO: Improve this error message.
                return report_error!(
                    self,
                    ctx,
                    STATUS_INVALID_ARGS.unwrap(),
                    redir_node,
                    "Invalid redirection target: %ls",
                    target
                );
            }

            // Make a redirection spec from the redirect token.
            assert!(oper.is_valid(), "expected to have a valid redirection");
            let spec = RedirectionSpec::new(oper.fd, oper.mode, target);

            // Validate this spec.
            if spec.mode == RedirectionMode::fd
                && !spec.is_close()
                && spec.get_target_as_fd().is_none()
            {
                return report_error!(
                    self,
                    ctx,
                    STATUS_INVALID_ARGS.unwrap(),
                    redir_node,
                    "Requested redirection to '%ls', which is not a valid file descriptor",
                    &spec.target
                );
            }
            out_redirections.push(spec);

            if oper.stderr_merge {
                // This was a redirect like &> which also modifies stderr.
                // Also redirect stderr to stdout.
                out_redirections.push(get_stderr_merge());
            }
        }
        EndExecutionReason::ok
    }

    fn run_1_job(
        &mut self,
        ctx: &OperationContext<'_>,
        job_node: &'a ast::JobPipeline,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        if let Some(ret) = self.check_end_execution(ctx) {
            return ret;
        }

        // We definitely do not want to execute anything if we're told we're --no-execute!
        if no_exec() {
            return EndExecutionReason::ok;
        }

        // Increment the eval_level for the duration of this command.
        let _saved_eval_level = scoped_push_replacer(
            |new_value| ctx.parser().eval_level.swap(new_value, Ordering::Relaxed),
            ctx.parser().eval_level.load(Ordering::Relaxed) + 1,
        );

        // Save the executing node.
        let line_counter = Rc::clone(&self.line_counter);
        let _saved_node = scoped_push_replacer(
            |node| line_counter.borrow_mut().set_node(node),
            Some(job_node),
        );

        // Profiling support.
        let profile_item_id = ctx.parser().create_profile_item();
        let start_time = if profile_item_id.is_some() {
            ProfileItem::now()
        } else {
            0
        };

        let job_is_background = job_node.bg.is_some();
        let _timer = {
            let wants_timing = job_node_wants_timing(job_node);
            // It's an error to have 'time' in a background job.
            if wants_timing && job_is_background {
                return report_error!(
                    self,
                    ctx,
                    STATUS_INVALID_ARGS.unwrap(),
                    job_node,
                    ERROR_TIME_BACKGROUND
                );
            }
            wants_timing.then(push_timer)
        };

        // When we encounter a block construct (e.g. while loop) in the general case, we create a "block
        // process" containing its node. This allows us to handle block-level redirections.
        // However, if there are no redirections, then we can just jump into the block directly, which
        // is significantly faster.
        if self.job_is_simple_block(job_node) {
            let mut block = None;
            let mut result =
                self.apply_variable_assignments(ctx, None, &job_node.variables, &mut block);
            let _scope = ScopeGuard::new((), |()| {
                if let Some(block) = block {
                    ctx.parser().pop_block(block);
                }
            });

            let specific_statement = &job_node.statement.contents;
            assert!(specific_statement_type_is_redirectable_block(
                specific_statement
            ));
            if result == EndExecutionReason::ok {
                result = match &**specific_statement {
                    StatementVariant::BlockStatement(block_statement) => {
                        self.run_block_statement(ctx, block_statement, associated_block)
                    }
                    StatementVariant::IfStatement(ifstmt) => {
                        self.run_if_statement(ctx, ifstmt, associated_block)
                    }
                    StatementVariant::SwitchStatement(switchstmt) => {
                        self.run_switch_statement(ctx, switchstmt)
                    }
                    // Other types should be impossible due to the
                    // specific_statement_type_is_redirectable_block check.
                    StatementVariant::NotStatement(_)
                    | StatementVariant::DecoratedStatement(_)
                    | StatementVariant::None => panic!(),
                };
            }

            if let Some(profile_item_id) = profile_item_id {
                let parser = ctx.parser();
                let mut profile_items = parser.profile_items_mut();
                let profile_item = &mut profile_items[profile_item_id];
                profile_item.duration = ProfileItem::now() - start_time;
                profile_item.level = ctx.parser().eval_level.load(Ordering::Relaxed);
                profile_item.cmd =
                    profiling_cmd_name_for_redirectable_block(specific_statement, self.pstree());
                profile_item.skipped = false;
            }

            return result;
        }

        let mut props = JobProperties::default();
        props.initial_background = job_is_background;
        {
            let parser = ctx.parser();
            let ld = &parser.libdata();
            props.skip_notification =
                ld.is_subshell || parser.is_block() || ld.is_event != 0 || !parser.is_interactive();
            props.from_event_handler = ld.is_event != 0;
        }

        let mut job = Job::new(props, self.node_source(job_node));

        // We are about to populate a job. One possible argument to the job is a command substitution
        // which may be interested in the job that's populating it, via '--on-job-exit caller'. Record
        // the job ID here.
        let _caller_id = scoped_push_replacer(
            |new_value| std::mem::replace(&mut ctx.parser().libdata_mut().caller_id, new_value),
            job.internal_job_id,
        );

        // Populate the job. This may fail for reasons like command_not_found. If this fails, an error
        // will have been printed.
        let pop_result = self.populate_job_from_job_node(ctx, &mut job, job_node, associated_block);
        ScopeGuarding::commit(_caller_id);

        // Clean up the job on failure or cancellation.
        if pop_result == EndExecutionReason::ok {
            self.setup_group(ctx, &mut job);
            assert!(job.group.is_some(), "Should have a group");
        }

        // Now that we're done mutating the Job, we can stick it in an Arc
        let job = Rc::new(job);

        if pop_result == EndExecutionReason::ok {
            // Give the job to the parser - it will clean it up.
            {
                let parser = ctx.parser();
                parser.job_add(job.clone());

                // Actually execute the job.
                if !exec_job(parser, &job, self.block_io.clone()) {
                    // No process in the job successfully launched.
                    // Ensure statuses are set (#7540).
                    if let Some(statuses) = job.get_statuses() {
                        parser.set_last_statuses(statuses);
                        parser.libdata_mut().status_count += 1;
                    }
                    remove_job(parser, &job);
                }

                // Update universal variables on external commands.
                // We only incorporate external changes if we had an external proc, for hysterical raisins.
                parser.sync_uvars_and_fire(job.has_external_proc() /* always */);
            }

            // If the job got a SIGINT or SIGQUIT, then we're going to start unwinding.
            if self.cancel_signal.is_none() {
                self.cancel_signal = job.group().get_cancel_signal();
            }
        }

        if let Some(profile_item_id) = profile_item_id {
            let parser = ctx.parser();
            let mut profile_items = parser.profile_items_mut();
            let profile_item = &mut profile_items[profile_item_id];
            profile_item.duration = ProfileItem::now() - start_time;
            profile_item.level = ctx.parser().eval_level.load(Ordering::Relaxed);
            profile_item.cmd = job.command().to_owned();
            profile_item.skipped = pop_result != EndExecutionReason::ok;
        }

        job_reap(ctx.parser(), false); // clean up jobs
        pop_result
    }

    fn test_and_run_1_job_conjunction(
        &mut self,
        ctx: &OperationContext<'_>,
        jc: &'a ast::JobConjunction,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        // Test this job conjunction if it has an 'and' or 'or' decorator.
        // If it passes, then run it.
        if let Some(reason) = self.check_end_execution(ctx) {
            return reason;
        }
        // Maybe skip the job if it has a leading and/or.
        let mut skip = false;
        if let Some(deco) = &jc.decorator {
            let last_status = ctx.parser().get_last_status();
            match deco.keyword() {
                ParseKeyword::kw_and => {
                    // AND. Skip if the last job failed.
                    skip = last_status != 0;
                }
                ParseKeyword::kw_or => {
                    // OR. Skip if the last job succeeded.
                    skip = last_status == 0;
                }
                _ => unreachable!(),
            }
        }
        // Skipping is treated as success.
        if skip {
            EndExecutionReason::ok
        } else {
            self.run_job_conjunction(ctx, jc, associated_block)
        }
    }

    fn run_job_conjunction(
        &mut self,
        ctx: &OperationContext<'_>,
        job_expr: &'a ast::JobConjunction,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        if let Some(reason) = self.check_end_execution(ctx) {
            return reason;
        }
        let mut result = self.run_1_job(ctx, &job_expr.job, associated_block);
        for jc in &job_expr.continuations {
            if result != EndExecutionReason::ok {
                return result;
            }
            if let Some(reason) = self.check_end_execution(ctx) {
                return reason;
            }
            // Check the conjunction type.
            let last_status = ctx.parser().get_last_status();
            let skip = match jc.conjunction.token_type() {
                ParseTokenType::andand => {
                    // AND. Skip if the last job failed.
                    last_status != 0
                }
                ParseTokenType::oror => {
                    // OR. Skip if the last job succeeded.
                    last_status == 0
                }
                _ => unreachable!(),
            };
            if !skip {
                result = self.run_1_job(ctx, &jc.job, associated_block);
            }
        }
        result
    }

    fn run_job_list(
        &mut self,
        ctx: &OperationContext<'_>,
        job_list_node: &'a ast::JobList,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        let mut result = EndExecutionReason::ok;
        for jc in job_list_node {
            result = self.test_and_run_1_job_conjunction(ctx, jc, associated_block);
        }
        // Returns the result of the last job executed or skipped.
        result
    }

    fn run_andor_job_list(
        &mut self,
        ctx: &OperationContext<'_>,
        job_list_node: &'a ast::AndorJobList,
        associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        let mut result = EndExecutionReason::ok;
        for aoj in job_list_node {
            result = self.test_and_run_1_job_conjunction(ctx, &aoj.job, associated_block);
        }
        // Returns the result of the last job executed or skipped.
        result
    }

    fn populate_job_from_job_node(
        &mut self,
        ctx: &OperationContext<'_>,
        j: &mut Job,
        job_node: &ast::JobPipeline,
        _associated_block: Option<BlockId>,
    ) -> EndExecutionReason {
        // We are going to construct process_t structures for every statement in the job.
        // Create processes. Each one may fail.
        let mut processes = ProcessList::new();
        processes.push(Box::new(Process::new()));
        let mut result = self.populate_job_process(
            ctx,
            j,
            &mut processes[0],
            &job_node.statement,
            &job_node.variables,
        );

        // Construct process_ts for job continuations (pipelines).
        for jc in &job_node.continuation {
            if result != EndExecutionReason::ok {
                break;
            }
            // Handle the pipe, whose fd may not be the obvious stdout.
            let parsed_pipe = PipeOrRedir::try_from(&self.node_source(&jc.pipe)[..])
                .expect("Failed to parse valid pipe");
            if !parsed_pipe.is_valid() {
                result = report_error!(
                    self,
                    ctx,
                    STATUS_INVALID_ARGS.unwrap(),
                    &jc.pipe,
                    ILLEGAL_FD_ERR_MSG,
                    &self.node_source(&jc.pipe)
                );
                break;
            }
            {
                let proc = processes.last_mut().unwrap();
                proc.pipe_write_fd = parsed_pipe.fd;
                if parsed_pipe.stderr_merge {
                    // This was a pipe like &| which redirects both stdout and stderr.
                    // Also redirect stderr to stdout.
                    let specs = proc.redirection_specs_mut();
                    specs.push(get_stderr_merge());
                }
            }

            // Store the new process (and maybe with an error).
            processes.push(Box::new(Process::new()));
            result = self.populate_job_process(
                ctx,
                j,
                processes.last_mut().unwrap(),
                &jc.statement,
                &jc.variables,
            );
        }

        // Inform our processes of who is first and last
        processes.first_mut().unwrap().is_first_in_job = true;
        processes.last_mut().unwrap().is_last_in_job = true;

        // Return what happened.
        if result == EndExecutionReason::ok {
            // Link up the processes.
            assert!(!processes.is_empty());
            *j.processes_mut() = processes;
        }
        result
    }

    // Assign a job group to the given job.
    fn setup_group(&self, ctx: &OperationContext<'_>, j: &mut Job) {
        // We can use the parent group if it's compatible and we're not backgrounded.
        if ctx.job_group.as_ref().map_or(false, |job_group| {
            job_group.has_job_id() || !j.wants_job_id()
        }) && !j.is_initially_background()
        {
            j.group = ctx.job_group.clone();
            return;
        }

        if j.processes()[0].is_internal() || !self.use_job_control(ctx) {
            // This job either doesn't have a pgroup (e.g. a simple block), or lives in fish's pgroup.
            j.group = Some(JobGroup::create(j.command().to_owned(), j.wants_job_id()));
        } else {
            // This is a "real job" that gets its own pgroup.
            j.processes_mut()[0].leads_pgrp = true;
            let wants_terminal = ctx.parser().libdata().is_event == 0;
            j.group = Some(JobGroup::create_with_job_control(
                j.command().to_owned(),
                wants_terminal,
            ));
        }
        j.group().is_foreground.store(!j.is_initially_background());
        j.mut_flags().is_group_root = true;
    }

    // Return whether we should apply job control to our processes.
    fn use_job_control(&self, ctx: &OperationContext<'_>) -> bool {
        if ctx.parser().is_command_substitution() {
            return false;
        }
        match get_job_control_mode() {
            JobControl::all => true,
            JobControl::interactive => ctx.parser().is_interactive(),
            JobControl::none => false,
        }
    }
}

#[derive(Eq, PartialEq)]
enum Globspec {
    failglob,
    nullglob,
}
type AstArgsList<'a> = Vec<&'a ast::Argument>;

/// These are the specific statement types that support redirections.
fn type_is_redirectable_block(typ: ast::Type) -> bool {
    [
        ast::Type::block_statement,
        ast::Type::if_statement,
        ast::Type::switch_statement,
    ]
    .contains(&typ)
}

fn specific_statement_type_is_redirectable_block(node: &ast::StatementVariant) -> bool {
    type_is_redirectable_block(node.typ())
}

/// Get the name of a redirectable block, for profiling purposes.
fn profiling_cmd_name_for_redirectable_block(
    node: &ast::StatementVariant,
    pstree: &ParsedSourceRef,
) -> WString {
    assert!(specific_statement_type_is_redirectable_block(node));

    let source_range = node.try_source_range().expect("No source range for block");

    let src_end = match node {
        StatementVariant::BlockStatement(block_statement) => {
            let block_header = &block_statement.header;
            match &**block_header {
                BlockStatementHeaderVariant::ForHeader(for_header) => {
                    for_header.semi_nl.source_range().start()
                }
                BlockStatementHeaderVariant::WhileHeader(while_header) => {
                    while_header.condition.source_range().start()
                }
                BlockStatementHeaderVariant::FunctionHeader(function_header) => {
                    function_header.semi_nl.source_range().start()
                }
                BlockStatementHeaderVariant::BeginHeader(begin_header) => {
                    begin_header.kw_begin.source_range().start()
                }
                BlockStatementHeaderVariant::None => panic!("Unexpected block header type"),
            }
        }
        StatementVariant::IfStatement(ifstmt) => {
            ifstmt.if_clause.condition.job.source_range().end()
        }
        StatementVariant::SwitchStatement(switchstmt) => switchstmt.semi_nl.source_range().start(),
        _ => {
            panic!("Not a redirectable block_type");
        }
    };

    assert!(src_end >= source_range.start(), "Invalid source end");

    // Get the source for the block, and cut it at the next statement terminator.
    let mut result = pstree.src[source_range.start()..src_end].to_owned();
    result.push_utfstr(L!("..."));
    result
}

/// Get a redirection from stderr to stdout (i.e. 2>&1).
fn get_stderr_merge() -> RedirectionSpec {
    let stdout_fileno_str = L!("1").to_owned();
    RedirectionSpec::new(STDERR_FILENO, RedirectionMode::fd, stdout_fileno_str)
}

/// Decide if a job node should be 'time'd.
/// For historical reasons the 'not' and 'time' prefix are "inside out". That is, it's
/// 'not time cmd'. Note that a time appearing anywhere in the pipeline affects the whole job.
/// `sleep 1 | not time true` will time the whole job!
fn job_node_wants_timing(job_node: &ast::JobPipeline) -> bool {
    // Does our job have the job-level time prefix?
    if job_node.time.is_some() {
        return true;
    }

    // Helper to return true if a node is 'not time ...' or 'not not time...' or...
    let is_timed_not_statement = |mut stat: &ast::Statement| loop {
        match &*stat.contents {
            StatementVariant::NotStatement(ns) => {
                if ns.time.is_some() {
                    return true;
                }
                stat = &ns.contents;
            }
            _ => return false,
        }
    };

    // Do we have a 'not time ...' anywhere in our pipeline?
    if is_timed_not_statement(&job_node.statement) {
        return true;
    }
    for jc in &job_node.continuation {
        if is_timed_not_statement(&jc.statement) {
            return true;
        }
    }

    false
}

fn remove_job(parser: &Parser, job: &JobRef) -> bool {
    let mut jobs = parser.jobs_mut();
    let num_jobs = jobs.len();
    for i in 0..num_jobs {
        if Rc::ptr_eq(&jobs[i], job) {
            jobs.remove(i);
            return true;
        }
    }
    false
}
