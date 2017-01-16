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

SLJIT_API_FUNC_ATTRIBUTE const char* sljit_get_platform_name(void)
{
	return "ARM-Thumb2" SLJIT_CPUINFO;
}

/* Length of an instruction word. */
typedef sljit_u32 sljit_ins;

/* Last register + 1. */
#define TMP_REG1	(SLJIT_NUMBER_OF_REGISTERS + 2)
#define TMP_REG2	(SLJIT_NUMBER_OF_REGISTERS + 3)
#define TMP_REG3	(SLJIT_NUMBER_OF_REGISTERS + 4)
#define TMP_PC		(SLJIT_NUMBER_OF_REGISTERS + 5)

#define TMP_FREG1	(0)
#define TMP_FREG2	(SLJIT_NUMBER_OF_FLOAT_REGISTERS + 1)

/* See sljit_emit_enter and sljit_emit_op0 if you want to change them. */
static const sljit_u8 reg_map[SLJIT_NUMBER_OF_REGISTERS + 6] = {
	0, 0, 1, 2, 12, 11, 10, 9, 8, 7, 6, 5, 13, 3, 4, 14, 15
};

#define COPY_BITS(src, from, to, bits) \
	((from >= to ? (src >> (from - to)) : (src << (to - from))) & (((1 << bits) - 1) << to))

/* Thumb16 encodings. */
#define RD3(rd) (reg_map[rd])
#define RN3(rn) (reg_map[rn] << 3)
#define RM3(rm) (reg_map[rm] << 6)
#define RDN3(rdn) (reg_map[rdn] << 8)
#define IMM3(imm) (imm << 6)
#define IMM8(imm) (imm)

/* Thumb16 helpers. */
#define SET_REGS44(rd, rn) \
	((reg_map[rn] << 3) | (reg_map[rd] & 0x7) | ((reg_map[rd] & 0x8) << 4))
#define IS_2_LO_REGS(reg1, reg2) \
	(reg_map[reg1] <= 7 && reg_map[reg2] <= 7)
#define IS_3_LO_REGS(reg1, reg2, reg3) \
	(reg_map[reg1] <= 7 && reg_map[reg2] <= 7 && reg_map[reg3] <= 7)

/* Thumb32 encodings. */
#define RD4(rd) (reg_map[rd] << 8)
#define RN4(rn) (reg_map[rn] << 16)
#define RM4(rm) (reg_map[rm])
#define RT4(rt) (reg_map[rt] << 12)
#define DD4(dd) ((dd) << 12)
#define DN4(dn) ((dn) << 16)
#define DM4(dm) (dm)
#define IMM5(imm) \
	(COPY_BITS(imm, 2, 12, 3) | ((imm & 0x3) << 6))
#define IMM12(imm) \
	(COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff))

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */

/* dot '.' changed to _
   I immediate form (possibly followed by number of immediate bits). */
#define ADCI		0xf1400000
#define ADCS		0x4140
#define ADC_W		0xeb400000
#define ADD		0x4400
#define ADDS		0x1800
#define ADDSI3		0x1c00
#define ADDSI8		0x3000
#define ADD_W		0xeb000000
#define ADDWI		0xf2000000
#define ADD_SP		0xb000
#define ADD_W		0xeb000000
#define ADD_WI		0xf1000000
#define ANDI		0xf0000000
#define ANDS		0x4000
#define AND_W		0xea000000
#define ASRS		0x4100
#define ASRSI		0x1000
#define ASR_W		0xfa40f000
#define ASR_WI		0xea4f0020
#define BICI		0xf0200000
#define BKPT		0xbe00
#define BLX		0x4780
#define BX		0x4700
#define CLZ		0xfab0f080
#define CMPI		0x2800
#define CMP_W		0xebb00f00
#define EORI		0xf0800000
#define EORS		0x4040
#define EOR_W		0xea800000
#define IT		0xbf00
#define LSLS		0x4080
#define LSLSI		0x0000
#define LSL_W		0xfa00f000
#define LSL_WI		0xea4f0000
#define LSRS		0x40c0
#define LSRSI		0x0800
#define LSR_W		0xfa20f000
#define LSR_WI		0xea4f0010
#define MOV		0x4600
#define MOVS		0x0000
#define MOVSI		0x2000
#define MOVT		0xf2c00000
#define MOVW		0xf2400000
#define MOV_W		0xea4f0000
#define MOV_WI		0xf04f0000
#define MUL		0xfb00f000
#define MVNS		0x43c0
#define MVN_W		0xea6f0000
#define MVN_WI		0xf06f0000
#define NOP		0xbf00
#define ORNI		0xf0600000
#define ORRI		0xf0400000
#define ORRS		0x4300
#define ORR_W		0xea400000
#define POP		0xbc00
#define POP_W		0xe8bd0000
#define PUSH		0xb400
#define PUSH_W		0xe92d0000
#define RSB_WI		0xf1c00000
#define RSBSI		0x4240
#define SBCI		0xf1600000
#define SBCS		0x4180
#define SBC_W		0xeb600000
#define SMULL		0xfb800000
#define STR_SP		0x9000
#define SUBS		0x1a00
#define SUBSI3		0x1e00
#define SUBSI8		0x3800
#define SUB_W		0xeba00000
#define SUBWI		0xf2a00000
#define SUB_SP		0xb080
#define SUB_WI		0xf1a00000
#define SXTB		0xb240
#define SXTB_W		0xfa4ff080
#define SXTH		0xb200
#define SXTH_W		0xfa0ff080
#define TST		0x4200
#define UMULL		0xfba00000
#define UXTB		0xb2c0
#define UXTB_W		0xfa5ff080
#define UXTH		0xb280
#define UXTH_W		0xfa1ff080
#define VABS_F32	0xeeb00ac0
#define VADD_F32	0xee300a00
#define VCMP_F32	0xeeb40a40
#define VCVT_F32_S32	0xeeb80ac0
#define VCVT_F64_F32	0xeeb70ac0
#define VCVT_S32_F32	0xeebd0ac0
#define VDIV_F32	0xee800a00
#define VMOV_F32	0xeeb00a40
#define VMOV		0xee000a10
#define VMRS		0xeef1fa10
#define VMUL_F32	0xee200a00
#define VNEG_F32	0xeeb10a40
#define VSTR_F32	0xed000a00
#define VSUB_F32	0xee300a40

static sljit_s32 push_inst16(struct sljit_compiler *compiler, sljit_ins inst)
{
	sljit_u16 *ptr;
	SLJIT_ASSERT(!(inst & 0xffff0000));

	ptr = (sljit_u16*)ensure_buf(compiler, sizeof(sljit_u16));
	FAIL_IF(!ptr);
	*ptr = inst;
	compiler->size++;
	return SLJIT_SUCCESS;
}

static sljit_s32 push_inst32(struct sljit_compiler *compiler, sljit_ins inst)
{
	sljit_u16 *ptr = (sljit_u16*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr++ = inst >> 16;
	*ptr = inst;
	compiler->size += 2;
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_s32 emit_imm32_const(struct sljit_compiler *compiler, sljit_s32 dst, sljit_uw imm)
{
	FAIL_IF(push_inst32(compiler, MOVW | RD4(dst) |
		COPY_BITS(imm, 12, 16, 4) | COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff)));
	return push_inst32(compiler, MOVT | RD4(dst) |
		COPY_BITS(imm, 12 + 16, 16, 4) | COPY_BITS(imm, 11 + 16, 26, 1) | COPY_BITS(imm, 8 + 16, 12, 3) | ((imm & 0xff0000) >> 16));
}

static SLJIT_INLINE void modify_imm32_const(sljit_u16 *inst, sljit_uw new_imm)
{
	sljit_s32 dst = inst[1] & 0x0f00;
	SLJIT_ASSERT(((inst[0] & 0xfbf0) == (MOVW >> 16)) && ((inst[2] & 0xfbf0) == (MOVT >> 16)) && dst == (inst[3] & 0x0f00));
	inst[0] = (MOVW >> 16) | COPY_BITS(new_imm, 12, 0, 4) | COPY_BITS(new_imm, 11, 10, 1);
	inst[1] = dst | COPY_BITS(new_imm, 8, 12, 3) | (new_imm & 0xff);
	inst[2] = (MOVT >> 16) | COPY_BITS(new_imm, 12 + 16, 0, 4) | COPY_BITS(new_imm, 11 + 16, 10, 1);
	inst[3] = dst | COPY_BITS(new_imm, 8 + 16, 12, 3) | ((new_imm & 0xff0000) >> 16);
}

static SLJIT_INLINE sljit_s32 detect_jump_type(struct sljit_jump *jump, sljit_u16 *code_ptr, sljit_u16 *code)
{
	sljit_sw diff;

	if (jump->flags & SLJIT_REWRITABLE_JUMP)
		return 0;

	if (jump->flags & JUMP_ADDR) {
		/* Branch to ARM code is not optimized yet. */
		if (!(jump->u.target & 0x1))
			return 0;
		diff = ((sljit_sw)jump->u.target - (sljit_sw)(code_ptr + 2)) >> 1;
	}
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		diff = ((sljit_sw)(code + jump->u.label->size) - (sljit_sw)(code_ptr + 2)) >> 1;
	}

	if (jump->flags & IS_COND) {
		SLJIT_ASSERT(!(jump->flags & IS_BL));
		if (diff <= 127 && diff >= -128) {
			jump->flags |= PATCH_TYPE1;
			return 5;
		}
		if (diff <= 524287 && diff >= -524288) {
			jump->flags |= PATCH_TYPE2;
			return 4;
		}
		/* +1 comes from the prefix IT instruction. */
		diff--;
		if (diff <= 8388607 && diff >= -8388608) {
			jump->flags |= PATCH_TYPE3;
			return 3;
		}
	}
	else if (jump->flags & IS_BL) {
		if (diff <= 8388607 && diff >= -8388608) {
			jump->flags |= PATCH_BL;
			return 3;
		}
	}
	else {
		if (diff <= 1023 && diff >= -1024) {
			jump->flags |= PATCH_TYPE4;
			return 4;
		}
		if (diff <= 8388607 && diff >= -8388608) {
			jump->flags |= PATCH_TYPE5;
			return 3;
		}
	}

	return 0;
}

