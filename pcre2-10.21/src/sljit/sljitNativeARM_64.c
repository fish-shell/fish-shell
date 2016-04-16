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
	return "ARM-64" SLJIT_CPUINFO;
}

/* Length of an instruction word */
typedef sljit_ui sljit_ins;

#define TMP_ZERO	(0)

#define TMP_REG1	(SLJIT_NUMBER_OF_REGISTERS + 2)
#define TMP_REG2	(SLJIT_NUMBER_OF_REGISTERS + 3)
#define TMP_REG3	(SLJIT_NUMBER_OF_REGISTERS + 4)
#define TMP_LR		(SLJIT_NUMBER_OF_REGISTERS + 5)
#define TMP_SP		(SLJIT_NUMBER_OF_REGISTERS + 6)

#define TMP_FREG1	(0)
#define TMP_FREG2	(SLJIT_NUMBER_OF_FLOAT_REGISTERS + 1)

static SLJIT_CONST sljit_ub reg_map[SLJIT_NUMBER_OF_REGISTERS + 8] = {
  31, 0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15, 16, 17, 8, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 29, 9, 10, 11, 30, 31
};

#define W_OP (1 << 31)
#define RD(rd) (reg_map[rd])
#define RT(rt) (reg_map[rt])
#define RN(rn) (reg_map[rn] << 5)
#define RT2(rt2) (reg_map[rt2] << 10)
#define RM(rm) (reg_map[rm] << 16)
#define VD(vd) (vd)
#define VT(vt) (vt)
#define VN(vn) ((vn) << 5)
#define VM(vm) ((vm) << 16)

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */

#define ADC 0x9a000000
#define ADD 0x8b000000
#define ADDI 0x91000000
#define AND 0x8a000000
#define ANDI 0x92000000
#define ASRV 0x9ac02800
#define B 0x14000000
#define B_CC 0x54000000
#define BL 0x94000000
#define BLR 0xd63f0000
#define BR 0xd61f0000
#define BRK 0xd4200000
#define CBZ 0xb4000000
#define CLZ 0xdac01000
#define CSINC 0x9a800400
#define EOR 0xca000000
#define EORI 0xd2000000
#define FABS 0x1e60c000
#define FADD 0x1e602800
#define FCMP 0x1e602000
#define FCVT 0x1e224000
#define FCVTZS 0x9e780000
#define FDIV 0x1e601800
#define FMOV 0x1e604000
#define FMUL 0x1e600800
#define FNEG 0x1e614000
#define FSUB 0x1e603800
#define LDRI 0xf9400000
#define LDP 0xa9400000
#define LDP_PST 0xa8c00000
#define LSLV 0x9ac02000
#define LSRV 0x9ac02400
#define MADD 0x9b000000
#define MOVK 0xf2800000
#define MOVN 0x92800000
#define MOVZ 0xd2800000
#define NOP 0xd503201f
#define ORN 0xaa200000
#define ORR 0xaa000000
#define ORRI 0xb2000000
#define RET 0xd65f0000
#define SBC 0xda000000
#define SBFM 0x93000000
#define SCVTF 0x9e620000
#define SDIV 0x9ac00c00
#define SMADDL 0x9b200000
#define SMULH 0x9b403c00
#define STP 0xa9000000
#define STP_PRE 0xa9800000
#define STRI 0xf9000000
#define STR_FI 0x3d000000
#define STR_FR 0x3c206800
#define STUR_FI 0x3c000000
#define SUB 0xcb000000
#define SUBI 0xd1000000
#define SUBS 0xeb000000
#define UBFM 0xd3000000
#define UDIV 0x9ac00800
#define UMULH 0x9bc03c00

/* dest_reg is the absolute name of the register
   Useful for reordering instructions in the delay slot. */
