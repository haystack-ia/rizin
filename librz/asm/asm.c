// SPDX-FileCopyrightText: 2009-2021 pancake <pancake@nopcode.org>
// SPDX-FileCopyrightText: 2009-2021 nibble <nibble.ds@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <stdio.h>
#include <rz_core.h>
#include <rz_types.h>
#include <rz_util.h>
#include <rz_asm.h>
#define USE_R2 1
#include <spp/spp.h>
#include <config.h>

RZ_LIB_VERSION(rz_asm);

static char *directives[] = {
	".include", ".error", ".warning",
	".echo", ".if", ".ifeq", ".endif",
	".else", ".set", ".get", NULL
};

static RzAsmPlugin *asm_static_plugins[] = { RZ_ASM_STATIC_PLUGINS };

static void parseHeap(RzParse *p, RzStrBuf *s) {
	char *op_buf_asm = rz_strbuf_get(s);
	size_t len = rz_strbuf_length(s);
	char *out = malloc(64 + (len * 2));
	if (out) {
		*out = 0;
		strcpy(out, op_buf_asm);
		// XXX we shouldn't pad here because we have t orefactor the RzParse API to handle boundaries and chunks properly
		rz_parse_parse(p, op_buf_asm, out);
		rz_strbuf_set(s, out);
		free(out);
	}
}

/* pseudo.c - private api */
//static int rz_asm_pseudo_align(RzAsmCode *acode, RzAsmOp *op, char *input) {
//	acode->code_align = rz_num_math(NULL, input);
//	return 0;
//}
//
//static int rz_asm_pseudo_string(RzAsmOp *op, char *input, int zero) {
//	int len = strlen(input) - 1;
//	if (len < 1) {
//		return 0;
//	}
//	// TODO: if not starting with '"'.. give up
//	if (input[len] == '"') {
//		input[len] = 0;
//	}
//	if (*input == '"') {
//		input++;
//	}
//	len = rz_str_unescape(input) + zero;
//	rz_strbuf_set(&op->buf, input); // uh?
//	return len;
//}

static inline int rz_asm_pseudo_arch(RzAsm *a, char *input) {
	if (!rz_asm_use(a, input)) {
		eprintf("Error: Unknown plugin\n");
		return -1;
	}
	return 0;
}

static inline int rz_asm_pseudo_bits(RzAsm *a, char *input) {
	if (!(rz_asm_set_bits(a, rz_num_math(NULL, input)))) {
		eprintf("Error: Unsupported value for .bits.\n");
		return -1;
	}
	return 0;
}

static inline int rz_asm_pseudo_org(RzAsm *a, char *input) {
	rz_asm_set_pc(a, rz_num_math(NULL, input));
	return 0;
}

static inline int rz_asm_pseudo_intN(RzAsm *a, RzAsmOp *op, char *input, int n) {
	short s;
	int i;
	long int l;
	ut64 s64 = rz_num_math(NULL, input);
	if (n != 8 && s64 >> (n * 8)) {
		eprintf("int16 Out is out of range\n");
		return 0;
	}
	// XXX honor endian here
	ut8 *buf = (ut8 *)rz_strbuf_get(&op->buf);
	if (!buf) {
		return 0;
	}
	if (n == 2) {
		s = (short)s64;
		rz_write_ble16(buf, s, a->big_endian);
	} else if (n == 4) {
		i = (int)s64;
		rz_write_ble32(buf, i, a->big_endian);
	} else if (n == 8) {
		l = (long int)s64;
		rz_write_ble64(buf, l, a->big_endian);
	} else {
		return 0;
	}
	return n;
}

static inline int rz_asm_pseudo_int16(RzAsm *a, RzAsmOp *op, char *input) {
	return rz_asm_pseudo_intN(a, op, input, 2);
}

static inline int rz_asm_pseudo_int32(RzAsm *a, RzAsmOp *op, char *input) {
	return rz_asm_pseudo_intN(a, op, input, 4);
}

static inline int rz_asm_pseudo_int64(RzAsm *a, RzAsmOp *op, char *input) {
	return rz_asm_pseudo_intN(a, op, input, 8);
}