static SLJIT_INLINE void set_jump_instruction(struct sljit_jump *jump)
{
	sljit_s32 type = (jump->flags >> 4) & 0xf;
	sljit_sw diff;
	sljit_u16 *jump_inst;
	sljit_s32 s, j1, j2;

	if (SLJIT_UNLIKELY(type == 0)) {
		modify_imm32_const((sljit_u16*)jump->addr, (jump->flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target);
		return;
	}

	if (jump->flags & JUMP_ADDR) {
		SLJIT_ASSERT(jump->u.target & 0x1);
		diff = ((sljit_sw)jump->u.target - (sljit_sw)(jump->addr + 4)) >> 1;
	}
	else
		diff = ((sljit_sw)(jump->u.label->addr) - (sljit_sw)(jump->addr + 4)) >> 1;
	jump_inst = (sljit_u16*)jump->addr;

	switch (type) {
	case 1:
		/* Encoding T1 of 'B' instruction */
		SLJIT_ASSERT(diff <= 127 && diff >= -128 && (jump->flags & IS_COND));
		jump_inst[0] = 0xd000 | (jump->flags & 0xf00) | (diff & 0xff);
		return;
	case 2:
		/* Encoding T3 of 'B' instruction */
		SLJIT_ASSERT(diff <= 524287 && diff >= -524288 && (jump->flags & IS_COND));
		jump_inst[0] = 0xf000 | COPY_BITS(jump->flags, 8, 6, 4) | COPY_BITS(diff, 11, 0, 6) | COPY_BITS(diff, 19, 10, 1);
		jump_inst[1] = 0x8000 | COPY_BITS(diff, 17, 13, 1) | COPY_BITS(diff, 18, 11, 1) | (diff & 0x7ff);
		return;
	case 3:
		SLJIT_ASSERT(jump->flags & IS_COND);
		*jump_inst++ = IT | ((jump->flags >> 4) & 0xf0) | 0x8;
		diff--;
		type = 5;
		break;
	case 4:
		/* Encoding T2 of 'B' instruction */
		SLJIT_ASSERT(diff <= 1023 && diff >= -1024 && !(jump->flags & IS_COND));
		jump_inst[0] = 0xe000 | (diff & 0x7ff);
		return;
	}

	SLJIT_ASSERT(diff <= 8388607 && diff >= -8388608);

	/* Really complex instruction form for branches. */
	s = (diff >> 23) & 0x1;
	j1 = (~(diff >> 21) ^ s) & 0x1;
	j2 = (~(diff >> 22) ^ s) & 0x1;
	jump_inst[0] = 0xf000 | (s << 10) | COPY_BITS(diff, 11, 0, 10);
	jump_inst[1] = (j1 << 13) | (j2 << 11) | (diff & 0x7ff);

	/* The others have a common form. */
	if (type == 5) /* Encoding T4 of 'B' instruction */
		jump_inst[1] |= 0x9000;
	else if (type == 6) /* Encoding T1 of 'BL' instruction */
		jump_inst[1] |= 0xd000;
	else
		SLJIT_ASSERT_STOP();
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_u16 *code;
	sljit_u16 *code_ptr;
	sljit_u16 *buf_ptr;
	sljit_u16 *buf_end;
	sljit_uw half_count;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_generate_code(compiler));
	reverse_buf(compiler);

	code = (sljit_u16*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_u16));
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	half_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;

	do {
		buf_ptr = (sljit_u16*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 1);
		do {
			*code_ptr = *buf_ptr++;
			/* These structures are ordered by their address. */
			SLJIT_ASSERT(!label || label->size >= half_count);
			SLJIT_ASSERT(!jump || jump->addr >= half_count);
			SLJIT_ASSERT(!const_ || const_->addr >= half_count);
			if (label && label->size == half_count) {
				label->addr = ((sljit_uw)code_ptr) | 0x1;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == half_count) {
					jump->addr = (sljit_uw)code_ptr - ((jump->flags & IS_COND) ? 10 : 8);
					code_ptr -= detect_jump_type(jump, code_ptr, code);
					jump = jump->next;
			}
			if (const_ && const_->addr == half_count) {
				const_->addr = (sljit_uw)code_ptr;
				const_ = const_->next;
			}
			code_ptr ++;
			half_count ++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	if (label && label->size == half_count) {
		label->addr = ((sljit_uw)code_ptr) | 0x1;
		label->size = code_ptr - code;
		label = label->next;
	}

	SLJIT_ASSERT(!label);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(!const_);
	SLJIT_ASSERT(code_ptr - code <= (sljit_sw)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		set_jump_instruction(jump);
		jump = jump->next;
	}

	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_size = (code_ptr - code) * sizeof(sljit_u16);
	SLJIT_CACHE_FLUSH(code, code_ptr);
	/* Set thumb mode flag. */
	return (void*)((sljit_uw)code | 0x1);
}

/* --------------------------------------------------------------------- */
/*  Core code generator functions.                                       */
/* --------------------------------------------------------------------- */

#define INVALID_IMM	0x80000000
static sljit_uw get_imm(sljit_uw imm)
{
	/* Thumb immediate form. */
	sljit_s32 counter;

	if (imm <= 0xff)
		return imm;

	if ((imm & 0xffff) == (imm >> 16)) {
		/* Some special cases. */
		if (!(imm & 0xff00))
			return (1 << 12) | (imm & 0xff);
		if (!(imm & 0xff))
			return (2 << 12) | ((imm >> 8) & 0xff);
		if ((imm & 0xff00) == ((imm & 0xff) << 8))
			return (3 << 12) | (imm & 0xff);
	}

	/* Assembly optimization: count leading zeroes? */
	counter = 8;
	if (!(imm & 0xffff0000)) {
		counter += 16;
		imm <<= 16;
	}
	if (!(imm & 0xff000000)) {
		counter += 8;
		imm <<= 8;
	}
	if (!(imm & 0xf0000000)) {
		counter += 4;
		imm <<= 4;
	}
	if (!(imm & 0xc0000000)) {
		counter += 2;
		imm <<= 2;
	}
	if (!(imm & 0x80000000)) {
		counter += 1;
		imm <<= 1;
	}
	/* Since imm >= 128, this must be true. */
	SLJIT_ASSERT(counter <= 31);

	if (imm & 0x00ffffff)
		return INVALID_IMM; /* Cannot be encoded. */

	return ((imm >> 24) & 0x7f) | COPY_BITS(counter, 4, 26, 1) | COPY_BITS(counter, 1, 12, 3) | COPY_BITS(counter, 0, 7, 1);
}

static sljit_s32 load_immediate(struct sljit_compiler *compiler, sljit_s32 dst, sljit_uw imm)
{
	sljit_uw tmp;

	if (imm >= 0x10000) {
		tmp = get_imm(imm);
		if (tmp != INVALID_IMM)
			return push_inst32(compiler, MOV_WI | RD4(dst) | tmp);
		tmp = get_imm(~imm);
		if (tmp != INVALID_IMM)
			return push_inst32(compiler, MVN_WI | RD4(dst) | tmp);
	}

	/* set low 16 bits, set hi 16 bits to 0. */
	FAIL_IF(push_inst32(compiler, MOVW | RD4(dst) |
		COPY_BITS(imm, 12, 16, 4) | COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff)));

	/* set hi 16 bit if needed. */
	if (imm >= 0x10000)
		return push_inst32(compiler, MOVT | RD4(dst) |
			COPY_BITS(imm, 12 + 16, 16, 4) | COPY_BITS(imm, 11 + 16, 26, 1) | COPY_BITS(imm, 8 + 16, 12, 3) | ((imm & 0xff0000) >> 16));
	return SLJIT_SUCCESS;
}