static sljit_si push_inst(struct sljit_compiler *compiler, sljit_ins ins)
{
	sljit_ins *ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_si emit_imm64_const(struct sljit_compiler *compiler, sljit_si dst, sljit_uw imm)
{
	FAIL_IF(push_inst(compiler, MOVZ | RD(dst) | ((imm & 0xffff) << 5)));
	FAIL_IF(push_inst(compiler, MOVK | RD(dst) | (((imm >> 16) & 0xffff) << 5) | (1 << 21)));
	FAIL_IF(push_inst(compiler, MOVK | RD(dst) | (((imm >> 32) & 0xffff) << 5) | (2 << 21)));
	return push_inst(compiler, MOVK | RD(dst) | ((imm >> 48) << 5) | (3 << 21));
}

static SLJIT_INLINE void modify_imm64_const(sljit_ins* inst, sljit_uw new_imm)
{
	sljit_si dst = inst[0] & 0x1f;
	SLJIT_ASSERT((inst[0] & 0xffe00000) == MOVZ && (inst[1] & 0xffe00000) == (MOVK | (1 << 21)));
	inst[0] = MOVZ | dst | ((new_imm & 0xffff) << 5);
	inst[1] = MOVK | dst | (((new_imm >> 16) & 0xffff) << 5) | (1 << 21);
	inst[2] = MOVK | dst | (((new_imm >> 32) & 0xffff) << 5) | (2 << 21);
	inst[3] = MOVK | dst | ((new_imm >> 48) << 5) | (3 << 21);
}

static SLJIT_INLINE sljit_si detect_jump_type(struct sljit_jump *jump, sljit_ins *code_ptr, sljit_ins *code)
{
	sljit_sw diff;
	sljit_uw target_addr;

	if (jump->flags & SLJIT_REWRITABLE_JUMP) {
		jump->flags |= PATCH_ABS64;
		return 0;
	}

	if (jump->flags & JUMP_ADDR)
		target_addr = jump->u.target;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		target_addr = (sljit_uw)(code + jump->u.label->size);
	}
	diff = (sljit_sw)target_addr - (sljit_sw)(code_ptr + 4);

	if (jump->flags & IS_COND) {
		diff += sizeof(sljit_ins);
		if (diff <= 0xfffff && diff >= -0x100000) {
			code_ptr[-5] ^= (jump->flags & IS_CBZ) ? (0x1 << 24) : 0x1;
			jump->addr -= sizeof(sljit_ins);
			jump->flags |= PATCH_COND;
			return 5;
		}
		diff -= sizeof(sljit_ins);
	}

	if (diff <= 0x7ffffff && diff >= -0x8000000) {
		jump->flags |= PATCH_B;
		return 4;
	}

	if (target_addr <= 0xffffffffl) {
		if (jump->flags & IS_COND)
			code_ptr[-5] -= (2 << 5);
		code_ptr[-2] = code_ptr[0];
		return 2;
	}
	if (target_addr <= 0xffffffffffffl) {
		if (jump->flags & IS_COND)
			code_ptr[-5] -= (1 << 5);
		jump->flags |= PATCH_ABS48;
		code_ptr[-1] = code_ptr[0];
		return 1;
	}

	jump->flags |= PATCH_ABS64;
	return 0;
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
	sljit_si dst;

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
			/* These structures are ordered by their address. */
			SLJIT_ASSERT(!label || label->size >= word_count);
			SLJIT_ASSERT(!jump || jump->addr >= word_count);
			SLJIT_ASSERT(!const_ || const_->addr >= word_count);
			if (label && label->size == word_count) {
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
					jump->addr = (sljit_uw)(code_ptr - 4);
					code_ptr -= detect_jump_type(jump, code_ptr, code);
					jump = jump->next;
			}
			if (const_ && const_->addr == word_count) {
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
	SLJIT_ASSERT(code_ptr - code <= (sljit_sw)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		do {
			addr = (jump->flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target;
			buf_ptr = (sljit_ins*)jump->addr;
			if (jump->flags & PATCH_B) {
				addr = (sljit_sw)(addr - jump->addr) >> 2;
				SLJIT_ASSERT((sljit_sw)addr <= 0x1ffffff && (sljit_sw)addr >= -0x2000000);
				buf_ptr[0] = ((jump->flags & IS_BL) ? BL : B) | (addr & 0x3ffffff);
				if (jump->flags & IS_COND)
					buf_ptr[-1] -= (4 << 5);
				break;
			}
			if (jump->flags & PATCH_COND) {
				addr = (sljit_sw)(addr - jump->addr) >> 2;
				SLJIT_ASSERT((sljit_sw)addr <= 0x3ffff && (sljit_sw)addr >= -0x40000);
				buf_ptr[0] = (buf_ptr[0] & ~0xffffe0) | ((addr & 0x7ffff) << 5);
				break;
			}

			SLJIT_ASSERT((jump->flags & (PATCH_ABS48 | PATCH_ABS64)) || addr <= 0xffffffffl);
			SLJIT_ASSERT((jump->flags & PATCH_ABS64) || addr <= 0xffffffffffffl);

			dst = buf_ptr[0] & 0x1f;
			buf_ptr[0] = MOVZ | dst | ((addr & 0xffff) << 5);
			buf_ptr[1] = MOVK | dst | (((addr >> 16) & 0xffff) << 5) | (1 << 21);
			if (jump->flags & (PATCH_ABS48 | PATCH_ABS64))
				buf_ptr[2] = MOVK | dst | (((addr >> 32) & 0xffff) << 5) | (2 << 21);
			if (jump->flags & PATCH_ABS64)
				buf_ptr[3] = MOVK | dst | (((addr >> 48) & 0xffff) << 5) | (3 << 21);
		} while (0);
		jump = jump->next;
	}

	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_size = (code_ptr - code) * sizeof(sljit_ins);
	SLJIT_CACHE_FLUSH(code, code_ptr);
	return code;
}

/* --------------------------------------------------------------------- */
/*  Core code generator functions.                                       */
/* --------------------------------------------------------------------- */

#define COUNT_TRAILING_ZERO(value, result) \
	result = 0; \
	if (!(value & 0xffffffff)) { \
		result += 32; \
		value >>= 32; \
	} \
	if (!(value & 0xffff)) { \
		result += 16; \
		value >>= 16; \
	} \
	if (!(value & 0xff)) { \
		result += 8; \
		value >>= 8; \
	} \
	if (!(value & 0xf)) { \
		result += 4; \
		value >>= 4; \
	} \
	if (!(value & 0x3)) { \
		result += 2; \
		value >>= 2; \
	} \
	if (!(value & 0x1)) { \
		result += 1; \
		value >>= 1; \
	}

#define LOGICAL_IMM_CHECK 0x100

static sljit_ins logical_imm(sljit_sw imm, sljit_si len)
{
	sljit_si negated, ones, right;
	sljit_uw mask, uimm;
	sljit_ins ins;

	if (len & LOGICAL_IMM_CHECK) {
		len &= ~LOGICAL_IMM_CHECK;
		if (len == 32 && (imm == 0 || imm == -1))
			return 0;
		if (len == 16 && ((sljit_si)imm == 0 || (sljit_si)imm == -1))
			return 0;
	}

	SLJIT_ASSERT((len == 32 && imm != 0 && imm != -1)
		|| (len == 16 && (sljit_si)imm != 0 && (sljit_si)imm != -1));
	uimm = (sljit_uw)imm;
	while (1) {
		if (len <= 0) {
			SLJIT_ASSERT_STOP();
			return 0;
		}
		mask = ((sljit_uw)1 << len) - 1;
		if ((uimm & mask) != ((uimm >> len) & mask))
			break;
		len >>= 1;
	}

	len <<= 1;

	negated = 0;
	if (uimm & 0x1) {
		negated = 1;
		uimm = ~uimm;
	}

	if (len < 64)
		uimm &= ((sljit_uw)1 << len) - 1;

	/* Unsigned right shift. */
	COUNT_TRAILING_ZERO(uimm, right);

	/* Signed shift. We also know that the highest bit is set. */
	imm = (sljit_sw)~uimm;
	SLJIT_ASSERT(imm < 0);

	COUNT_TRAILING_ZERO(imm, ones);

	if (~imm)
		return 0;

	if (len == 64)
		ins = 1 << 22;
	else
		ins = (0x3f - ((len << 1) - 1)) << 10;

	if (negated)
		return ins | ((len - ones - 1) << 10) | ((len - ones - right) << 16);

	return ins | ((ones - 1) << 10) | ((len - right) << 16);
}

#undef COUNT_TRAILING_ZERO

static sljit_si load_immediate(struct sljit_compiler *compiler, sljit_si dst, sljit_sw simm)
{
	sljit_uw imm = (sljit_uw)simm;
	sljit_si i, zeros, ones, first;
	sljit_ins bitmask;

	if (imm <= 0xffff)
		return push_inst(compiler, MOVZ | RD(dst) | (imm << 5));

	if (simm >= -0x10000 && simm < 0)
		return push_inst(compiler, MOVN | RD(dst) | ((~imm & 0xffff) << 5));

	if (imm <= 0xffffffffl) {
		if ((imm & 0xffff0000l) == 0xffff0000)
			return push_inst(compiler, (MOVN ^ W_OP) | RD(dst) | ((~imm & 0xffff) << 5));
		if ((imm & 0xffff) == 0xffff)
			return push_inst(compiler, (MOVN ^ W_OP) | RD(dst) | ((~imm & 0xffff0000l) >> (16 - 5)) | (1 << 21));
		bitmask = logical_imm(simm, 16);
		if (bitmask != 0)
			return push_inst(compiler, (ORRI ^ W_OP) | RD(dst) | RN(TMP_ZERO) | bitmask);
	}
	else {
		bitmask = logical_imm(simm, 32);
		if (bitmask != 0)
			return push_inst(compiler, ORRI | RD(dst) | RN(TMP_ZERO) | bitmask);
	}

	if (imm <= 0xffffffffl) {
		FAIL_IF(push_inst(compiler, MOVZ | RD(dst) | ((imm & 0xffff) << 5)));
		return push_inst(compiler, MOVK | RD(dst) | ((imm & 0xffff0000l) >> (16 - 5)) | (1 << 21));
	}

	if (simm >= -0x100000000l && simm < 0) {
		FAIL_IF(push_inst(compiler, MOVN | RD(dst) | ((~imm & 0xffff) << 5)));
		return push_inst(compiler, MOVK | RD(dst) | ((imm & 0xffff0000l) >> (16 - 5)) | (1 << 21));
	}

	/* A large amount of number can be constructed from ORR and MOVx,
	but computing them is costly. We don't  */

	zeros = 0;
	ones = 0;
	for (i = 4; i > 0; i--) {
		if ((simm & 0xffff) == 0)
			zeros++;
		if ((simm & 0xffff) == 0xffff)
			ones++;
		simm >>= 16;
	}

	simm = (sljit_sw)imm;
	first = 1;
	if (ones > zeros) {
		simm = ~simm;
		for (i = 0; i < 4; i++) {
			if (!(simm & 0xffff)) {
				simm >>= 16;
				continue;
			}
			if (first) {
				first = 0;
				FAIL_IF(push_inst(compiler, MOVN | RD(dst) | ((simm & 0xffff) << 5) | (i << 21)));
			}
			else
				FAIL_IF(push_inst(compiler, MOVK | RD(dst) | ((~simm & 0xffff) << 5) | (i << 21)));
			simm >>= 16;
		}
		return SLJIT_SUCCESS;
	}

	for (i = 0; i < 4; i++) {
		if (!(simm & 0xffff)) {
			simm >>= 16;
			continue;
		}
		if (first) {
			first = 0;
			FAIL_IF(push_inst(compiler, MOVZ | RD(dst) | ((simm & 0xffff) << 5) | (i << 21)));
		}
		else
			FAIL_IF(push_inst(compiler, MOVK | RD(dst) | ((simm & 0xffff) << 5) | (i << 21)));
		simm >>= 16;
	}
	return SLJIT_SUCCESS;
}

#define ARG1_IMM	0x0010000
#define ARG2_IMM	0x0020000
#define INT_OP		0x0040000
#define SET_FLAGS	0x0080000
#define UNUSED_RETURN	0x0100000
#define SLOW_DEST	0x0200000
#define SLOW_SRC1	0x0400000
#define SLOW_SRC2	0x0800000

#define CHECK_FLAGS(flag_bits) \
	if (flags & SET_FLAGS) { \
		inv_bits |= flag_bits; \
		if (flags & UNUSED_RETURN) \
			dst = TMP_ZERO; \
	}

static sljit_si emit_op_imm(struct sljit_compiler *compiler, sljit_si flags, sljit_si dst, sljit_sw arg1, sljit_sw arg2)
{
	/* dst must be register, TMP_REG1
	   arg1 must be register, TMP_REG1, imm
	   arg2 must be register, TMP_REG2, imm */
	sljit_ins inv_bits = (flags & INT_OP) ? (1 << 31) : 0;
	sljit_ins inst_bits;
	sljit_si op = (flags & 0xffff);
	sljit_si reg;
	sljit_sw imm, nimm;

	if (SLJIT_UNLIKELY((flags & (ARG1_IMM | ARG2_IMM)) == (ARG1_IMM | ARG2_IMM))) {
		/* Both are immediates. */
		flags &= ~ARG1_IMM;
		if (arg1 == 0 && op != SLJIT_ADD && op != SLJIT_SUB)
			arg1 = TMP_ZERO;
		else {
			FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
			arg1 = TMP_REG1;
		}
	}

	if (flags & (ARG1_IMM | ARG2_IMM)) {
		reg = (flags & ARG2_IMM) ? arg1 : arg2;
		imm = (flags & ARG2_IMM) ? arg2 : arg1;

		switch (op) {
		case SLJIT_MUL:
		case SLJIT_NEG:
		case SLJIT_CLZ:
		case SLJIT_ADDC:
		case SLJIT_SUBC:
			/* No form with immediate operand (except imm 0, which
			is represented by a ZERO register). */
			break;
		case SLJIT_MOV:
			SLJIT_ASSERT(!(flags & SET_FLAGS) && (flags & ARG2_IMM) && arg1 == TMP_REG1);
			return load_immediate(compiler, dst, imm);
		case SLJIT_NOT:
			SLJIT_ASSERT(flags & ARG2_IMM);
			FAIL_IF(load_immediate(compiler, dst, (flags & INT_OP) ? (~imm & 0xffffffff) : ~imm));
			goto set_flags;
		case SLJIT_SUB:
			if (flags & ARG1_IMM)
				break;
			imm = -imm;
			/* Fall through. */
		case SLJIT_ADD:
			if (imm == 0) {
				CHECK_FLAGS(1 << 29);
				return push_inst(compiler, ((op == SLJIT_ADD ? ADDI : SUBI) ^ inv_bits) | RD(dst) | RN(reg));
			}
			if (imm > 0 && imm <= 0xfff) {
				CHECK_FLAGS(1 << 29);
				return push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(reg) | (imm << 10));
			}
			nimm = -imm;
			if (nimm > 0 && nimm <= 0xfff) {
				CHECK_FLAGS(1 << 29);
				return push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(reg) | (nimm << 10));
			}
			if (imm > 0 && imm <= 0xffffff && !(imm & 0xfff)) {
				CHECK_FLAGS(1 << 29);
				return push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(reg) | ((imm >> 12) << 10) | (1 << 22));
			}
			if (nimm > 0 && nimm <= 0xffffff && !(nimm & 0xfff)) {
				CHECK_FLAGS(1 << 29);
				return push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(reg) | ((nimm >> 12) << 10) | (1 << 22));
			}
			if (imm > 0 && imm <= 0xffffff && !(flags & SET_FLAGS)) {
				FAIL_IF(push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(reg) | ((imm >> 12) << 10) | (1 << 22)));
				return push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(dst) | ((imm & 0xfff) << 10));
			}
			if (nimm > 0 && nimm <= 0xffffff && !(flags & SET_FLAGS)) {
				FAIL_IF(push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(reg) | ((nimm >> 12) << 10) | (1 << 22)));
				return push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(dst) | ((nimm & 0xfff) << 10));
			}
			break;
		case SLJIT_AND:
			inst_bits = logical_imm(imm, LOGICAL_IMM_CHECK | ((flags & INT_OP) ? 16 : 32));
			if (!inst_bits)
				break;
			CHECK_FLAGS(3 << 29);
			return push_inst(compiler, (ANDI ^ inv_bits) | RD(dst) | RN(reg) | inst_bits);
		case SLJIT_OR:
		case SLJIT_XOR:
			inst_bits = logical_imm(imm, LOGICAL_IMM_CHECK | ((flags & INT_OP) ? 16 : 32));
			if (!inst_bits)
				break;
			if (op == SLJIT_OR)
				inst_bits |= ORRI;
			else
				inst_bits |= EORI;
			FAIL_IF(push_inst(compiler, (inst_bits ^ inv_bits) | RD(dst) | RN(reg)));
			goto set_flags;
		case SLJIT_SHL:
			if (flags & ARG1_IMM)
				break;
			if (flags & INT_OP) {
				imm &= 0x1f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | ((-imm & 0x1f) << 16) | ((31 - imm) << 10)));
			}
			else {
				imm &= 0x3f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | (1 << 22) | ((-imm & 0x3f) << 16) | ((63 - imm) << 10)));
			}
			goto set_flags;
		case SLJIT_LSHR:
		case SLJIT_ASHR:
			if (flags & ARG1_IMM)
				break;
			if (op == SLJIT_ASHR)
				inv_bits |= 1 << 30;
			if (flags & INT_OP) {
				imm &= 0x1f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | (imm << 16) | (31 << 10)));
			}
			else {
				imm &= 0x3f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | (1 << 22) | (imm << 16) | (63 << 10)));
			}
			goto set_flags;
		default:
			SLJIT_ASSERT_STOP();
			break;
		}

		if (flags & ARG2_IMM) {
			if (arg2 == 0)
				arg2 = TMP_ZERO;
			else {
				FAIL_IF(load_immediate(compiler, TMP_REG2, arg2));
				arg2 = TMP_REG2;
			}
		}
		else {
			if (arg1 == 0)
				arg1 = TMP_ZERO;
			else {
				FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
				arg1 = TMP_REG1;
			}
		}
	}

	/* Both arguments are registers. */
	switch (op) {
	case SLJIT_MOV:
	case SLJIT_MOV_P:
	case SLJIT_MOVU:
	case SLJIT_MOVU_P:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst(compiler, ORR | RD(dst) | RN(TMP_ZERO) | RM(arg2));
	case SLJIT_MOV_UB:
	case SLJIT_MOVU_UB:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		return push_inst(compiler, (UBFM ^ (1 << 31)) | RD(dst) | RN(arg2) | (7 << 10));
	case SLJIT_MOV_SB:
	case SLJIT_MOVU_SB:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (!(flags & INT_OP))
			inv_bits |= 1 << 22;
		return push_inst(compiler, (SBFM ^ inv_bits) | RD(dst) | RN(arg2) | (7 << 10));
	case SLJIT_MOV_UH:
	case SLJIT_MOVU_UH:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		return push_inst(compiler, (UBFM ^ (1 << 31)) | RD(dst) | RN(arg2) | (15 << 10));
	case SLJIT_MOV_SH:
	case SLJIT_MOVU_SH:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (!(flags & INT_OP))
			inv_bits |= 1 << 22;
		return push_inst(compiler, (SBFM ^ inv_bits) | RD(dst) | RN(arg2) | (15 << 10));
	case SLJIT_MOV_UI:
	case SLJIT_MOVU_UI:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if ((flags & INT_OP) && dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst(compiler, (ORR ^ (1 << 31)) | RD(dst) | RN(TMP_ZERO) | RM(arg2));
	case SLJIT_MOV_SI:
	case SLJIT_MOVU_SI:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if ((flags & INT_OP) && dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst(compiler, SBFM | (1 << 22) | RD(dst) | RN(arg2) | (31 << 10));
	case SLJIT_NOT:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		FAIL_IF(push_inst(compiler, (ORN ^ inv_bits) | RD(dst) | RN(TMP_ZERO) | RM(arg2)));
		goto set_flags;
	case SLJIT_NEG:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		if (flags & SET_FLAGS)
			inv_bits |= 1 << 29;
		return push_inst(compiler, (SUB ^ inv_bits) | RD(dst) | RN(TMP_ZERO) | RM(arg2));
	case SLJIT_CLZ:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		FAIL_IF(push_inst(compiler, (CLZ ^ inv_bits) | RD(dst) | RN(arg2)));
		goto set_flags;
	case SLJIT_ADD:
		CHECK_FLAGS(1 << 29);
		return push_inst(compiler, (ADD ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_ADDC:
		CHECK_FLAGS(1 << 29);
		return push_inst(compiler, (ADC ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_SUB:
		CHECK_FLAGS(1 << 29);
		return push_inst(compiler, (SUB ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_SUBC:
		CHECK_FLAGS(1 << 29);
		return push_inst(compiler, (SBC ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_MUL:
		if (!(flags & SET_FLAGS))
			return push_inst(compiler, (MADD ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2) | RT2(TMP_ZERO));
		if (flags & INT_OP) {
			FAIL_IF(push_inst(compiler, SMADDL | RD(dst) | RN(arg1) | RM(arg2) | (31 << 10)));
			FAIL_IF(push_inst(compiler, ADD | RD(TMP_LR) | RN(TMP_ZERO) | RM(dst) | (2 << 22) | (31 << 10)));
			return push_inst(compiler, SUBS | RD(TMP_ZERO) | RN(TMP_LR) | RM(dst) | (2 << 22) | (63 << 10));
		}
		FAIL_IF(push_inst(compiler, SMULH | RD(TMP_LR) | RN(arg1) | RM(arg2)));
		FAIL_IF(push_inst(compiler, MADD | RD(dst) | RN(arg1) | RM(arg2) | RT2(TMP_ZERO)));
		return push_inst(compiler, SUBS | RD(TMP_ZERO) | RN(TMP_LR) | RM(dst) | (2 << 22) | (63 << 10));
	case SLJIT_AND:
		CHECK_FLAGS(3 << 29);
		return push_inst(compiler, (AND ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_OR:
		FAIL_IF(push_inst(compiler, (ORR ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_XOR:
		FAIL_IF(push_inst(compiler, (EOR ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_SHL:
		FAIL_IF(push_inst(compiler, (LSLV ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_LSHR:
		FAIL_IF(push_inst(compiler, (LSRV ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_ASHR:
		FAIL_IF(push_inst(compiler, (ASRV ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;

set_flags:
	if (flags & SET_FLAGS)
		return push_inst(compiler, (SUBS ^ inv_bits) | RD(TMP_ZERO) | RN(dst) | RM(TMP_ZERO));
	return SLJIT_SUCCESS;
}

#define STORE		0x01
#define SIGNED		0x02

#define UPDATE		0x04
#define ARG_TEST	0x08

#define BYTE_SIZE	0x000
#define HALF_SIZE	0x100
#define INT_SIZE	0x200
#define WORD_SIZE	0x300

#define MEM_SIZE_SHIFT(flags) ((flags) >> 8)

static SLJIT_CONST sljit_ins sljit_mem_imm[4] = {
/* u l */ 0x39400000 /* ldrb [reg,imm] */,
/* u s */ 0x39000000 /* strb [reg,imm] */,
/* s l */ 0x39800000 /* ldrsb [reg,imm] */,
/* s s */ 0x39000000 /* strb [reg,imm] */,
};

static SLJIT_CONST sljit_ins sljit_mem_simm[4] = {
/* u l */ 0x38400000 /* ldurb [reg,imm] */,
/* u s */ 0x38000000 /* sturb [reg,imm] */,
/* s l */ 0x38800000 /* ldursb [reg,imm] */,
/* s s */ 0x38000000 /* sturb [reg,imm] */,
};

static SLJIT_CONST sljit_ins sljit_mem_pre_simm[4] = {
/* u l */ 0x38400c00 /* ldrb [reg,imm]! */,
/* u s */ 0x38000c00 /* strb [reg,imm]! */,
/* s l */ 0x38800c00 /* ldrsb [reg,imm]! */,
/* s s */ 0x38000c00 /* strb [reg,imm]! */,
};

static SLJIT_CONST sljit_ins sljit_mem_reg[4] = {
/* u l */ 0x38606800 /* ldrb [reg,reg] */,
/* u s */ 0x38206800 /* strb [reg,reg] */,
/* s l */ 0x38a06800 /* ldrsb [reg,reg] */,
/* s s */ 0x38206800 /* strb [reg,reg] */,
};

/* Helper function. Dst should be reg + value, using at most 1 instruction, flags does not set. */
static sljit_si emit_set_delta(struct sljit_compiler *compiler, sljit_si dst, sljit_si reg, sljit_sw value)
{
	if (value >= 0) {
		if (value <= 0xfff)
			return push_inst(compiler, ADDI | RD(dst) | RN(reg) | (value << 10));
		if (value <= 0xffffff && !(value & 0xfff))
			return push_inst(compiler, ADDI | (1 << 22) | RD(dst) | RN(reg) | (value >> 2));
	}
	else {
		value = -value;
		if (value <= 0xfff)
			return push_inst(compiler, SUBI | RD(dst) | RN(reg) | (value << 10));
		if (value <= 0xffffff && !(value & 0xfff))
			return push_inst(compiler, SUBI | (1 << 22) | RD(dst) | RN(reg) | (value >> 2));
	}
	return SLJIT_ERR_UNSUPPORTED;
}

/* Can perform an operation using at most 1 instruction. */
static sljit_si getput_arg_fast(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg, sljit_sw argw)
{
	sljit_ui shift = MEM_SIZE_SHIFT(flags);

	SLJIT_ASSERT(arg & SLJIT_MEM);

	if (SLJIT_UNLIKELY(flags & UPDATE)) {
		if ((arg & REG_MASK) && !(arg & OFFS_REG_MASK) && argw <= 255 && argw >= -256) {
			if (SLJIT_UNLIKELY(flags & ARG_TEST))
				return 1;

			arg &= REG_MASK;
			argw &= 0x1ff;
			FAIL_IF(push_inst(compiler, sljit_mem_pre_simm[flags & 0x3]
				| (shift << 30) | RT(reg) | RN(arg) | (argw << 12)));
			return -1;
		}
		return 0;
	}

	if (SLJIT_UNLIKELY(arg & OFFS_REG_MASK)) {
		argw &= 0x3;
		if (argw && argw != shift)
			return 0;

		if (SLJIT_UNLIKELY(flags & ARG_TEST))
			return 1;

		FAIL_IF(push_inst(compiler, sljit_mem_reg[flags & 0x3] | (shift << 30) | RT(reg)
			| RN(arg & REG_MASK) | RM(OFFS_REG(arg)) | (argw ? (1 << 12) : 0)));
		return -1;
	}

	arg &= REG_MASK;
	if (argw >= 0 && (argw >> shift) <= 0xfff && (argw & ((1 << shift) - 1)) == 0) {
		if (SLJIT_UNLIKELY(flags & ARG_TEST))
			return 1;

		FAIL_IF(push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30)
			| RT(reg) | RN(arg) | (argw << (10 - shift))));
		return -1;
	}

	if (argw > 255 || argw < -256)
		return 0;

	if (SLJIT_UNLIKELY(flags & ARG_TEST))
		return 1;

	FAIL_IF(push_inst(compiler, sljit_mem_simm[flags & 0x3] | (shift << 30)
		| RT(reg) | RN(arg) | ((argw & 0x1ff) << 12)));
	return -1;
}

/* see getput_arg below.
   Note: can_cache is called only for binary operators. Those
   operators always uses word arguments without write back. */
static sljit_si can_cache(sljit_si arg, sljit_sw argw, sljit_si next_arg, sljit_sw next_argw)
{
	sljit_sw diff;
	if ((arg & OFFS_REG_MASK) || !(next_arg & SLJIT_MEM))
		return 0;

	if (!(arg & REG_MASK)) {
		diff = argw - next_argw;
		if (diff <= 0xfff && diff >= -0xfff)
			return 1;
		return 0;
	}

	if (argw == next_argw)
		return 1;

	diff = argw - next_argw;
	if (arg == next_arg && diff <= 0xfff && diff >= -0xfff)
		return 1;

	return 0;
}

/* Emit the necessary instructions. See can_cache above. */
static sljit_si getput_arg(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg,
	sljit_si arg, sljit_sw argw, sljit_si next_arg, sljit_sw next_argw)
{
	sljit_ui shift = MEM_SIZE_SHIFT(flags);
	sljit_si tmp_r, other_r;
	sljit_sw diff;

	SLJIT_ASSERT(arg & SLJIT_MEM);
	if (!(next_arg & SLJIT_MEM)) {
		next_arg = 0;
		next_argw = 0;
	}

	tmp_r = (flags & STORE) ? TMP_REG3 : reg;

	if (SLJIT_UNLIKELY((flags & UPDATE) && (arg & REG_MASK))) {
		/* Update only applies if a base register exists. */
		other_r = OFFS_REG(arg);
		if (!other_r) {
			other_r = arg & REG_MASK;
			if (other_r != reg && argw >= 0 && argw <= 0xffffff) {
				if ((argw & 0xfff) != 0)
					FAIL_IF(push_inst(compiler, ADDI | RD(other_r) | RN(other_r) | ((argw & 0xfff) << 10)));
				if (argw >> 12)
					FAIL_IF(push_inst(compiler, ADDI | (1 << 22) | RD(other_r) | RN(other_r) | ((argw >> 12) << 10)));
				return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(other_r));
			}
			else if (other_r != reg && argw < 0 && argw >= -0xffffff) {
				argw = -argw;
				if ((argw & 0xfff) != 0)
					FAIL_IF(push_inst(compiler, SUBI | RD(other_r) | RN(other_r) | ((argw & 0xfff) << 10)));
				if (argw >> 12)
					FAIL_IF(push_inst(compiler, SUBI | (1 << 22) | RD(other_r) | RN(other_r) | ((argw >> 12) << 10)));
				return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(other_r));
			}

			if (compiler->cache_arg == SLJIT_MEM) {
				if (argw == compiler->cache_argw) {
					other_r = TMP_REG3;
					argw = 0;
				}
				else if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, argw - compiler->cache_argw) != SLJIT_ERR_UNSUPPORTED) {
					FAIL_IF(compiler->error);
					compiler->cache_argw = argw;
					other_r = TMP_REG3;
					argw = 0;
				}
			}

			if (argw) {
				FAIL_IF(load_immediate(compiler, TMP_REG3, argw));
				compiler->cache_arg = SLJIT_MEM;
				compiler->cache_argw = argw;
				other_r = TMP_REG3;
				argw = 0;
			}
		}

		/* No caching here. */
		arg &= REG_MASK;
		argw &= 0x3;
		if (!argw || argw == shift) {
			FAIL_IF(push_inst(compiler, sljit_mem_reg[flags & 0x3] | (shift << 30) | RT(reg) | RN(arg) | RM(other_r) | (argw ? (1 << 12) : 0)));
			return push_inst(compiler, ADD | RD(arg) | RN(arg) | RM(other_r) | (argw << 10));
		}
		if (arg != reg) {
			FAIL_IF(push_inst(compiler, ADD | RD(arg) | RN(arg) | RM(other_r) | (argw << 10)));
			return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(arg));
		}
		FAIL_IF(push_inst(compiler, ADD | RD(TMP_LR) | RN(arg) | RM(other_r) | (argw << 10)));
		FAIL_IF(push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(TMP_LR)));
		return push_inst(compiler, ORR | RD(arg) | RN(TMP_ZERO) | RM(TMP_LR));
	}

	if (arg & OFFS_REG_MASK) {
		other_r = OFFS_REG(arg);
		arg &= REG_MASK;
		FAIL_IF(push_inst(compiler, ADD | RD(tmp_r) | RN(arg) | RM(other_r) | ((argw & 0x3) << 10)));
		return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(tmp_r));
	}

	if (compiler->cache_arg == arg) {
		diff = argw - compiler->cache_argw;
		if (diff <= 255 && diff >= -256)
			return push_inst(compiler, sljit_mem_simm[flags & 0x3] | (shift << 30)
				| RT(reg) | RN(TMP_REG3) | ((diff & 0x1ff) << 12));
		if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, diff) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(arg));
		}
	}

	if (argw >= 0 && argw <= 0xffffff && (argw & ((1 << shift) - 1)) == 0) {
		FAIL_IF(push_inst(compiler, ADDI | (1 << 22) | RD(tmp_r) | RN(arg & REG_MASK) | ((argw >> 12) << 10)));
		return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30)
			| RT(reg) | RN(tmp_r) | ((argw & 0xfff) << (10 - shift)));
	}

	diff = argw - next_argw;
	next_arg = (arg & REG_MASK) && (arg == next_arg) && diff <= 0xfff && diff >= -0xfff && diff != 0;
	arg &= REG_MASK;

	if (arg && compiler->cache_arg == SLJIT_MEM) {
		if (compiler->cache_argw == argw)
			return push_inst(compiler, sljit_mem_reg[flags & 0x3] | (shift << 30) | RT(reg) | RN(arg) | RM(TMP_REG3));
		if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, argw - compiler->cache_argw) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			compiler->cache_argw = argw;
			return push_inst(compiler, sljit_mem_reg[flags & 0x3] | (shift << 30) | RT(reg) | RN(arg) | RM(TMP_REG3));
		}
	}

	compiler->cache_argw = argw;
	if (next_arg && emit_set_delta(compiler, TMP_REG3, arg, argw) != SLJIT_ERR_UNSUPPORTED) {
		FAIL_IF(compiler->error);
		compiler->cache_arg = SLJIT_MEM | arg;
		arg = 0;
	}
	else {
		FAIL_IF(load_immediate(compiler, TMP_REG3, argw));
		compiler->cache_arg = SLJIT_MEM;

		if (next_arg) {
			FAIL_IF(push_inst(compiler, ADD | RD(TMP_REG3) | RN(TMP_REG3) | RM(arg)));
			compiler->cache_arg = SLJIT_MEM | arg;
			arg = 0;
		}
	}

	if (arg)
		return push_inst(compiler, sljit_mem_reg[flags & 0x3] | (shift << 30) | RT(reg) | RN(arg) | RM(TMP_REG3));
	return push_inst(compiler, sljit_mem_imm[flags & 0x3] | (shift << 30) | RT(reg) | RN(TMP_REG3));
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

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_enter(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	sljit_si i, tmp, offs, prev, saved_regs_size;

	CHECK_ERROR();
	CHECK(check_sljit_emit_enter(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size));
	set_emit_enter(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size);

	saved_regs_size = GET_SAVED_REGISTERS_SIZE(scratches, saveds, 0);
	local_size += saved_regs_size + SLJIT_LOCALS_OFFSET;
	local_size = (local_size + 15) & ~0xf;
	compiler->local_size = local_size;

	if (local_size <= (63 * sizeof(sljit_sw))) {
		FAIL_IF(push_inst(compiler, STP_PRE | 29 | RT2(TMP_LR)
			| RN(TMP_SP) | ((-(local_size >> 3) & 0x7f) << 15)));
		FAIL_IF(push_inst(compiler, ADDI | RD(SLJIT_SP) | RN(TMP_SP) | (0 << 10)));
		offs = (local_size - saved_regs_size) << (15 - 3);
	} else {
		offs = 0 << 15;
		if (saved_regs_size & 0x8) {
			offs = 1 << 15;
			saved_regs_size += sizeof(sljit_sw);
		}
		local_size -= saved_regs_size + SLJIT_LOCALS_OFFSET;
		if (saved_regs_size > 0)
			FAIL_IF(push_inst(compiler, SUBI | RD(TMP_SP) | RN(TMP_SP) | (saved_regs_size << 10)));
	}

	tmp = saveds < SLJIT_NUMBER_OF_SAVED_REGISTERS ? (SLJIT_S0 + 1 - saveds) : SLJIT_FIRST_SAVED_REG;
	prev = -1;
	for (i = SLJIT_S0; i >= tmp; i--) {
		if (prev == -1) {
			if (!(offs & (1 << 15))) {
				prev = i;
				continue;
			}
			FAIL_IF(push_inst(compiler, STRI | RT(i) | RN(TMP_SP) | (offs >> 5)));
			offs += 1 << 15;
			continue;
		}
		FAIL_IF(push_inst(compiler, STP | RT(prev) | RT2(i) | RN(TMP_SP) | offs));
		offs += 2 << 15;
		prev = -1;
	}

	for (i = scratches; i >= SLJIT_FIRST_SAVED_REG; i--) {
		if (prev == -1) {
			if (!(offs & (1 << 15))) {
				prev = i;
				continue;
			}
			FAIL_IF(push_inst(compiler, STRI | RT(i) | RN(TMP_SP) | (offs >> 5)));
			offs += 1 << 15;
			continue;
		}
		FAIL_IF(push_inst(compiler, STP | RT(prev) | RT2(i) | RN(TMP_SP) | offs));
		offs += 2 << 15;
		prev = -1;
	}

	SLJIT_ASSERT(prev == -1);

	if (compiler->local_size > (63 * sizeof(sljit_sw))) {
		/* The local_size is already adjusted by the saved registers. */
		if (local_size > 0xfff) {
			FAIL_IF(push_inst(compiler, SUBI | RD(TMP_SP) | RN(TMP_SP) | ((local_size >> 12) << 10) | (1 << 22)));
			local_size &= 0xfff;
		}
		if (local_size)
			FAIL_IF(push_inst(compiler, SUBI | RD(TMP_SP) | RN(TMP_SP) | (local_size << 10)));
		FAIL_IF(push_inst(compiler, STP_PRE | 29 | RT2(TMP_LR)
			| RN(TMP_SP) | ((-(16 >> 3) & 0x7f) << 15)));
		FAIL_IF(push_inst(compiler, ADDI | RD(SLJIT_SP) | RN(TMP_SP) | (0 << 10)));
	}

	if (args >= 1)
		FAIL_IF(push_inst(compiler, ORR | RD(SLJIT_S0) | RN(TMP_ZERO) | RM(SLJIT_R0)));
	if (args >= 2)
		FAIL_IF(push_inst(compiler, ORR | RD(SLJIT_S1) | RN(TMP_ZERO) | RM(SLJIT_R1)));
	if (args >= 3)
		FAIL_IF(push_inst(compiler, ORR | RD(SLJIT_S2) | RN(TMP_ZERO) | RM(SLJIT_R2)));

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_set_context(struct sljit_compiler *compiler,
	sljit_si options, sljit_si args, sljit_si scratches, sljit_si saveds,
	sljit_si fscratches, sljit_si fsaveds, sljit_si local_size)
{
	CHECK_ERROR();
	CHECK(check_sljit_set_context(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size));
	set_set_context(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size);

	local_size += GET_SAVED_REGISTERS_SIZE(scratches, saveds, 0) + SLJIT_LOCALS_OFFSET;
	local_size = (local_size + 15) & ~0xf;
	compiler->local_size = local_size;
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
	sljit_si local_size;
	sljit_si i, tmp, offs, prev, saved_regs_size;

	CHECK_ERROR();
	CHECK(check_sljit_emit_return(compiler, op, src, srcw));

	FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));

	local_size = compiler->local_size;

	saved_regs_size = GET_SAVED_REGISTERS_SIZE(compiler->scratches, compiler->saveds, 0);
	if (local_size <= (63 * sizeof(sljit_sw)))
		offs = (local_size - saved_regs_size) << (15 - 3);
	else {
		FAIL_IF(push_inst(compiler, LDP_PST | 29 | RT2(TMP_LR)
			| RN(TMP_SP) | (((16 >> 3) & 0x7f) << 15)));
		offs = 0 << 15;
		if (saved_regs_size & 0x8) {
			offs = 1 << 15;
			saved_regs_size += sizeof(sljit_sw);
		}
		local_size -= saved_regs_size + SLJIT_LOCALS_OFFSET;
		if (local_size > 0xfff) {
			FAIL_IF(push_inst(compiler, ADDI | RD(TMP_SP) | RN(TMP_SP) | ((local_size >> 12) << 10) | (1 << 22)));
			local_size &= 0xfff;
		}
		if (local_size)
			FAIL_IF(push_inst(compiler, ADDI | RD(TMP_SP) | RN(TMP_SP) | (local_size << 10)));
	}

	tmp = compiler->saveds < SLJIT_NUMBER_OF_SAVED_REGISTERS ? (SLJIT_S0 + 1 - compiler->saveds) : SLJIT_FIRST_SAVED_REG;
	prev = -1;
	for (i = SLJIT_S0; i >= tmp; i--) {
		if (prev == -1) {
			if (!(offs & (1 << 15))) {
				prev = i;
				continue;
			}
			FAIL_IF(push_inst(compiler, LDRI | RT(i) | RN(TMP_SP) | (offs >> 5)));
			offs += 1 << 15;
			continue;
		}
		FAIL_IF(push_inst(compiler, LDP | RT(prev) | RT2(i) | RN(TMP_SP) | offs));
		offs += 2 << 15;
		prev = -1;
	}

	for (i = compiler->scratches; i >= SLJIT_FIRST_SAVED_REG; i--) {
		if (prev == -1) {
			if (!(offs & (1 << 15))) {
				prev = i;
				continue;
			}
			FAIL_IF(push_inst(compiler, LDRI | RT(i) | RN(TMP_SP) | (offs >> 5)));
			offs += 1 << 15;
			continue;
		}
		FAIL_IF(push_inst(compiler, LDP | RT(prev) | RT2(i) | RN(TMP_SP) | offs));
		offs += 2 << 15;
		prev = -1;
	}

	SLJIT_ASSERT(prev == -1);

	if (compiler->local_size <= (63 * sizeof(sljit_sw))) {
		FAIL_IF(push_inst(compiler, LDP_PST | 29 | RT2(TMP_LR)
			| RN(TMP_SP) | (((local_size >> 3) & 0x7f) << 15)));
	} else if (saved_regs_size > 0) {
		FAIL_IF(push_inst(compiler, ADDI | RD(TMP_SP) | RN(TMP_SP) | (saved_regs_size << 10)));
	}

	FAIL_IF(push_inst(compiler, RET | RN(TMP_LR)));
	return SLJIT_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op0(struct sljit_compiler *compiler, sljit_si op)
{
	sljit_ins inv_bits = (op & SLJIT_INT_OP) ? (1 << 31) : 0;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op0(compiler, op));

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
		return push_inst(compiler, BRK);
	case SLJIT_NOP:
		return push_inst(compiler, NOP);
	case SLJIT_LUMUL:
	case SLJIT_LSMUL:
		FAIL_IF(push_inst(compiler, ORR | RD(TMP_REG1) | RN(TMP_ZERO) | RM(SLJIT_R0)));
		FAIL_IF(push_inst(compiler, MADD | RD(SLJIT_R0) | RN(SLJIT_R0) | RM(SLJIT_R1) | RT2(TMP_ZERO)));
		return push_inst(compiler, (op == SLJIT_LUMUL ? UMULH : SMULH) | RD(SLJIT_R1) | RN(TMP_REG1) | RM(SLJIT_R1));
	case SLJIT_UDIVMOD:
	case SLJIT_SDIVMOD:
		FAIL_IF(push_inst(compiler, (ORR ^ inv_bits) | RD(TMP_REG1) | RN(TMP_ZERO) | RM(SLJIT_R0)));
		FAIL_IF(push_inst(compiler, ((op == SLJIT_UDIVMOD ? UDIV : SDIV) ^ inv_bits) | RD(SLJIT_R0) | RN(SLJIT_R0) | RM(SLJIT_R1)));
		FAIL_IF(push_inst(compiler, (MADD ^ inv_bits) | RD(SLJIT_R1) | RN(SLJIT_R0) | RM(SLJIT_R1) | RT2(TMP_ZERO)));
		return push_inst(compiler, (SUB ^ inv_bits) | RD(SLJIT_R1) | RN(TMP_REG1) | RM(SLJIT_R1));
	case SLJIT_UDIVI:
	case SLJIT_SDIVI:
		return push_inst(compiler, ((op == SLJIT_UDIVI ? UDIV : SDIV) ^ inv_bits) | RD(SLJIT_R0) | RN(SLJIT_R0) | RM(SLJIT_R1));
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r, flags, mem_flags;
	sljit_si op_flags = GET_ALL_FLAGS(op);

	CHECK_ERROR();
	CHECK(check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = SLOW_IS_REG(dst) ? dst : TMP_REG1;

	op = GET_OPCODE(op);
	if (op >= SLJIT_MOV && op <= SLJIT_MOVU_P) {
		switch (op) {
		case SLJIT_MOV:
		case SLJIT_MOV_P:
			flags = WORD_SIZE;
			break;
		case SLJIT_MOV_UB:
			flags = BYTE_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ub)srcw;
			break;
		case SLJIT_MOV_SB:
			flags = BYTE_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_sb)srcw;
			break;
		case SLJIT_MOV_UH:
			flags = HALF_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_uh)srcw;
			break;
		case SLJIT_MOV_SH:
			flags = HALF_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_sh)srcw;
			break;
		case SLJIT_MOV_UI:
			flags = INT_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ui)srcw;
			break;
		case SLJIT_MOV_SI:
			flags = INT_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_si)srcw;
			break;
		case SLJIT_MOVU:
		case SLJIT_MOVU_P:
			flags = WORD_SIZE | UPDATE;
			break;
		case SLJIT_MOVU_UB:
			flags = BYTE_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ub)srcw;
			break;
		case SLJIT_MOVU_SB:
			flags = BYTE_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_sb)srcw;
			break;
		case SLJIT_MOVU_UH:
			flags = HALF_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_uh)srcw;
			break;
		case SLJIT_MOVU_SH:
			flags = HALF_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_sh)srcw;
			break;
		case SLJIT_MOVU_UI:
			flags = INT_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ui)srcw;
			break;
		case SLJIT_MOVU_SI:
			flags = INT_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_si)srcw;
			break;
		default:
			SLJIT_ASSERT_STOP();
			flags = 0;
			break;
		}

		if (src & SLJIT_IMM)
			FAIL_IF(emit_op_imm(compiler, SLJIT_MOV | ARG2_IMM, dst_r, TMP_REG1, srcw));
		else if (src & SLJIT_MEM) {
			if (getput_arg_fast(compiler, flags, dst_r, src, srcw))
				FAIL_IF(compiler->error);
			else
				FAIL_IF(getput_arg(compiler, flags, dst_r, src, srcw, dst, dstw));
		} else {
			if (dst_r != TMP_REG1)
				return emit_op_imm(compiler, op | ((op_flags & SLJIT_INT_OP) ? INT_OP : 0), dst_r, TMP_REG1, src);
			dst_r = src;
		}

		if (dst & SLJIT_MEM) {
			if (getput_arg_fast(compiler, flags | STORE, dst_r, dst, dstw))
				return compiler->error;
			else
				return getput_arg(compiler, flags | STORE, dst_r, dst, dstw, 0, 0);
		}
		return SLJIT_SUCCESS;
	}

	flags = GET_FLAGS(op_flags) ? SET_FLAGS : 0;
	mem_flags = WORD_SIZE;
	if (op_flags & SLJIT_INT_OP) {
		flags |= INT_OP;
		mem_flags = INT_SIZE;
	}

	if (dst == SLJIT_UNUSED)
		flags |= UNUSED_RETURN;

	if (src & SLJIT_MEM) {
		if (getput_arg_fast(compiler, mem_flags, TMP_REG2, src, srcw))
			FAIL_IF(compiler->error);
		else
			FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG2, src, srcw, dst, dstw));
		src = TMP_REG2;
	}

	if (src & SLJIT_IMM) {
		flags |= ARG2_IMM;
		if (op_flags & SLJIT_INT_OP)
			srcw = (sljit_si)srcw;
	} else
		srcw = src;

	emit_op_imm(compiler, flags | op, dst_r, TMP_REG1, srcw);

	if (dst & SLJIT_MEM) {
		if (getput_arg_fast(compiler, mem_flags | STORE, dst_r, dst, dstw))
			return compiler->error;
		else
			return getput_arg(compiler, mem_flags | STORE, dst_r, dst, dstw, 0, 0);
	}
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si dst_r, flags, mem_flags;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = SLOW_IS_REG(dst) ? dst : TMP_REG1;
	flags = GET_FLAGS(op) ? SET_FLAGS : 0;
	mem_flags = WORD_SIZE;
	if (op & SLJIT_INT_OP) {
		flags |= INT_OP;
		mem_flags = INT_SIZE;
	}

	if (dst == SLJIT_UNUSED)
		flags |= UNUSED_RETURN;

	if ((dst & SLJIT_MEM) && !getput_arg_fast(compiler, mem_flags | STORE | ARG_TEST, TMP_REG1, dst, dstw))
		flags |= SLOW_DEST;

	if (src1 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, mem_flags, TMP_REG1, src1, src1w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC1;
	}
	if (src2 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, mem_flags, TMP_REG2, src2, src2w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC2;
	}

	if ((flags & (SLOW_SRC1 | SLOW_SRC2)) == (SLOW_SRC1 | SLOW_SRC2)) {
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & SLOW_SRC1)
		FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG1, src1, src1w, dst, dstw));
	else if (flags & SLOW_SRC2)
		FAIL_IF(getput_arg(compiler, mem_flags, TMP_REG2, src2, src2w, dst, dstw));

	if (src1 & SLJIT_MEM)
		src1 = TMP_REG1;
	if (src2 & SLJIT_MEM)
		src2 = TMP_REG2;

	if (src1 & SLJIT_IMM)
		flags |= ARG1_IMM;
	else
		src1w = src1;
	if (src2 & SLJIT_IMM)
		flags |= ARG2_IMM;
	else
		src2w = src2;

	emit_op_imm(compiler, flags | GET_OPCODE(op), dst_r, src1w, src2w);

	if (dst & SLJIT_MEM) {
		if (!(flags & SLOW_DEST)) {
			getput_arg_fast(compiler, mem_flags | STORE, dst_r, dst, dstw);
			return compiler->error;
		}
		return getput_arg(compiler, mem_flags | STORE, TMP_REG1, dst, dstw, 0, 0);
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
	return reg;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_si size)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_custom(compiler, instruction, size));

	return push_inst(compiler, *(sljit_ins*)instruction);
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

