/**\file parse_execution.cpp

   Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.).

*/

#include "parse_execution.h"
#include "complete.h"
#include "builtin.h"
#include "parser.h"
#include "expand.h"
#include "wutil.h"
#include "exec.h"
#include "path.h"


parse_execution_context_t::parse_execution_context_t(const parse_node_tree_t &t, const wcstring s, parser_t *p) : tree(t), src(s), parser(p), eval_level(0)
{
}

/* Utilities */

wcstring parse_execution_context_t::get_source(const parse_node_t &node) const
{
    return node.get_source(this->src);
}

const parse_node_t *parse_execution_context_t::get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type) const
{
    return this->tree.get_child(parent, which, expected_type);
}

node_offset_t parse_execution_context_t::get_offset(const parse_node_t &node) const
{
    /* Get the offset of a node via pointer arithmetic, very hackish */
    const parse_node_t *addr = &node;
    const parse_node_t *base = &this->tree.at(0);
    assert(addr >= base);
    node_offset_t offset = addr - base;
    assert(offset < this->tree.size());
    assert(&tree.at(offset) == &node);
    return offset;
}


bool parse_execution_context_t::should_cancel() const
{
    return false;
}

void parse_execution_context_t::run_while_process(const parse_node_t &header, const parse_node_t &statement)
{
    assert(header.type == symbol_while_header);
    assert(statement.type == symbol_block_statement);
    
    while_block_t *wb = new while_block_t();
    wb->status = WHILE_TEST_FIRST;
    wb->node_offset = this->get_offset(statement);
    parser->push_block(wb);
    
    // The condition of the while loop, as a job
    const parse_node_t &while_condition = *get_child(header, 1, symbol_job);
    
    // The contents of the while loop, as a job list
    const parse_node_t &block_contents = *get_child(statement, 2, symbol_job_list);
    
    // A while loop is a while loop!
    while (! this->should_cancel() && this->run_1_job(while_condition) == EXIT_SUCCESS)
    {
        this->run_job_list(block_contents);
    }
    
    parser->pop_block(wb);
}


bool parse_execution_context_t::append_error(const parse_node_t &node, const wchar_t *fmt, ...)
{
    parse_error_t error;
    error.source_start = node.source_start;
    error.source_length = node.source_length;
    error.code = parse_error_syntax; //hackish
    
    va_list va;
    va_start(va, fmt);
    error.text = vformat_string(fmt, va);
    va_end(va);
    
    this->errors.push_back(error);
    return true;
}