#define ARG1_IMM	0x0010000
#define ARG2_IMM	0x0020000
#define KEEP_FLAGS	0x0040000
/* SET_FLAGS must be 0x100000 as it is also the value of S bit (can be used for optimization). */
#define SET_FLAGS	0x0100000
#define UNUSED_RETURN	0x0200000
#define SLOW_DEST	0x0400000
#define SLOW_SRC1	0x0800000
#define SLOW_SRC2	0x1000000

static sljit_s32 emit_op_imm(struct sljit_compiler *compiler, sljit_s32 flags, sljit_s32 dst, sljit_uw arg1, sljit_uw arg2)
{
	/* dst must be register, TMP_REG1
	   arg1 must be register, TMP_REG1, imm
	   arg2 must be register, TMP_REG2, imm */
	sljit_s32 reg;
	sljit_uw imm, nimm;

	if (SLJIT_UNLIKELY((flags & (ARG1_IMM | ARG2_IMM)) == (ARG1_IMM | ARG2_IMM))) {
		/* Both are immediates. */
		flags &= ~ARG1_IMM;
		FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
		arg1 = TMP_REG1;
	}

	if (flags & (ARG1_IMM | ARG2_IMM)) {
		reg = (flags & ARG2_IMM) ? arg1 : arg2;
		imm = (flags & ARG2_IMM) ? arg2 : arg1;

		switch (flags & 0xffff) {
		case SLJIT_CLZ:
		case SLJIT_MUL:
			/* No form with immediate operand. */
			break;
		case SLJIT_MOV:
			SLJIT_ASSERT(!(flags & SET_FLAGS) && (flags & ARG2_IMM) && arg1 == TMP_REG1);
			return load_immediate(compiler, dst, imm);
		case SLJIT_NOT:
			if (!(flags & SET_FLAGS))
				return load_immediate(compiler, dst, ~imm);
			/* Since the flags should be set, we just fallback to the register mode.
			   Although some clever things could be done here, "NOT IMM" does not worth the efforts. */
			break;
		case SLJIT_ADD:
			nimm = -imm;
			if (!(flags & KEEP_FLAGS) && IS_2_LO_REGS(reg, dst)) {
				if (imm <= 0x7)
					return push_inst16(compiler, ADDSI3 | IMM3(imm) | RD3(dst) | RN3(reg));
				if (nimm <= 0x7)
					return push_inst16(compiler, SUBSI3 | IMM3(nimm) | RD3(dst) | RN3(reg));
				if (reg == dst) {
					if (imm <= 0xff)
						return push_inst16(compiler, ADDSI8 | IMM8(imm) | RDN3(dst));
					if (nimm <= 0xff)
						return push_inst16(compiler, SUBSI8 | IMM8(nimm) | RDN3(dst));
				}
			}
			if (!(flags & SET_FLAGS)) {
				if (imm <= 0xfff)
					return push_inst32(compiler, ADDWI | RD4(dst) | RN4(reg) | IMM12(imm));
				if (nimm <= 0xfff)
					return push_inst32(compiler, SUBWI | RD4(dst) | RN4(reg) | IMM12(nimm));
			}
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ADD_WI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_ADDC:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ADCI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SUB:
			if (flags & ARG1_IMM) {
				if (!(flags & KEEP_FLAGS) && imm == 0 && IS_2_LO_REGS(reg, dst))
					return push_inst16(compiler, RSBSI | RD3(dst) | RN3(reg));
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, RSB_WI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
				break;
			}
			nimm = -imm;
			if (!(flags & KEEP_FLAGS) && IS_2_LO_REGS(reg, dst)) {
				if (imm <= 0x7)
					return push_inst16(compiler, SUBSI3 | IMM3(imm) | RD3(dst) | RN3(reg));
				if (nimm <= 0x7)
					return push_inst16(compiler, ADDSI3 | IMM3(nimm) | RD3(dst) | RN3(reg));
				if (reg == dst) {
					if (imm <= 0xff)
						return push_inst16(compiler, SUBSI8 | IMM8(imm) | RDN3(dst));
					if (nimm <= 0xff)
						return push_inst16(compiler, ADDSI8 | IMM8(nimm) | RDN3(dst));
				}
				if (imm <= 0xff && (flags & UNUSED_RETURN))
					return push_inst16(compiler, CMPI | IMM8(imm) | RDN3(reg));
			}
			if (!(flags & SET_FLAGS)) {
				if (imm <= 0xfff)
					return push_inst32(compiler, SUBWI | RD4(dst) | RN4(reg) | IMM12(imm));
				if (nimm <= 0xfff)
					return push_inst32(compiler, ADDWI | RD4(dst) | RN4(reg) | IMM12(nimm));
			}
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, SUB_WI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SUBC:
			if (flags & ARG1_IMM)
				break;
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, SBCI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_AND:
			nimm = get_imm(imm);
			if (nimm != INVALID_IMM)
				return push_inst32(compiler, ANDI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | nimm);
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, BICI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_OR:
			nimm = get_imm(imm);
			if (nimm != INVALID_IMM)
				return push_inst32(compiler, ORRI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | nimm);
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ORNI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_XOR:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, EORI | (flags & SET_FLAGS) | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SHL:
		case SLJIT_LSHR:
		case SLJIT_ASHR:
			if (flags & ARG1_IMM)
				break;
			imm &= 0x1f;
			if (imm == 0) {
				if (!(flags & SET_FLAGS))
					return push_inst16(compiler, MOV | SET_REGS44(dst, reg));
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, MOVS | RD3(dst) | RN3(reg));
				return push_inst32(compiler, MOV_W | SET_FLAGS | RD4(dst) | RM4(reg));
			}
			switch (flags & 0xffff) {
			case SLJIT_SHL:
				if (!(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, LSLSI | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, LSL_WI | (flags & SET_FLAGS) | RD4(dst) | RM4(reg) | IMM5(imm));
			case SLJIT_LSHR:
				if (!(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, LSRSI | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, LSR_WI | (flags & SET_FLAGS) | RD4(dst) | RM4(reg) | IMM5(imm));
			default: /* SLJIT_ASHR */
				if (!(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, ASRSI | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, ASR_WI | (flags & SET_FLAGS) | RD4(dst) | RM4(reg) | IMM5(imm));
			}
		default:
			SLJIT_ASSERT_STOP();
			break;
		}

		if (flags & ARG2_IMM) {
			FAIL_IF(load_immediate(compiler, TMP_REG2, arg2));
			arg2 = TMP_REG2;
		}
		else {
			FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
			arg1 = TMP_REG1;
		}
	}

	/* Both arguments are registers. */
	switch (flags & 0xffff) {
	case SLJIT_MOV:
	case SLJIT_MOV_U32:
	case SLJIT_MOV_S32:
	case SLJIT_MOV_P:
	case SLJIT_MOVU:
	case SLJIT_MOVU_U32:
	case SLJIT_MOVU_S32:
	case SLJIT_MOVU_P:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst16(compiler, MOV | SET_REGS44(dst, arg2));
	case SLJIT_MOV_U8:
	case SLJIT_MOVU_U8:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, UXTB | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, UXTB_W | RD4(dst) | RM4(arg2));
	case SLJIT_MOV_S8:
	case SLJIT_MOVU_S8:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, SXTB | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, SXTB_W | RD4(dst) | RM4(arg2));
	case SLJIT_MOV_U16:
	case SLJIT_MOVU_U16:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, UXTH | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, UXTH_W | RD4(dst) | RM4(arg2));
	case SLJIT_MOV_S16:
	case SLJIT_MOVU_S16:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, SXTH | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, SXTH_W | RD4(dst) | RM4(arg2));
	case SLJIT_NOT:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		if (!(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, MVNS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, MVN_W | (flags & SET_FLAGS) | RD4(dst) | RM4(arg2));
	case SLJIT_CLZ:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		FAIL_IF(push_inst32(compiler, CLZ | RN4(arg2) | RD4(dst) | RM4(arg2)));
		if (flags & SET_FLAGS) {
			if (reg_map[dst] <= 7)
				return push_inst16(compiler, CMPI | RDN3(dst));
			return push_inst32(compiler, ADD_WI | SET_FLAGS | RN4(dst) | RD4(dst));
		}
		return SLJIT_SUCCESS;
	case SLJIT_ADD:
		if (!(flags & KEEP_FLAGS) && IS_3_LO_REGS(dst, arg1, arg2))
			return push_inst16(compiler, ADDS | RD3(dst) | RN3(arg1) | RM3(arg2));
		if (dst == arg1 && !(flags & SET_FLAGS))
			return push_inst16(compiler, ADD | SET_REGS44(dst, arg2));
		return push_inst32(compiler, ADD_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_ADDC:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ADCS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ADC_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SUB:
		if (!(flags & KEEP_FLAGS) && IS_3_LO_REGS(dst, arg1, arg2))
			return push_inst16(compiler, SUBS | RD3(dst) | RN3(arg1) | RM3(arg2));
		return push_inst32(compiler, SUB_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SUBC:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, SBCS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, SBC_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_MUL:
		if (!(flags & SET_FLAGS))
			return push_inst32(compiler, MUL | RD4(dst) | RN4(arg1) | RM4(arg2));
		SLJIT_ASSERT(reg_map[TMP_REG2] <= 7 && dst != TMP_REG2);
		FAIL_IF(push_inst32(compiler, SMULL | RT4(dst) | RD4(TMP_REG2) | RN4(arg1) | RM4(arg2)));
		/* cmp TMP_REG2, dst asr #31. */
		return push_inst32(compiler, CMP_W | RN4(TMP_REG2) | 0x70e0 | RM4(dst));
	case SLJIT_AND:
		if (!(flags & KEEP_FLAGS)) {
			if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
				return push_inst16(compiler, ANDS | RD3(dst) | RN3(arg2));
			if ((flags & UNUSED_RETURN) && IS_2_LO_REGS(arg1, arg2))
				return push_inst16(compiler, TST | RD3(arg1) | RN3(arg2));
		}
		return push_inst32(compiler, AND_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_OR:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ORRS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ORR_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_XOR:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, EORS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, EOR_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SHL:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, LSLS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, LSL_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_LSHR:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, LSRS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, LSR_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_ASHR:
		if (dst == arg1 && !(flags & KEEP_FLAGS) && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ASRS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ASR_W | (flags & SET_FLAGS) | RD4(dst) | RN4(arg1) | RM4(arg2));
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
}

#define STORE		0x01
#define SIGNED		0x02

#define WORD_SIZE	0x00
#define BYTE_SIZE	0x04
#define HALF_SIZE	0x08

#define UPDATE		0x10
#define ARG_TEST	0x20

#define IS_WORD_SIZE(flags)		(!(flags & (BYTE_SIZE | HALF_SIZE)))
#define OFFSET_CHECK(imm, shift)	(!(argw & ~(imm << shift)))

/*
  1st letter:
  w = word
  b = byte
  h = half

  2nd letter:
  s = signed
  u = unsigned

  3rd letter:
  l = load
  s = store
*/

static const sljit_ins sljit_mem16[12] = {
/* w u l */ 0x5800 /* ldr */,
/* w u s */ 0x5000 /* str */,
/* w s l */ 0x5800 /* ldr */,
/* w s s */ 0x5000 /* str */,

/* b u l */ 0x5c00 /* ldrb */,
/* b u s */ 0x5400 /* strb */,
/* b s l */ 0x5600 /* ldrsb */,
/* b s s */ 0x5400 /* strb */,

/* h u l */ 0x5a00 /* ldrh */,
/* h u s */ 0x5200 /* strh */,
/* h s l */ 0x5e00 /* ldrsh */,
/* h s s */ 0x5200 /* strh */,
};

static const sljit_ins sljit_mem16_imm5[12] = {
/* w u l */ 0x6800 /* ldr imm5 */,
/* w u s */ 0x6000 /* str imm5 */,
/* w s l */ 0x6800 /* ldr imm5 */,
/* w s s */ 0x6000 /* str imm5 */,

/* b u l */ 0x7800 /* ldrb imm5 */,
/* b u s */ 0x7000 /* strb imm5 */,
/* b s l */ 0x0000 /* not allowed */,
/* b s s */ 0x7000 /* strb imm5 */,

/* h u l */ 0x8800 /* ldrh imm5 */,
/* h u s */ 0x8000 /* strh imm5 */,
/* h s l */ 0x0000 /* not allowed */,
/* h s s */ 0x8000 /* strh imm5 */,
};

#define MEM_IMM8	0xc00
#define MEM_IMM12	0x800000
static const sljit_ins sljit_mem32[12] = {
/* w u l */ 0xf8500000 /* ldr.w */,
/* w u s */ 0xf8400000 /* str.w */,
/* w s l */ 0xf8500000 /* ldr.w */,
/* w s s */ 0xf8400000 /* str.w */,

/* b u l */ 0xf8100000 /* ldrb.w */,
/* b u s */ 0xf8000000 /* strb.w */,
/* b s l */ 0xf9100000 /* ldrsb.w */,
/* b s s */ 0xf8000000 /* strb.w */,

/* h u l */ 0xf8300000 /* ldrh.w */,
/* h u s */ 0xf8200000 /* strsh.w */,
/* h s l */ 0xf9300000 /* ldrsh.w */,
/* h s s */ 0xf8200000 /* strsh.w */,
};

/* Helper function. Dst should be reg + value, using at most 1 instruction, flags does not set. */
static sljit_s32 emit_set_delta(struct sljit_compiler *compiler, sljit_s32 dst, sljit_s32 reg, sljit_sw value)
{
	if (value >= 0) {
		if (value <= 0xfff)
			return push_inst32(compiler, ADDWI | RD4(dst) | RN4(reg) | IMM12(value));
		value = get_imm(value);
		if (value != INVALID_IMM)
			return push_inst32(compiler, ADD_WI | RD4(dst) | RN4(reg) | value);
	}
	else {
		value = -value;
		if (value <= 0xfff)
			return push_inst32(compiler, SUBWI | RD4(dst) | RN4(reg) | IMM12(value));
		value = get_imm(value);
		if (value != INVALID_IMM)
			return push_inst32(compiler, SUB_WI | RD4(dst) | RN4(reg) | value);
	}
	return SLJIT_ERR_UNSUPPORTED;
}

/* Can perform an operation using at most 1 instruction. */
static sljit_s32 getput_arg_fast(struct sljit_compiler *compiler, sljit_s32 flags, sljit_s32 reg, sljit_s32 arg, sljit_sw argw)
{
	sljit_s32 other_r, shift;

	SLJIT_ASSERT(arg & SLJIT_MEM);

	if (SLJIT_UNLIKELY(flags & UPDATE)) {
		if ((arg & REG_MASK) && !(arg & OFFS_REG_MASK) && argw <= 0xff && argw >= -0xff) {
			if (SLJIT_UNLIKELY(flags & ARG_TEST))
				return 1;

			flags &= ~UPDATE;
			arg &= 0xf;
			if (argw >= 0)
				argw |= 0x200;
			else {
				argw = -argw;
			}

			SLJIT_ASSERT(argw >= 0 && (argw & 0xff) <= 0xff);
			FAIL_IF(push_inst32(compiler, sljit_mem32[flags] | MEM_IMM8 | RT4(reg) | RN4(arg) | 0x100 | argw));
			return -1;
		}
		return 0;
	}

	if (SLJIT_UNLIKELY(arg & OFFS_REG_MASK)) {
		if (SLJIT_UNLIKELY(flags & ARG_TEST))
			return 1;

		argw &= 0x3;
		other_r = OFFS_REG(arg);
		arg &= 0xf;

		if (!argw && IS_3_LO_REGS(reg, arg, other_r))
			FAIL_IF(push_inst16(compiler, sljit_mem16[flags] | RD3(reg) | RN3(arg) | RM3(other_r)));
		else
			FAIL_IF(push_inst32(compiler, sljit_mem32[flags] | RT4(reg) | RN4(arg) | RM4(other_r) | (argw << 4)));
		return -1;
	}

	if (!(arg & REG_MASK) || argw > 0xfff || argw < -0xff)
		return 0;

	if (SLJIT_UNLIKELY(flags & ARG_TEST))
		return 1;

	arg &= 0xf;
	if (IS_2_LO_REGS(reg, arg) && sljit_mem16_imm5[flags]) {
		shift = 3;
		if (IS_WORD_SIZE(flags)) {
			if (OFFSET_CHECK(0x1f, 2))
				shift = 2;
		}
		else if (flags & BYTE_SIZE)
		{
			if (OFFSET_CHECK(0x1f, 0))
				shift = 0;
		}
		else {
			SLJIT_ASSERT(flags & HALF_SIZE);
			if (OFFSET_CHECK(0x1f, 1))
				shift = 1;
		}

		if (shift != 3) {
			FAIL_IF(push_inst16(compiler, sljit_mem16_imm5[flags] | RD3(reg) | RN3(arg) | (argw << (6 - shift))));
			return -1;
		}
	}

	/* SP based immediate. */
	if (SLJIT_UNLIKELY(arg == SLJIT_SP) && OFFSET_CHECK(0xff, 2) && IS_WORD_SIZE(flags) && reg_map[reg] <= 7) {
		FAIL_IF(push_inst16(compiler, STR_SP | ((flags & STORE) ? 0 : 0x800) | RDN3(reg) | (argw >> 2)));
		return -1;
	}

	if (argw >= 0)
		FAIL_IF(push_inst32(compiler, sljit_mem32[flags] | MEM_IMM12 | RT4(reg) | RN4(arg) | argw));
	else
		FAIL_IF(push_inst32(compiler, sljit_mem32[flags] | MEM_IMM8 | RT4(reg) | RN4(arg) | -argw));
	return -1;
}

/* see getput_arg below.
   Note: can_cache is called only for binary operators. Those
   operators always uses word arguments without write back. */
static sljit_s32 can_cache(sljit_s32 arg, sljit_sw argw, sljit_s32 next_arg, sljit_sw next_argw)
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
static sljit_s32 getput_arg(struct sljit_compiler *compiler, sljit_s32 flags, sljit_s32 reg,
	sljit_s32 arg, sljit_sw argw, sljit_s32 next_arg, sljit_sw next_argw)
{
	sljit_s32 tmp_r, other_r;
	sljit_sw diff;

	SLJIT_ASSERT(arg & SLJIT_MEM);
	if (!(next_arg & SLJIT_MEM)) {
		next_arg = 0;
		next_argw = 0;
	}

	tmp_r = (flags & STORE) ? TMP_REG3 : reg;

	if (SLJIT_UNLIKELY((flags & UPDATE) && (arg & REG_MASK))) {
		/* Update only applies if a base register exists. */
		/* There is no caching here. */
		other_r = OFFS_REG(arg);
		arg &= 0xf;
		flags &= ~UPDATE;

		if (!other_r) {
			if (!(argw & ~0xfff)) {
				FAIL_IF(push_inst32(compiler, sljit_mem32[flags] | MEM_IMM12 | RT4(reg) | RN4(arg) | argw));
				return push_inst32(compiler, ADDWI | RD4(arg) | RN4(arg) | IMM12(argw));
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

		argw &= 0x3;
		if (!argw && IS_3_LO_REGS(reg, arg, other_r)) {
			FAIL_IF(push_inst16(compiler, sljit_mem16[flags] | RD3(reg) | RN3(arg) | RM3(other_r)));
			return push_inst16(compiler, ADD | SET_REGS44(arg, other_r));
		}
		FAIL_IF(push_inst32(compiler, sljit_mem32[flags] | RT4(reg) | RN4(arg) | RM4(other_r) | (argw << 4)));
		return push_inst32(compiler, ADD_W | RD4(arg) | RN4(arg) | RM4(other_r) | (argw << 6));
	}
	flags &= ~UPDATE;

	SLJIT_ASSERT(!(arg & OFFS_REG_MASK));

	if (compiler->cache_arg == arg) {
		diff = argw - compiler->cache_argw;
		if (!(diff & ~0xfff))
			return push_inst32(compiler, sljit_mem32[flags] | MEM_IMM12 | RT4(reg) | RN4(TMP_REG3) | diff);
		if (!((compiler->cache_argw - argw) & ~0xff))
			return push_inst32(compiler, sljit_mem32[flags] | MEM_IMM8 | RT4(reg) | RN4(TMP_REG3) | (compiler->cache_argw - argw));
		if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, diff) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			return push_inst32(compiler, sljit_mem32[flags] | MEM_IMM12 | RT4(reg) | RN4(TMP_REG3) | 0);
		}
	}

	next_arg = (arg & REG_MASK) && (arg == next_arg) && (argw != next_argw);
	arg &= 0xf;
	if (arg && compiler->cache_arg == SLJIT_MEM) {
		if (compiler->cache_argw == argw)
			return push_inst32(compiler, sljit_mem32[flags] | RT4(reg) | RN4(arg) | RM4(TMP_REG3));
		if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, argw - compiler->cache_argw) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			compiler->cache_argw = argw;
			return push_inst32(compiler, sljit_mem32[flags] | RT4(reg) | RN4(arg) | RM4(TMP_REG3));
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

		diff = argw - next_argw;
		if (next_arg && diff <= 0xfff && diff >= -0xfff) {
			FAIL_IF(push_inst16(compiler, ADD | SET_REGS44(TMP_REG3, arg)));
			compiler->cache_arg = SLJIT_MEM | arg;
			arg = 0;
		}
	}

	if (arg)
		return push_inst32(compiler, sljit_mem32[flags] | RT4(reg) | RN4(arg) | RM4(TMP_REG3));
	return push_inst32(compiler, sljit_mem32[flags] | MEM_IMM12 | RT4(reg) | RN4(TMP_REG3) | 0);
}

static SLJIT_INLINE sljit_s32 emit_op_mem(struct sljit_compiler *compiler, sljit_s32 flags, sljit_s32 reg, sljit_s32 arg, sljit_sw argw)
{
	if (getput_arg_fast(compiler, flags, reg, arg, argw))
		return compiler->error;
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	return getput_arg(compiler, flags, reg, arg, argw, 0, 0);
}

static SLJIT_INLINE sljit_s32 emit_op_mem2(struct sljit_compiler *compiler, sljit_s32 flags, sljit_s32 reg, sljit_s32 arg1, sljit_sw arg1w, sljit_s32 arg2, sljit_sw arg2w)
{
	if (getput_arg_fast(compiler, flags, reg, arg1, arg1w))
		return compiler->error;
	return getput_arg(compiler, flags, reg, arg1, arg1w, arg2, arg2w);
}

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_enter(struct sljit_compiler *compiler,
	sljit_s32 options, sljit_s32 args, sljit_s32 scratches, sljit_s32 saveds,
	sljit_s32 fscratches, sljit_s32 fsaveds, sljit_s32 local_size)
{
	sljit_s32 size, i, tmp;
	sljit_ins push;

	CHECK_ERROR();
	CHECK(check_sljit_emit_enter(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size));
	set_emit_enter(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size);

	push = (1 << 4);

	tmp = saveds < SLJIT_NUMBER_OF_SAVED_REGISTERS ? (SLJIT_S0 + 1 - saveds) : SLJIT_FIRST_SAVED_REG;
	for (i = SLJIT_S0; i >= tmp; i--)
		push |= 1 << reg_map[i];

	for (i = scratches; i >= SLJIT_FIRST_SAVED_REG; i--)
		push |= 1 << reg_map[i];

	FAIL_IF((push & 0xff00)
		? push_inst32(compiler, PUSH_W | (1 << 14) | push)
		: push_inst16(compiler, PUSH | (1 << 8) | push));

	/* Stack must be aligned to 8 bytes: (LR, R4) */
	size = GET_SAVED_REGISTERS_SIZE(scratches, saveds, 2);
	local_size = ((size + local_size + 7) & ~7) - size;
	compiler->local_size = local_size;
	if (local_size > 0) {
		if (local_size <= (127 << 2))
			FAIL_IF(push_inst16(compiler, SUB_SP | (local_size >> 2)));
		else
			FAIL_IF(emit_op_imm(compiler, SLJIT_SUB | ARG2_IMM, SLJIT_SP, SLJIT_SP, local_size));
	}

	if (args >= 1)
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_S0, SLJIT_R0)));
	if (args >= 2)
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_S1, SLJIT_R1)));
	if (args >= 3)
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_S2, SLJIT_R2)));

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_set_context(struct sljit_compiler *compiler,
	sljit_s32 options, sljit_s32 args, sljit_s32 scratches, sljit_s32 saveds,
	sljit_s32 fscratches, sljit_s32 fsaveds, sljit_s32 local_size)
{
	sljit_s32 size;

	CHECK_ERROR();
	CHECK(check_sljit_set_context(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size));
	set_set_context(compiler, options, args, scratches, saveds, fscratches, fsaveds, local_size);

	size = GET_SAVED_REGISTERS_SIZE(scratches, saveds, 2);
	compiler->local_size = ((size + local_size + 7) & ~7) - size;
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_return(struct sljit_compiler *compiler, sljit_s32 op, sljit_s32 src, sljit_sw srcw)
{
	sljit_s32 i, tmp;
	sljit_ins pop;

	CHECK_ERROR();
	CHECK(check_sljit_emit_return(compiler, op, src, srcw));

	FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));

	if (compiler->local_size > 0) {
		if (compiler->local_size <= (127 << 2))
			FAIL_IF(push_inst16(compiler, ADD_SP | (compiler->local_size >> 2)));
		else
			FAIL_IF(emit_op_imm(compiler, SLJIT_ADD | ARG2_IMM, SLJIT_SP, SLJIT_SP, compiler->local_size));
	}

	pop = (1 << 4);

	tmp = compiler->saveds < SLJIT_NUMBER_OF_SAVED_REGISTERS ? (SLJIT_S0 + 1 - compiler->saveds) : SLJIT_FIRST_SAVED_REG;
	for (i = SLJIT_S0; i >= tmp; i--)
		pop |= 1 << reg_map[i];

	for (i = compiler->scratches; i >= SLJIT_FIRST_SAVED_REG; i--)
		pop |= 1 << reg_map[i];

	return (pop & 0xff00)
		? push_inst32(compiler, POP_W | (1 << 15) | pop)
		: push_inst16(compiler, POP | (1 << 8) | pop);
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
extern unsigned int __aeabi_uidivmod(unsigned int numerator, int unsigned denominator);
extern int __aeabi_idivmod(int numerator, int denominator);
#else
#error "Software divmod functions are needed"
#endif

