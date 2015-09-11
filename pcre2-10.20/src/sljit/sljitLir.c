/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2012 Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *      of conditions and the following disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sljitLir.h"

#define CHECK_ERROR() \
	do { \
		if (SLJIT_UNLIKELY(compiler->error)) \
			return compiler->error; \
	} while (0)

#define CHECK_ERROR_PTR() \
	do { \
		if (SLJIT_UNLIKELY(compiler->error)) \
			return NULL; \
	} while (0)

#define FAIL_IF(expr) \
	do { \
		if (SLJIT_UNLIKELY(expr)) \
			return compiler->error; \
	} while (0)

#define PTR_FAIL_IF(expr) \
	do { \
		if (SLJIT_UNLIKELY(expr)) \
			return NULL; \
	} while (0)

#define FAIL_IF_NULL(ptr) \
	do { \
		if (SLJIT_UNLIKELY(!(ptr))) { \
			compiler->error = SLJIT_ERR_ALLOC_FAILED; \
			return SLJIT_ERR_ALLOC_FAILED; \
		} \
	} while (0)

#define PTR_FAIL_IF_NULL(ptr) \
	do { \
		if (SLJIT_UNLIKELY(!(ptr))) { \
			compiler->error = SLJIT_ERR_ALLOC_FAILED; \
			return NULL; \
		} \
	} while (0)

#define PTR_FAIL_WITH_EXEC_IF(ptr) \
	do { \
		if (SLJIT_UNLIKELY(!(ptr))) { \
			compiler->error = SLJIT_ERR_EX_ALLOC_FAILED; \
			return NULL; \
		} \
	} while (0)

#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)

#define GET_OPCODE(op) \
	((op) & ~(SLJIT_INT_OP | SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C | SLJIT_KEEP_FLAGS))

#define GET_FLAGS(op) \
	((op) & (SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C))

#define GET_ALL_FLAGS(op) \
	((op) & (SLJIT_INT_OP | SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C | SLJIT_KEEP_FLAGS))

#define TYPE_CAST_NEEDED(op) \
	(((op) >= SLJIT_MOV_UB && (op) <= SLJIT_MOV_SH) || ((op) >= SLJIT_MOVU_UB && (op) <= SLJIT_MOVU_SH))

#define BUF_SIZE	4096

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
#define ABUF_SIZE	2048
#else
#define ABUF_SIZE	4096
#endif

/* Parameter parsing. */
#define REG_MASK		0x3f
#define OFFS_REG(reg)		(((reg) >> 8) & REG_MASK)
#define OFFS_REG_MASK		(REG_MASK << 8)
#define TO_OFFS_REG(reg)	((reg) << 8)
/* When reg cannot be unused. */
#define FAST_IS_REG(reg)	((reg) <= REG_MASK)
/* When reg can be unused. */
#define SLOW_IS_REG(reg)	((reg) > 0 && (reg) <= REG_MASK)

/* Jump flags. */
#define JUMP_LABEL	0x1
#define JUMP_ADDR	0x2
/* SLJIT_REWRITABLE_JUMP is 0x1000. */

#if (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
#	define PATCH_MB	0x4
#	define PATCH_MW	0x8
#if (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
#	define PATCH_MD	0x10
#endif
#endif

#if (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5) || (defined SLJIT_CONFIG_ARM_V7 && SLJIT_CONFIG_ARM_V7)
#	define IS_BL		0x4
#	define PATCH_B		0x8
#endif

#if (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5)
#	define CPOOL_SIZE	512
#endif

#if (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
#	define IS_COND		0x04
#	define IS_BL		0x08
	/* conditional + imm8 */
#	define PATCH_TYPE1	0x10
	/* conditional + imm20 */
#	define PATCH_TYPE2	0x20
	/* IT + imm24 */
#	define PATCH_TYPE3	0x30
	/* imm11 */
#	define PATCH_TYPE4	0x40
	/* imm24 */
#	define PATCH_TYPE5	0x50
	/* BL + imm24 */
#	define PATCH_BL		0x60
	/* 0xf00 cc code for branches */
#endif

#if (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
#	define IS_COND		0x004
#	define IS_CBZ		0x008
#	define IS_BL		0x010
#	define PATCH_B		0x020
#	define PATCH_COND	0x040
#	define PATCH_ABS48	0x080
#	define PATCH_ABS64	0x100
#endif

#if (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
#	define IS_COND		0x004
#	define IS_CALL		0x008
#	define PATCH_B		0x010
#	define PATCH_ABS_B	0x020
#if (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
#	define PATCH_ABS32	0x040
#	define PATCH_ABS48	0x080
#endif
#	define REMOVE_COND	0x100
#endif

#if (defined SLJIT_CONFIG_MIPS && SLJIT_CONFIG_MIPS)
#	define IS_MOVABLE	0x004
#	define IS_JAL		0x008
#	define IS_CALL		0x010
#	define IS_BIT26_COND	0x020
#	define IS_BIT16_COND	0x040

#	define IS_COND		(IS_BIT26_COND | IS_BIT16_COND)

#	define PATCH_B		0x080
#	define PATCH_J		0x100

#if (defined SLJIT_CONFIG_MIPS_64 && SLJIT_CONFIG_MIPS_64)
#	define PATCH_ABS32	0x200
#	define PATCH_ABS48	0x400
#endif

	/* instruction types */
#	define MOVABLE_INS	0
	/* 1 - 31 last destination register */
	/* no destination (i.e: store) */
#	define UNMOVABLE_INS	32
	/* FPU status register */
#	define FCSR_FCC		33
#endif

#if (defined SLJIT_CONFIG_TILEGX && SLJIT_CONFIG_TILEGX)
#	define IS_JAL		0x04
#	define IS_COND		0x08

#	define PATCH_B		0x10
#	define PATCH_J		0x20
#endif

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#	define IS_MOVABLE	0x04
#	define IS_COND		0x08
#	define IS_CALL		0x10

#	define PATCH_B		0x20
#	define PATCH_CALL	0x40

	/* instruction types */
#	define MOVABLE_INS	0
	/* 1 - 31 last destination register */
	/* no destination (i.e: store) */
#	define UNMOVABLE_INS	32

#	define DST_INS_MASK	0xff

	/* ICC_SET is the same as SET_FLAGS. */
#	define ICC_IS_SET	(1 << 23)
#	define FCC_IS_SET	(1 << 24)
#endif

/* Stack management. */

#define GET_SAVED_REGISTERS_SIZE(scratches, saveds, extra) \
	(((scratches < SLJIT_NUMBER_OF_SCRATCH_REGISTERS ? 0 : (scratches - SLJIT_NUMBER_OF_SCRATCH_REGISTERS)) + \
		(saveds < SLJIT_NUMBER_OF_SAVED_REGISTERS ? saveds : SLJIT_NUMBER_OF_SAVED_REGISTERS) + \
		extra) * sizeof(sljit_sw))

#define ADJUST_LOCAL_OFFSET(p, i) \
	if ((p) == (SLJIT_MEM1(SLJIT_SP))) \
		(i) += SLJIT_LOCALS_OFFSET;

#endif /* !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED) */

/* Utils can still be used even if SLJIT_CONFIG_UNSUPPORTED is set. */
#include "sljitUtils.c"

#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)

#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
#include "sljitExecAllocator.c"
#endif

/* Argument checking features. */

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)

/* Returns with error when an invalid argument is passed. */

#define CHECK_ARGUMENT(x) \
	do { \
		if (SLJIT_UNLIKELY(!(x))) \
			return 1; \
	} while (0)

#define CHECK_RETURN_TYPE sljit_si
#define CHECK_RETURN_OK return 0

#define CHECK(x) \
	do { \
		if (SLJIT_UNLIKELY(x)) { \
			compiler->error = SLJIT_ERR_BAD_ARGUMENT; \
			return SLJIT_ERR_BAD_ARGUMENT; \
		} \
	} while (0)