process_t *parse_execution_context_t::create_plain_process(job_t *job, const parse_node_t &statement)
{
    bool errored = false;
    
    /* Get the decoration */
    assert(statement.type == symbol_plain_statement);
    
    /* Get the command. We expect to always get it here. */
    wcstring cmd;
    bool got_cmd = tree.command_for_plain_statement(statement, src, &cmd);
    assert(got_cmd);
    
    /* Expand it as a command. Return NULL on failure. */
    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES);
    if (! expanded)
    {
        errored = append_error(statement, ILLEGAL_CMD_ERR_MSG, cmd.c_str());
    }
    
    if (errored)
        return NULL;
    
    /* The list of arguments. The command is the first argument. TODO: count hack */
    const parse_node_t *unmatched_wildcard = NULL;
    wcstring_list_t argument_list = this->determine_arguments(statement, &unmatched_wildcard);
    argument_list.insert(argument_list.begin(), cmd);
    
    /* If we were not able to expand any wildcards, here is the first one that failed */
    if (unmatched_wildcard != NULL)
    {
        job_set_flag(job, JOB_WILDCARD_ERROR, 1);
        proc_set_last_status(STATUS_UNMATCHED_WILDCARD);
        errored = append_error(*unmatched_wildcard, WILDCARD_ERR_MSG, unmatched_wildcard->get_source(src).c_str());
    }
    
    if (errored)
        return NULL;
    
    /* The set of IO redirections that we construct for the process */
    io_chain_t process_io_chain;
    errored = ! this->determine_io_chain(statement, &process_io_chain);
    if (errored)
        return NULL;
    
    /* Determine the process type, which depends on the statement decoration (command, builtin, etc) */
    enum parse_statement_decoration_t decoration = tree.decoration_for_plain_statement(statement);
    enum process_type_t process_type = EXTERNAL;
    
    /* exec hack */
    if (decoration != parse_statement_decoration_command && cmd == L"exec")
    {
        /* Either 'builtin exec' or just plain 'exec', and definitely not 'command exec'. Note we don't allow overriding exec with a function. */
        process_type = INTERNAL_EXEC;
    }
    else if (decoration == parse_statement_decoration_command)
    {
        /* Always a command */
        process_type = EXTERNAL;
    }
    else if (decoration == parse_statement_decoration_builtin)
    {
        /* What happens if this builtin is not valid? */
        process_type = INTERNAL_BUILTIN;
    }
    else if (function_exists(cmd))
    {
        process_type = INTERNAL_FUNCTION;
    }
    else if (builtin_exists(cmd))
    {
        process_type = INTERNAL_BUILTIN;
    }
    else
    {
        process_type = EXTERNAL;
    }
    
    wcstring actual_cmd;
    if (process_type == EXTERNAL)
    {
        /* Determine the actual command. Need to support implicit cd here */
        bool has_command = path_get_path(cmd, &actual_cmd);
        
        if (! has_command)
        {
            /* TODO: support fish_command_not_found, implicit cd, etc. here */
            errored = true;
        }
        return NULL;
    }
    
    /* Return the process, or NULL on error */
    process_t *result = NULL;
    if (! errored)
    {
        result = new process_t();
        result->type = process_type;
        result->set_argv(argument_list);
        result->set_io_chain(process_io_chain);
        result->actual_cmd = actual_cmd;
    }
    return result;
}

/* Determine the list of arguments, expanding stuff. If we have a wildcard and none could be expanded, return the unexpandable wildcard node by reference. */
wcstring_list_t parse_execution_context_t::determine_arguments(const parse_node_t &parent, const parse_node_t **out_unmatched_wildcard_node)
{
    wcstring_list_t argument_list;
    
    /* Whether we failed to match any wildcards, and succeeded in matching any wildcards */
    bool unmatched_wildcard = false, matched_wildcard = false;
    
    /* First node that failed to expand as a wildcard (if any) */
    const parse_node_t *unmatched_wildcard_node = NULL;
    
    /* Get all argument nodes underneath the statement */
    const parse_node_tree_t::parse_node_list_t argument_nodes = tree.find_nodes(parent, symbol_argument);
    argument_list.reserve(argument_nodes.size());
    for (size_t i=0; i < argument_nodes.size(); i++)
    {
        const parse_node_t &arg_node = *argument_nodes.at(i);
        
        /* Expect all arguments to have source */
        assert(arg_node.has_source());
        const wcstring arg_str = arg_node.get_source(src);
        
        /* Expand this string */
        std::vector<completion_t> arg_expanded;
        int expand_ret = expand_string(arg_str, arg_expanded, 0);
        switch (expand_ret)
        {
            case EXPAND_ERROR:
            {
                this->append_error(arg_node,
                      _(L"Could not expand string '%ls'"),
                      arg_str.c_str());
                break;
            }
                
            case EXPAND_WILDCARD_NO_MATCH:
            {
                /* Store the node that failed to expand */
                unmatched_wildcard = true;
                if (! unmatched_wildcard_node)
                {
                    unmatched_wildcard_node = &arg_node;
                }
                break;
            }
            
            case EXPAND_WILDCARD_MATCH:
            {
                matched_wildcard = true;
                break;
            }
            
            case EXPAND_OK:
            {
                break;
            }
        }
        
        /* Now copy over any expanded arguments */
        for (size_t i=0; i < arg_expanded.size(); i++)
        {
            argument_list.push_back(arg_expanded.at(i).completion);
        }
    }
    
    /* Return if we had a wildcard problem */
    if (unmatched_wildcard && ! matched_wildcard)
    {
        *out_unmatched_wildcard_node = unmatched_wildcard_node;
    }
    
    return argument_list;
}