#ifdef __cplusplus
}
#endif

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op0(struct sljit_compiler *compiler, sljit_s32 op)
{
	sljit_sw saved_reg_list[3];
	sljit_sw saved_reg_count;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op0(compiler, op));

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
		return push_inst16(compiler, BKPT);
	case SLJIT_NOP:
		return push_inst16(compiler, NOP);
	case SLJIT_LMUL_UW:
	case SLJIT_LMUL_SW:
		return push_inst32(compiler, (op == SLJIT_LMUL_UW ? UMULL : SMULL)
			| (reg_map[SLJIT_R1] << 8)
			| (reg_map[SLJIT_R0] << 12)
			| (reg_map[SLJIT_R0] << 16)
			| reg_map[SLJIT_R1]);
	case SLJIT_DIVMOD_UW:
	case SLJIT_DIVMOD_SW:
	case SLJIT_DIV_UW:
	case SLJIT_DIV_SW:
		SLJIT_COMPILE_ASSERT((SLJIT_DIVMOD_UW & 0x2) == 0 && SLJIT_DIV_UW - 0x2 == SLJIT_DIVMOD_UW, bad_div_opcode_assignments);
		SLJIT_COMPILE_ASSERT(reg_map[2] == 1 && reg_map[3] == 2 && reg_map[4] == 12, bad_register_mapping);

		saved_reg_count = 0;
		if (compiler->scratches >= 4)
			saved_reg_list[saved_reg_count++] = 12;
		if (compiler->scratches >= 3)
			saved_reg_list[saved_reg_count++] = 2;
		if (op >= SLJIT_DIV_UW)
			saved_reg_list[saved_reg_count++] = 1;

		if (saved_reg_count > 0) {
			FAIL_IF(push_inst32(compiler, 0xf84d0d00 | (saved_reg_count >= 3 ? 16 : 8)
						| (saved_reg_list[0] << 12) /* str rX, [sp, #-8/-16]! */));
			if (saved_reg_count >= 2) {
				SLJIT_ASSERT(saved_reg_list[1] < 8);
				FAIL_IF(push_inst16(compiler, 0x9001 | (saved_reg_list[1] << 8) /* str rX, [sp, #4] */));
			}
			if (saved_reg_count >= 3) {
				SLJIT_ASSERT(saved_reg_list[2] < 8);
				FAIL_IF(push_inst16(compiler, 0x9002 | (saved_reg_list[2] << 8) /* str rX, [sp, #8] */));
			}
		}

