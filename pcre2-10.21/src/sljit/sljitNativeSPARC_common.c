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

SLJIT_API_FUNC_ATTRIBUTE SLJIT_CONST char* sljit_get_platform_name(void)
{
	return "SPARC" SLJIT_CPUINFO;
}

/* Length of an instruction word
   Both for sparc-32 and sparc-64 */
typedef sljit_ui sljit_ins;

static void sparc_cache_flush(sljit_ins *from, sljit_ins *to)
{
#if defined(__SUNPRO_C) && __SUNPRO_C < 0x590
	__asm (
		/* if (from == to) return */
		"cmp %i0, %i1\n"
		"be .leave\n"
		"nop\n"

		/* loop until from >= to */
		".mainloop:\n"
		"flush %i0\n"
		"add %i0, 8, %i0\n"
		"cmp %i0, %i1\n"
		"bcs .mainloop\n"
		"nop\n"

		/* The comparison was done above. */
		"bne .leave\n"
		/* nop is not necessary here, since the
		   sub operation has no side effect. */
		"sub %i0, 4, %i0\n"
		"flush %i0\n"
		".leave:"
	);
#else
	if (SLJIT_UNLIKELY(from == to))
		return;

	do {
		__asm__ volatile (
			"flush %0\n"
			: : "r"(from)
		);
		/* Operates at least on doubleword. */
		from += 2;
	} while (from < to);

	if (from == to) {
		/* Flush the last word. */
		from --;
		__asm__ volatile (
			"flush %0\n"
			: : "r"(from)
		);
	}
#endif
}

/* TMP_REG2 is not used by getput_arg */
#define TMP_REG1	(SLJIT_NUMBER_OF_REGISTERS + 2)
#define TMP_REG2	(SLJIT_NUMBER_OF_REGISTERS + 3)
#define TMP_REG3	(SLJIT_NUMBER_OF_REGISTERS + 4)
#define TMP_LINK	(SLJIT_NUMBER_OF_REGISTERS + 5)

#define TMP_FREG1	(0)
#define TMP_FREG2	((SLJIT_NUMBER_OF_FLOAT_REGISTERS + 1) << 1)

static SLJIT_CONST sljit_ub reg_map[SLJIT_NUMBER_OF_REGISTERS + 6] = {
	0, 8, 9, 10, 13, 29, 28, 27, 23, 22, 21, 20, 19, 18, 17, 16, 26, 25, 24, 14, 1, 11, 12, 15
};

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */

#define D(d)		(reg_map[d] << 25)
#define DA(d)		((d) << 25)
#define S1(s1)		(reg_map[s1] << 14)
#define S2(s2)		(reg_map[s2])
#define S1A(s1)		((s1) << 14)
#define S2A(s2)		(s2)
#define IMM_ARG		0x2000
#define DOP(op)		((op) << 5)
#define IMM(imm)	(((imm) & 0x1fff) | IMM_ARG)

#define DR(dr)		(reg_map[dr])
#define OPC1(opcode)	((opcode) << 30)
#define OPC2(opcode)	((opcode) << 22)
#define OPC3(opcode)	((opcode) << 19)
#define SET_FLAGS	OPC3(0x10)

#define ADD		(OPC1(0x2) | OPC3(0x00))
#define ADDC		(OPC1(0x2) | OPC3(0x08))
#define AND		(OPC1(0x2) | OPC3(0x01))
#define ANDN		(OPC1(0x2) | OPC3(0x05))
#define CALL		(OPC1(0x1))
#define FABSS		(OPC1(0x2) | OPC3(0x34) | DOP(0x09))
#define FADDD		(OPC1(0x2) | OPC3(0x34) | DOP(0x42))
#define FADDS		(OPC1(0x2) | OPC3(0x34) | DOP(0x41))
#define FCMPD		(OPC1(0x2) | OPC3(0x35) | DOP(0x52))
#define FCMPS		(OPC1(0x2) | OPC3(0x35) | DOP(0x51))
#define FDIVD		(OPC1(0x2) | OPC3(0x34) | DOP(0x4e))
#define FDIVS		(OPC1(0x2) | OPC3(0x34) | DOP(0x4d))
#define FDTOI		(OPC1(0x2) | OPC3(0x34) | DOP(0xd2))
#define FDTOS		(OPC1(0x2) | OPC3(0x34) | DOP(0xc6))
#define FITOD		(OPC1(0x2) | OPC3(0x34) | DOP(0xc8))
#define FITOS		(OPC1(0x2) | OPC3(0x34) | DOP(0xc4))
#define FMOVS		(OPC1(0x2) | OPC3(0x34) | DOP(0x01))
#define FMULD		(OPC1(0x2) | OPC3(0x34) | DOP(0x4a))
#define FMULS		(OPC1(0x2) | OPC3(0x34) | DOP(0x49))
#define FNEGS		(OPC1(0x2) | OPC3(0x34) | DOP(0x05))
#define FSTOD		(OPC1(0x2) | OPC3(0x34) | DOP(0xc9))
#define FSTOI		(OPC1(0x2) | OPC3(0x34) | DOP(0xd1))
#define FSUBD		(OPC1(0x2) | OPC3(0x34) | DOP(0x46))
#define FSUBS		(OPC1(0x2) | OPC3(0x34) | DOP(0x45))
#define JMPL		(OPC1(0x2) | OPC3(0x38))
#define NOP		(OPC1(0x0) | OPC2(0x04))
#define OR		(OPC1(0x2) | OPC3(0x02))
#define ORN		(OPC1(0x2) | OPC3(0x06))
#define RDY		(OPC1(0x2) | OPC3(0x28) | S1A(0))
#define RESTORE		(OPC1(0x2) | OPC3(0x3d))
#define SAVE		(OPC1(0x2) | OPC3(0x3c))
#define SETHI		(OPC1(0x0) | OPC2(0x04))
#define SLL		(OPC1(0x2) | OPC3(0x25))
#define SLLX		(OPC1(0x2) | OPC3(0x25) | (1 << 12))
#define SRA		(OPC1(0x2) | OPC3(0x27))
#define SRAX		(OPC1(0x2) | OPC3(0x27) | (1 << 12))
#define SRL		(OPC1(0x2) | OPC3(0x26))
#define SRLX		(OPC1(0x2) | OPC3(0x26) | (1 << 12))
#define SUB		(OPC1(0x2) | OPC3(0x04))
#define SUBC		(OPC1(0x2) | OPC3(0x0c))
#define TA		(OPC1(0x2) | OPC3(0x3a) | (8 << 25))
#define WRY		(OPC1(0x2) | OPC3(0x30) | DA(0))
#define XOR		(OPC1(0x2) | OPC3(0x03))
#define XNOR		(OPC1(0x2) | OPC3(0x07))

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#define MAX_DISP	(0x1fffff)
#define MIN_DISP	(-0x200000)
#define DISP_MASK	(0x3fffff)