bool parse_execution_context_t::determine_io_chain(const parse_node_t &statement, io_chain_t *out_chain)
{
    io_chain_t result;
    bool errored = false;
    
    /* Get all redirection nodes underneath the statement */
    const parse_node_tree_t::parse_node_list_t redirect_nodes = tree.find_nodes(statement, symbol_redirection);
    for (size_t i=0; i < redirect_nodes.size(); i++)
    {
        const parse_node_t &redirect_node = *redirect_nodes.at(i);
        
        int source_fd = -1; /* source fd */
        wcstring target; /* file path or target fd */
        enum token_type redirect_type = tree.type_for_redirection(redirect_node, src, &source_fd, &target);
        
        /* PCA: I can't justify this EXPAND_SKIP_VARIABLES flag. It was like this when I got here. */
        bool target_expanded = expand_one(target, no_exec ? EXPAND_SKIP_VARIABLES : 0);
        if (! target_expanded || target.empty())
        {
            /* Should improve this error message */
            errored = append_error(redirect_node,
                  _(L"Invalid redirection target: %ls"),
                  target.c_str());
        }
        
        
        /* Generate the actual IO redirection */
        shared_ptr<io_data_t> new_io;
        assert(redirect_type != TOK_NONE);
        switch (redirect_type)
        {
            case TOK_REDIRECT_FD:
            {
                if (target == L"-")
                {
                    new_io.reset(new io_close_t(source_fd));
                }
                else
                {
                    wchar_t *end = NULL;
                    errno = 0;
                    int old_fd = fish_wcstoi(target.c_str(), &end, 10);
                    if (old_fd < 0 || errno || *end)
                    {
                        errored = append_error(redirect_node,
                              _(L"Requested redirection to something that is not a file descriptor %ls"),
                              target.c_str());
                    }
                    else
                    {
                        new_io.reset(new io_fd_t(source_fd, old_fd));
                    }
                }
                break;
            }
            
            case TOK_REDIRECT_OUT:
            case TOK_REDIRECT_APPEND:
            case TOK_REDIRECT_IN:
            case TOK_REDIRECT_NOCLOB:
            {
                int oflags = oflags_for_redirection_type(redirect_type);
                io_file_t *new_io_file = new io_file_t(source_fd, target, oflags);
                new_io.reset(new_io_file);
                break;
            }
            
            default:
            {
                // Should be unreachable
                fprintf(stderr, "Unexpected redirection type %ld. aborting.\n", (long)redirect_type);
                PARSER_DIE();
                break;
            }
        }
        
        /* Append the new_io if we got one */
        if (new_io.get() != NULL)
        {
            result.push_back(new_io);
        }
    }
    
    if (out_chain && ! errored)
    {
        std::swap(*out_chain, result);
    }
    return ! errored;
}

process_t *parse_execution_context_t::create_boolean_process(job_t *job, const parse_node_t &bool_statement)
{
    // Handle a boolean statement
    bool skip_job = false;
    assert(bool_statement.type == symbol_boolean_statement);
    switch (bool_statement.production_idx)
    {
        // These magic numbers correspond to productions for boolean_statement
        case 0:
            // AND. Skip if the last job failed.
            skip_job = (proc_get_last_status() != 0);
            break;
            
        case 1:
            // OR. Skip if the last job succeeded.
            skip_job = (proc_get_last_status() == 0);
            break;

        case 2:
            // NOT. Negate it.
            job_set_flag(job, JOB_NEGATE, !job_get_flag(job, JOB_NEGATE));
            break;
            
        default:
        {
            fprintf(stderr, "Unexpected production in boolean statement\n");
            PARSER_DIE();
            break;
        }
    }
    
    process_t *result = NULL;
    if (! skip_job)
    {
        const parse_node_t &subject = *tree.get_child(bool_statement, 1, symbol_statement);
        result = this->create_job_process(job, subject);
    }
    return result;
}