#if defined(__GNUC__)
		FAIL_IF(sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM,
			((op | 0x2) == SLJIT_DIV_UW ? SLJIT_FUNC_OFFSET(__aeabi_uidivmod) : SLJIT_FUNC_OFFSET(__aeabi_idivmod))));
#else
#error "Software divmod functions are needed"
#endif

		if (saved_reg_count > 0) {
			if (saved_reg_count >= 3) {
				SLJIT_ASSERT(saved_reg_list[2] < 8);
				FAIL_IF(push_inst16(compiler, 0x9802 | (saved_reg_list[2] << 8) /* ldr rX, [sp, #8] */));
			}
			if (saved_reg_count >= 2) {
				SLJIT_ASSERT(saved_reg_list[1] < 8);
				FAIL_IF(push_inst16(compiler, 0x9801 | (saved_reg_list[1] << 8) /* ldr rX, [sp, #4] */));
			}
			return push_inst32(compiler, 0xf85d0b00 | (saved_reg_count >= 3 ? 16 : 8)
						| (saved_reg_list[0] << 12) /* ldr rX, [sp], #8/16 */);
		}
		return SLJIT_SUCCESS;
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op1(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src, sljit_sw srcw)
{
	sljit_s32 dst_r, flags;
	sljit_s32 op_flags = GET_ALL_FLAGS(op);

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
		case SLJIT_MOV_U32:
		case SLJIT_MOV_S32:
		case SLJIT_MOV_P:
			flags = WORD_SIZE;
			break;
		case SLJIT_MOV_U8:
			flags = BYTE_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_u8)srcw;
			break;
		case SLJIT_MOV_S8:
			flags = BYTE_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_s8)srcw;
			break;
		case SLJIT_MOV_U16:
			flags = HALF_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_u16)srcw;
			break;
		case SLJIT_MOV_S16:
			flags = HALF_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_s16)srcw;
			break;
		case SLJIT_MOVU:
		case SLJIT_MOVU_U32:
		case SLJIT_MOVU_S32:
		case SLJIT_MOVU_P:
			flags = WORD_SIZE | UPDATE;
			break;
		case SLJIT_MOVU_U8:
			flags = BYTE_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_u8)srcw;
			break;
		case SLJIT_MOVU_S8:
			flags = BYTE_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_s8)srcw;
			break;
		case SLJIT_MOVU_U16:
			flags = HALF_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_u16)srcw;
			break;
		case SLJIT_MOVU_S16:
			flags = HALF_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_s16)srcw;
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
				return emit_op_imm(compiler, op, dst_r, TMP_REG1, src);
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

	if (op == SLJIT_NEG) {
#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
			|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
		compiler->skip_checks = 1;
#endif
		return sljit_emit_op2(compiler, SLJIT_SUB | op_flags, dst, dstw, SLJIT_IMM, 0, src, srcw);
	}

	flags = (GET_FLAGS(op_flags) ? SET_FLAGS : 0) | ((op_flags & SLJIT_KEEP_FLAGS) ? KEEP_FLAGS : 0);
	if (src & SLJIT_MEM) {
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG2, src, srcw))
			FAIL_IF(compiler->error);
		else
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src, srcw, dst, dstw));
		src = TMP_REG2;
	}

	if (src & SLJIT_IMM)
		flags |= ARG2_IMM;
	else
		srcw = src;

	emit_op_imm(compiler, flags | op, dst_r, TMP_REG1, srcw);

	if (dst & SLJIT_MEM) {
		if (getput_arg_fast(compiler, flags | STORE, dst_r, dst, dstw))
			return compiler->error;
		else
			return getput_arg(compiler, flags | STORE, dst_r, dst, dstw, 0, 0);
	}
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op2(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
	sljit_s32 dst_r, flags;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = SLOW_IS_REG(dst) ? dst : TMP_REG1;
	flags = (GET_FLAGS(op) ? SET_FLAGS : 0) | ((op & SLJIT_KEEP_FLAGS) ? KEEP_FLAGS : 0);

	if ((dst & SLJIT_MEM) && !getput_arg_fast(compiler, WORD_SIZE | STORE | ARG_TEST, TMP_REG1, dst, dstw))
		flags |= SLOW_DEST;

	if (src1 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG1, src1, src1w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC1;
	}
	if (src2 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG2, src2, src2w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC2;
	}

	if ((flags & (SLOW_SRC1 | SLOW_SRC2)) == (SLOW_SRC1 | SLOW_SRC2)) {
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & SLOW_SRC1)
		FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG1, src1, src1w, dst, dstw));
	else if (flags & SLOW_SRC2)
		FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src2, src2w, dst, dstw));

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

	if (dst == SLJIT_UNUSED)
		flags |= UNUSED_RETURN;

	emit_op_imm(compiler, flags | GET_OPCODE(op), dst_r, src1w, src2w);

	if (dst & SLJIT_MEM) {
		if (!(flags & SLOW_DEST)) {
			getput_arg_fast(compiler, WORD_SIZE | STORE, dst_r, dst, dstw);
			return compiler->error;
		}
		return getput_arg(compiler, WORD_SIZE | STORE, TMP_REG1, dst, dstw, 0, 0);
	}
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_get_register_index(sljit_s32 reg)
{
	CHECK_REG_INDEX(check_sljit_get_register_index(reg));
	return reg_map[reg];
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_get_float_register_index(sljit_s32 reg)
{
	CHECK_REG_INDEX(check_sljit_get_float_register_index(reg));
	return reg << 1;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_s32 size)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_custom(compiler, instruction, size));

	if (size == 2)
		return push_inst16(compiler, *(sljit_u16*)instruction);
	return push_inst32(compiler, *(sljit_ins*)instruction);
}