static sljit_si emit_fop_mem(struct sljit_compiler *compiler, sljit_si flags, sljit_si reg, sljit_si arg, sljit_sw argw)
{
	sljit_ui shift = MEM_SIZE_SHIFT(flags);
	sljit_ins ins_bits = (shift << 30);
	sljit_si other_r;
	sljit_sw diff;

	SLJIT_ASSERT(arg & SLJIT_MEM);

	if (!(flags & STORE))
		ins_bits |= 1 << 22;

	if (arg & OFFS_REG_MASK) {
		argw &= 3;
		if (!argw || argw == shift)
			return push_inst(compiler, STR_FR | ins_bits | VT(reg)
				| RN(arg & REG_MASK) | RM(OFFS_REG(arg)) | (argw ? (1 << 12) : 0));
		other_r = OFFS_REG(arg);
		arg &= REG_MASK;
		FAIL_IF(push_inst(compiler, ADD | RD(TMP_REG1) | RN(arg) | RM(other_r) | (argw << 10)));
		arg = TMP_REG1;
		argw = 0;
	}

	arg &= REG_MASK;
	if (arg && argw >= 0 && ((argw >> shift) <= 0xfff) && (argw & ((1 << shift) - 1)) == 0)
		return push_inst(compiler, STR_FI | ins_bits | VT(reg) | RN(arg) | (argw << (10 - shift)));

	if (arg && argw <= 255 && argw >= -256)
		return push_inst(compiler, STUR_FI | ins_bits | VT(reg) | RN(arg) | ((argw & 0x1ff) << 12));

	/* Slow cases */
	if (compiler->cache_arg == SLJIT_MEM && argw != compiler->cache_argw) {
		diff = argw - compiler->cache_argw;
		if (!arg && diff <= 255 && diff >= -256)
			return push_inst(compiler, STUR_FI | ins_bits | VT(reg) | RN(TMP_REG3) | ((diff & 0x1ff) << 12));
		if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, argw - compiler->cache_argw) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			compiler->cache_argw = argw;
		}
	}

	if (compiler->cache_arg != SLJIT_MEM || argw != compiler->cache_argw) {
		compiler->cache_arg = SLJIT_MEM;
		compiler->cache_argw = argw;
		FAIL_IF(load_immediate(compiler, TMP_REG3, argw));
	}

	if (arg & REG_MASK)
		return push_inst(compiler, STR_FR | ins_bits | VT(reg) | RN(arg) | RM(TMP_REG3));
	return push_inst(compiler, STR_FI | ins_bits | VT(reg) | RN(TMP_REG3));
}