static inline int rz_asm_pseudo_byte(RzAsmOp *op, char *input) {
	int i, len = 0;
	rz_str_replace_char(input, ',', ' ');
	len = rz_str_word_count(input);
	rz_str_word_set0(input);
	ut8 *buf = malloc(len);
	if (!buf) {
		return 0;
	}
	for (i = 0; i < len; i++) {
		const char *word = rz_str_word_get0(input, i);
		int num = (int)rz_num_math(NULL, word);
		buf[i] = num;
	}
	rz_asm_op_set_buf(op, buf, len);
	free(buf);
	return len;
}

static inline int rz_asm_pseudo_fill(RzAsmOp *op, char *input) {
	int i, repeat = 0, size = 0, value = 0;
	sscanf(input, "%d,%d,%d", &repeat, &size, &value); // use rz_num?
	size *= (sizeof(value) * repeat);
	if (size > 0) {
		ut8 *buf = malloc(size);
		if (buf) {
			for (i = 0; i < size; i += sizeof(value)) {
				memcpy(&buf[i], &value, sizeof(value));
			}
			rz_asm_op_set_buf(op, buf, size);
			free(buf);
		}
	} else {
		size = 0;
	}
	return size;
}

static inline int rz_asm_pseudo_incbin(RzAsmOp *op, char *input) {
	size_t bytes_read = 0;
	rz_str_replace_char(input, ',', ' ');
	// int len = rz_str_word_count (input);
	rz_str_word_set0(input);
	//const char *filename = rz_str_word_get0 (input, 0);
	size_t skip = (size_t)rz_num_math(NULL, rz_str_word_get0(input, 1));
	size_t count = (size_t)rz_num_math(NULL, rz_str_word_get0(input, 2));
	char *content = rz_file_slurp(input, &bytes_read);
	if (!content) {
		eprintf("Could not open '%s'.\n", input);
		return -1;
	}
	if (skip > 0) {
		skip = skip > bytes_read ? bytes_read : skip;
	}
	if (count > 0) {
		count = count > bytes_read ? 0 : count;
	} else {
		count = bytes_read;
	}
	// Need to handle arbitrary amount of data
	rz_buf_free(op->buf_inc);
	op->buf_inc = rz_buf_new_with_string(content + skip);
	// Terminate the original buffer
	free(content);
	return count;
}

static void plugin_fini(RzAsm *a) {
	if (a->cur && a->cur->fini && !a->cur->fini(a->plugin_data)) {
		RZ_LOG_ERROR("asm plugin '%s' failed to terminate.\n", a->cur->name);
	}
}

RZ_API RzAsm *rz_asm_new(void) {
	int i;
	RzAsm *a = RZ_NEW0(RzAsm);
	if (!a) {
		return NULL;
	}
	a->dataalign = 1;
	a->bits = RZ_SYS_BITS;
	a->bitshift = 0;
	a->syntax = RZ_ASM_SYNTAX_INTEL;
	a->plugins = rz_list_newf(NULL);
	if (!a->plugins) {
		free(a);
		return NULL;
	}
	for (i = 0; asm_static_plugins[i]; i++) {
		rz_asm_add(a, asm_static_plugins[i]);
	}
	return a;
}

RZ_API bool rz_asm_setup(RzAsm *a, const char *arch, int bits, int big_endian) {
	rz_return_val_if_fail(a && arch, false);
	bool ret = !rz_asm_use(a, arch);
	return ret | !rz_asm_set_bits(a, bits);
}

// TODO: spagueti
RZ_API int rz_asm_sub_names_input(RzAsm *a, const char *f) {
	rz_return_val_if_fail(a && f, false);
	if (!a->ifilter) {
		a->ifilter = rz_parse_new();
	}
	if (!rz_parse_use(a->ifilter, f)) {
		rz_parse_free(a->ifilter);
		a->ifilter = NULL;
		return false;
	}
	return true;
}

RZ_API int rz_asm_sub_names_output(RzAsm *a, const char *f) {
	if (!a->ofilter) {
		a->ofilter = rz_parse_new();
	}
	if (!rz_parse_use(a->ofilter, f)) {
		rz_parse_free(a->ofilter);
		a->ofilter = NULL;
		return false;
	}
	return true;
}