/* --------------------------------------------------------------------- */
/*  Floating point operators                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_is_fpu_available(void)
{
#ifdef SLJIT_IS_FPU_AVAILABLE
	return SLJIT_IS_FPU_AVAILABLE;
#else
	/* Available by default. */
	return 1;
#endif
}

#define FPU_LOAD (1 << 20)

static sljit_s32 emit_fop_mem(struct sljit_compiler *compiler, sljit_s32 flags, sljit_s32 reg, sljit_s32 arg, sljit_sw argw)
{
	sljit_sw tmp;
	sljit_uw imm;
	sljit_sw inst = VSTR_F32 | (flags & (SLJIT_F32_OP | FPU_LOAD));

	SLJIT_ASSERT(arg & SLJIT_MEM);

	/* Fast loads and stores. */
	if (SLJIT_UNLIKELY(arg & OFFS_REG_MASK)) {
		FAIL_IF(push_inst32(compiler, ADD_W | RD4(TMP_REG2) | RN4(arg & REG_MASK) | RM4(OFFS_REG(arg)) | ((argw & 0x3) << 6)));
		arg = SLJIT_MEM | TMP_REG2;
		argw = 0;
	}

	if ((arg & REG_MASK) && (argw & 0x3) == 0) {
		if (!(argw & ~0x3fc))
			return push_inst32(compiler, inst | 0x800000 | RN4(arg & REG_MASK) | DD4(reg) | (argw >> 2));
		if (!(-argw & ~0x3fc))
			return push_inst32(compiler, inst | RN4(arg & REG_MASK) | DD4(reg) | (-argw >> 2));
	}

	/* Slow cases */
	SLJIT_ASSERT(!(arg & OFFS_REG_MASK));
	if (compiler->cache_arg == arg) {
		tmp = argw - compiler->cache_argw;
		if (!(tmp & ~0x3fc))
			return push_inst32(compiler, inst | 0x800000 | RN4(TMP_REG3) | DD4(reg) | (tmp >> 2));
		if (!(-tmp & ~0x3fc))
			return push_inst32(compiler, inst | RN4(TMP_REG3) | DD4(reg) | (-tmp >> 2));
		if (emit_set_delta(compiler, TMP_REG3, TMP_REG3, tmp) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			compiler->cache_argw = argw;
			return push_inst32(compiler, inst | 0x800000 | RN4(TMP_REG3) | DD4(reg));
		}
	}

	if (arg & REG_MASK) {
		if (emit_set_delta(compiler, TMP_REG1, arg & REG_MASK, argw) != SLJIT_ERR_UNSUPPORTED) {
			FAIL_IF(compiler->error);
			return push_inst32(compiler, inst | 0x800000 | RN4(TMP_REG1) | DD4(reg));
		}
		imm = get_imm(argw & ~0x3fc);
		if (imm != INVALID_IMM) {
			FAIL_IF(push_inst32(compiler, ADD_WI | RD4(TMP_REG1) | RN4(arg & REG_MASK) | imm));
			return push_inst32(compiler, inst | 0x800000 | RN4(TMP_REG1) | DD4(reg) | ((argw & 0x3fc) >> 2));
		}
		imm = get_imm(-argw & ~0x3fc);
		if (imm != INVALID_IMM) {
			argw = -argw;
			FAIL_IF(push_inst32(compiler, SUB_WI | RD4(TMP_REG1) | RN4(arg & REG_MASK) | imm));
			return push_inst32(compiler, inst | RN4(TMP_REG1) | DD4(reg) | ((argw & 0x3fc) >> 2));
		}
	}

	compiler->cache_arg = arg;
	compiler->cache_argw = argw;

	FAIL_IF(load_immediate(compiler, TMP_REG3, argw));
	if (arg & REG_MASK)
		FAIL_IF(push_inst16(compiler, ADD | SET_REGS44(TMP_REG3, (arg & REG_MASK))));
	return push_inst32(compiler, inst | 0x800000 | RN4(TMP_REG3) | DD4(reg));
}