#define BICC		(OPC1(0x0) | OPC2(0x2))
#define FBFCC		(OPC1(0x0) | OPC2(0x6))
#define SLL_W		SLL
#define SDIV		(OPC1(0x2) | OPC3(0x0f))
#define SMUL		(OPC1(0x2) | OPC3(0x0b))
#define UDIV		(OPC1(0x2) | OPC3(0x0e))
#define UMUL		(OPC1(0x2) | OPC3(0x0a))
#else
#define SLL_W		SLLX
#endif

#define SIMM_MAX	(0x0fff)
#define SIMM_MIN	(-0x1000)

/* dest_reg is the absolute name of the register
   Useful for reordering instructions in the delay slot. */
static sljit_si push_inst(struct sljit_compiler *compiler, sljit_ins ins, sljit_si delay_slot)
{
	sljit_ins *ptr;
	SLJIT_ASSERT((delay_slot & DST_INS_MASK) == UNMOVABLE_INS
		|| (delay_slot & DST_INS_MASK) == MOVABLE_INS
		|| (delay_slot & DST_INS_MASK) == ((ins >> 25) & 0x1f));
	ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	compiler->delay_slot = delay_slot;
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_ins* detect_jump_type(struct sljit_jump *jump, sljit_ins *code_ptr, sljit_ins *code)
{
	sljit_sw diff;
	sljit_uw target_addr;
	sljit_ins *inst;
	sljit_ins saved_inst;

	if (jump->flags & SLJIT_REWRITABLE_JUMP)
		return code_ptr;

	if (jump->flags & JUMP_ADDR)
		target_addr = jump->u.target;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		target_addr = (sljit_uw)(code + jump->u.label->size);
	}
	inst = (sljit_ins*)jump->addr;

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
	if (jump->flags & IS_CALL) {
		/* Call is always patchable on sparc 32. */
		jump->flags |= PATCH_CALL;
		if (jump->flags & IS_MOVABLE) {
			inst[0] = inst[-1];
			inst[-1] = CALL;
			jump->addr -= sizeof(sljit_ins);
			return inst;
		}
		inst[0] = CALL;
		inst[1] = NOP;
		return inst + 1;
	}
#else
	/* Both calls and BPr instructions shall not pass this point. */
#error "Implementation required"
#endif

	if (jump->flags & IS_COND)
		inst--;

	if (jump->flags & IS_MOVABLE) {
		diff = ((sljit_sw)target_addr - (sljit_sw)(inst - 1)) >> 2;
		if (diff <= MAX_DISP && diff >= MIN_DISP) {
			jump->flags |= PATCH_B;
			inst--;
			if (jump->flags & IS_COND) {
				saved_inst = inst[0];
				inst[0] = inst[1] ^ (1 << 28);
				inst[1] = saved_inst;
			} else {
				inst[1] = inst[0];
				inst[0] = BICC | DA(0x8);
			}
			jump->addr = (sljit_uw)inst;
			return inst + 1;
		}
	}

	diff = ((sljit_sw)target_addr - (sljit_sw)(inst)) >> 2;
	if (diff <= MAX_DISP && diff >= MIN_DISP) {
		jump->flags |= PATCH_B;
		if (jump->flags & IS_COND)
			inst[0] ^= (1 << 28);
		else
			inst[0] = BICC | DA(0x8);
		inst[1] = NOP;
		jump->addr = (sljit_uw)inst;
		return inst + 1;
	}

	return code_ptr;
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_ins *code;
	sljit_ins *code_ptr;
	sljit_ins *buf_ptr;
	sljit_ins *buf_end;
	sljit_uw word_count;
	sljit_uw addr;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_generate_code(compiler));
	reverse_buf(compiler);

	code = (sljit_ins*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_ins));
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	word_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = (sljit_ins*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr = *buf_ptr++;
			SLJIT_ASSERT(!label || label->size >= word_count);
			SLJIT_ASSERT(!jump || jump->addr >= word_count);
			SLJIT_ASSERT(!const_ || const_->addr >= word_count);
			/* These structures are ordered by their address. */
			if (label && label->size == word_count) {
				/* Just recording the address. */
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
				jump->addr = (sljit_uw)(code_ptr - 3);
#else
				jump->addr = (sljit_uw)(code_ptr - 6);
#endif
				code_ptr = detect_jump_type(jump, code_ptr, code);
				jump = jump->next;
			}
			if (const_ && const_->addr == word_count) {
				/* Just recording the address. */
				const_->addr = (sljit_uw)code_ptr;
				const_ = const_->next;
			}
			code_ptr ++;
			word_count ++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	if (label && label->size == word_count) {
		label->addr = (sljit_uw)code_ptr;
		label->size = code_ptr - code;
		label = label->next;
	}

	SLJIT_ASSERT(!label);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(!const_);
	SLJIT_ASSERT(code_ptr - code <= (sljit_si)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		do {
			addr = (jump->flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target;
			buf_ptr = (sljit_ins*)jump->addr;

			if (jump->flags & PATCH_CALL) {
				addr = (sljit_sw)(addr - jump->addr) >> 2;
				SLJIT_ASSERT((sljit_sw)addr <= 0x1fffffff && (sljit_sw)addr >= -0x20000000);
				buf_ptr[0] = CALL | (addr & 0x3fffffff);
				break;
			}
			if (jump->flags & PATCH_B) {
				addr = (sljit_sw)(addr - jump->addr) >> 2;
				SLJIT_ASSERT((sljit_sw)addr <= MAX_DISP && (sljit_sw)addr >= MIN_DISP);
				buf_ptr[0] = (buf_ptr[0] & ~DISP_MASK) | (addr & DISP_MASK);
				break;
			}

			/* Set the fields of immediate loads. */
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
			buf_ptr[0] = (buf_ptr[0] & 0xffc00000) | ((addr >> 10) & 0x3fffff);
			buf_ptr[1] = (buf_ptr[1] & 0xfffffc00) | (addr & 0x3ff);
#else
#error "Implementation required"
#endif
		} while (0);
		jump = jump->next;
	}


	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_size = (code_ptr - code) * sizeof(sljit_ins);
	SLJIT_CACHE_FLUSH(code, code_ptr);
	return code;
}

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

/* Creates an index in data_transfer_insts array. */
#define LOAD_DATA	0x01
#define WORD_DATA	0x00
#define BYTE_DATA	0x02
#define HALF_DATA	0x04
#define INT_DATA	0x06
#define SIGNED_DATA	0x08
/* Separates integer and floating point registers */
#define GPR_REG		0x0f
#define DOUBLE_DATA	0x10
#define SINGLE_DATA	0x12

#define MEM_MASK	0x1f

#define WRITE_BACK	0x00020
#define ARG_TEST	0x00040
#define ALT_KEEP_CACHE	0x00080
#define CUMULATIVE_OP	0x00100
#define IMM_OP		0x00200
#define SRC2_IMM	0x00400

#define REG_DEST	0x00800
#define REG2_SOURCE	0x01000
#define SLOW_SRC1	0x02000
#define SLOW_SRC2	0x04000
#define SLOW_DEST	0x08000

/* SET_FLAGS (0x10 << 19) also belong here! */

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#include "sljitNativeSPARC_32.c"
#else
#include "sljitNativeSPARC_64.c"
#endif

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_enter(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_enter(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size));
	set_emit_enter(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size);

	local_size = (local_size + SLJIT_LOCALS_OFFSET + 7) & ~0x7;
	compiler->local_size = local_size;

	if (local_size <= SIMM_MAX) {
		FAIL_IF(push_inst(compiler, SAVE | D(SLJIT_SP) | S1(SLJIT_SP) | IMM(-local_size), UNMOVABLE_INS));
	}
	else {
		FAIL_IF(load_immediate(compiler, TMP_REG1, -local_size));
		FAIL_IF(push_inst(compiler, SAVE | D(SLJIT_SP) | S1(SLJIT_SP) | S2(TMP_REG1), UNMOVABLE_INS));
	}

	/* Arguments are in their appropriate registers. */

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_set_context(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	CHECK_ERROR();
	CHECK(check_sljit_set_context(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size));
	set_set_context(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size);

	compiler->local_size = (local_size + SLJIT_LOCALS_OFFSET + 7) & ~0x7;
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_return(compiler, op, src, srcw));

	if (op != SLJIT_MOV || !FAST_IS_REG(src)) {
		FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));
		src = SLJIT_R0;
	}

	FAIL_IF(push_inst(compiler, JMPL | D(0) | S1A(31) | IMM(8), UNMOVABLE_INS));
	return push_inst(compiler, RESTORE | D(SLJIT_R0) | S1(src) | S2(0), UNMOVABLE_INS);
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#define ARCH_32_64(a, b)	a
#else
#define ARCH_32_64(a, b)	b
#endif

