#include "parse_productions.h"

using namespace parse_productions;

#define PRODUCTIONS(sym) static const Production_t sym##_productions

PRODUCTIONS(job_list) =
    {
        {},
        {symbol_job, symbol_job_list},
        {parse_token_type_end, symbol_job_list}
    };



/* A job_list is a list of jobs, separated by semicolons or newlines */

DEC(job_list) {
    symbol_job_list,
    {
        {},
        {symbol_job, symbol_job_list},
        {parse_token_type_end, symbol_job_list}
    },
    resolve_job_list
};

static int resolve_job_list(parse_token_type_t token_type, parse_keyword_t token_keyword)
    {
        switch (token_type)
        {
            case parse_token_type_string:
                // 'end' is special
                switch (token_keyword)
                {
                    case parse_keyword_end:
                    case parse_keyword_else:
                        // End this job list
                        return 0;

                    default:
                        // Normal string
                        return 1;
                }

            case parse_token_type_pipe:
            case parse_token_type_redirection:
            case parse_token_type_background:
                return 1;

            case parse_token_type_end:
                // Empty line
                return 2;

            case parse_token_type_terminate:
                // no more commands, just transition to empty
                return 0;
                break;

            default:
                return NO_PRODUCTION;
        }
    }