#define CHECK_PTR(x) \
	do { \
		if (SLJIT_UNLIKELY(x)) { \
			compiler->error = SLJIT_ERR_BAD_ARGUMENT; \
			return NULL; \
		} \
	} while (0)

#define CHECK_REG_INDEX(x) \
	do { \
		if (SLJIT_UNLIKELY(x)) { \
			return -2; \
		} \
	} while (0)

#elif (defined SLJIT_DEBUG && SLJIT_DEBUG)

/* Assertion failure occures if an invalid argument is passed. */
#undef SLJIT_ARGUMENT_CHECKS
#define SLJIT_ARGUMENT_CHECKS 1

#define CHECK_ARGUMENT(x) SLJIT_ASSERT(x)
#define CHECK_RETURN_TYPE void
#define CHECK_RETURN_OK return
#define CHECK(x) x
#define CHECK_PTR(x) x
#define CHECK_REG_INDEX(x) x

#elif (defined SLJIT_VERBOSE && SLJIT_VERBOSE)

/* Arguments are not checked. */
#define CHECK_RETURN_TYPE void
#define CHECK_RETURN_OK return
#define CHECK(x) x
#define CHECK_PTR(x) x
#define CHECK_REG_INDEX(x) x

#else

/* Arguments are not checked. */
#define CHECK(x)
#define CHECK_PTR(x)
#define CHECK_REG_INDEX(x)

#endif /* SLJIT_ARGUMENT_CHECKS */

/* --------------------------------------------------------------------- */
/*  Public functions                                                     */
/* --------------------------------------------------------------------- */

#if (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5) || (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
#define SLJIT_NEEDS_COMPILER_INIT 1
static sljit_si compiler_initialized = 0;
/* A thread safe initialization. */
static void init_compiler(void);
#endif

SLJIT_API_FUNC_ATTRIBUTE struct sljit_compiler* sljit_create_compiler(void *allocator_data)
{
	struct sljit_compiler *compiler = (struct sljit_compiler*)SLJIT_MALLOC(sizeof(struct sljit_compiler), allocator_data);
	if (!compiler)
		return NULL;
	SLJIT_ZEROMEM(compiler, sizeof(struct sljit_compiler));

	SLJIT_COMPILE_ASSERT(
		sizeof(sljit_sb) == 1 && sizeof(sljit_ub) == 1
		&& sizeof(sljit_sh) == 2 && sizeof(sljit_uh) == 2
		&& sizeof(sljit_si) == 4 && sizeof(sljit_ui) == 4
		&& (sizeof(sljit_p) == 4 || sizeof(sljit_p) == 8)
		&& sizeof(sljit_p) <= sizeof(sljit_sw)
		&& (sizeof(sljit_sw) == 4 || sizeof(sljit_sw) == 8)
		&& (sizeof(sljit_uw) == 4 || sizeof(sljit_uw) == 8),
		invalid_integer_types);
	SLJIT_COMPILE_ASSERT(SLJIT_INT_OP == SLJIT_SINGLE_OP,
		int_op_and_single_op_must_be_the_same);
	SLJIT_COMPILE_ASSERT(SLJIT_REWRITABLE_JUMP != SLJIT_SINGLE_OP,
		rewritable_jump_and_single_op_must_not_be_the_same);

	/* Only the non-zero members must be set. */
	compiler->error = SLJIT_SUCCESS;

	compiler->allocator_data = allocator_data;
	compiler->buf = (struct sljit_memory_fragment*)SLJIT_MALLOC(BUF_SIZE, allocator_data);
	compiler->abuf = (struct sljit_memory_fragment*)SLJIT_MALLOC(ABUF_SIZE, allocator_data);

	if (!compiler->buf || !compiler->abuf) {
		if (compiler->buf)
			SLJIT_FREE(compiler->buf, allocator_data);
		if (compiler->abuf)
			SLJIT_FREE(compiler->abuf, allocator_data);
		SLJIT_FREE(compiler, allocator_data);
		return NULL;
	}

	compiler->buf->next = NULL;
	compiler->buf->used_size = 0;
	compiler->abuf->next = NULL;
	compiler->abuf->used_size = 0;

	compiler->scratches = -1;
	compiler->saveds = -1;
	compiler->fscratches = -1;
	compiler->fsaveds = -1;
	compiler->local_size = -1;

#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	compiler->args = -1;
#endif

#if (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5)
	compiler->cpool = (sljit_uw*)SLJIT_MALLOC(CPOOL_SIZE * sizeof(sljit_uw)
		+ CPOOL_SIZE * sizeof(sljit_ub), allocator_data);
	if (!compiler->cpool) {
		SLJIT_FREE(compiler->buf, allocator_data);
		SLJIT_FREE(compiler->abuf, allocator_data);
		SLJIT_FREE(compiler, allocator_data);
		return NULL;
	}
	compiler->cpool_unique = (sljit_ub*)(compiler->cpool + CPOOL_SIZE);
	compiler->cpool_diff = 0xffffffff;
#endif

#if (defined SLJIT_CONFIG_MIPS && SLJIT_CONFIG_MIPS)
	compiler->delay_slot = UNMOVABLE_INS;
#endif

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
	compiler->delay_slot = UNMOVABLE_INS;
#endif

#if (defined SLJIT_NEEDS_COMPILER_INIT && SLJIT_NEEDS_COMPILER_INIT)
	if (!compiler_initialized) {
		init_compiler();
		compiler_initialized = 1;
	}
#endif

	return compiler;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_compiler(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	struct sljit_memory_fragment *curr;
	void *allocator_data = compiler->allocator_data;
	SLJIT_UNUSED_ARG(allocator_data);

	buf = compiler->buf;
	while (buf) {
		curr = buf;
		buf = buf->next;
		SLJIT_FREE(curr, allocator_data);
	}

	buf = compiler->abuf;
	while (buf) {
		curr = buf;
		buf = buf->next;
		SLJIT_FREE(curr, allocator_data);
	}

#if (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5)
	SLJIT_FREE(compiler->cpool, allocator_data);
#endif
	SLJIT_FREE(compiler, allocator_data);
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_compiler_memory_error(struct sljit_compiler *compiler)
{
	if (compiler->error == SLJIT_SUCCESS)
		compiler->error = SLJIT_ERR_ALLOC_FAILED;
}

#if (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_code(void* code)
{
	/* Remove thumb mode flag. */
	SLJIT_FREE_EXEC((void*)((sljit_uw)code & ~0x1));
}
#elif (defined SLJIT_INDIRECT_CALL && SLJIT_INDIRECT_CALL)
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_code(void* code)
{
	/* Resolve indirection. */
	code = (void*)(*(sljit_uw*)code);
	SLJIT_FREE_EXEC(code);
}
#else
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}
#endif

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label)
{
	if (SLJIT_LIKELY(!!jump) && SLJIT_LIKELY(!!label)) {
		jump->flags &= ~JUMP_ADDR;
		jump->flags |= JUMP_LABEL;
		jump->u.label = label;
	}
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_target(struct sljit_jump *jump, sljit_uw target)
{
	if (SLJIT_LIKELY(!!jump)) {
		jump->flags &= ~JUMP_LABEL;
		jump->flags |= JUMP_ADDR;
		jump->u.target = target;
	}
}

/* --------------------------------------------------------------------- */
/*  Private functions                                                    */
/* --------------------------------------------------------------------- */

static void* ensure_buf(struct sljit_compiler *compiler, sljit_uw size)
{
	sljit_ub *ret;
	struct sljit_memory_fragment *new_frag;

	SLJIT_ASSERT(size <= 256);
	if (compiler->buf->used_size + size <= (BUF_SIZE - (sljit_uw)SLJIT_OFFSETOF(struct sljit_memory_fragment, memory))) {
		ret = compiler->buf->memory + compiler->buf->used_size;
		compiler->buf->used_size += size;
		return ret;
	}
	new_frag = (struct sljit_memory_fragment*)SLJIT_MALLOC(BUF_SIZE, compiler->allocator_data);
	PTR_FAIL_IF_NULL(new_frag);
	new_frag->next = compiler->buf;
	compiler->buf = new_frag;
	new_frag->used_size = size;
	return new_frag->memory;
}

static void* ensure_abuf(struct sljit_compiler *compiler, sljit_uw size)
{
	sljit_ub *ret;
	struct sljit_memory_fragment *new_frag;

	SLJIT_ASSERT(size <= 256);
	if (compiler->abuf->used_size + size <= (ABUF_SIZE - (sljit_uw)SLJIT_OFFSETOF(struct sljit_memory_fragment, memory))) {
		ret = compiler->abuf->memory + compiler->abuf->used_size;
		compiler->abuf->used_size += size;
		return ret;
	}
	new_frag = (struct sljit_memory_fragment*)SLJIT_MALLOC(ABUF_SIZE, compiler->allocator_data);
	PTR_FAIL_IF_NULL(new_frag);
	new_frag->next = compiler->abuf;
	compiler->abuf = new_frag;
	new_frag->used_size = size;
	return new_frag->memory;
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_alloc_memory(struct sljit_compiler *compiler, sljit_si size)
{
	CHECK_ERROR_PTR();

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	if (size <= 0 || size > 128)
		return NULL;
	size = (size + 7) & ~7;
#else
	if (size <= 0 || size > 64)
		return NULL;
	size = (size + 3) & ~3;
#endif
	return ensure_abuf(compiler, size);
}

static SLJIT_INLINE void reverse_buf(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf = compiler->buf;
	struct sljit_memory_fragment *prev = NULL;
	struct sljit_memory_fragment *tmp;

	do {
		tmp = buf->next;
		buf->next = prev;
		prev = buf;
		buf = tmp;
	} while (buf != NULL);

	compiler->buf = prev;
}

static SLJIT_INLINE void set_emit_enter(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	SLJIT_UNUSED_ARG(args);
	SLJIT_UNUSED_ARG(local_size);

	compiler->options = options;
	compiler->scratches = scratches;
	compiler->saveds = saveds;
	compiler->fscratches = fscratches;
	compiler->fsaveds = fsaveds;
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->logical_local_size = local_size;
#endif
}

static SLJIT_INLINE void set_set_context(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	SLJIT_UNUSED_ARG(args);
	SLJIT_UNUSED_ARG(local_size);

	compiler->options = options;
	compiler->scratches = scratches;
	compiler->saveds = saveds;
	compiler->fscratches = fscratches;
	compiler->fsaveds = fsaveds;
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->logical_local_size = local_size;
#endif
}

static SLJIT_INLINE void set_label(struct sljit_label *label, struct sljit_compiler *compiler)
{
	label->next = NULL;
	label->size = compiler->size;
	if (compiler->last_label)
		compiler->last_label->next = label;
	else
		compiler->labels = label;
	compiler->last_label = label;
}

static SLJIT_INLINE void set_jump(struct sljit_jump *jump, struct sljit_compiler *compiler, sljit_si flags)
{
	jump->next = NULL;
	jump->flags = flags;
	if (compiler->last_jump)
		compiler->last_jump->next = jump;
	else
		compiler->jumps = jump;
	compiler->last_jump = jump;
}

static SLJIT_INLINE void set_const(struct sljit_const *const_, struct sljit_compiler *compiler)
{
	const_->next = NULL;
	const_->addr = compiler->size;
	if (compiler->last_const)
		compiler->last_const->next = const_;
	else
		compiler->consts = const_;
	compiler->last_const = const_;
}

#define ADDRESSING_DEPENDS_ON(exp, reg) \
	(((exp) & SLJIT_MEM) && (((exp) & REG_MASK) == reg || OFFS_REG(exp) == reg))

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
#define FUNCTION_CHECK_OP() \
	CHECK_ARGUMENT(!GET_FLAGS(op) || !(op & SLJIT_KEEP_FLAGS)); \
	switch (GET_OPCODE(op)) { \
	case SLJIT_NOT: \
	case SLJIT_CLZ: \
	case SLJIT_AND: \
	case SLJIT_OR: \
	case SLJIT_XOR: \
	case SLJIT_SHL: \
	case SLJIT_LSHR: \
	case SLJIT_ASHR: \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C))); \
		break; \
	case SLJIT_NEG: \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_C))); \
		break; \
	case SLJIT_MUL: \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_C))); \
		break; \
	case SLJIT_ADD: \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_U | SLJIT_SET_S))); \
		break; \
	case SLJIT_SUB: \
		break; \
	case SLJIT_ADDC: \
	case SLJIT_SUBC: \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O))); \
		break; \
	case SLJIT_BREAKPOINT: \
	case SLJIT_NOP: \
	case SLJIT_LUMUL: \
	case SLJIT_LSMUL: \
	case SLJIT_MOV: \
	case SLJIT_MOV_UI: \
	case SLJIT_MOV_P: \
	case SLJIT_MOVU: \
	case SLJIT_MOVU_UI: \
	case SLJIT_MOVU_P: \
		/* Nothing allowed */ \
		CHECK_ARGUMENT(!(op & (SLJIT_INT_OP | SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C | SLJIT_KEEP_FLAGS))); \
		break; \
	default: \
		/* Only SLJIT_INT_OP or SLJIT_SINGLE_OP is allowed. */ \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C | SLJIT_KEEP_FLAGS))); \
		break; \
	}