static SLJIT_CONST sljit_ins data_transfer_insts[16 + 4] = {
/* u w s */ ARCH_32_64(OPC1(3) | OPC3(0x04) /* stw */, OPC1(3) | OPC3(0x0e) /* stx */),
/* u w l */ ARCH_32_64(OPC1(3) | OPC3(0x00) /* lduw */, OPC1(3) | OPC3(0x0b) /* ldx */),
/* u b s */ OPC1(3) | OPC3(0x05) /* stb */,
/* u b l */ OPC1(3) | OPC3(0x01) /* ldub */,
/* u h s */ OPC1(3) | OPC3(0x06) /* sth */,
/* u h l */ OPC1(3) | OPC3(0x02) /* lduh */,
/* u i s */ OPC1(3) | OPC3(0x04) /* stw */,
/* u i l */ OPC1(3) | OPC3(0x00) /* lduw */,

/* s w s */ ARCH_32_64(OPC1(3) | OPC3(0x04) /* stw */, OPC1(3) | OPC3(0x0e) /* stx */),
/* s w l */ ARCH_32_64(OPC1(3) | OPC3(0x00) /* lduw */, OPC1(3) | OPC3(0x0b) /* ldx */),
/* s b s */ OPC1(3) | OPC3(0x05) /* stb */,
/* s b l */ OPC1(3) | OPC3(0x09) /* ldsb */,
/* s h s */ OPC1(3) | OPC3(0x06) /* sth */,
/* s h l */ OPC1(3) | OPC3(0x0a) /* ldsh */,
/* s i s */ OPC1(3) | OPC3(0x04) /* stw */,
/* s i l */ ARCH_32_64(OPC1(3) | OPC3(0x00) /* lduw */, OPC1(3) | OPC3(0x08) /* ldsw */),

/* d   s */ OPC1(3) | OPC3(0x27),
/* d   l */ OPC1(3) | OPC3(0x23),
/* s   s */ OPC1(3) | OPC3(0x24),
/* s   l */ OPC1(3) | OPC3(0x20),
};

#undef ARCH_32_64

/* Can perform an operation using at most 1 instruction. */
static sljit_si getput_arg_fast(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg, sljit_sw argw)
{
	SLJIT_ASSERT(arg & SLJIT_MEM);

	if (!(flags & WRITE_BACK) || !(arg & REG_MASK)) {
		if ((!(arg & OFFS_REG_MASK) && argw <= SIMM_MAX && argw >= SIMM_MIN)
				|| ((arg & OFFS_REG_MASK) && (argw & 0x3) == 0)) {
			/* Works for both absoulte and relative addresses (immediate case). */
			if (SLJIT_UNLIKELY(flags & ARG_TEST))
				return 1;
			FAIL_IF(push_inst(compiler, data_transfer_insts[flags & MEM_MASK]
				| ((flags & MEM_MASK) <= GPR_REG ? D(reg) : DA(reg))
				| S1(arg & REG_MASK) | ((arg & OFFS_REG_MASK) ? S2(OFFS_REG(arg)) : IMM(argw)),
				((flags & MEM_MASK) <= GPR_REG && (flags & LOAD_DATA)) ? DR(reg) : MOVABLE_INS));
			return -1;
		}
	}
	return 0;
}