RZ_API void rz_asm_free(RzAsm *a) {
	if (!a) {
		return;
	}
	plugin_fini(a);
	if (a->plugins) {
		rz_list_free(a->plugins);
		a->plugins = NULL;
	}
	rz_syscall_free(a->syscall);
	free(a->cpu);
	sdb_free(a->pair);
	ht_pp_free(a->flags);
	a->pair = NULL;
	free(a);
}

RZ_API bool rz_asm_add(RzAsm *a, RzAsmPlugin *p) {
	if (!p->name) {
		return false;
	}
	if (rz_asm_is_valid(a, p->name)) {
		return false;
	}
	rz_list_append(a->plugins, p);
	return true;
}

RZ_API int rz_asm_del(RzAsm *a, const char *name) {
	/* TODO: Implement rz_asm_del */
	return false;
}

RZ_API bool rz_asm_is_valid(RzAsm *a, const char *name) {
	RzAsmPlugin *h;
	RzListIter *iter;
	if (!name || !*name) {
		return false;
	}
	rz_list_foreach (a->plugins, iter, h) {
		if (!strcmp(h->name, name)) {
			return true;
		}
	}
	return false;
}

RZ_API bool rz_asm_use_assembler(RzAsm *a, const char *name) {
	RzAsmPlugin *h;
	RzListIter *iter;
	if (a) {
		if (name && *name) {
			rz_list_foreach (a->plugins, iter, h) {
				if (h->assemble && !strcmp(h->name, name)) {
					a->acur = h;
					return true;
				}
			}
		}
		a->acur = NULL;
	}
	return false;
}

// TODO: this can be optimized using rz_str_hash()
RZ_API bool rz_asm_use(RzAsm *a, const char *name) {
	RzAsmPlugin *h;
	RzListIter *iter;
	if (!a || !name) {
		return false;
	}
	if (a->cur && !strcmp(a->cur->arch, name)) {
		return true;
	}
	rz_list_foreach (a->plugins, iter, h) {
		if (h->arch && h->name && !strcmp(h->name, name)) {
			if (!a->cur || (a->cur && strcmp(a->cur->arch, h->arch))) {
				plugin_fini(a);
				char *rzprefix = rz_str_rz_prefix(RZ_SDB_OPCODES);
				char *file = rz_str_newf("%s/%s.sdb", rz_str_get_null(rzprefix), h->arch);
				if (file) {
					rz_asm_set_cpu(a, NULL);
					sdb_free(a->pair);
					a->pair = sdb_new(NULL, file, 0);
					free(file);
				}
				free(rzprefix);
			}
			if (h->init && !h->init(&a->plugin_data)) {
				RZ_LOG_ERROR("asm plugin '%s' failed to initialize.\n", h->name);
				return false;
			}
			a->cur = h;
			return true;
		}
	}
	sdb_free(a->pair);
	a->pair = NULL;
	return false;
}

RZ_DEPRECATE RZ_API void rz_asm_set_cpu(RzAsm *a, const char *cpu) {
	if (a) {
		free(a->cpu);
		a->cpu = cpu ? strdup(cpu) : NULL;
	}
}

static bool has_bits(RzAsmPlugin *h, int bits) {
	return (h && h->bits && (bits & h->bits));
}

RZ_DEPRECATE RZ_API int rz_asm_set_bits(RzAsm *a, int bits) {
	if (has_bits(a->cur, bits)) {
		a->bits = bits; // TODO : use OR? :)
		return true;
	}
	return false;
}

RZ_API bool rz_asm_set_big_endian(RzAsm *a, bool b) {
	rz_return_val_if_fail(a && a->cur, false);
	a->big_endian = false; // little endian by default
	switch (a->cur->endian) {
	case RZ_SYS_ENDIAN_NONE:
	case RZ_SYS_ENDIAN_BI:
		// TODO: not yet implemented
		a->big_endian = b;
		break;
	case RZ_SYS_ENDIAN_LITTLE:
		a->big_endian = false;
		break;
	case RZ_SYS_ENDIAN_BIG:
		a->big_endian = true;
		break;
	default:
		eprintf("RzAsmPlugin doesn't specify endianness\n");
		break;
	}
	return a->big_endian;
}

