// Programmatic representation of fish code.
#ifndef FISH_PARSE_PRODUCTIONS_H
#define FISH_PARSE_PRODUCTIONS_H

#include <memory>

#include "ast.h"
#include "common.h"
#include "parse_constants.h"
#include "tokenizer.h"

#if INCLUDE_RUST_HEADERS
#include "parse_tree.rs.h"
using parsed_source_ref_t = ParsedSourceRefFFI;
#else
struct ParsedSourceRefFFI;
using parsed_source_ref_t = ParsedSourceRefFFI;
#endif

/// Error message when a command may not be in a pipeline.
#define INVALID_PIPELINE_CMD_ERR_MSG _(L"The '%ls' command can not be used in a pipeline")

#endif