#define FUNCTION_CHECK_FOP() \
	CHECK_ARGUMENT(!GET_FLAGS(op) || !(op & SLJIT_KEEP_FLAGS)); \
	switch (GET_OPCODE(op)) { \
	case SLJIT_DCMP: \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_U | SLJIT_SET_O | SLJIT_SET_C | SLJIT_KEEP_FLAGS))); \
		CHECK_ARGUMENT((op & (SLJIT_SET_E | SLJIT_SET_S))); \
		break; \
	default: \
		/* Only SLJIT_INT_OP or SLJIT_SINGLE_OP is allowed. */ \
		CHECK_ARGUMENT(!(op & (SLJIT_SET_E | SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C | SLJIT_KEEP_FLAGS))); \
		break; \
	}

#define FUNCTION_CHECK_IS_REG(r) \
	(((r) >= SLJIT_R0 && (r) < (SLJIT_R0 + compiler->scratches)) || \
	((r) > (SLJIT_S0 - compiler->saveds) && (r) <= SLJIT_S0))

#define FUNCTION_CHECK_IS_REG_OR_UNUSED(r) \
	((r) == SLJIT_UNUSED || \
	((r) >= SLJIT_R0 && (r) < (SLJIT_R0 + compiler->scratches)) || \
	((r) > (SLJIT_S0 - compiler->saveds) && (r) <= SLJIT_S0))

#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
#define CHECK_NOT_VIRTUAL_REGISTER(p) \
	CHECK_ARGUMENT((p) < SLJIT_R3 || (p) > SLJIT_R6);
#else
#define CHECK_NOT_VIRTUAL_REGISTER(p)
#endif

#define FUNCTION_CHECK_SRC(p, i) \
	CHECK_ARGUMENT(compiler->scratches != -1 && compiler->saveds != -1); \
	if (FUNCTION_CHECK_IS_REG(p)) \
		CHECK_ARGUMENT((i) == 0); \
	else if ((p) == SLJIT_IMM) \
		; \
	else if ((p) == (SLJIT_MEM1(SLJIT_SP))) \
		CHECK_ARGUMENT((i) >= 0 && (i) < compiler->logical_local_size); \
	else { \
		CHECK_ARGUMENT((p) & SLJIT_MEM); \
		CHECK_ARGUMENT(FUNCTION_CHECK_IS_REG_OR_UNUSED((p) & REG_MASK)); \
		CHECK_NOT_VIRTUAL_REGISTER((p) & REG_MASK); \
		if ((p) & OFFS_REG_MASK) { \
			CHECK_ARGUMENT(((p) & REG_MASK) != SLJIT_UNUSED); \
			CHECK_ARGUMENT(FUNCTION_CHECK_IS_REG(OFFS_REG(p))); \
			CHECK_NOT_VIRTUAL_REGISTER(OFFS_REG(p)); \
			CHECK_ARGUMENT(!((i) & ~0x3)); \
		} \
		CHECK_ARGUMENT(!((p) & ~(SLJIT_MEM | SLJIT_IMM | REG_MASK | OFFS_REG_MASK))); \
	}