/* See getput_arg below.
   Note: can_cache is called only for binary operators. Those
   operators always uses word arguments without write back. */
static sljit_si can_cache(sljit_si arg, sljit_sw argw, sljit_si next_arg, sljit_sw next_argw)
{
	SLJIT_ASSERT((arg & SLJIT_MEM) && (next_arg & SLJIT_MEM));

	/* Simple operation except for updates. */
	if (arg & OFFS_REG_MASK) {
		argw &= 0x3;
		SLJIT_ASSERT(argw);
		next_argw &= 0x3;
		if ((arg & OFFS_REG_MASK) == (next_arg & OFFS_REG_MASK) && argw == next_argw)
			return 1;
		return 0;
	}

	if (((next_argw - argw) <= SIMM_MAX && (next_argw - argw) >= SIMM_MIN))
		return 1;
	return 0;
}

/* Emit the necessary instructions. See can_cache above. */
static sljit_si getput_arg(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg, sljit_sw argw, sljit_si next_arg, sljit_sw next_argw)
{
	sljit_si base, arg2, delay_slot;
	sljit_ins dest;

	SLJIT_ASSERT(arg & SLJIT_MEM);
	if (!(next_arg & SLJIT_MEM)) {
		next_arg = 0;
		next_argw = 0;
	}

	base = arg & REG_MASK;
	if (SLJIT_UNLIKELY(arg & OFFS_REG_MASK)) {
		argw &= 0x3;
		SLJIT_ASSERT(argw != 0);

		/* Using the cache. */
		if (((SLJIT_MEM | (arg & OFFS_REG_MASK)) == compiler->cache_arg) && (argw == compiler->cache_argw))
			arg2 = TMP_REG3;
		else {
			if ((arg & OFFS_REG_MASK) == (next_arg & OFFS_REG_MASK) && argw == (next_argw & 0x3)) {
				compiler->cache_arg = SLJIT_MEM | (arg & OFFS_REG_MASK);
				compiler->cache_argw = argw;
				arg2 = TMP_REG3;
			}
			else if ((flags & LOAD_DATA) && ((flags & MEM_MASK) <= GPR_REG) && reg != base && reg != OFFS_REG(arg))
				arg2 = reg;
			else /* It must be a mov operation, so tmp1 must be free to use. */
				arg2 = TMP_REG1;
			FAIL_IF(push_inst(compiler, SLL_W | D(arg2) | S1(OFFS_REG(arg)) | IMM_ARG | argw, DR(arg2)));
		}
	}
	else {
		/* Using the cache. */
		if ((compiler->cache_arg == SLJIT_MEM) && (argw - compiler->cache_argw) <= SIMM_MAX && (argw - compiler->cache_argw) >= SIMM_MIN) {
			if (argw != compiler->cache_argw) {
				FAIL_IF(push_inst(compiler, ADD | D(TMP_REG3) | S1(TMP_REG3) | IMM(argw - compiler->cache_argw), DR(TMP_REG3)));
				compiler->cache_argw = argw;
			}
			arg2 = TMP_REG3;
		} else {
			if ((next_argw - argw) <= SIMM_MAX && (next_argw - argw) >= SIMM_MIN) {
				compiler->cache_arg = SLJIT_MEM;
				compiler->cache_argw = argw;
				arg2 = TMP_REG3;
			}
			else if ((flags & LOAD_DATA) && ((flags & MEM_MASK) <= GPR_REG) && reg != base)
				arg2 = reg;
			else /* It must be a mov operation, so tmp1 must be free to use. */
				arg2 = TMP_REG1;
			FAIL_IF(load_immediate(compiler, arg2, argw));
		}
	}

	dest = ((flags & MEM_MASK) <= GPR_REG ? D(reg) : DA(reg));
	delay_slot = ((flags & MEM_MASK) <= GPR_REG && (flags & LOAD_DATA)) ? DR(reg) : MOVABLE_INS;
	if (!base)
		return push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | dest | S1(arg2) | IMM(0), delay_slot);
	if (!(flags & WRITE_BACK))
		return push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | dest | S1(base) | S2(arg2), delay_slot);
	FAIL_IF(push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | dest | S1(base) | S2(arg2), delay_slot));
	return push_inst(compiler, ADD | D(base) | S1(base) | S2(arg2), DR(base));
}

static SLJIT_INLINE sljit_si emit_op_mem(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg, sljit_sw argw)
{
	if (getput_arg_fast(compiler, flags, reg, arg, argw))
		return compiler->error;
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	return getput_arg(compiler, flags, reg, arg, argw, 0, 0);
}

static SLJIT_INLINE sljit_si emit_op_mem2(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg1, sljit_sw arg1w, sljit_si arg2, sljit_sw arg2w)
{
	if (getput_arg_fast(compiler, flags, reg, arg1, arg1w))
		return compiler->error;
	return getput_arg(compiler, flags, reg, arg1, arg1w, arg2, arg2w);
}