static SLJIT_INLINE sljit_s32 sljit_emit_fop1_conv_sw_from_f64(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src, sljit_sw srcw)
{
	if (src & SLJIT_MEM) {
		FAIL_IF(emit_fop_mem(compiler, (op & SLJIT_F32_OP) | FPU_LOAD, TMP_FREG1, src, srcw));
		src = TMP_FREG1;
	}

	FAIL_IF(push_inst32(compiler, VCVT_S32_F32 | (op & SLJIT_F32_OP) | DD4(TMP_FREG1) | DM4(src)));

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	if (FAST_IS_REG(dst))
		return push_inst32(compiler, VMOV | (1 << 20) | RT4(dst) | DN4(TMP_FREG1));

	/* Store the integer value from a VFP register. */
	return emit_fop_mem(compiler, 0, TMP_FREG1, dst, dstw);
}

static SLJIT_INLINE sljit_s32 sljit_emit_fop1_conv_f64_from_sw(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src, sljit_sw srcw)
{
	sljit_s32 dst_r = FAST_IS_REG(dst) ? dst : TMP_FREG1;

	if (FAST_IS_REG(src))
		FAIL_IF(push_inst32(compiler, VMOV | RT4(src) | DN4(TMP_FREG1)));
	else if (src & SLJIT_MEM) {
		/* Load the integer value into a VFP register. */
		FAIL_IF(emit_fop_mem(compiler, FPU_LOAD, TMP_FREG1, src, srcw));
	}
	else {
		FAIL_IF(load_immediate(compiler, TMP_REG1, srcw));
		FAIL_IF(push_inst32(compiler, VMOV | RT4(TMP_REG1) | DN4(TMP_FREG1)));
	}

	FAIL_IF(push_inst32(compiler, VCVT_F32_S32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DM4(TMP_FREG1)));

	if (dst & SLJIT_MEM)
		return emit_fop_mem(compiler, (op & SLJIT_F32_OP), TMP_FREG1, dst, dstw);
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_s32 sljit_emit_fop1_cmp(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
	if (src1 & SLJIT_MEM) {
		emit_fop_mem(compiler, (op & SLJIT_F32_OP) | FPU_LOAD, TMP_FREG1, src1, src1w);
		src1 = TMP_FREG1;
	}

	if (src2 & SLJIT_MEM) {
		emit_fop_mem(compiler, (op & SLJIT_F32_OP) | FPU_LOAD, TMP_FREG2, src2, src2w);
		src2 = TMP_FREG2;
	}

	FAIL_IF(push_inst32(compiler, VCMP_F32 | (op & SLJIT_F32_OP) | DD4(src1) | DM4(src2)));
	return push_inst32(compiler, VMRS);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fop1(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src, sljit_sw srcw)
{
	sljit_s32 dst_r;

	CHECK_ERROR();
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	if (GET_OPCODE(op) != SLJIT_CONV_F64_FROM_F32)
		op ^= SLJIT_F32_OP;

	SLJIT_COMPILE_ASSERT((SLJIT_F32_OP == 0x100), float_transfer_bit_error);
	SELECT_FOP1_OPERATION_WITH_CHECKS(compiler, op, dst, dstw, src, srcw);

	dst_r = FAST_IS_REG(dst) ? dst : TMP_FREG1;

	if (src & SLJIT_MEM) {
		emit_fop_mem(compiler, (op & SLJIT_F32_OP) | FPU_LOAD, dst_r, src, srcw);
		src = dst_r;
	}

	switch (GET_OPCODE(op)) {
	case SLJIT_MOV_F64:
		if (src != dst_r) {
			if (dst_r != TMP_FREG1)
				FAIL_IF(push_inst32(compiler, VMOV_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DM4(src)));
			else
				dst_r = src;
		}
		break;
	case SLJIT_NEG_F64:
		FAIL_IF(push_inst32(compiler, VNEG_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DM4(src)));
		break;
	case SLJIT_ABS_F64:
		FAIL_IF(push_inst32(compiler, VABS_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DM4(src)));
		break;
	case SLJIT_CONV_F64_FROM_F32:
		FAIL_IF(push_inst32(compiler, VCVT_F64_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DM4(src)));
		op ^= SLJIT_F32_OP;
		break;
	}

	if (dst & SLJIT_MEM)
		return emit_fop_mem(compiler, (op & SLJIT_F32_OP), dst_r, dst, dstw);
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fop2(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
	sljit_s32 dst_r;

	CHECK_ERROR();
	CHECK(check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	op ^= SLJIT_F32_OP;

	dst_r = FAST_IS_REG(dst) ? dst : TMP_FREG1;
	if (src1 & SLJIT_MEM) {
		emit_fop_mem(compiler, (op & SLJIT_F32_OP) | FPU_LOAD, TMP_FREG1, src1, src1w);
		src1 = TMP_FREG1;
	}
	if (src2 & SLJIT_MEM) {
		emit_fop_mem(compiler, (op & SLJIT_F32_OP) | FPU_LOAD, TMP_FREG2, src2, src2w);
		src2 = TMP_FREG2;
	}

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD_F64:
		FAIL_IF(push_inst32(compiler, VADD_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	case SLJIT_SUB_F64:
		FAIL_IF(push_inst32(compiler, VSUB_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	case SLJIT_MUL_F64:
		FAIL_IF(push_inst32(compiler, VMUL_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	case SLJIT_DIV_F64:
		FAIL_IF(push_inst32(compiler, VDIV_F32 | (op & SLJIT_F32_OP) | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	}

	if (!(dst & SLJIT_MEM))
		return SLJIT_SUCCESS;
	return emit_fop_mem(compiler, (op & SLJIT_F32_OP), TMP_FREG1, dst, dstw);
}

#undef FPU_LOAD

/* --------------------------------------------------------------------- */
/*  Other instructions                                                   */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_s32 dst, sljit_sw dstw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_enter(compiler, dst, dstw));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	/* For UNUSED dst. Uncommon, but possible. */
	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	if (FAST_IS_REG(dst))
		return push_inst16(compiler, MOV | SET_REGS44(dst, TMP_REG3));

	/* Memory. */
	if (getput_arg_fast(compiler, WORD_SIZE | STORE, TMP_REG3, dst, dstw))
		return compiler->error;
	/* TMP_REG3 is used for caching. */
	FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(TMP_REG2, TMP_REG3)));
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	return getput_arg(compiler, WORD_SIZE | STORE, TMP_REG2, dst, dstw, 0, 0);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_return(compiler, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (FAST_IS_REG(src))
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(TMP_REG3, src)));
	else if (src & SLJIT_MEM) {
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG3, src, srcw))
			FAIL_IF(compiler->error);
		else {
			compiler->cache_arg = 0;
			compiler->cache_argw = 0;
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src, srcw, 0, 0));
			FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(TMP_REG3, TMP_REG2)));
		}
	}
	else if (src & SLJIT_IMM)
		FAIL_IF(load_immediate(compiler, TMP_REG3, srcw));
	return push_inst16(compiler, BLX | RN3(TMP_REG3));
}