#define FUNCTION_CHECK_DST(p, i) \
	CHECK_ARGUMENT(compiler->scratches != -1 && compiler->saveds != -1); \
	if (FUNCTION_CHECK_IS_REG_OR_UNUSED(p)) \
		CHECK_ARGUMENT((i) == 0); \
	else if ((p) == (SLJIT_MEM1(SLJIT_SP))) \
		CHECK_ARGUMENT((i) >= 0 && (i) < compiler->logical_local_size); \
	else { \
		CHECK_ARGUMENT((p) & SLJIT_MEM); \
		CHECK_ARGUMENT(FUNCTION_CHECK_IS_REG_OR_UNUSED((p) & REG_MASK)); \
		CHECK_NOT_VIRTUAL_REGISTER((p) & REG_MASK); \
		if ((p) & OFFS_REG_MASK) { \
			CHECK_ARGUMENT(((p) & REG_MASK) != SLJIT_UNUSED); \
			CHECK_ARGUMENT(FUNCTION_CHECK_IS_REG(OFFS_REG(p))); \
			CHECK_NOT_VIRTUAL_REGISTER(OFFS_REG(p)); \
			CHECK_ARGUMENT(!((i) & ~0x3)); \
		} \
		CHECK_ARGUMENT(!((p) & ~(SLJIT_MEM | SLJIT_IMM | REG_MASK | OFFS_REG_MASK))); \
	}

#define FUNCTION_FCHECK(p, i) \
	CHECK_ARGUMENT(compiler->fscratches != -1 && compiler->fsaveds != -1); \
	if (((p) >= SLJIT_FR0 && (p) < (SLJIT_FR0 + compiler->fscratches)) || \
			((p) > (SLJIT_FS0 - compiler->fsaveds) && (p) <= SLJIT_FS0)) \
		CHECK_ARGUMENT(i == 0); \
	else if ((p) == (SLJIT_MEM1(SLJIT_SP))) \
		CHECK_ARGUMENT((i) >= 0 && (i) < compiler->logical_local_size); \
	else { \
		CHECK_ARGUMENT((p) & SLJIT_MEM); \
		CHECK_ARGUMENT(FUNCTION_CHECK_IS_REG_OR_UNUSED((p) & REG_MASK)); \
		CHECK_NOT_VIRTUAL_REGISTER((p) & REG_MASK); \
		if ((p) & OFFS_REG_MASK) { \
			CHECK_ARGUMENT(((p) & REG_MASK) != SLJIT_UNUSED); \
			CHECK_ARGUMENT(FUNCTION_CHECK_IS_REG(OFFS_REG(p))); \
			CHECK_NOT_VIRTUAL_REGISTER(OFFS_REG(p)); \
			CHECK_ARGUMENT(((p) & OFFS_REG_MASK) != TO_OFFS_REG(SLJIT_SP) && !(i & ~0x3)); \
		} \
		CHECK_ARGUMENT(!((p) & ~(SLJIT_MEM | SLJIT_IMM | REG_MASK | OFFS_REG_MASK))); \
	}

#define FUNCTION_CHECK_OP1() \
	if (GET_OPCODE(op) >= SLJIT_MOVU && GET_OPCODE(op) <= SLJIT_MOVU_P) { \
		CHECK_ARGUMENT(!(src & SLJIT_MEM) || (src & REG_MASK) != SLJIT_SP); \
		CHECK_ARGUMENT(!(dst & SLJIT_MEM) || (dst & REG_MASK) != SLJIT_SP); \
		if ((src & SLJIT_MEM) && (src & REG_MASK)) \
			CHECK_ARGUMENT((dst & REG_MASK) != (src & REG_MASK) && OFFS_REG(dst) != (src & REG_MASK)); \
	}

#endif /* SLJIT_ARGUMENT_CHECKS */

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)

SLJIT_API_FUNC_ATTRIBUTE void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose)
{
	compiler->verbose = verbose;
}

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#ifdef _WIN64
#	define SLJIT_PRINT_D	"I64"
#else
#	define SLJIT_PRINT_D	"l"
#endif
#else
#	define SLJIT_PRINT_D	""
#endif

#define sljit_verbose_reg(compiler, r) \
	do { \
		if ((r) < (SLJIT_R0 + compiler->scratches)) \
			fprintf(compiler->verbose, "r%d", (r) - SLJIT_R0); \
		else \
			fprintf(compiler->verbose, "s%d", SLJIT_NUMBER_OF_REGISTERS - (r)); \
	} while (0)

#define sljit_verbose_param(compiler, p, i) \
	if ((p) & SLJIT_IMM) \
		fprintf(compiler->verbose, "#%" SLJIT_PRINT_D "d", (i)); \
	else if ((p) & SLJIT_MEM) { \
		if ((p) & REG_MASK) { \
			fputc('[', compiler->verbose); \
			sljit_verbose_reg(compiler, (p) & REG_MASK); \
			if ((p) & OFFS_REG_MASK) { \
				fprintf(compiler->verbose, " + "); \
				sljit_verbose_reg(compiler, OFFS_REG(p)); \
				if (i) \
					fprintf(compiler->verbose, " * %d", 1 << (i)); \
			} \
			else if (i) \
				fprintf(compiler->verbose, " + %" SLJIT_PRINT_D "d", (i)); \
			fputc(']', compiler->verbose); \
		} \
		else \
			fprintf(compiler->verbose, "[#%" SLJIT_PRINT_D "d]", (i)); \
	} else if (p) \
		sljit_verbose_reg(compiler, p); \
	else \
		fprintf(compiler->verbose, "unused");

#define sljit_verbose_fparam(compiler, p, i) \
	if ((p) & SLJIT_MEM) { \
		if ((p) & REG_MASK) { \
			fputc('[', compiler->verbose); \
			sljit_verbose_reg(compiler, (p) & REG_MASK); \
			if ((p) & OFFS_REG_MASK) { \
				fprintf(compiler->verbose, " + "); \
				sljit_verbose_reg(compiler, OFFS_REG(p)); \
				if (i) \
					fprintf(compiler->verbose, "%d", 1 << (i)); \
			} \
			else if (i) \
				fprintf(compiler->verbose, "%" SLJIT_PRINT_D "d", (i)); \
			fputc(']', compiler->verbose); \
		} \
		else \
			fprintf(compiler->verbose, "[#%" SLJIT_PRINT_D "d]", (i)); \
	} \
	else { \
		if ((p) < (SLJIT_FR0 + compiler->fscratches)) \
			fprintf(compiler->verbose, "fr%d", (p) - SLJIT_FR0); \
		else \
			fprintf(compiler->verbose, "fs%d", SLJIT_NUMBER_OF_FLOAT_REGISTERS - (p)); \
	}

static SLJIT_CONST char* op0_names[] = {
	(char*)"breakpoint", (char*)"nop", (char*)"lumul", (char*)"lsmul",
	(char*)"udivmod", (char*)"sdivmod", (char*)"udivi", (char*)"sdivi"
};

static SLJIT_CONST char* op1_names[] = {
	(char*)"mov", (char*)"mov_ub", (char*)"mov_sb", (char*)"mov_uh",
	(char*)"mov_sh", (char*)"mov_ui", (char*)"mov_si", (char*)"mov_p",
	(char*)"movu", (char*)"movu_ub", (char*)"movu_sb", (char*)"movu_uh",
	(char*)"movu_sh", (char*)"movu_ui", (char*)"movu_si", (char*)"movu_p",
	(char*)"not", (char*)"neg", (char*)"clz",
};