RZ_API bool rz_asm_set_syntax(RzAsm *a, int syntax) {
	// TODO: move into rz_arch ?
	switch (syntax) {
	case RZ_ASM_SYNTAX_REGNUM:
	case RZ_ASM_SYNTAX_INTEL:
	case RZ_ASM_SYNTAX_MASM:
	case RZ_ASM_SYNTAX_ATT:
	case RZ_ASM_SYNTAX_JZ:
		a->syntax = syntax;
		return true;
	default:
		return false;
	}
}

RZ_API int rz_asm_set_pc(RzAsm *a, ut64 pc) {
	a->pc = pc;
	return true;
}

static bool __isInvalid(RzAsmOp *op) {
	const char *buf_asm = rz_strbuf_get(&op->buf_asm);
	return (buf_asm && *buf_asm && !strcmp(buf_asm, "invalid"));
}

RZ_API int rz_asm_disassemble(RzAsm *a, RzAsmOp *op, const ut8 *buf, int len) {
	rz_asm_op_init(op);
	rz_return_val_if_fail(a && buf && op, -1);
	if (len < 1) {
		return 0;
	}

	int ret = op->payload = 0;
	op->size = 4;
	op->bitsize = 0;
	rz_asm_op_set_asm(op, "");
	if (a->pcalign) {
		const int mod = a->pc % a->pcalign;
		if (mod) {
			op->size = a->pcalign - mod;
			rz_strbuf_set(&op->buf_asm, "unaligned");
			return -1;
		}
	}
	if (a->cur && a->cur->disassemble) {
		// shift buf N bits
		if (a->bitshift > 0) {
			ut8 *tmp = calloc(len, 1);
			if (tmp) {
				rz_mem_copybits_delta(tmp, 0, buf, a->bitshift, (len * 8) - a->bitshift);
				ret = a->cur->disassemble(a, op, tmp, len);
				free(tmp);
			}
		} else {
			ret = a->cur->disassemble(a, op, buf, len);
		}
	}
	if (ret < 0) {
		ret = 0;
	}
	if (op->bitsize > 0) {
		op->size = op->bitsize / 8;
		a->bitshift += op->bitsize % 8;
		int count = a->bitshift / 8;
		if (count > 0) {
			op->size = op->size + count;
			a->bitshift %= 8;
		}
	}

	if (op->size < 1 || __isInvalid(op)) {
		if (a->invhex) {
			if (a->bits == 16) {
				ut16 b = rz_read_le16(buf);
				rz_strbuf_set(&op->buf_asm, sdb_fmt(".word 0x%04x", b));
			} else {
				ut32 b = rz_read_le32(buf);
				rz_strbuf_set(&op->buf_asm, sdb_fmt(".dword 0x%08x", b));
			}
			// TODO: something for 64bits too?
		} else {
			rz_strbuf_set(&op->buf_asm, "invalid");
		}
	}
	if (a->ofilter) {
		parseHeap(a->ofilter, &op->buf_asm);
	}
	int opsz = (op->size > 0) ? RZ_MAX(0, RZ_MIN(len, op->size)) : 1;
	rz_asm_op_set_buf(op, buf, opsz);
	return ret;
}

typedef int (*Ase)(RzAsm *a, RzAsmOp *op, const char *buf);

static bool assemblerMatches(RzAsm *a, RzAsmPlugin *h) {
	if (!a || !h->arch || !h->assemble || !has_bits(h, a->bits)) {
		return false;
	}
	return (!strncmp(a->cur->arch, h->arch, strlen(a->cur->arch)));
}

static Ase findAssembler(RzAsm *a, const char *kw) {
	Ase ase = NULL;
	RzAsmPlugin *h;
	RzListIter *iter;
	if (a->acur && a->acur->assemble) {
		return a->acur->assemble;
	}
	rz_list_foreach (a->plugins, iter, h) {
		if (assemblerMatches(a, h)) {
			if (kw) {
				if (strstr(h->name, kw)) {
					return h->assemble;
				}
			} else {
				ase = h->assemble;
			}
		}
	}
	return ase;
}