/* --------------------------------------------------------------------- */
/*  Conditional instructions                                             */
/* --------------------------------------------------------------------- */

static sljit_uw get_cc(sljit_s32 type)
{
	switch (type) {
	case SLJIT_EQUAL:
	case SLJIT_MUL_NOT_OVERFLOW:
	case SLJIT_EQUAL_F64:
		return 0x0;

	case SLJIT_NOT_EQUAL:
	case SLJIT_MUL_OVERFLOW:
	case SLJIT_NOT_EQUAL_F64:
		return 0x1;

	case SLJIT_LESS:
	case SLJIT_LESS_F64:
		return 0x3;

	case SLJIT_GREATER_EQUAL:
	case SLJIT_GREATER_EQUAL_F64:
		return 0x2;

	case SLJIT_GREATER:
	case SLJIT_GREATER_F64:
		return 0x8;

	case SLJIT_LESS_EQUAL:
	case SLJIT_LESS_EQUAL_F64:
		return 0x9;

	case SLJIT_SIG_LESS:
		return 0xb;

	case SLJIT_SIG_GREATER_EQUAL:
		return 0xa;

	case SLJIT_SIG_GREATER:
		return 0xc;

	case SLJIT_SIG_LESS_EQUAL:
		return 0xd;

	case SLJIT_OVERFLOW:
	case SLJIT_UNORDERED_F64:
		return 0x6;

	case SLJIT_NOT_OVERFLOW:
	case SLJIT_ORDERED_F64:
		return 0x7;

	default: /* SLJIT_JUMP */
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

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_s32 type)
{
	struct sljit_jump *jump;
	sljit_ins cc;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_jump(compiler, type));

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);
	type &= 0xff;

	/* In ARM, we don't need to touch the arguments. */
	PTR_FAIL_IF(emit_imm32_const(compiler, TMP_REG1, 0));
	if (type < SLJIT_JUMP) {
		jump->flags |= IS_COND;
		cc = get_cc(type);
		jump->flags |= cc << 8;
		PTR_FAIL_IF(push_inst16(compiler, IT | (cc << 4) | 0x8));
	}

	jump->addr = compiler->size;
	if (type <= SLJIT_JUMP)
		PTR_FAIL_IF(push_inst16(compiler, BX | RN3(TMP_REG1)));
	else {
		jump->flags |= IS_BL;
		PTR_FAIL_IF(push_inst16(compiler, BLX | RN3(TMP_REG1)));
	}

	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_ijump(struct sljit_compiler *compiler, sljit_s32 type, sljit_s32 src, sljit_sw srcw)
{
	struct sljit_jump *jump;

	CHECK_ERROR();
	CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	/* In ARM, we don't need to touch the arguments. */
	if (!(src & SLJIT_IMM)) {
		if (FAST_IS_REG(src))
			return push_inst16(compiler, (type <= SLJIT_JUMP ? BX : BLX) | RN3(src));

		FAIL_IF(emit_op_mem(compiler, WORD_SIZE, type <= SLJIT_JUMP ? TMP_PC : TMP_REG1, src, srcw));
		if (type >= SLJIT_FAST_CALL)
			return push_inst16(compiler, BLX | RN3(TMP_REG1));
	}

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	FAIL_IF(!jump);
	set_jump(jump, compiler, JUMP_ADDR | ((type >= SLJIT_FAST_CALL) ? IS_BL : 0));
	jump->u.target = srcw;

	FAIL_IF(emit_imm32_const(compiler, TMP_REG1, 0));
	jump->addr = compiler->size;
	return push_inst16(compiler, (type <= SLJIT_JUMP ? BX : BLX) | RN3(TMP_REG1));
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src, sljit_sw srcw,
	sljit_s32 type)
{
	sljit_s32 dst_r, flags = GET_ALL_FLAGS(op);
	sljit_ins cc, ins;

	CHECK_ERROR();
	CHECK(check_sljit_emit_op_flags(compiler, op, dst, dstw, src, srcw, type));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	op = GET_OPCODE(op);
	cc = get_cc(type & 0xff);
	dst_r = FAST_IS_REG(dst) ? dst : TMP_REG2;

	if (op < SLJIT_ADD) {
		FAIL_IF(push_inst16(compiler, IT | (cc << 4) | (((cc & 0x1) ^ 0x1) << 3) | 0x4));
		if (reg_map[dst_r] > 7) {
			FAIL_IF(push_inst32(compiler, MOV_WI | RD4(dst_r) | 1));
			FAIL_IF(push_inst32(compiler, MOV_WI | RD4(dst_r) | 0));
		} else {
			FAIL_IF(push_inst16(compiler, MOVSI | RDN3(dst_r) | 1));
			FAIL_IF(push_inst16(compiler, MOVSI | RDN3(dst_r) | 0));
		}
		if (dst_r != TMP_REG2)
			return SLJIT_SUCCESS;
		return emit_op_mem(compiler, WORD_SIZE | STORE, TMP_REG2, dst, dstw);
	}

	ins = (op == SLJIT_AND ? ANDI : (op == SLJIT_OR ? ORRI : EORI));
	if ((op == SLJIT_OR || op == SLJIT_XOR) && FAST_IS_REG(dst) && dst == src) {
		/* Does not change the other bits. */
		FAIL_IF(push_inst16(compiler, IT | (cc << 4) | 0x8));
		FAIL_IF(push_inst32(compiler, ins | RN4(src) | RD4(dst) | 1));
		if (flags & SLJIT_SET_E) {
			/* The condition must always be set, even if the ORRI/EORI is not executed above. */
			if (reg_map[dst] <= 7)
				return push_inst16(compiler, MOVS | RD3(TMP_REG1) | RN3(dst));
			return push_inst32(compiler, MOV_W | SET_FLAGS | RD4(TMP_REG1) | RM4(dst));
		}
		return SLJIT_SUCCESS;
	}

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	if (src & SLJIT_MEM) {
		FAIL_IF(emit_op_mem2(compiler, WORD_SIZE, TMP_REG2, src, srcw, dst, dstw));
		src = TMP_REG2;
		srcw = 0;
	} else if (src & SLJIT_IMM) {
		FAIL_IF(load_immediate(compiler, TMP_REG2, srcw));
		src = TMP_REG2;
		srcw = 0;
	}

	if (op == SLJIT_AND || src != dst_r) {
		FAIL_IF(push_inst16(compiler, IT | (cc << 4) | (((cc & 0x1) ^ 0x1) << 3) | 0x4));
		FAIL_IF(push_inst32(compiler, ins | RN4(src) | RD4(dst_r) | 1));
		FAIL_IF(push_inst32(compiler, ins | RN4(src) | RD4(dst_r) | 0));
	}
	else {
		FAIL_IF(push_inst16(compiler, IT | (cc << 4) | 0x8));
		FAIL_IF(push_inst32(compiler, ins | RN4(src) | RD4(dst_r) | 1));
	}

	if (dst_r == TMP_REG2)
		FAIL_IF(emit_op_mem2(compiler, WORD_SIZE | STORE, TMP_REG2, dst, dstw, 0, 0));

	if (flags & SLJIT_SET_E) {
		/* The condition must always be set, even if the ORR/EORI is not executed above. */
		if (reg_map[dst_r] <= 7)
			return push_inst16(compiler, MOVS | RD3(TMP_REG1) | RN3(dst_r));
		return push_inst32(compiler, MOV_W | SET_FLAGS | RD4(TMP_REG1) | RM4(dst_r));
	}
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_s32 dst, sljit_sw dstw, sljit_sw init_value)
{
	struct sljit_const *const_;
	sljit_s32 dst_r;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_const(compiler, dst, dstw, init_value));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);
	set_const(const_, compiler);

	dst_r = SLOW_IS_REG(dst) ? dst : TMP_REG1;
	PTR_FAIL_IF(emit_imm32_const(compiler, dst_r, init_value));

	if (dst & SLJIT_MEM)
		PTR_FAIL_IF(emit_op_mem(compiler, WORD_SIZE | STORE, dst_r, dst, dstw));
	return const_;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_u16 *inst = (sljit_u16*)addr;
	modify_imm32_const(inst, new_addr);
	SLJIT_CACHE_FLUSH(inst, inst + 4);
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_const(sljit_uw addr, sljit_sw new_constant)
{
	sljit_u16 *inst = (sljit_u16*)addr;
	modify_imm32_const(inst, new_constant);
	SLJIT_CACHE_FLUSH(inst, inst + 4);
}