static sljit_si emit_op(struct sljit_compiler *compiler, sljit_si op, sljit_si flags,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	/* arg1 goes to TMP_REG1 or src reg
	   arg2 goes to TMP_REG2, imm or src reg
	   TMP_REG3 can be used for caching
	   result goes to TMP_REG2, so put result can use TMP_REG1 and TMP_REG3. */
	sljit_si dst_r = TMP_REG2;
	sljit_si src1_r;
	sljit_sw src2_r = 0;
	sljit_si sugg_src2_r = TMP_REG2;

	if (!(flags & ALT_KEEP_CACHE)) {
		compiler->cache_arg = 0;
		compiler->cache_argw = 0;
	}

	if (SLJIT_UNLIKELY(dst == SLJIT_UNUSED)) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI && !(src2 & SLJIT_MEM))
			return SLJIT_SUCCESS;
	}
	else if (FAST_IS_REG(dst)) {
		dst_r = dst;
		flags |= REG_DEST;
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
			sugg_src2_r = dst_r;
	}
	else if ((dst & SLJIT_MEM) && !getput_arg_fast(compiler, flags | ARG_TEST, TMP_REG1, dst, dstw))
		flags |= SLOW_DEST;

	if (flags & IMM_OP) {
		if ((src2 & SLJIT_IMM) && src2w) {
			if (src2w <= SIMM_MAX && src2w >= SIMM_MIN) {
				flags |= SRC2_IMM;
				src2_r = src2w;
			}
		}
		if (!(flags & SRC2_IMM) && (flags & CUMULATIVE_OP) && (src1 & SLJIT_IMM) && src1w) {
			if (src1w <= SIMM_MAX && src1w >= SIMM_MIN) {
				flags |= SRC2_IMM;
				src2_r = src1w;

				/* And swap arguments. */
				src1 = src2;
				src1w = src2w;
				src2 = SLJIT_IMM;
				/* src2w = src2_r unneeded. */
			}
		}
	}

	/* Source 1. */
	if (FAST_IS_REG(src1))
		src1_r = src1;
	else if (src1 & SLJIT_IMM) {
		if (src1w) {
			FAIL_IF(load_immediate(compiler, TMP_REG1, src1w));
			src1_r = TMP_REG1;
		}
		else
			src1_r = 0;
	}
	else {
		if (getput_arg_fast(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC1;
		src1_r = TMP_REG1;
	}

	/* Source 2. */
	if (FAST_IS_REG(src2)) {
		src2_r = src2;
		flags |= REG2_SOURCE;
		if (!(flags & REG_DEST) && op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
			dst_r = src2_r;
	}
	else if (src2 & SLJIT_IMM) {
		if (!(flags & SRC2_IMM)) {
			if (src2w) {
				FAIL_IF(load_immediate(compiler, sugg_src2_r, src2w));
				src2_r = sugg_src2_r;
			}
			else {
				src2_r = 0;
				if ((op >= SLJIT_MOV && op <= SLJIT_MOVU_SI) && (dst & SLJIT_MEM))
					dst_r = 0;
			}
		}
	}
	else {
		if (getput_arg_fast(compiler, flags | LOAD_DATA, sugg_src2_r, src2, src2w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC2;
		src2_r = sugg_src2_r;
	}

	if ((flags & (SLOW_SRC1 | SLOW_SRC2)) == (SLOW_SRC1 | SLOW_SRC2)) {
		SLJIT_ASSERT(src2_r == TMP_REG2);
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & SLOW_SRC1)
		FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
	else if (flags & SLOW_SRC2)
		FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, sugg_src2_r, src2, src2w, dst, dstw));

	FAIL_IF(emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r));

	if (dst & SLJIT_MEM) {
		if (!(flags & SLOW_DEST)) {
			getput_arg_fast(compiler, flags, dst_r, dst, dstw);
			return compiler->error;
		}
		return getput_arg(compiler, flags, dst_r, dst, dstw, 0, 0);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op0(struct sljit_compiler *compiler, sljit_si op)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op0(compiler, op));

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
		return push_inst(compiler, TA, UNMOVABLE_INS);
	case SLJIT_NOP:
		return push_inst(compiler, NOP, UNMOVABLE_INS);
	case SLJIT_LUMUL:
	case SLJIT_LSMUL:
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
		FAIL_IF(push_inst(compiler, (op == SLJIT_LUMUL ? UMUL : SMUL) | D(SLJIT_R0) | S1(SLJIT_R0) | S2(SLJIT_R1), DR(SLJIT_R0)));
		return push_inst(compiler, RDY | D(SLJIT_R1), DR(SLJIT_R1));
#else
#error "Implementation required"
#endif
	case SLJIT_UDIVMOD:
	case SLJIT_SDIVMOD:
	case SLJIT_UDIVI:
	case SLJIT_SDIVI:
		SLJIT_COMPILE_ASSERT((SLJIT_UDIVMOD & 0x2) == 0 && SLJIT_UDIVI - 0x2 == SLJIT_UDIVMOD, bad_div_opcode_assignments);
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
		if ((op | 0x2) == SLJIT_UDIVI)
			FAIL_IF(push_inst(compiler, WRY | S1(0), MOVABLE_INS));
		else {
			FAIL_IF(push_inst(compiler, SRA | D(TMP_REG1) | S1(SLJIT_R0) | IMM(31), DR(TMP_REG1)));
			FAIL_IF(push_inst(compiler, WRY | S1(TMP_REG1), MOVABLE_INS));
		}
		if (op <= SLJIT_SDIVMOD)
			FAIL_IF(push_inst(compiler, OR | D(TMP_REG2) | S1(0) | S2(SLJIT_R0), DR(TMP_REG2)));
		FAIL_IF(push_inst(compiler, ((op | 0x2) == SLJIT_UDIVI ? UDIV : SDIV) | D(SLJIT_R0) | S1(SLJIT_R0) | S2(SLJIT_R1), DR(SLJIT_R0)));
		if (op >= SLJIT_UDIVI)
			return SLJIT_SUCCESS;
		FAIL_IF(push_inst(compiler, SMUL | D(SLJIT_R1) | S1(SLJIT_R0) | S2(SLJIT_R1), DR(SLJIT_R1)));
		return push_inst(compiler, SUB | D(SLJIT_R1) | S1(TMP_REG2) | S2(SLJIT_R1), DR(SLJIT_R1));
#else
#error "Implementation required"
#endif
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si flags = GET_FLAGS(op) ? SET_FLAGS : 0;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_MOV:
	case SLJIT_MOV_P:
		return emit_op(compiler, SLJIT_MOV, flags | WORD_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UI:
		return emit_op(compiler, SLJIT_MOV_UI, flags | INT_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SI:
		return emit_op(compiler, SLJIT_MOV_SI, flags | INT_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UB:
		return emit_op(compiler, SLJIT_MOV_UB, flags | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_ub)srcw : srcw);

	case SLJIT_MOV_SB:
		return emit_op(compiler, SLJIT_MOV_SB, flags | BYTE_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_sb)srcw : srcw);

	case SLJIT_MOV_UH:
		return emit_op(compiler, SLJIT_MOV_UH, flags | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_uh)srcw : srcw);

	case SLJIT_MOV_SH:
		return emit_op(compiler, SLJIT_MOV_SH, flags | HALF_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_sh)srcw : srcw);

	case SLJIT_MOVU:
	case SLJIT_MOVU_P:
		return emit_op(compiler, SLJIT_MOV, flags | WORD_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UI:
		return emit_op(compiler, SLJIT_MOV_UI, flags | INT_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_SI:
		return emit_op(compiler, SLJIT_MOV_SI, flags | INT_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UB:
		return emit_op(compiler, SLJIT_MOV_UB, flags | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_ub)srcw : srcw);

	case SLJIT_MOVU_SB:
		return emit_op(compiler, SLJIT_MOV_SB, flags | BYTE_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_sb)srcw : srcw);

	case SLJIT_MOVU_UH:
		return emit_op(compiler, SLJIT_MOV_UH, flags | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_uh)srcw : srcw);

	case SLJIT_MOVU_SH:
		return emit_op(compiler, SLJIT_MOV_SH, flags | HALF_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (sljit_sh)srcw : srcw);

	case SLJIT_NOT:
	case SLJIT_CLZ:
		return emit_op(compiler, op, flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return emit_op(compiler, SLJIT_SUB, flags | IMM_OP, dst, dstw, SLJIT_IMM, 0, src, srcw);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si flags = GET_FLAGS(op) ? SET_FLAGS : 0;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_ADD:
	case SLJIT_ADDC:
	case SLJIT_MUL:
	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		return emit_op(compiler, op, flags | CUMULATIVE_OP | IMM_OP, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUB:
	case SLJIT_SUBC:
		return emit_op(compiler, op, flags | IMM_OP, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SHL:
	case SLJIT_LSHR:
	case SLJIT_ASHR:
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
		if (src2 & SLJIT_IMM)
			src2w &= 0x1f;
#else
		SLJIT_ASSERT_STOP();
#endif
		return emit_op(compiler, op, flags | IMM_OP, dst, dstw, src1, src1w, src2, src2w);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_register_index(sljit_si reg)
{
	CHECK_REG_INDEX(check_sljit_get_register_index(reg));
	return reg_map[reg];
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_float_register_index(sljit_si reg)
{
	CHECK_REG_INDEX(check_sljit_get_float_register_index(reg));
	return reg << 1;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_si size)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_custom(compiler, instruction, size));

	return push_inst(compiler, *(sljit_ins*)instruction, UNMOVABLE_INS);
}

/* --------------------------------------------------------------------- */
/*  Floating point operators                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_is_fpu_available(void)
{
#ifdef SLJIT_IS_FPU_AVAILABLE
	return SLJIT_IS_FPU_AVAILABLE;
#else
	/* Available by default. */
	return 1;
#endif
}

#define FLOAT_DATA(op) (DOUBLE_DATA | ((op & SLJIT_SINGLE_OP) >> 7))
#define SELECT_FOP(op, single, double) ((op & SLJIT_SINGLE_OP) ? single : double)
#define FLOAT_TMP_MEM_OFFSET (22 * sizeof(sljit_sw))

static SLJIT_INLINE sljit_si sljit_emit_fop1_convw_fromd(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	if (src & SLJIT_MEM) {
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src, srcw, dst, dstw));
		src = TMP_FREG1;
	}
	else
		src <<= 1;

	FAIL_IF(push_inst(compiler, SELECT_FOP(op, FSTOI, FDTOI) | DA(TMP_FREG1) | S2A(src), MOVABLE_INS));

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	if (FAST_IS_REG(dst)) {
		FAIL_IF(emit_op_mem2(compiler, SINGLE_DATA, TMP_FREG1, SLJIT_MEM1(SLJIT_SP), FLOAT_TMP_MEM_OFFSET, SLJIT_MEM1(SLJIT_SP), FLOAT_TMP_MEM_OFFSET));
		return emit_op_mem2(compiler, WORD_DATA | LOAD_DATA, dst, SLJIT_MEM1(SLJIT_SP), FLOAT_TMP_MEM_OFFSET, SLJIT_MEM1(SLJIT_SP), FLOAT_TMP_MEM_OFFSET);
	}

	/* Store the integer value from a VFP register. */
	return emit_op_mem2(compiler, SINGLE_DATA, TMP_FREG1, dst, dstw, 0, 0);
}