static SLJIT_CONST char* op2_names[] = {
	(char*)"add", (char*)"addc", (char*)"sub", (char*)"subc",
	(char*)"mul", (char*)"and", (char*)"or", (char*)"xor",
	(char*)"shl", (char*)"lshr", (char*)"ashr",
};

static SLJIT_CONST char* fop1_names[] = {
	(char*)"mov", (char*)"conv", (char*)"conv", (char*)"conv",
	(char*)"conv", (char*)"conv", (char*)"cmp", (char*)"neg",
	(char*)"abs",
};

static SLJIT_CONST char* fop2_names[] = {
	(char*)"add", (char*)"sub", (char*)"mul", (char*)"div"
};

#define JUMP_PREFIX(type) \
	((type & 0xff) <= SLJIT_MUL_NOT_OVERFLOW ? ((type & SLJIT_INT_OP) ? "i_" : "") \
	: ((type & 0xff) <= SLJIT_D_ORDERED ? ((type & SLJIT_SINGLE_OP) ? "s_" : "d_") : ""))

static char* jump_names[] = {
	(char*)"equal", (char*)"not_equal",
	(char*)"less", (char*)"greater_equal",
	(char*)"greater", (char*)"less_equal",
	(char*)"sig_less", (char*)"sig_greater_equal",
	(char*)"sig_greater", (char*)"sig_less_equal",
	(char*)"overflow", (char*)"not_overflow",
	(char*)"mul_overflow", (char*)"mul_not_overflow",
	(char*)"equal", (char*)"not_equal",
	(char*)"less", (char*)"greater_equal",
	(char*)"greater", (char*)"less_equal",
	(char*)"unordered", (char*)"ordered",
	(char*)"jump", (char*)"fast_call",
	(char*)"call0", (char*)"call1", (char*)"call2", (char*)"call3"
};

#endif /* SLJIT_VERBOSE */