process_t *parse_execution_context_t::create_block_process(job_t *job, const parse_node_t &statement_node)
{
    /* We handle block statements by creating INTERNAL_BLOCKs, that will bounce back to us when it's time to execute them */
    assert(statement_node.type == symbol_block_statement || statement_node.type == symbol_if_statement || statement_node.type == symbol_switch_statement);
    process_t *result = new process_t();
    result->type = INTERNAL_BLOCK;
    result->internal_block_node = this->get_offset(statement_node);
    return result;
}


/* Returns a process_t allocated with new. It's the caller's responsibility to delete it (!) */
process_t *parse_execution_context_t::create_job_process(job_t *job, const parse_node_t &statement_node)
{
    assert(statement_node.type == symbol_statement);
    assert(statement_node.child_count == 1);
    
    // Get the "specific statement" which is boolean / block / if / switch / decorated
    const parse_node_t &specific_statement = *get_child(statement_node, 0);
    
    process_t *result = NULL;
    
    switch (specific_statement.type)
    {
        case symbol_boolean_statement:
        {
            result = this->create_boolean_process(job, specific_statement);
            break;
        }
        
        case symbol_block_statement:
        case symbol_if_statement:
        case symbol_switch_statement:
        {
            result = this->create_block_process(job, specific_statement);
            break;
        }
        
        case symbol_decorated_statement:
        {
            /* Get the plain statement. It will pull out the decoration itself */
            const parse_node_t &plain_statement = tree.find_child(specific_statement, symbol_plain_statement);
            result = this->create_plain_process(job, plain_statement);
            break;
        }
        
        default:
            fprintf(stderr, "'%ls' not handled by new parser yet\n", specific_statement.describe().c_str());
            PARSER_DIE();
            break;
    }
    
    return result;
}


bool parse_execution_context_t::populate_job_from_job_node(job_t *j, const parse_node_t &job_node)
{
    assert(job_node.type == symbol_job);
    
    /* Track whether we had an error */
    bool process_errored = false;
    
    /* Tell the job what its command is */
    j->set_command(get_source(job_node));
    
    /* We are going to construct process_t structures for every statement in the job. Get the first statement. */
    const parse_node_t *statement_node = get_child(job_node, 0, symbol_statement);
    assert(statement_node != NULL);
    
    /* Create the process (may fail!) */
    j->first_process = this->create_job_process(j, *statement_node);
    if (j->first_process == NULL)
        process_errored = true;
    
    /* Construct process_ts for job continuations (pipelines), by walking the list until we hit the terminal (empty) job continuation */
    const parse_node_t *job_cont = get_child(job_node, 1, symbol_job_continuation);
    process_t *last_process = j->first_process;
    while (! process_errored && job_cont != NULL && job_cont->child_count > 0)
    {
        assert(job_cont->type == symbol_job_continuation);
        
        /* Get the statement node and make a process from it */
        const parse_node_t *statement_node = get_child(*job_cont, 1, symbol_statement);
        assert(statement_node != NULL);
        
        /* Store the new process (and maybe with an error) */
        last_process->next = this->create_job_process(j, *statement_node);
        if (last_process->next == NULL)
            process_errored = true;

        /* Link the process and get the next continuation */
        last_process = last_process->next;
        job_cont = get_child(*job_cont, 2, symbol_job_continuation);
    }
    
    /* Return success */
    return ! process_errored;
}