static SLJIT_INLINE sljit_si sljit_emit_fop1_convd_fromw(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r = FAST_IS_REG(dst) ? (dst << 1) : TMP_FREG1;

	if (src & SLJIT_IMM) {
#if (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
		if (GET_OPCODE(op) == SLJIT_CONVD_FROMI)
			srcw = (sljit_si)srcw;
#endif
		FAIL_IF(load_immediate(compiler, TMP_REG1, srcw));
		src = TMP_REG1;
		srcw = 0;
	}

	if (FAST_IS_REG(src)) {
		FAIL_IF(emit_op_mem2(compiler, WORD_DATA, src, SLJIT_MEM1(SLJIT_SP), FLOAT_TMP_MEM_OFFSET, SLJIT_MEM1(SLJIT_SP), FLOAT_TMP_MEM_OFFSET));
		src = SLJIT_MEM1(SLJIT_SP);
		srcw = FLOAT_TMP_MEM_OFFSET;
	}

	FAIL_IF(emit_op_mem2(compiler, SINGLE_DATA | LOAD_DATA, TMP_FREG1, src, srcw, dst, dstw));
	FAIL_IF(push_inst(compiler, SELECT_FOP(op, FITOS, FITOD) | DA(dst_r) | S2A(TMP_FREG1), MOVABLE_INS));

	if (dst & SLJIT_MEM)
		return emit_op_mem2(compiler, FLOAT_DATA(op), TMP_FREG1, dst, dstw, 0, 0);
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_si sljit_emit_fop1_cmp(struct sljit_compiler *compiler, sljit_si op,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	if (src1 & SLJIT_MEM) {
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, src2, src2w));
		src1 = TMP_FREG1;
	}
	else
		src1 <<= 1;

	if (src2 & SLJIT_MEM) {
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, 0, 0));
		src2 = TMP_FREG2;
	}
	else
		src2 <<= 1;

	return push_inst(compiler, SELECT_FOP(op, FCMPS, FCMPD) | S1A(src1) | S2A(src2), FCC_IS_SET | MOVABLE_INS);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r;

	CHECK_ERROR();
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	SLJIT_COMPILE_ASSERT((SLJIT_SINGLE_OP == 0x100) && !(DOUBLE_DATA & 0x2), float_transfer_bit_error);
	SELECT_FOP1_OPERATION_WITH_CHECKS(compiler, op, dst, dstw, src, srcw);

	if (GET_OPCODE(op) == SLJIT_CONVD_FROMS)
		op ^= SLJIT_SINGLE_OP;

	dst_r = FAST_IS_REG(dst) ? (dst << 1) : TMP_FREG1;

	if (src & SLJIT_MEM) {
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op) | LOAD_DATA, dst_r, src, srcw, dst, dstw));
		src = dst_r;
	}
	else
		src <<= 1;

	switch (GET_OPCODE(op)) {
	case SLJIT_DMOV:
		if (src != dst_r) {
			if (dst_r != TMP_FREG1) {
				FAIL_IF(push_inst(compiler, FMOVS | DA(dst_r) | S2A(src), MOVABLE_INS));
				if (!(op & SLJIT_SINGLE_OP))
					FAIL_IF(push_inst(compiler, FMOVS | DA(dst_r | 1) | S2A(src | 1), MOVABLE_INS));
			}
			else
				dst_r = src;
		}
		break;
	case SLJIT_DNEG:
		FAIL_IF(push_inst(compiler, FNEGS | DA(dst_r) | S2A(src), MOVABLE_INS));
		if (dst_r != src && !(op & SLJIT_SINGLE_OP))
			FAIL_IF(push_inst(compiler, FMOVS | DA(dst_r | 1) | S2A(src | 1), MOVABLE_INS));
		break;
	case SLJIT_DABS:
		FAIL_IF(push_inst(compiler, FABSS | DA(dst_r) | S2A(src), MOVABLE_INS));
		if (dst_r != src && !(op & SLJIT_SINGLE_OP))
			FAIL_IF(push_inst(compiler, FMOVS | DA(dst_r | 1) | S2A(src | 1), MOVABLE_INS));
		break;
	case SLJIT_CONVD_FROMS:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FSTOD, FDTOS) | DA(dst_r) | S2A(src), MOVABLE_INS));
		op ^= SLJIT_SINGLE_OP;
		break;
	}

	if (dst & SLJIT_MEM)
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op), dst_r, dst, dstw, 0, 0));
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si dst_r, flags = 0;

	CHECK_ERROR();
	CHECK(check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = FAST_IS_REG(dst) ? (dst << 1) : TMP_FREG2;

	if (src1 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w)) {
			FAIL_IF(compiler->error);
			src1 = TMP_FREG1;
		} else
			flags |= SLOW_SRC1;
	}
	else
		src1 <<= 1;

	if (src2 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w)) {
			FAIL_IF(compiler->error);
			src2 = TMP_FREG2;
		} else
			flags |= SLOW_SRC2;
	}
	else
		src2 <<= 1;

	if ((flags & (SLOW_SRC1 | SLOW_SRC2)) == (SLOW_SRC1 | SLOW_SRC2)) {
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & SLOW_SRC1)
		FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG1, src1, src1w, dst, dstw));
	else if (flags & SLOW_SRC2)
		FAIL_IF(getput_arg(compiler, FLOAT_DATA(op) | LOAD_DATA, TMP_FREG2, src2, src2w, dst, dstw));

	if (flags & SLOW_SRC1)
		src1 = TMP_FREG1;
	if (flags & SLOW_SRC2)
		src2 = TMP_FREG2;

	switch (GET_OPCODE(op)) {
	case SLJIT_DADD:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FADDS, FADDD) | DA(dst_r) | S1A(src1) | S2A(src2), MOVABLE_INS));
		break;

	case SLJIT_DSUB:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FSUBS, FSUBD) | DA(dst_r) | S1A(src1) | S2A(src2), MOVABLE_INS));
		break;

	case SLJIT_DMUL:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FMULS, FMULD) | DA(dst_r) | S1A(src1) | S2A(src2), MOVABLE_INS));
		break;

	case SLJIT_DDIV:
		FAIL_IF(push_inst(compiler, SELECT_FOP(op, FDIVS, FDIVD) | DA(dst_r) | S1A(src1) | S2A(src2), MOVABLE_INS));
		break;
	}

	if (dst_r == TMP_FREG2)
		FAIL_IF(emit_op_mem2(compiler, FLOAT_DATA(op), TMP_FREG2, dst, dstw, 0, 0));

	return SLJIT_SUCCESS;
}