RZ_API void rz_asm_list_directives(void) {
	int i = 0;
	char *dir = directives[i++];
	while (dir) {
		printf("%s\n", dir);
		dir = directives[i++];
	}
}

// returns instruction size
RZ_API int rz_asm_assemble(RzAsm *a, RzAsmOp *op, const char *buf) {
	rz_return_val_if_fail(a && op && buf, 0);
	int ret = 0;
	char *b = strdup(buf);
	if (!b) {
		return 0;
	}
	if (a->ifilter) {
		rz_parse_parse(a->ifilter, buf, b);
	}
	rz_str_case(b, 0); // to-lower
	memset(op, 0, sizeof(RzAsmOp));
	if (a->cur) {
		Ase ase = NULL;
		if (!a->cur->assemble) {
			/* find callback if no assembler support in current plugin */
			ase = findAssembler(a, ".ks");
			if (!ase) {
				ase = findAssembler(a, ".nz");
				if (!ase) {
					ase = findAssembler(a, NULL);
				}
			}
		} else {
			ase = a->cur->assemble;
		}
		if (ase) {
			ret = ase(a, op, b);
		}
	}
	// XXX delete this block, the ase thing should be setting asm, buf and hex
	if (op && ret > 0) {
		op->size = ret; // XXX shouldn't be necessary
		rz_asm_op_set_asm(op, b); // XXX ase should be updating this already, isn't?
		ut8 *opbuf = (ut8 *)rz_strbuf_get(&op->buf);
		rz_asm_op_set_buf(op, opbuf, ret);
	}
	free(b);
	return ret;
}

// TODO: Use RzStrBuf api here pls
RZ_API RzAsmCode *rz_asm_mdisassemble(RzAsm *a, const ut8 *buf, int len) {
	rz_return_val_if_fail(a && buf && len >= 0, NULL);

	RzStrBuf *buf_asm;
	RzAsmCode *acode;
	ut64 pc = a->pc;
	RzAsmOp op;
	ut64 idx;
	size_t ret;
	const size_t addrbytes = a->core ? ((RzCore *)a->core)->io->addrbytes : 1;

	if (!(acode = rz_asm_code_new())) {
		return NULL;
	}
	if (!(acode->bytes = malloc(1 + len))) {
		return rz_asm_code_free(acode);
	}
	memcpy(acode->bytes, buf, len);
	if (!(buf_asm = rz_strbuf_new(NULL))) {
		return rz_asm_code_free(acode);
	}
	for (idx = 0; idx + addrbytes <= len; idx += (addrbytes * ret)) {
		rz_asm_set_pc(a, pc + idx);
		ret = rz_asm_disassemble(a, &op, buf + idx, len - idx);
		if (ret < 1) {
			ret = 1;
		}
		if (a->ofilter) {
			parseHeap(a->ofilter, &op.buf_asm);
		}
		rz_strbuf_append(buf_asm, rz_strbuf_get(&op.buf_asm));
		rz_strbuf_append(buf_asm, "\n");
	}
	acode->assembly = rz_strbuf_drain(buf_asm);
	acode->len = idx;
	return acode;
}

RZ_API RzAsmCode *rz_asm_mdisassemble_hexstr(RzAsm *a, RzParse *p, const char *hexstr) {
	ut8 *buf = malloc(strlen(hexstr) + 1);
	if (!buf) {
		return NULL;
	}
	int len = rz_hex_str2bin(hexstr, buf);
	if (len < 1) {
		free(buf);
		return NULL;
	}
	RzAsmCode *ret = rz_asm_mdisassemble(a, buf, (ut64)len);
	if (ret && p) {
		// XXX this can crash
		rz_parse_parse(p, ret->assembly, ret->assembly);
	}
	free(buf);
	return ret;
}

//static void __flag_free_kv(HtPPKv *kv) {
//	free(kv->key);
//	free(kv->value);
//}

