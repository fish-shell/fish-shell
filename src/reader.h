#ifndef FISH_READER_H
#define FISH_READER_H

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "complete.h"
#include "env.h"
#include "highlight.h"
#include "io.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parser.h"
#include "wutil.h"

#if INCLUDE_RUST_HEADERS
#include "reader.rs.h"
#endif

#include "editable_line.h"

#endif