#undef FLOAT_DATA
#undef SELECT_FOP

/* --------------------------------------------------------------------- */
/*  Other instructions                                                   */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_enter(compiler, dst, dstw));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	/* For UNUSED dst. Uncommon, but possible. */
	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	if (FAST_IS_REG(dst))
		return push_inst(compiler, OR | D(dst) | S1(0) | S2(TMP_LINK), DR(dst));

	/* Memory. */
	return emit_op_mem(compiler, WORD_DATA, TMP_LINK, dst, dstw);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_return(compiler, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (FAST_IS_REG(src))
		FAIL_IF(push_inst(compiler, OR | D(TMP_LINK) | S1(0) | S2(src), DR(TMP_LINK)));
	else if (src & SLJIT_MEM)
		FAIL_IF(emit_op_mem(compiler, WORD_DATA | LOAD_DATA, TMP_LINK, src, srcw));
	else if (src & SLJIT_IMM)
		FAIL_IF(load_immediate(compiler, TMP_LINK, srcw));

	FAIL_IF(push_inst(compiler, JMPL | D(0) | S1(TMP_LINK) | IMM(8), UNMOVABLE_INS));
	return push_inst(compiler, NOP, UNMOVABLE_INS);
}

/* --------------------------------------------------------------------- */
/*  Conditional instructions                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_label(compiler));

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	PTR_FAIL_IF(!label);
	set_label(label, compiler);
	compiler->delay_slot = UNMOVABLE_INS;
	return label;
}

static sljit_ins get_cc(sljit_si type)
{
	switch (type) {
	case SLJIT_EQUAL:
	case SLJIT_MUL_NOT_OVERFLOW:
	case SLJIT_D_NOT_EQUAL: /* Unordered. */
		return DA(0x1);

	case SLJIT_NOT_EQUAL:
	case SLJIT_MUL_OVERFLOW:
	case SLJIT_D_EQUAL:
		return DA(0x9);

	case SLJIT_LESS:
	case SLJIT_D_GREATER: /* Unordered. */
		return DA(0x5);

	case SLJIT_GREATER_EQUAL:
	case SLJIT_D_LESS_EQUAL:
		return DA(0xd);

	case SLJIT_GREATER:
	case SLJIT_D_GREATER_EQUAL: /* Unordered. */
		return DA(0xc);

	case SLJIT_LESS_EQUAL:
	case SLJIT_D_LESS:
		return DA(0x4);

	case SLJIT_SIG_LESS:
		return DA(0x3);

	case SLJIT_SIG_GREATER_EQUAL:
		return DA(0xb);

	case SLJIT_SIG_GREATER:
		return DA(0xa);

	case SLJIT_SIG_LESS_EQUAL:
		return DA(0x2);

	case SLJIT_OVERFLOW:
	case SLJIT_D_UNORDERED:
		return DA(0x7);

	case SLJIT_NOT_OVERFLOW:
	case SLJIT_D_ORDERED:
		return DA(0xf);

	default:
		SLJIT_ASSERT_STOP();
		return DA(0x8);
	}
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_si type)
{
	struct sljit_jump *jump;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_jump(compiler, type));

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);
	type &= 0xff;

	if (type < SLJIT_D_EQUAL) {
		jump->flags |= IS_COND;
		if (((compiler->delay_slot & DST_INS_MASK) != UNMOVABLE_INS) && !(compiler->delay_slot & ICC_IS_SET))
			jump->flags |= IS_MOVABLE;
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
		PTR_FAIL_IF(push_inst(compiler, BICC | get_cc(type ^ 1) | 5, UNMOVABLE_INS));
#else
#error "Implementation required"
#endif
	}
	else if (type < SLJIT_JUMP) {
		jump->flags |= IS_COND;
		if (((compiler->delay_slot & DST_INS_MASK) != UNMOVABLE_INS) && !(compiler->delay_slot & FCC_IS_SET))
			jump->flags |= IS_MOVABLE;
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
		PTR_FAIL_IF(push_inst(compiler, FBFCC | get_cc(type ^ 1) | 5, UNMOVABLE_INS));
#else
#error "Implementation required"
#endif
	} else {
		if ((compiler->delay_slot & DST_INS_MASK) != UNMOVABLE_INS)
			jump->flags |= IS_MOVABLE;
		if (type >= SLJIT_FAST_CALL)
			jump->flags |= IS_CALL;
	}

	PTR_FAIL_IF(emit_const(compiler, TMP_REG2, 0));
	PTR_FAIL_IF(push_inst(compiler, JMPL | D(type >= SLJIT_FAST_CALL ? TMP_LINK : 0) | S1(TMP_REG2) | IMM(0), UNMOVABLE_INS));
	jump->addr = compiler->size;
	PTR_FAIL_IF(push_inst(compiler, NOP, UNMOVABLE_INS));

	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_ijump(struct sljit_compiler *compiler, sljit_si type, sljit_si src, sljit_sw srcw)
{
	struct sljit_jump *jump = NULL;
	sljit_si src_r;

	CHECK_ERROR();
	CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (FAST_IS_REG(src))
		src_r = src;
	else if (src & SLJIT_IMM) {
		jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		FAIL_IF(!jump);
		set_jump(jump, compiler, JUMP_ADDR);
		jump->u.target = srcw;
		if ((compiler->delay_slot & DST_INS_MASK) != UNMOVABLE_INS)
			jump->flags |= IS_MOVABLE;
		if (type >= SLJIT_FAST_CALL)
			jump->flags |= IS_CALL;

		FAIL_IF(emit_const(compiler, TMP_REG2, 0));
		src_r = TMP_REG2;
	}
	else {
		FAIL_IF(emit_op_mem(compiler, WORD_DATA | LOAD_DATA, TMP_REG2, src, srcw));
		src_r = TMP_REG2;
	}

	FAIL_IF(push_inst(compiler, JMPL | D(type >= SLJIT_FAST_CALL ? TMP_LINK : 0) | S1(src_r) | IMM(0), UNMOVABLE_INS));
	if (jump)
		jump->addr = compiler->size;
	return push_inst(compiler, NOP, UNMOVABLE_INS);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw,
	sljit_si type)
{
	sljit_si reg, flags = (GET_FLAGS(op) ? SET_FLAGS : 0);

	CHECK_ERROR();
	CHECK(check_sljit_emit_op_flags(compiler, op, dst, dstw, src, srcw, type));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
	op = GET_OPCODE(op);
	reg = (op < SLJIT_ADD && FAST_IS_REG(dst)) ? dst : TMP_REG2;

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	if (op >= SLJIT_ADD && (src & SLJIT_MEM)) {
		ADJUST_LOCAL_OFFSET(src, srcw);
		FAIL_IF(emit_op_mem2(compiler, WORD_DATA | LOAD_DATA, TMP_REG1, src, srcw, dst, dstw));
		src = TMP_REG1;
		srcw = 0;
	}

	type &= 0xff;
	if (type < SLJIT_D_EQUAL)
		FAIL_IF(push_inst(compiler, BICC | get_cc(type) | 3, UNMOVABLE_INS));
	else
		FAIL_IF(push_inst(compiler, FBFCC | get_cc(type) | 3, UNMOVABLE_INS));

	FAIL_IF(push_inst(compiler, OR | D(reg) | S1(0) | IMM(1), UNMOVABLE_INS));
	FAIL_IF(push_inst(compiler, OR | D(reg) | S1(0) | IMM(0), UNMOVABLE_INS));

	if (op >= SLJIT_ADD)
		return emit_op(compiler, op, flags | CUMULATIVE_OP | IMM_OP | ALT_KEEP_CACHE, dst, dstw, src, srcw, TMP_REG2, 0);

	return (reg == TMP_REG2) ? emit_op_mem(compiler, WORD_DATA, TMP_REG2, dst, dstw) : SLJIT_SUCCESS;
#else
#error "Implementation required"
#endif
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw init_value)
{
	sljit_si reg;
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_const(compiler, dst, dstw, init_value));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);
	set_const(const_, compiler);

	reg = SLOW_IS_REG(dst) ? dst : TMP_REG2;

	PTR_FAIL_IF(emit_const(compiler, reg, init_value));

	if (dst & SLJIT_MEM)
		PTR_FAIL_IF(emit_op_mem(compiler, WORD_DATA, TMP_REG2, dst, dstw));
	return const_;
}