int parse_execution_context_t::run_1_job(const parse_node_t &job_node)
{
    // Get terminal modes
    struct termios tmodes = {};
    if (get_is_interactive())
    {
        if (tcgetattr(STDIN_FILENO, &tmodes))
        {
            // need real error handling here
            wperror(L"tcgetattr");
            return EXIT_FAILURE;
        }
    }
    
    /* Increment the eval_level for the duration of this command */
    scoped_push<int> saved_eval_level(&eval_level, eval_level + 1);
    
    /* TODO: blocks-without-redirections optimization */
    
    /* Profiling support */
    long long start_time = 0, parse_time = 0, exec_time = 0;
    const bool do_profile = profile;
    profile_item_t *profile_item = NULL;
    if (do_profile)
    {
        profile_item = new profile_item_t();
        profile_item->skipped = 1;
        profile_items.push_back(profile_item);
        start_time = get_time();
    }
    
    job_t *j = parser->job_create();
    job_set_flag(j, JOB_FOREGROUND, 1);
    job_set_flag(j, JOB_TERMINAL, job_get_flag(j, JOB_CONTROL));
    job_set_flag(j, JOB_TERMINAL, job_get_flag(j, JOB_CONTROL) \
                 && (!is_subshell && !is_event));
    job_set_flag(j, JOB_SKIP_NOTIFICATION, is_subshell \
                 || is_block \
                 || is_event \
                 || (!get_is_interactive()));
    
    parser->current_block()->job = j;
    
    /* Populate the job. This may fail for reasons like command_not_found */
    bool process_errored = ! this->populate_job_from_job_node(j, job_node);

    /* Store time it took to 'parse' the command */
    if (do_profile)
    {
        parse_time = get_time();
        profile_item->cmd = j->command();
        profile_item->skipped=parser->current_block()->skip;
    }
    
    if (! process_errored)
    {
        /* Check to see if this contained any external commands */
        bool job_contained_external_command = false;
        for (const process_t *proc = j->first_process; proc != NULL; proc = proc->next)
        {
            if (proc->type == EXTERNAL)
            {
                job_contained_external_command = true;
                break;
            }
        }
        
        /* Actually execute the job */
        exec_job(*this->parser, j);
        
        /* Only external commands require a new fishd barrier */
        if (!job_contained_external_command)
        {
            set_proc_had_barrier(false);
        }
    }

    /* Need support for skipped_exec here */
    if (do_profile)
    {
        exec_time = get_time();
        profile_item->level=eval_level;
        profile_item->parse = (int)(parse_time-start_time);
        profile_item->exec=(int)(exec_time-parse_time);
        profile_item->skipped = process_errored;
    }

    /* Set the last status to 1 if the job could not be executed */
    if (process_errored)
        proc_set_last_status(1);
    const int ret = proc_get_last_status();
    
    /* Clean up jobs. Do this after we've determined the return value, since this may trigger event handlers */
    job_reap(0);
    
    /* All done */
    return ret;
}

void parse_execution_context_t::run_job_list(const parse_node_t &job_list_node)
{
    assert(job_list_node.type == symbol_job_list);
    
    const parse_node_t *job_list = &job_list_node;
    while (job_list != NULL)
    {
        assert(job_list->type == symbol_job_list);
        
        // These correspond to the three productions of job_list
        // Try pulling out a job
        const parse_node_t *job = NULL;
        switch (job_list->production_idx)
        {
            case 0: // empty
                job_list = NULL;
                break;
            
            case 1: //job, job_list
                job = get_child(*job_list, 0, symbol_job);
                job_list = get_child(*job_list, 1, symbol_job_list);
                break;
            
            case 2: //blank line, job_list
                job = NULL;
                job_list = get_child(*job_list, 1, symbol_job_list);
                break;
                
            default: //if we get here, it means more productions have been added to job_list, which is bad
                PARSER_DIE();
        }
        
        if (job != NULL)
        {
            this->run_1_job(*job);
        }
    }
    
}


void parse_execution_context_t::eval_job_list(const parse_node_t &job_list_node)
{
    this->run_job_list(job_list_node);
}