/* --------------------------------------------------------------------- */
/*  Arch dependent                                                       */
/* --------------------------------------------------------------------- */

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS) \
	|| (defined SLJIT_VERBOSE && SLJIT_VERBOSE)

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_generate_code(struct sljit_compiler *compiler)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	struct sljit_jump *jump;
#endif

	SLJIT_UNUSED_ARG(compiler);

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(compiler->size > 0);
	jump = compiler->jumps;
	while (jump) {
		/* All jumps have target. */
		CHECK_ARGUMENT(jump->flags & (JUMP_LABEL | JUMP_ADDR));
		jump = jump->next;
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_enter(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	SLJIT_UNUSED_ARG(compiler);

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(!(options & ~SLJIT_DOUBLE_ALIGNMENT));
	CHECK_ARGUMENT(args >= 0 && args <= 3);
	CHECK_ARGUMENT(scratches >= 0 && scratches <= SLJIT_NUMBER_OF_REGISTERS);
	CHECK_ARGUMENT(saveds >= 0 && saveds <= SLJIT_NUMBER_OF_REGISTERS);
	CHECK_ARGUMENT(scratches + saveds <= SLJIT_NUMBER_OF_REGISTERS);
	CHECK_ARGUMENT(args <= saveds);
	CHECK_ARGUMENT(fscratches >= 0 && fscratches <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
	CHECK_ARGUMENT(fsaveds >= 0 && fsaveds <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
	CHECK_ARGUMENT(fscratches + fsaveds <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
	CHECK_ARGUMENT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose))
		fprintf(compiler->verbose, "  enter options:none args:%d scratches:%d saveds:%d fscratches:%d fsaveds:%d local_size:%d\n",
			args, scratches, saveds, fscratches, fsaveds, local_size);
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_set_context(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(!(options & ~SLJIT_DOUBLE_ALIGNMENT));
	CHECK_ARGUMENT(args >= 0 && args <= 3);
	CHECK_ARGUMENT(scratches >= 0 && scratches <= SLJIT_NUMBER_OF_REGISTERS);
	CHECK_ARGUMENT(saveds >= 0 && saveds <= SLJIT_NUMBER_OF_REGISTERS);
	CHECK_ARGUMENT(scratches + saveds <= SLJIT_NUMBER_OF_REGISTERS);
	CHECK_ARGUMENT(args <= saveds);
	CHECK_ARGUMENT(fscratches >= 0 && fscratches <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
	CHECK_ARGUMENT(fsaveds >= 0 && fsaveds <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
	CHECK_ARGUMENT(fscratches + fsaveds <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
	CHECK_ARGUMENT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose))
		fprintf(compiler->verbose, "  set_context options:none args:%d scratches:%d saveds:%d fscratches:%d fsaveds:%d local_size:%d\n",
			args, scratches, saveds, fscratches, fsaveds, local_size);
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(compiler->scratches >= 0);
	if (op != SLJIT_UNUSED) {
		CHECK_ARGUMENT(op >= SLJIT_MOV && op <= SLJIT_MOV_P);
		FUNCTION_CHECK_SRC(src, srcw);
	}
	else
		CHECK_ARGUMENT(src == 0 && srcw == 0);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		if (op == SLJIT_UNUSED)
			fprintf(compiler->verbose, "  return\n");
		else {
			fprintf(compiler->verbose, "  return.%s ", op1_names[op - SLJIT_OP1_BASE]);
			sljit_verbose_param(compiler, src, srcw);
			fprintf(compiler->verbose, "\n");
		}
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	FUNCTION_CHECK_DST(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  fast_enter ");
		sljit_verbose_param(compiler, dst, dstw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_si src, sljit_sw srcw)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	FUNCTION_CHECK_SRC(src, srcw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  fast_return ");
		sljit_verbose_param(compiler, src, srcw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_op0(struct sljit_compiler *compiler, sljit_si op)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT((op >= SLJIT_BREAKPOINT && op <= SLJIT_LSMUL)
		|| ((op & ~SLJIT_INT_OP) >= SLJIT_UDIVMOD && (op & ~SLJIT_INT_OP) <= SLJIT_SDIVI));
	CHECK_ARGUMENT(op < SLJIT_LUMUL || compiler->scratches >= 2);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose))
		fprintf(compiler->verbose, "  %s%s\n", !(op & SLJIT_INT_OP) ? "" : "i", op0_names[GET_OPCODE(op) - SLJIT_OP0_BASE]);
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_op1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_CLZ);
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  %s%s%s%s%s%s%s%s ", !(op & SLJIT_INT_OP) ? "" : "i", op1_names[GET_OPCODE(op) - SLJIT_OP1_BASE],
			!(op & SLJIT_SET_E) ? "" : ".e", !(op & SLJIT_SET_U) ? "" : ".u", !(op & SLJIT_SET_S) ? "" : ".s",
			!(op & SLJIT_SET_O) ? "" : ".o", !(op & SLJIT_SET_C) ? "" : ".c", !(op & SLJIT_KEEP_FLAGS) ? "" : ".k");
		sljit_verbose_param(compiler, dst, dstw);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_param(compiler, src, srcw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_op2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(GET_OPCODE(op) >= SLJIT_ADD && GET_OPCODE(op) <= SLJIT_ASHR);
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  %s%s%s%s%s%s%s%s ", !(op & SLJIT_INT_OP) ? "" : "i", op2_names[GET_OPCODE(op) - SLJIT_OP2_BASE],
			!(op & SLJIT_SET_E) ? "" : ".e", !(op & SLJIT_SET_U) ? "" : ".u", !(op & SLJIT_SET_S) ? "" : ".s",
			!(op & SLJIT_SET_O) ? "" : ".o", !(op & SLJIT_SET_C) ? "" : ".c", !(op & SLJIT_KEEP_FLAGS) ? "" : ".k");
		sljit_verbose_param(compiler, dst, dstw);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_param(compiler, src1, src1w);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_param(compiler, src2, src2w);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_get_register_index(sljit_si reg)
{
	SLJIT_UNUSED_ARG(reg);
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(reg > 0 && reg <= SLJIT_NUMBER_OF_REGISTERS);
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_get_float_register_index(sljit_si reg)
{
	SLJIT_UNUSED_ARG(reg);
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(reg > 0 && reg <= SLJIT_NUMBER_OF_FLOAT_REGISTERS);
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_si size)
{
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	int i;
#endif

	SLJIT_UNUSED_ARG(compiler);

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(instruction);
#if (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	CHECK_ARGUMENT(size > 0 && size < 16);
#elif (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
	CHECK_ARGUMENT((size == 2 && (((sljit_sw)instruction) & 0x1) == 0)
		|| (size == 4 && (((sljit_sw)instruction) & 0x3) == 0));
#else
	CHECK_ARGUMENT(size == 4 && (((sljit_sw)instruction) & 0x3) == 0);
#endif

#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  op_custom");
		for (i = 0; i < size; i++)
			fprintf(compiler->verbose, " 0x%x", ((sljit_ub*)instruction)[i]);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fop1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(sljit_is_fpu_available());
	CHECK_ARGUMENT(GET_OPCODE(op) >= SLJIT_DMOV && GET_OPCODE(op) <= SLJIT_DABS);
	FUNCTION_CHECK_FOP();
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		if (GET_OPCODE(op) == SLJIT_CONVD_FROMS)
			fprintf(compiler->verbose, "  %s%s ", fop1_names[SLJIT_CONVD_FROMS - SLJIT_FOP1_BASE],
				(op & SLJIT_SINGLE_OP) ? "s.fromd" : "d.froms");
		else
			fprintf(compiler->verbose, "  %s%s ", (op & SLJIT_SINGLE_OP) ? "s" : "d",
				fop1_names[GET_OPCODE(op) - SLJIT_FOP1_BASE]);

		sljit_verbose_fparam(compiler, dst, dstw);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_fparam(compiler, src, srcw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fop1_cmp(struct sljit_compiler *compiler, sljit_si op,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(sljit_is_fpu_available());
	CHECK_ARGUMENT(GET_OPCODE(op) == SLJIT_DCMP);
	FUNCTION_CHECK_FOP();
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  %s%s%s%s ", (op & SLJIT_SINGLE_OP) ? "s" : "d", fop1_names[SLJIT_DCMP - SLJIT_FOP1_BASE],
			(op & SLJIT_SET_E) ? ".e" : "", (op & SLJIT_SET_S) ? ".s" : "");
		sljit_verbose_fparam(compiler, src1, src1w);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_fparam(compiler, src2, src2w);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fop1_convw_fromd(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(sljit_is_fpu_available());
	CHECK_ARGUMENT(GET_OPCODE(op) >= SLJIT_CONVW_FROMD && GET_OPCODE(op) <= SLJIT_CONVI_FROMD);
	FUNCTION_CHECK_FOP();
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  %s%s.from%s ", fop1_names[GET_OPCODE(op) - SLJIT_FOP1_BASE],
			(GET_OPCODE(op) == SLJIT_CONVI_FROMD) ? "i" : "w",
			(op & SLJIT_SINGLE_OP) ? "s" : "d");
		sljit_verbose_param(compiler, dst, dstw);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_fparam(compiler, src, srcw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fop1_convd_fromw(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(sljit_is_fpu_available());
	CHECK_ARGUMENT(GET_OPCODE(op) >= SLJIT_CONVD_FROMW && GET_OPCODE(op) <= SLJIT_CONVD_FROMI);
	FUNCTION_CHECK_FOP();
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  %s%s.from%s ", fop1_names[GET_OPCODE(op) - SLJIT_FOP1_BASE],
			(op & SLJIT_SINGLE_OP) ? "s" : "d",
			(GET_OPCODE(op) == SLJIT_CONVD_FROMI) ? "i" : "w");
		sljit_verbose_fparam(compiler, dst, dstw);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_param(compiler, src, srcw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fop2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(sljit_is_fpu_available());
	CHECK_ARGUMENT(GET_OPCODE(op) >= SLJIT_DADD && GET_OPCODE(op) <= SLJIT_DDIV);
	FUNCTION_CHECK_FOP();
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  %s%s ", (op & SLJIT_SINGLE_OP) ? "s" : "d", fop2_names[GET_OPCODE(op) - SLJIT_FOP2_BASE]);
		sljit_verbose_fparam(compiler, dst, dstw);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_fparam(compiler, src1, src1w);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_fparam(compiler, src2, src2w);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_label(struct sljit_compiler *compiler)
{
	SLJIT_UNUSED_ARG(compiler);

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose))
		fprintf(compiler->verbose, "label:\n");
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_jump(struct sljit_compiler *compiler, sljit_si type)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(!(type & ~(0xff | SLJIT_REWRITABLE_JUMP | SLJIT_INT_OP)));
	CHECK_ARGUMENT((type & 0xff) >= SLJIT_EQUAL && (type & 0xff) <= SLJIT_CALL3);
	CHECK_ARGUMENT((type & 0xff) < SLJIT_JUMP || !(type & SLJIT_INT_OP));
	CHECK_ARGUMENT((type & 0xff) <= SLJIT_CALL0 || ((type & 0xff) - SLJIT_CALL0) <= compiler->scratches);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose))
		fprintf(compiler->verbose, "  jump%s.%s%s\n", !(type & SLJIT_REWRITABLE_JUMP) ? "" : ".r",
			JUMP_PREFIX(type), jump_names[type & 0xff]);
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_cmp(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(!(type & ~(0xff | SLJIT_REWRITABLE_JUMP | SLJIT_INT_OP)));
	CHECK_ARGUMENT((type & 0xff) >= SLJIT_EQUAL && (type & 0xff) <= SLJIT_SIG_LESS_EQUAL);
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  cmp%s.%s%s ", !(type & SLJIT_REWRITABLE_JUMP) ? "" : ".r",
			(type & SLJIT_INT_OP) ? "i_" : "", jump_names[type & 0xff]);
		sljit_verbose_param(compiler, src1, src1w);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_param(compiler, src2, src2w);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_fcmp(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(sljit_is_fpu_available());
	CHECK_ARGUMENT(!(type & ~(0xff | SLJIT_REWRITABLE_JUMP | SLJIT_SINGLE_OP)));
	CHECK_ARGUMENT((type & 0xff) >= SLJIT_D_EQUAL && (type & 0xff) <= SLJIT_D_ORDERED);
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  fcmp%s.%s%s ", !(type & SLJIT_REWRITABLE_JUMP) ? "" : ".r",
			(type & SLJIT_SINGLE_OP) ? "s_" : "d_", jump_names[type & 0xff]);
		sljit_verbose_fparam(compiler, src1, src1w);
		fprintf(compiler->verbose, ", ");
		sljit_verbose_fparam(compiler, src2, src2w);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_ijump(struct sljit_compiler *compiler, sljit_si type, sljit_si src, sljit_sw srcw)
{
	if (SLJIT_UNLIKELY(compiler->skip_checks)) {
		compiler->skip_checks = 0;
		CHECK_RETURN_OK;
	}

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
	CHECK_ARGUMENT(type <= SLJIT_CALL0 || (type - SLJIT_CALL0) <= compiler->scratches);
	FUNCTION_CHECK_SRC(src, srcw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  ijump.%s ", jump_names[type]);
		sljit_verbose_param(compiler, src, srcw);
		fprintf(compiler->verbose, "\n");
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw,
	sljit_si type)
{
#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	CHECK_ARGUMENT(!(type & ~(0xff | SLJIT_INT_OP)));
	CHECK_ARGUMENT((type & 0xff) >= SLJIT_EQUAL && (type & 0xff) <= SLJIT_D_ORDERED);
	CHECK_ARGUMENT(op == SLJIT_MOV || GET_OPCODE(op) == SLJIT_MOV_UI || GET_OPCODE(op) == SLJIT_MOV_SI
		|| (GET_OPCODE(op) >= SLJIT_AND && GET_OPCODE(op) <= SLJIT_XOR));
	CHECK_ARGUMENT((op & (SLJIT_SET_U | SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C)) == 0);
	CHECK_ARGUMENT((op & (SLJIT_SET_E | SLJIT_KEEP_FLAGS)) != (SLJIT_SET_E | SLJIT_KEEP_FLAGS));
	if (GET_OPCODE(op) < SLJIT_ADD) {
		CHECK_ARGUMENT(src == SLJIT_UNUSED && srcw == 0);
	} else {
		CHECK_ARGUMENT(src == dst && srcw == dstw);
	}
	FUNCTION_CHECK_DST(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  flags.%s%s%s%s ", !(op & SLJIT_INT_OP) ? "" : "i",
			GET_OPCODE(op) >= SLJIT_OP2_BASE ? op2_names[GET_OPCODE(op) - SLJIT_OP2_BASE] : op1_names[GET_OPCODE(op) - SLJIT_OP1_BASE],
			!(op & SLJIT_SET_E) ? "" : ".e", !(op & SLJIT_KEEP_FLAGS) ? "" : ".k");
		sljit_verbose_param(compiler, dst, dstw);
		if (src != SLJIT_UNUSED) {
			fprintf(compiler->verbose, ", ");
			sljit_verbose_param(compiler, src, srcw);
		}
		fprintf(compiler->verbose, ", %s%s\n", JUMP_PREFIX(type), jump_names[type & 0xff]);
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_get_local_base(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw offset)
{
	SLJIT_UNUSED_ARG(offset);

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	FUNCTION_CHECK_DST(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  local_base ");
		sljit_verbose_param(compiler, dst, dstw);
		fprintf(compiler->verbose, ", #%" SLJIT_PRINT_D "d\n", offset);
	}
#endif
	CHECK_RETURN_OK;
}

static SLJIT_INLINE CHECK_RETURN_TYPE check_sljit_emit_const(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw init_value)
{
	SLJIT_UNUSED_ARG(init_value);

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	FUNCTION_CHECK_DST(dst, dstw);
#endif
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	if (SLJIT_UNLIKELY(!!compiler->verbose)) {
		fprintf(compiler->verbose, "  const ");
		sljit_verbose_param(compiler, dst, dstw);
		fprintf(compiler->verbose, ", #%" SLJIT_PRINT_D "d\n", init_value);
	}
#endif
	CHECK_RETURN_OK;
}

#endif /* SLJIT_ARGUMENT_CHECKS || SLJIT_VERBOSE */

#define SELECT_FOP1_OPERATION_WITH_CHECKS(compiler, op, dst, dstw, src, srcw) \
	SLJIT_COMPILE_ASSERT(!(SLJIT_CONVW_FROMD & 0x1) && !(SLJIT_CONVD_FROMW & 0x1), \
		invalid_float_opcodes); \
	if (GET_OPCODE(op) >= SLJIT_CONVW_FROMD && GET_OPCODE(op) <= SLJIT_DCMP) { \
		if (GET_OPCODE(op) == SLJIT_DCMP) { \
			CHECK(check_sljit_emit_fop1_cmp(compiler, op, dst, dstw, src, srcw)); \
			ADJUST_LOCAL_OFFSET(dst, dstw); \
			ADJUST_LOCAL_OFFSET(src, srcw); \
			return sljit_emit_fop1_cmp(compiler, op, dst, dstw, src, srcw); \
		} \
		if ((GET_OPCODE(op) | 0x1) == SLJIT_CONVI_FROMD) { \
			CHECK(check_sljit_emit_fop1_convw_fromd(compiler, op, dst, dstw, src, srcw)); \
			ADJUST_LOCAL_OFFSET(dst, dstw); \
			ADJUST_LOCAL_OFFSET(src, srcw); \
			return sljit_emit_fop1_convw_fromd(compiler, op, dst, dstw, src, srcw); \
		} \
		CHECK(check_sljit_emit_fop1_convd_fromw(compiler, op, dst, dstw, src, srcw)); \
		ADJUST_LOCAL_OFFSET(dst, dstw); \
		ADJUST_LOCAL_OFFSET(src, srcw); \
		return sljit_emit_fop1_convd_fromw(compiler, op, dst, dstw, src, srcw); \
	} \
	CHECK(check_sljit_emit_fop1(compiler, op, dst, dstw, src, srcw)); \
	ADJUST_LOCAL_OFFSET(dst, dstw); \
	ADJUST_LOCAL_OFFSET(src, srcw);

static SLJIT_INLINE sljit_si emit_mov_before_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
	/* Return if don't need to do anything. */
	if (op == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	/* At the moment the pointer size is always equal to sljit_sw. May be changed in the future. */
	if (src == SLJIT_RETURN_REG && (op == SLJIT_MOV || op == SLJIT_MOV_P))
		return SLJIT_SUCCESS;
#else
	if (src == SLJIT_RETURN_REG && (op == SLJIT_MOV || op == SLJIT_MOV_UI || op == SLJIT_MOV_SI || op == SLJIT_MOV_P))
		return SLJIT_SUCCESS;
#endif

#if (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS) \
		|| (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
	compiler->skip_checks = 1;
#endif
	return sljit_emit_op1(compiler, op, SLJIT_RETURN_REG, 0, src, srcw);
}

/* CPU description section */

#if (defined SLJIT_32BIT_ARCHITECTURE && SLJIT_32BIT_ARCHITECTURE)
#define SLJIT_CPUINFO_PART1 " 32bit ("
#elif (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define SLJIT_CPUINFO_PART1 " 64bit ("
#else
#error "Internal error: CPU type info missing"
#endif

#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
#define SLJIT_CPUINFO_PART2 "little endian + "
#elif (defined SLJIT_BIG_ENDIAN && SLJIT_BIG_ENDIAN)
#define SLJIT_CPUINFO_PART2 "big endian + "
#else
#error "Internal error: CPU type info missing"
#endif

#if (defined SLJIT_UNALIGNED && SLJIT_UNALIGNED)
#define SLJIT_CPUINFO_PART3 "unaligned)"
#else
#define SLJIT_CPUINFO_PART3 "aligned)"
#endif

#define SLJIT_CPUINFO SLJIT_CPUINFO_PART1 SLJIT_CPUINFO_PART2 SLJIT_CPUINFO_PART3

#if (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
#	include "sljitNativeX86_common.c"
#elif (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5)
#	include "sljitNativeARM_32.c"
#elif (defined SLJIT_CONFIG_ARM_V7 && SLJIT_CONFIG_ARM_V7)
#	include "sljitNativeARM_32.c"
#elif (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
#	include "sljitNativeARM_T2_32.c"
#elif (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
#	include "sljitNativeARM_64.c"
#elif (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
#	include "sljitNativePPC_common.c"
#elif (defined SLJIT_CONFIG_MIPS && SLJIT_CONFIG_MIPS)
#	include "sljitNativeMIPS_common.c"
#elif (defined SLJIT_CONFIG_SPARC && SLJIT_CONFIG_SPARC)
#	include "sljitNativeSPARC_common.c"
#elif (defined SLJIT_CONFIG_TILEGX && SLJIT_CONFIG_TILEGX)
#	include "sljitNativeTILEGX_64.c"
#endif

#if !(defined SLJIT_CONFIG_MIPS && SLJIT_CONFIG_MIPS)

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_cmp(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	/* Default compare for most architectures. */
	sljit_si flags, tmp_src, condition;
	sljit_sw tmp_srcw;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_cmp(compiler, type, src1, src1w, src2, src2w));

	condition = type & 0xff;
#if (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
	if ((condition == SLJIT_EQUAL || condition == SLJIT_NOT_EQUAL)) {
		if ((src1 & SLJIT_IMM) && !src1w) {
			src1 = src2;
			src1w = src2w;
			src2 = SLJIT_IMM;
			src2w = 0;
		}
		if ((src2 & SLJIT_IMM) && !src2w)
			return emit_cmp_to0(compiler, type, src1, src1w);
	}
#endif

	if (SLJIT_UNLIKELY((src1 & SLJIT_IMM) && !(src2 & SLJIT_IMM))) {
		/* Immediate is prefered as second argument by most architectures. */
		switch (condition) {
		case SLJIT_LESS:
			condition = SLJIT_GREATER;
			break;
		case SLJIT_GREATER_EQUAL:
			condition = SLJIT_LESS_EQUAL;
			break;
		case SLJIT_GREATER:
			condition = SLJIT_LESS;
			break;
		case SLJIT_LESS_EQUAL:
			condition = SLJIT_GREATER_EQUAL;
			break;
		case SLJIT_SIG_LESS:
			condition = SLJIT_SIG_GREATER;
			break;
		case SLJIT_SIG_GREATER_EQUAL:
			condition = SLJIT_SIG_LESS_EQUAL;
			break;
		case SLJIT_SIG_GREATER:
			condition = SLJIT_SIG_LESS;
			break;
		case SLJIT_SIG_LESS_EQUAL:
			condition = SLJIT_SIG_GREATER_EQUAL;
			break;
		}
		type = condition | (type & (SLJIT_INT_OP | SLJIT_REWRITABLE_JUMP));
		tmp_src = src1;
		src1 = src2;
		src2 = tmp_src;
		tmp_srcw = src1w;
		src1w = src2w;
		src2w = tmp_srcw;
	}

	if (condition <= SLJIT_NOT_ZERO)
		flags = SLJIT_SET_E;
	else if (condition <= SLJIT_LESS_EQUAL)
		flags = SLJIT_SET_U;
	else
		flags = SLJIT_SET_S;

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif
	PTR_FAIL_IF(sljit_emit_op2(compiler, SLJIT_SUB | flags | (type & SLJIT_INT_OP),
		SLJIT_UNUSED, 0, src1, src1w, src2, src2w));
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif
	return sljit_emit_jump(compiler, condition | (type & SLJIT_REWRITABLE_JUMP));
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_fcmp(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si flags, condition;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_fcmp(compiler, type, src1, src1w, src2, src2w));

	condition = type & 0xff;
	flags = (condition <= SLJIT_D_NOT_EQUAL) ? SLJIT_SET_E : SLJIT_SET_S;
	if (type & SLJIT_SINGLE_OP)
		flags |= SLJIT_SINGLE_OP;

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif
	sljit_emit_fop1(compiler, SLJIT_DCMP | flags, src1, src1w, src2, src2w);

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif
	return sljit_emit_jump(compiler, condition | (type & SLJIT_REWRITABLE_JUMP));
}

#endif

#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_local_base(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw offset)
{
	CHECK_ERROR();
	CHECK(check_sljit_get_local_base(compiler, dst, dstw, offset));

	ADJUST_LOCAL_OFFSET(SLJIT_MEM1(SLJIT_SP), offset);
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif
	if (offset != 0)
		return sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_KEEP_FLAGS, dst, dstw, SLJIT_SP, 0, SLJIT_IMM, offset);
	return sljit_emit_op1(compiler, SLJIT_MOV, dst, dstw, SLJIT_SP, 0);
}

#endif

#else /* SLJIT_CONFIG_UNSUPPORTED */

/* Empty function bodies for those machines, which are not (yet) supported. */

SLJIT_API_FUNC_ATTRIBUTE SLJIT_CONST char* sljit_get_platform_name(void)
{
	return "unsupported";
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_compiler* sljit_create_compiler(void)
{
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_compiler(struct sljit_compiler *compiler)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_ASSERT_STOP();
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_alloc_memory(struct sljit_compiler *compiler, sljit_si size)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(size);
	SLJIT_ASSERT_STOP();
	return NULL;
}

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
SLJIT_API_FUNC_ATTRIBUTE void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(verbose);
	SLJIT_ASSERT_STOP();
}
#endif

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_code(void* code)
{
	SLJIT_UNUSED_ARG(code);
	SLJIT_ASSERT_STOP();
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_enter(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(options);
	SLJIT_UNUSED_ARG(args);
	SLJIT_UNUSED_ARG(scratches);
	SLJIT_UNUSED_ARG(saveds);
	SLJIT_UNUSED_ARG(fscratches);
	SLJIT_UNUSED_ARG(fsaveds);
	SLJIT_UNUSED_ARG(local_size);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_set_context(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(options);
	SLJIT_UNUSED_ARG(args);
	SLJIT_UNUSED_ARG(scratches);
	SLJIT_UNUSED_ARG(saveds);
	SLJIT_UNUSED_ARG(fscratches);
	SLJIT_UNUSED_ARG(fsaveds);
	SLJIT_UNUSED_ARG(local_size);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_UNUSED_ARG(src);
	SLJIT_UNUSED_ARG(srcw);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_si src, sljit_sw srcw)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(src);
	SLJIT_UNUSED_ARG(srcw);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op0(struct sljit_compiler *compiler, sljit_si op)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(src);
	SLJIT_UNUSED_ARG(srcw);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(src1);
	SLJIT_UNUSED_ARG(src1w);
	SLJIT_UNUSED_ARG(src2);
	SLJIT_UNUSED_ARG(src2w);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_register_index(sljit_si reg)
{
	SLJIT_ASSERT_STOP();
	return reg;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_si size)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(instruction);
	SLJIT_UNUSED_ARG(size);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_is_fpu_available(void)
{
	SLJIT_ASSERT_STOP();
	return 0;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(src);
	SLJIT_UNUSED_ARG(srcw);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(src1);
	SLJIT_UNUSED_ARG(src1w);
	SLJIT_UNUSED_ARG(src2);
	SLJIT_UNUSED_ARG(src2w);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_si type)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(type);
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_cmp(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(type);
	SLJIT_UNUSED_ARG(src1);
	SLJIT_UNUSED_ARG(src1w);
	SLJIT_UNUSED_ARG(src2);
	SLJIT_UNUSED_ARG(src2w);
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_fcmp(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(type);
	SLJIT_UNUSED_ARG(src1);
	SLJIT_UNUSED_ARG(src1w);
	SLJIT_UNUSED_ARG(src2);
	SLJIT_UNUSED_ARG(src2w);
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label)
{
	SLJIT_UNUSED_ARG(jump);
	SLJIT_UNUSED_ARG(label);
	SLJIT_ASSERT_STOP();
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_target(struct sljit_jump *jump, sljit_uw target)
{
	SLJIT_UNUSED_ARG(jump);
	SLJIT_UNUSED_ARG(target);
	SLJIT_ASSERT_STOP();
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_ijump(struct sljit_compiler *compiler, sljit_si type, sljit_si src, sljit_sw srcw)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(type);
	SLJIT_UNUSED_ARG(src);
	SLJIT_UNUSED_ARG(srcw);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw,
	sljit_si type)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(op);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(src);
	SLJIT_UNUSED_ARG(srcw);
	SLJIT_UNUSED_ARG(type);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_local_base(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw offset)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(offset);
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw initval)
{
	SLJIT_UNUSED_ARG(compiler);
	SLJIT_UNUSED_ARG(dst);
	SLJIT_UNUSED_ARG(dstw);
	SLJIT_UNUSED_ARG(initval);
	SLJIT_ASSERT_STOP();
	return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	SLJIT_UNUSED_ARG(addr);
	SLJIT_UNUSED_ARG(new_addr);
	SLJIT_ASSERT_STOP();
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_const(sljit_uw addr, sljit_sw new_constant)
{
	SLJIT_UNUSED_ARG(addr);
	SLJIT_UNUSED_ARG(new_constant);
	SLJIT_ASSERT_STOP();
}

#endif