//static void *__dup_val(const void *v) {
//	return (void *)strdup((char *)v);
//}

#include "assembler.c"

RZ_API bool rz_asm_modify(RzAsm *a, ut8 *buf, int field, ut64 val) {
	return (a->cur && a->cur->modify) ? a->cur->modify(a, buf, field, val) : false;
}

RZ_API int rz_asm_get_offset(RzAsm *a, int type, int idx) { // link to rbin
	if (a && a->binb.bin && a->binb.get_offset) {
		return a->binb.get_offset(a->binb.bin, type, idx);
	}
	return -1;
}

RZ_API char *rz_asm_describe(RzAsm *a, const char *str) {
	return (a && a->pair) ? sdb_get(a->pair, str, 0) : NULL;
}

RZ_API RzList *rz_asm_get_plugins(RzAsm *a) {
	return a->plugins;
}

RZ_API bool rz_asm_set_arch(RzAsm *a, const char *name, int bits) {
	return rz_asm_use(a, name) ? rz_asm_set_bits(a, bits) : false;
}

/* to ease the use of the native bindings (not used in rizin) */
RZ_API char *rz_asm_to_string(RzAsm *a, ut64 addr, const ut8 *b, int l) {
	rz_return_val_if_fail(a && b && l >= 0, NULL);
	rz_asm_set_pc(a, addr);
	RzAsmCode *code = rz_asm_mdisassemble(a, b, l);
	if (code) {
		char *buf_asm = code->assembly;
		code->assembly = NULL;
		rz_asm_code_free(code);
		return buf_asm;
	}
	return NULL;
}

RZ_API ut8 *rz_asm_from_string(RzAsm *a, ut64 addr, const char *b, int *l) {
	rz_asm_set_pc(a, addr);
	RzAsmCode *code = rz_asm_massemble(a, b);
	if (code) {
		ut8 *buf = code->bytes;
		if (l) {
			*l = code->len;
		}
		rz_asm_code_free(code);
		return buf;
	}
	return NULL;
}

RZ_API int rz_asm_syntax_from_string(const char *name) {
	rz_return_val_if_fail(name, -1);
	if (!strcmp(name, "regnum")) {
		return RZ_ASM_SYNTAX_REGNUM;
	}
	if (!strcmp(name, "jz")) {
		return RZ_ASM_SYNTAX_JZ;
	}
	if (!strcmp(name, "intel")) {
		return RZ_ASM_SYNTAX_INTEL;
	}
	if (!strcmp(name, "masm")) {
		return RZ_ASM_SYNTAX_MASM;
	}
	if (!strcmp(name, "att")) {
		return RZ_ASM_SYNTAX_ATT;
	}
	return -1;
}

RZ_API char *rz_asm_mnemonics(RzAsm *a, int id, bool json) {
	rz_return_val_if_fail(a && a->cur, NULL);
	if (a->cur->mnemonics) {
		return a->cur->mnemonics(a, id, json);
	}
	return NULL;
}

RZ_API int rz_asm_mnemonics_byname(RzAsm *a, const char *name) {
	rz_return_val_if_fail(a && a->cur, 0);
	if (a->cur->mnemonics) {
		int i;
		for (i = 0; i < 1024; i++) {
			char *n = a->cur->mnemonics(a, i, false);
			if (n && !strcmp(n, name)) {
				return i;
			}
			free(n);
		}
	}
	return 0;
}

RZ_API RzAsmCode *rz_asm_rasm_assemble(RzAsm *a, const char *buf, bool use_spp) {
	rz_return_val_if_fail(a && buf, NULL);
	//if (use_spp) {
	//	Output out;
	//	out.fout = NULL;
	//	out.cout = rz_strbuf_new("");
	//	rz_strbuf_init(out.cout);
	//	struct Proc proc;
	//	spp_proc_set(&proc, "spp", 1);
	//	lbuf = replace_directives(lbuf);
	//	spp_eval(lbuf, &out);
	//	free(lbuf);
	//	lbuf = strdup(rz_strbuf_get(out.cout));
	//}
	return rz_asm_massemble(a, buf);
}