static SLJIT_INLINE sljit_si sljit_emit_fop1_convw_fromd(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r = SLOW_IS_REG(dst) ? dst : TMP_REG1;
	sljit_ins inv_bits = (op & SLJIT_SINGLE_OP) ? (1 << 22) : 0;

	if (GET_OPCODE(op) == SLJIT_CONVI_FROMD)
		inv_bits |= (1 << 31);

	if (src & SLJIT_MEM) {
		emit_fop_mem(compiler, (op & SLJIT_SINGLE_OP) ? INT_SIZE : WORD_SIZE, TMP_FREG1, src, srcw);
		src = TMP_FREG1;
	}

	FAIL_IF(push_inst(compiler, (FCVTZS ^ inv_bits) | RD(dst_r) | VN(src)));

	if (dst_r == TMP_REG1 && dst != SLJIT_UNUSED)
		return emit_op_mem(compiler, ((GET_OPCODE(op) == SLJIT_CONVI_FROMD) ? INT_SIZE : WORD_SIZE) | STORE, TMP_REG1, dst, dstw);
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_si sljit_emit_fop1_convd_fromw(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r = FAST_IS_REG(dst) ? dst : TMP_FREG1;
	sljit_ins inv_bits = (op & SLJIT_SINGLE_OP) ? (1 << 22) : 0;

	if (GET_OPCODE(op) == SLJIT_CONVD_FROMI)
		inv_bits |= (1 << 31);

	if (src & SLJIT_MEM) {
		emit_op_mem(compiler, ((GET_OPCODE(op) == SLJIT_CONVD_FROMI) ? INT_SIZE : WORD_SIZE), TMP_REG1, src, srcw);
		src = TMP_REG1;
	} else if (src & SLJIT_IMM) {
#if (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
		if (GET_OPCODE(op) == SLJIT_CONVD_FROMI)
			srcw = (sljit_si)srcw;
#endif
		FAIL_IF(load_immediate(compiler, TMP_REG1, srcw));
		src = TMP_REG1;
	}

	FAIL_IF(push_inst(compiler, (SCVTF ^ inv_bits) | VD(dst_r) | RN(src)));

	if (dst & SLJIT_MEM)
		return emit_fop_mem(compiler, ((op & SLJIT_SINGLE_OP) ? INT_SIZE : WORD_SIZE) | STORE, TMP_FREG1, dst, dstw);
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_si sljit_emit_fop1_cmp(struct sljit_compiler *compiler, sljit_si op,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si mem_flags = (op & SLJIT_SINGLE_OP) ? INT_SIZE : WORD_SIZE;
	sljit_ins inv_bits = (op & SLJIT_SINGLE_OP) ? (1 << 22) : 0;

	if (src1 & SLJIT_MEM) {
		emit_fop_mem(compiler, mem_flags, TMP_FREG1, src1, src1w);
		src1 = TMP_FREG1;
	}

	if (src2 & SLJIT_MEM) {
		emit_fop_mem(compiler, mem_flags, TMP_FREG2, src2, src2w);
		src2 = TMP_FREG2;
	}

	return push_inst(compiler, (FCMP ^ inv_bits) | VN(src1) | VM(src2));
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r, mem_flags = (op & SLJIT_SINGLE_OP) ? INT_SIZE : WORD_SIZE;
	sljit_ins inv_bits;

	CHECK_ERROR();
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	SLJIT_COMPILE_ASSERT((INT_SIZE ^ 0x100) == WORD_SIZE, must_be_one_bit_difference);
	SELECT_FOP1_OPERATION_WITH_CHECKS(compiler, op, dst, dstw, src, srcw);

	inv_bits = (op & SLJIT_SINGLE_OP) ? (1 << 22) : 0;
	dst_r = FAST_IS_REG(dst) ? dst : TMP_FREG1;

	if (src & SLJIT_MEM) {
		emit_fop_mem(compiler, (GET_OPCODE(op) == SLJIT_CONVD_FROMS) ? (mem_flags ^ 0x100) : mem_flags, dst_r, src, srcw);
		src = dst_r;
	}

	switch (GET_OPCODE(op)) {
	case SLJIT_DMOV:
		if (src != dst_r) {
			if (dst_r != TMP_FREG1)
				FAIL_IF(push_inst(compiler, (FMOV ^ inv_bits) | VD(dst_r) | VN(src)));
			else
				dst_r = src;
		}
		break;
	case SLJIT_DNEG:
		FAIL_IF(push_inst(compiler, (FNEG ^ inv_bits) | VD(dst_r) | VN(src)));
		break;
	case SLJIT_DABS:
		FAIL_IF(push_inst(compiler, (FABS ^ inv_bits) | VD(dst_r) | VN(src)));
		break;
	case SLJIT_CONVD_FROMS:
		FAIL_IF(push_inst(compiler, FCVT | ((op & SLJIT_SINGLE_OP) ? (1 << 22) : (1 << 15)) | VD(dst_r) | VN(src)));
		break;
	}

	if (dst & SLJIT_MEM)
		return emit_fop_mem(compiler, mem_flags | STORE, dst_r, dst, dstw);
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si dst_r, mem_flags = (op & SLJIT_SINGLE_OP) ? INT_SIZE : WORD_SIZE;
	sljit_ins inv_bits = (op & SLJIT_SINGLE_OP) ? (1 << 22) : 0;

	CHECK_ERROR();
	CHECK(check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = FAST_IS_REG(dst) ? dst : TMP_FREG1;
	if (src1 & SLJIT_MEM) {
		emit_fop_mem(compiler, mem_flags, TMP_FREG1, src1, src1w);
		src1 = TMP_FREG1;
	}
	if (src2 & SLJIT_MEM) {
		emit_fop_mem(compiler, mem_flags, TMP_FREG2, src2, src2w);
		src2 = TMP_FREG2;
	}

	switch (GET_OPCODE(op)) {
	case SLJIT_DADD:
		FAIL_IF(push_inst(compiler, (FADD ^ inv_bits) | VD(dst_r) | VN(src1) | VM(src2)));
		break;
	case SLJIT_DSUB:
		FAIL_IF(push_inst(compiler, (FSUB ^ inv_bits) | VD(dst_r) | VN(src1) | VM(src2)));
		break;
	case SLJIT_DMUL:
		FAIL_IF(push_inst(compiler, (FMUL ^ inv_bits) | VD(dst_r) | VN(src1) | VM(src2)));
		break;
	case SLJIT_DDIV:
		FAIL_IF(push_inst(compiler, (FDIV ^ inv_bits) | VD(dst_r) | VN(src1) | VM(src2)));
		break;
	}

	if (!(dst & SLJIT_MEM))
		return SLJIT_SUCCESS;
	return emit_fop_mem(compiler, mem_flags | STORE, TMP_FREG1, dst, dstw);
}

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
		return push_inst(compiler, ORR | RD(dst) | RN(TMP_ZERO) | RM(TMP_LR));

	/* Memory. */
	return emit_op_mem(compiler, WORD_SIZE | STORE, TMP_LR, dst, dstw);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_return(compiler, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (FAST_IS_REG(src))
		FAIL_IF(push_inst(compiler, ORR | RD(TMP_LR) | RN(TMP_ZERO) | RM(src)));
	else if (src & SLJIT_MEM)
		FAIL_IF(emit_op_mem(compiler, WORD_SIZE, TMP_LR, src, srcw));
	else if (src & SLJIT_IMM)
		FAIL_IF(load_immediate(compiler, TMP_LR, srcw));

	return push_inst(compiler, RET | RN(TMP_LR));
}

/* --------------------------------------------------------------------- */
/*  Conditional instructions                                             */
/* --------------------------------------------------------------------- */

static sljit_uw get_cc(sljit_si type)
{
	switch (type) {
	case SLJIT_EQUAL:
	case SLJIT_MUL_NOT_OVERFLOW:
	case SLJIT_D_EQUAL:
		return 0x1;

	case SLJIT_NOT_EQUAL:
	case SLJIT_MUL_OVERFLOW:
	case SLJIT_D_NOT_EQUAL:
		return 0x0;

	case SLJIT_LESS:
	case SLJIT_D_LESS:
		return 0x2;

	case SLJIT_GREATER_EQUAL:
	case SLJIT_D_GREATER_EQUAL:
		return 0x3;

	case SLJIT_GREATER:
	case SLJIT_D_GREATER:
		return 0x9;

	case SLJIT_LESS_EQUAL:
	case SLJIT_D_LESS_EQUAL:
		return 0x8;

	case SLJIT_SIG_LESS:
		return 0xa;

	case SLJIT_SIG_GREATER_EQUAL:
		return 0xb;

	case SLJIT_SIG_GREATER:
		return 0xd;

	case SLJIT_SIG_LESS_EQUAL:
		return 0xc;

	case SLJIT_OVERFLOW:
	case SLJIT_D_UNORDERED:
		return 0x7;

	case SLJIT_NOT_OVERFLOW:
	case SLJIT_D_ORDERED:
		return 0x6;

	default:
		SLJIT_ASSERT_STOP();
		return 0xe;
	}
}

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
	return label;
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

	if (type < SLJIT_JUMP) {
		jump->flags |= IS_COND;
		PTR_FAIL_IF(push_inst(compiler, B_CC | (6 << 5) | get_cc(type)));
	}
	else if (type >= SLJIT_FAST_CALL)
		jump->flags |= IS_BL;

	PTR_FAIL_IF(emit_imm64_const(compiler, TMP_REG1, 0));
	jump->addr = compiler->size;
	PTR_FAIL_IF(push_inst(compiler, ((type >= SLJIT_FAST_CALL) ? BLR : BR) | RN(TMP_REG1)));

	return jump;
}

static SLJIT_INLINE struct sljit_jump* emit_cmp_to0(struct sljit_compiler *compiler, sljit_si type,
	sljit_si src, sljit_sw srcw)
{
	struct sljit_jump *jump;
	sljit_ins inv_bits = (type & SLJIT_INT_OP) ? (1 << 31) : 0;

	SLJIT_ASSERT((type & 0xff) == SLJIT_EQUAL || (type & 0xff) == SLJIT_NOT_EQUAL);
	ADJUST_LOCAL_OFFSET(src, srcw);

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);
	jump->flags |= IS_CBZ | IS_COND;

	if (src & SLJIT_MEM) {
		PTR_FAIL_IF(emit_op_mem(compiler, inv_bits ? INT_SIZE : WORD_SIZE, TMP_REG1, src, srcw));
		src = TMP_REG1;
	}
	else if (src & SLJIT_IMM) {
		PTR_FAIL_IF(load_immediate(compiler, TMP_REG1, srcw));
		src = TMP_REG1;
	}
	SLJIT_ASSERT(FAST_IS_REG(src));

	if ((type & 0xff) == SLJIT_EQUAL)
		inv_bits |= 1 << 24;

	PTR_FAIL_IF(push_inst(compiler, (CBZ ^ inv_bits) | (6 << 5) | RT(src)));
	PTR_FAIL_IF(emit_imm64_const(compiler, TMP_REG1, 0));
	jump->addr = compiler->size;
	PTR_FAIL_IF(push_inst(compiler, BR | RN(TMP_REG1)));
	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_ijump(struct sljit_compiler *compiler, sljit_si type, sljit_si src, sljit_sw srcw)
{
	struct sljit_jump *jump;

	CHECK_ERROR();
	CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	/* In ARM, we don't need to touch the arguments. */
	if (!(src & SLJIT_IMM)) {
		if (src & SLJIT_MEM) {
			FAIL_IF(emit_op_mem(compiler, WORD_SIZE, TMP_REG1, src, srcw));
			src = TMP_REG1;
		}
		return push_inst(compiler, ((type >= SLJIT_FAST_CALL) ? BLR : BR) | RN(src));
	}

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	FAIL_IF(!jump);
	set_jump(jump, compiler, JUMP_ADDR | ((type >= SLJIT_FAST_CALL) ? IS_BL : 0));
	jump->u.target = srcw;

	FAIL_IF(emit_imm64_const(compiler, TMP_REG1, 0));
	jump->addr = compiler->size;
	return push_inst(compiler, ((type >= SLJIT_FAST_CALL) ? BLR : BR) | RN(TMP_REG1));
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw,
	sljit_si type)
{
	sljit_si dst_r, flags, mem_flags;
	sljit_ins cc;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op_flags(compiler, op, dst, dstw, src, srcw, type));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	cc = get_cc(type & 0xff);
	dst_r = FAST_IS_REG(dst) ? dst : TMP_REG1;

	if (GET_OPCODE(op) < SLJIT_ADD) {
		FAIL_IF(push_inst(compiler, CSINC | (cc << 12) | RD(dst_r) | RN(TMP_ZERO) | RM(TMP_ZERO)));
		if (dst_r != TMP_REG1)
			return SLJIT_SUCCESS;
		return emit_op_mem(compiler, (GET_OPCODE(op) == SLJIT_MOV ? WORD_SIZE : INT_SIZE) | STORE, TMP_REG1, dst, dstw);
	}

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	flags = GET_FLAGS(op) ? SET_FLAGS : 0;
	mem_flags = WORD_SIZE;
	if (op & SLJIT_INT_OP) {
		flags |= INT_OP;
		mem_flags = INT_SIZE;
	}

	if (src & SLJIT_MEM) {
		FAIL_IF(emit_op_mem2(compiler, mem_flags, TMP_REG1, src, srcw, dst, dstw));
		src = TMP_REG1;
		srcw = 0;
	} else if (src & SLJIT_IMM)
		flags |= ARG1_IMM;

	FAIL_IF(push_inst(compiler, CSINC | (cc << 12) | RD(TMP_REG2) | RN(TMP_ZERO) | RM(TMP_ZERO)));
	emit_op_imm(compiler, flags | GET_OPCODE(op), dst_r, src, TMP_REG2);

	if (dst_r != TMP_REG1)
		return SLJIT_SUCCESS;
	return emit_op_mem2(compiler, mem_flags | STORE, TMP_REG1, dst, dstw, 0, 0);
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw init_value)
{
	struct sljit_const *const_;
	sljit_si dst_r;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_const(compiler, dst, dstw, init_value));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);
	set_const(const_, compiler);

	dst_r = SLOW_IS_REG(dst) ? dst : TMP_REG1;
	PTR_FAIL_IF(emit_imm64_const(compiler, dst_r, init_value));

	if (dst & SLJIT_MEM)
		PTR_FAIL_IF(emit_op_mem(compiler, WORD_SIZE | STORE, dst_r, dst, dstw));
	return const_;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_ins* inst = (sljit_ins*)addr;
	modify_imm64_const(inst, new_addr);
	SLJIT_CACHE_FLUSH(inst, inst + 4);
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_const(sljit_uw addr, sljit_sw new_constant)
{
	sljit_ins* inst = (sljit_ins*)addr;
	modify_imm64_const(inst, new_constant);
	SLJIT_CACHE_FLUSH(inst, inst + 4);
}
