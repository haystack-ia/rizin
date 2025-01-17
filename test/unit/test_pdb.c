// SPDX-FileCopyrightText: 2020 HoundThe <cgkajm@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_util.h>
#include "minunit.h"
#include <rz_bin.h>
#include <rz_core.h>
#include <rz_bin_dwarf.h>
#include "../../librz/bin/pdb/types.h"
#include "../../librz/bin/pdb/tpi.h"
#include "test_types.h"

#define MODE 2

// copy from cbin.c modified to get pdb back
int pdb_info(const char *file, RzPdb *pdb) {
	pdb->cb_printf = rz_cons_printf;
	if (!init_pdb_parser(pdb, file)) {
		return false;
	}
	if (!pdb->pdb_parse(pdb)) {
		eprintf("pdb was not parsed\n");
		pdb->finish_pdb_parse(pdb);
		return false;
	}
	return true;
}

int pdb_info_save_types(RzAnalysis *analysis, const char *file, RzPdb *pdb) {
	pdb->cb_printf = rz_cons_printf;
	if (!init_pdb_parser(pdb, file)) {
		return false;
	}
	if (!pdb->pdb_parse(pdb)) {
		eprintf("pdb was not parsed\n");
		pdb->finish_pdb_parse(pdb);
		return false;
	}
	rz_parse_pdb_types(analysis->typedb, pdb);
	pdb->finish_pdb_parse(pdb);
	return true;
}

bool test_pdb_tpi_cpp(void) {
	RzPdb pdb = RZ_EMPTY;
	mu_assert_true(pdb_info("bins/pdb/Project1.pdb", &pdb), "pdb parsing failed");

	RzList *plist = pdb.pdb_streams;
	mu_assert_notnull(plist, "PDB streams is NULL");

	mu_assert_eq(pdb.root_stream->num_streams, 50, "Incorrect number of streams");

	STpiStream *tpi_stream = rz_list_get_n(plist, ePDB_STREAM_TPI);
	mu_assert_notnull(tpi_stream, "TPIs stream not found in current PDB");
	mu_assert_eq(tpi_stream->header.hdr_size + tpi_stream->header.follow_size, 117156, "Wrong TPI size");
	mu_assert_eq(tpi_stream->header.idx_begin, 0x1000, "Wrong beginning index");

	// tpi_stream->header.
	mu_assert_eq(tpi_stream->types->length, 1148, "Incorrect number of types");
	RzListIter *it = rz_list_iterator(tpi_stream->types);
	SType *type;
	while (rz_list_iter_next(it)) {
		type = rz_list_iter_get(it);
		STypeInfo *type_info = &type->type_data;
		if (type->tpi_idx == 0x1028) {
			mu_assert_eq(type_info->leaf_type, eLF_PROCEDURE, "Incorrect data type");
			SType *arglist;
			arglist = rz_bin_pdb_stype_by_index(((SLF_PROCEDURE *)(type_info->type_info))->arg_list);
			mu_assert_eq(arglist->tpi_idx, 0x1027, "Wrong type index");
			SType *return_type;
			return_type = rz_bin_pdb_stype_by_index(((SLF_PROCEDURE *)(type_info->type_info))->return_type);
			mu_assert_eq(return_type->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = return_type->type_data.type_info;
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "int32_t", "Incorrect return type");
		} else if (type->tpi_idx == 0x1161) {
			mu_assert_eq(type_info->leaf_type, eLF_POINTER, "Incorrect data type");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "struct _RTC_framedesc*", "Wrong pointer print type");
		} else if (type->tpi_idx == 0x1004) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE, "Incorrect data type");
			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_true(forward_ref, "Wrong fwdref");
		} else if (type->tpi_idx == 0x113F) {
			mu_assert_eq(type_info->leaf_type, eLF_ARRAY, "Incorrect data type");
			char *type;
			type_info->get_print_type(type_info, &type);
			SType *dump;
			dump = rz_bin_pdb_stype_by_index(((SLF_ARRAY *)(type_info->type_info))->index_type);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_ULONG, "Incorrect return type");
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "uint32_t", "Incorrect return type");
			dump = rz_bin_pdb_stype_by_index(((SLF_ARRAY *)(type_info->type_info))->element_type);
			mu_assert_eq(dump->tpi_idx, 0x113E, "Wrong element type index");
			ut64 size = type_info->get_val(type_info);
			mu_assert_eq(size, 20, "Wrong array size");
		} else if (type->tpi_idx == 0x145A) {
			mu_assert_eq(type_info->leaf_type, eLF_ENUM, "Incorrect data type");
			SType *dump;
			RzList *members;
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "EXCEPTION_DEBUGGER_ENUM", "wrong enum name");
			dump = rz_bin_pdb_stype_by_index(((SLF_ENUM *)(type_info->type_info))->utype);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_INT4, "Incorrect return type");
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "int32_t", "Incorrect return type");
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 6, "wrong enum members length");
		} else if (type->tpi_idx == 0x1414) {
			mu_assert_eq(type_info->leaf_type, eLF_VTSHAPE, "Incorrect data type");
		} else if (type->tpi_idx == 0x1421) {
			mu_assert_eq(type_info->leaf_type, eLF_MODIFIER, "Incorrect data type");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_MODIFIER *)(type_info->type_info))->modified_type);
			mu_assert_eq(stype->tpi_idx, 0x120F, "Incorrect modified type");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "const struct Stream", "Incorrect modifier print type");
		} else if (type->tpi_idx == 0x1003) {
			mu_assert_eq(type_info->leaf_type, eLF_UNION, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "R2_TEST_UNION", "wrong union name");
			RzList *members;
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 2, "wrong union member count");
		} else if (type->tpi_idx == 0x100B) {
			mu_assert_eq(type_info->leaf_type, eLF_CLASS, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "TEST_CLASS", "wrong class name");
			RzList *members;
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 2, "wrong class member count");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->vshape);
			mu_assert_null(stype, "wrong class vshape");
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->derived);
			mu_assert_null(stype, "wrong class derived");
			//} else if (type->tpi_idx == 0x1062) {
			//	mu_assert_eq(type_info->leaf_type, eLF_BITFIELD, "Incorrect data type");
			//	SType *base_type = NULL;
			//	type_info->get_base_type(type_info, (void **)&base_type);
			//	char *type;
			//	type_info->get_print_type(type_info, &type);
			//	mu_assert_streq(type, "bitfield uint32_t : 1", "Incorrect bitfield print type");
		} else if (type->tpi_idx == 0x1258) {
			mu_assert_eq(type_info->leaf_type, eLF_METHODLIST, "Incorrect data type");
			// Nothing from methodlist is currently being parsed
		} else if (type->tpi_idx == 0x107A) {
			mu_assert_eq(type_info->leaf_type, eLF_MFUNCTION, "Incorrect data type");
			SType *type;
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->return_type);
			mu_assert_eq(type->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = type->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_BOOL08, "Incorrect return type");
			mu_assert_eq(simple_type->size, 1, "Incorrect return type");
			mu_assert_streq(simple_type->type, "bool", "Incorrect return type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->class_type);
			mu_assert_eq(type->tpi_idx, 0x1079, "incorrect mfunction class type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->arglist);
			mu_assert_eq(type->tpi_idx, 0x1027, "incorrect mfunction arglist");
		} else if (type->tpi_idx == 0x113F) {
			mu_assert_eq(type_info->leaf_type, eLF_FIELDLIST, "Incorrect data type");
			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 2725, "Incorrect members length");
			RzListIter *it = rz_list_iterator(members);
			int i = 0;
			while (rz_list_iter_next(it)) {
				STypeInfo *type_info = (STypeInfo *)rz_list_iter_get(it);
				mu_assert_eq(type_info->leaf_type, eLF_ENUMERATE, "Incorrect data type");
				if (i == 0) {
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "CV_ALLREG_ERR", "Wrong enum name");
					ut64 value = type_info->get_val(type_info);

					mu_assert_eq(value, 30000, "Wrong enumerate value");
				}
				if (i == 2724) {
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "CV_AMD64_YMM15D3", "Wrong enum name");
					ut64 value = type_info->get_val(type_info);

					mu_assert_eq(value, 687, "Wrong enumerate value");
				}
				i++;
			}
		} else if (type->tpi_idx == 0x1231) {
			mu_assert_eq(type_info->leaf_type, eLF_ARGLIST, "Incorrect data type");
		} else if (type->tpi_idx == 0x101A) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE, "Incorrect data type");
			char *name;
			name = type_info->get_name(&type->type_data);
			mu_assert_streq(name, "threadlocaleinfostruct", "Wrong name");
			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_false(forward_ref, "Wrong fwdref");
			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 18, "Incorrect members count");
			RzListIter *it = rz_list_iterator(members);
			int i = 0;
			while (rz_list_iter_next(it)) {
				STypeInfo *type_info = (STypeInfo *)rz_list_iter_get(it);
				if (i == 0) {
					mu_assert_eq(type_info->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "refcount", "Wrong member name");
					char *type;
					type_info->get_print_type(type_info, &type);
					mu_assert_streq(type, "int32_t", "Wrong member type");
				}
				if (i == 1) {
					mu_assert_eq(type_info->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "lc_codepage", "Wrong member name");
					char *type;
					type_info->get_print_type(type_info, &type);
					mu_assert_streq(type, "uint32_t", "Wrong member type");
				}
				if (i == 17) {
					mu_assert_eq(type_info->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "locale_name", "Wrong method name");
					char *type;
					type_info->get_print_type(type_info, &type);
					mu_assert_streq(type, "wchar_t *[24]", "Wrong method type");
				}
				i++;
			}
		}
	};
	pdb.finish_pdb_parse(&pdb);
	mu_end;
}

bool test_pdb_tpi_rust(void) {
	RzPdb pdb = RZ_EMPTY;
	mu_assert_true(pdb_info("bins/pdb/ghidra_rust_pdb_bug.pdb", &pdb), "pdb parsing failed");

	RzList *plist = pdb.pdb_streams;
	mu_assert_notnull(plist, "PDB streams is NULL");

	mu_assert_eq(pdb.root_stream->num_streams, 88, "Incorrect number of streams");

	STpiStream *tpi_stream = rz_list_get_n(plist, ePDB_STREAM_TPI);
	mu_assert_notnull(tpi_stream, "TPIs stream not found in current PDB");
	mu_assert_eq(tpi_stream->header.hdr_size + tpi_stream->header.follow_size, 305632, "Wrong TPI size");
	mu_assert_eq(tpi_stream->header.idx_begin, 0x1000, "Wrong beginning index");

	// tpi_stream->header.
	mu_assert_eq(tpi_stream->types->length, 4031, "Incorrect number of types");
	RzListIter *it = rz_list_iterator(tpi_stream->types);
	SType *type;
	while (rz_list_iter_next(it)) {
		type = rz_list_iter_get(it);
		STypeInfo *type_info = &type->type_data;
		if (type->tpi_idx == 0x101B) {
			mu_assert_eq(type_info->leaf_type, eLF_PROCEDURE, "Incorrect data type");
			SType *arglist;
			arglist = rz_bin_pdb_stype_by_index(((SLF_PROCEDURE *)(type_info->type_info))->arg_list);
			mu_assert_eq(arglist->tpi_idx, 0x101A, "Wrong type index");
			SType *return_type;
			return_type = rz_bin_pdb_stype_by_index(((SLF_PROCEDURE *)(type_info->type_info))->return_type);
			mu_assert_eq(return_type->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = return_type->type_data.type_info;
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "int32_t", "Incorrect return type");
		} else if (type->tpi_idx == 0x1163) {
			mu_assert_eq(type_info->leaf_type, eLF_POINTER, "Incorrect data type");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "struct core::fmt::Void*", "Wrong pointer print type");
		} else if (type->tpi_idx == 0x1005) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE, "Incorrect data type");
			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_true(forward_ref, "Wrong fwdref");
		} else if (type->tpi_idx == 0x114A) {
			mu_assert_eq(type_info->leaf_type, eLF_ARRAY, "Incorrect data type");
			SType *dump;
			dump = rz_bin_pdb_stype_by_index(((SLF_ARRAY *)(type_info->type_info))->index_type);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_UQUAD, "Incorrect return type");
			mu_assert_eq(simple_type->size, 8, "Incorrect return type");
			mu_assert_streq(simple_type->type, "uint64_t", "Incorrect return type");
			dump = rz_bin_pdb_stype_by_index(((SLF_ARRAY *)(type_info->type_info))->element_type);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_UCHAR, "Incorrect return type");
			mu_assert_eq(simple_type->size, 1, "Incorrect return type");
			mu_assert_streq(simple_type->type, "uint8_t", "Incorrect return type");

			ut64 size = type_info->get_val(type_info);
			mu_assert_eq(size, 16, "Wrong array size");
		} else if (type->tpi_idx == 0x1FB4) {
			mu_assert_eq(type_info->leaf_type, eLF_ENUM, "Incorrect data type");
			SType *dump;
			RzList *members;
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "ISA_AVAILABILITY", "wrong enum name");
			dump = rz_bin_pdb_stype_by_index(((SLF_ENUM *)(type_info->type_info))->utype);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_INT4, "Incorrect return type");
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "int32_t", "Incorrect return type");
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 10, "wrong enum members length");
		} else if (type->tpi_idx == 0x1E31) {
			mu_assert_eq(type_info->leaf_type, eLF_VTSHAPE, "Incorrect data type");
		} else if (type->tpi_idx == 0x1FB7) {
			mu_assert_eq(type_info->leaf_type, eLF_MODIFIER, "Incorrect data type");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_MODIFIER *)(type_info->type_info))->modified_type);
			mu_assert_eq(stype->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect modified type");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "const volatile uint64_t", "Incorrect modifier print type");
			// } else if (type->tpi_idx == 0x1F4E) { This whole thing isn't parsed correctly for some reason TODO
			// 	mu_assert_eq (type_info->leaf_type, eLF_UNION, "Incorrect data type");
			// 	char *name;
			// 	type_info->get_name (type_info, &name);
			// 	mu_assert_streq (name, "_SLIST_HEADER", "wrong union name");
			// 	RzList *members;
			// 	type_info->get_members (type_info, &members);
			// 	mu_assert_eq (members->length, 3, "wrong union member count");
			// 	// mu_assert_eq (members->length, 4, "wrong union member count"); // Doesn't work, missing one (last) member for some reason TODO
			// 	int size = 0;
			// 	type_info->get_val (type_info, &size);
			// 	// mu_assert_eq (size, 48, " Wrong union size"); // parse wrong should be 48, is 16
			// 	mu_assert_eq (size, 16, " Wrong union size");
		} else if (type->tpi_idx == 0x1EA9) {
			mu_assert_eq(type_info->leaf_type, eLF_CLASS, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "std::bad_typeid", "wrong class name");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->vshape);
			mu_assert_notnull(stype, "wrong class vshape");
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->derived);
			mu_assert_null(stype, "wrong class derived");
			//} else if (type->tpi_idx == 0x1F50) {
			//	mu_assert_eq(type_info->leaf_type, eLF_BITFIELD, "Incorrect data type");
			//	SType *base_type = NULL;
			//	type_info->get_base_type(type_info, (void **)&base_type);
			//	char *type;
			//	type_info->get_print_type(type_info, &type);
			//	mu_assert_streq(type, "bitfield uint64_t : 48", "Incorrect bitfield print type");
		} else if (type->tpi_idx == 0x1E27) {
			mu_assert_eq(type_info->leaf_type, eLF_METHODLIST, "Incorrect data type");
			// Nothing from methodlist is currently being parsed
		} else if (type->tpi_idx == 0x181C) {
			mu_assert_eq(type_info->leaf_type, eLF_MFUNCTION, "Incorrect data type");
			SType *type;
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->return_type);
			mu_assert_eq(type->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = type->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_VOID, "Incorrect return type");
			mu_assert_eq(simple_type->size, 0, "Incorrect return type");
			mu_assert_streq(simple_type->type, "void", "Incorrect return type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->class_type);
			mu_assert_eq(type->tpi_idx, 0x107F, "incorrect mfunction class type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->arglist);
			mu_assert_eq(type->tpi_idx, 0x1000, "incorrect mfunction arglist");
		} else if (type->tpi_idx == 0x13BF) {
			mu_assert_eq(type_info->leaf_type, eLF_FIELDLIST, "Incorrect data type");
			// check size
			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 3, "Incorrect members length");
			RzListIter *it = rz_list_iterator(members);
			int i = 0;
			while (rz_list_iter_next(it)) {
				STypeInfo *type_info = (STypeInfo *)rz_list_iter_get(it);
				mu_assert_eq(type_info->leaf_type, eLF_MEMBER, "Incorrect data type");
				if (i == 0) {
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "RUST$ENUM$DISR", "Wrong member name");
					// get type
				}
				if (i == 2) {
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "__0", "Wrong member name");
					// get type
				}
				i++;
			}
		} else if (type->tpi_idx == 0x1164) {
			mu_assert_eq(type_info->leaf_type, eLF_ARGLIST, "Incorrect data type");
		} else if (type->tpi_idx == 0x1058) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE, "Incorrect data type");
			char *name;
			name = type_info->get_name(&type->type_data);
			mu_assert_streq(name, "std::thread::local::fast::Key<core::cell::Cell<core::option::Option<core::ptr::non_null::NonNull<core::task::wake::Context>>>>", "Wrong name");

			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_false(forward_ref, "Wrong fwdref");

			ut64 size = type_info->get_val(type_info);

			mu_assert_eq(size, 24, "Wrong struct size");

			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 2, "Incorrect members count");

			RzListIter *it = rz_list_iterator(members);
			int i = 0;
			while (rz_list_iter_next(it)) {
				STypeInfo *type_info = (STypeInfo *)rz_list_iter_get(it);
				if (i == 0) {
					mu_assert_eq(type_info->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "inner", "Wrong member name");
					// todo add type idx
				}
				if (i == 1) {
					mu_assert_eq(type_info->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "dtor_state", "Wrong member name");
				}
				i++;
			}
		}
	};
	pdb.finish_pdb_parse(&pdb);
	mu_end;
}

bool test_pdb_type_save(void) {
	RzPdb pdb = RZ_EMPTY;
	RzAnalysis *analysis = rz_analysis_new();
	const char *dir_prefix = rz_sys_prefix(NULL);
	rz_type_db_init(analysis->typedb, dir_prefix, "x86", 32, "windows");

	mu_assert_true(pdb_info_save_types(analysis, "bins/pdb/Project1.pdb", &pdb), "pdb parsing failed");

	// Check the enum presence and validity
	RzBaseType *test_enum = rz_type_db_get_base_type(analysis->typedb, "R2_TEST_ENUM");
	mu_assert_eq(test_enum->kind, RZ_BASE_TYPE_KIND_ENUM, "R2_TEST_ENUM is enum");
	mu_assert_true(has_enum_val(test_enum, "eENUM1_R2", 0x10), "eNUM1_R2 = 0x10");
	mu_assert_true(has_enum_val(test_enum, "eENUM2_R2", 0x20), "eNUM2_R2 = 0x20");
	mu_assert_true(has_enum_val(test_enum, "eENUM_R2_MAX", 0x21), "eNUM2_R2 = 0x21");

	mu_assert_false(has_enum_case(test_enum, "no_case"), "no such enum case");

	// Check the union presence and validity
	RzBaseType *test_union = rz_type_db_get_base_type(analysis->typedb, "R2_TEST_UNION");
	mu_assert_eq(test_union->kind, RZ_BASE_TYPE_KIND_UNION, "R2_TEST_UNION is union");
	mu_assert_true(has_union_member(test_union, "r2_union_var_1"), "r2_union_var_1");
	mu_assert_true(has_union_member(test_union, "r2_union_var_2"), "r2_union_var_2");
	// Test member types also
	mu_assert_true(has_union_member_type(analysis->typedb, test_union, "r2_union_var_1", "int32_t"), "r2_union_var_1 type");
	mu_assert_true(has_union_member_type(analysis->typedb, test_union, "r2_union_var_2", "double"), "rz_union_var_2 type");
	mu_assert_false(has_union_member(test_union, "noSuchMember"), "no such struct member");

	RzBaseType *m64_union = rz_type_db_get_base_type(analysis->typedb, "__m64");
	mu_assert_eq(m64_union->kind, RZ_BASE_TYPE_KIND_UNION, "__m64 is union");
	mu_assert_true(has_union_member(m64_union, "m64_f32"), "m64_f32");
	mu_assert_true(has_union_member(m64_union, "m64_i8"), "m64_i8");
	mu_assert_true(has_union_member(m64_union, "m64_i16"), "m64_i16");
	mu_assert_true(has_union_member(m64_union, "m64_i32"), "m64_i32");
	mu_assert_true(has_union_member(m64_union, "m64_i64"), "m64_i64");
	mu_assert_true(has_union_member(m64_union, "m64_u8"), "m64_u8");
	mu_assert_true(has_union_member(m64_union, "m64_u16"), "m64_u16");
	mu_assert_true(has_union_member(m64_union, "m64_u32"), "m64_u32");
	mu_assert_true(has_union_member(m64_union, "m64_u64"), "m64_u64");
	// Test member types also
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_u64", "uint64_t"), "m64_u64 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_f32", "float[8]"), "m64_f32 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_i8", "char[8]"), "m64_i8 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_i32", "int32_t[8]"), "m64_i32 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_i16", "uint16_t[8]"), "m64_i16 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_i64", "int64_t"), "m64_i64 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_u8", "uint8_t[8]"), "m64_u8 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_u16", "uint16_t[8]"), "m64_u16 type");
	mu_assert_true(has_union_member_type(analysis->typedb, m64_union, "m64_u32", "uint32_t[8]"), "m64_u32 type");

	mu_assert_false(has_union_member(m64_union, "noSuchMember"), "no such union member");
	// We dont handle class integration for now, so disable the following unit test.
	// Check the structure presence and validity
	// RzBaseType *test_class = rz_type_db_get_base_type(analysis->typedb, "TEST_CLASS");
	// mu_assert_eq(test_class->kind, RZ_BASE_TYPE_KIND_STRUCT, "TEST_CLASS is struct");
	// mu_assert_true(has_struct_member(test_class, "class_var1"), "class_var1");
	// mu_assert_true(has_struct_member(test_class, "calss_var2"), "calss_var2");
	// TODO: test member types also
	//check_kv("struct.TEST_CLASS.class_var1", "int32_t,0,0");
	//check_kv("struct.TEST_CLASS.calss_var2", "uint16_t,4,0");

	// mu_assert_false(has_struct_member(test_class, "noSuchMember"), "no such struct member");
	// Check the structure presence and validity

	// Forward defined structure, not handled. https://github.com/rizinorg/rizin/issues/1377
	// RzBaseType *localeinfo = rz_type_db_get_base_type(analysis->typedb, "localeinfo_struct");
	// mu_assert_eq(localeinfo->kind, RZ_BASE_TYPE_KIND_STRUCT, "localeinfo_struct is struct");
	// mu_assert_true(has_struct_member(localeinfo, "locinfo"), "locinfo");
	// mu_assert_true(has_struct_member(localeinfo, "mbcinfo"), "mbcinfo");
	// // Test member types also
	// mu_assert_true(has_struct_member_type(analysis->typedb, localeinfo, "locinfo", "struct threadlocaleinfostruct *"), "locinfo type");
	// // FIXME: For some reason this type doesn't load from PDB
	// //mu_assert_true(has_struct_member_type(analysis->typedb, localeinfo, "mbcinfo", "struct threadmbcinfostruct *"), "mbcinfo type");

	// mu_assert_false(has_struct_member(localeinfo, "noSuchMember"), "no such struct member");

	rz_analysis_free(analysis);
	mu_end;
}

bool test_pdb_tpi_cpp_vs2019(void) {
	RzPdb pdb = RZ_EMPTY;
	mu_assert_true(pdb_info("bins/pdb/vs2019_cpp_override.pdb", &pdb), "pdb parsing failed");

	RzList *plist = pdb.pdb_streams;
	mu_assert_notnull(plist, "PDB streams is NULL");

	mu_assert_eq(pdb.root_stream->num_streams, 75, "Incorrect number of streams");

	STpiStream *tpi_stream = rz_list_get_n(plist, ePDB_STREAM_TPI);
	mu_assert_notnull(tpi_stream, "TPIs stream not found in current PDB");
	mu_assert_eq(tpi_stream->header.hdr_size + tpi_stream->header.follow_size, 233588, "Wrong TPI size");
	mu_assert_eq(tpi_stream->header.idx_begin, 0x1000, "Wrong beginning index");
	mu_assert_eq(tpi_stream->types->length, 5107, "Incorrect number of types");

	RzListIter *it;
	SType *type;
	rz_list_foreach (tpi_stream->types, it, type) {
		STypeInfo *type_info = &type->type_data;
		if (type->tpi_idx == 0x1A5F) {
			mu_assert_eq(type_info->leaf_type, eLF_PROCEDURE, "Incorrect data type");
			SType *arglist;
			arglist = rz_bin_pdb_stype_by_index(((SLF_PROCEDURE *)(type_info->type_info))->arg_list);
			mu_assert_eq(arglist->tpi_idx, 0x1A5E, "Wrong type index");
			SType *return_type;
			return_type = rz_bin_pdb_stype_by_index(((SLF_PROCEDURE *)(type_info->type_info))->return_type);
			mu_assert_eq(return_type->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = return_type->type_data.type_info;
			mu_assert_eq(simple_type->size, 0, "Incorrect return type");
			mu_assert_streq(simple_type->type, "void", "Incorrect return type");
		} else if (type->tpi_idx == 0x1A64) {
			mu_assert_eq(type_info->leaf_type, eLF_POINTER, "Incorrect data type");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "const class std::ios_base*", "Wrong pointer print type");
		} else if (type->tpi_idx == 0x1ACD) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE, "Incorrect data type");
			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_false(forward_ref, "Wrong fwdref");
		} else if (type->tpi_idx == 0x1B3C) {
			mu_assert_eq(type_info->leaf_type, eLF_ARRAY, "Incorrect data type");
			char *type;
			type_info->get_print_type(type_info, &type);
			SType *dump;
			dump = rz_bin_pdb_stype_by_index(((SLF_ARRAY *)(type_info->type_info))->index_type);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_ULONG, "Incorrect return type");
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "uint32_t", "Incorrect return type");
			dump = rz_bin_pdb_stype_by_index(((SLF_ARRAY *)(type_info->type_info))->element_type);
			mu_assert_eq(dump->tpi_idx, 0, "Wrong element type index");
			ut64 size = type_info->get_val(type_info);
			mu_assert_eq(size, 16, "Wrong array size");
		} else if (type->tpi_idx == 0x20D6) {
			mu_assert_eq(type_info->leaf_type, eLF_ENUM, "Incorrect data type");
			SType *dump;
			RzList *members;
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "ReplacesCorHdrNumericDefines", "wrong enum name");
			dump = rz_bin_pdb_stype_by_index(((SLF_ENUM *)(type_info->type_info))->utype);
			mu_assert_eq(dump->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = dump->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_INT4, "Incorrect return type");
			mu_assert_eq(simple_type->size, 4, "Incorrect return type");
			mu_assert_streq(simple_type->type, "int32_t", "Incorrect return type");
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 25, "wrong enum members length");
		} else if (type->tpi_idx == 0x1A5A) {
			mu_assert_eq(type_info->leaf_type, eLF_VTSHAPE, "Incorrect data type");
		} else if (type->tpi_idx == 0x2163) {
			mu_assert_eq(type_info->leaf_type, eLF_MODIFIER, "Incorrect data type");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_MODIFIER *)(type_info->type_info))->modified_type);
			mu_assert_eq(stype->tpi_idx, 0, "Incorrect modified type");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "volatile uint32_t", "Incorrect modifier print type");
		} else if (type->tpi_idx == 0x2151) {
			mu_assert_eq(type_info->leaf_type, eLF_UNION, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "__m64", "wrong union name");
			RzList *members;
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 9, "wrong union member count");
		} else if (type->tpi_idx == 0x239B) {
			mu_assert_eq(type_info->leaf_type, eLF_CLASS, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "CTest1", "wrong class name");
			RzList *members;
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 5, "wrong class member count");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->vshape);
			mu_assert_notnull(stype, "wrong class vshape");
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->derived);
			mu_assert_null(stype, "wrong class derived");
		} else if (type->tpi_idx == 0x23DC) {
			mu_assert_eq(type_info->leaf_type, eLF_CLASS, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "CTest2", "wrong class name");
			RzList *members;
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 4, "wrong class member count");
			RzListIter *it_ctest2;
			STypeInfo *member_type;
			int i = 0;
			rz_list_foreach (members, it_ctest2, member_type) {
				if (i == 0) {
					mu_assert_eq(member_type->leaf_type, eLF_BCLASS, "Incorrect data type");
				} else if (i == 1) {
					mu_assert_eq(member_type->leaf_type, eLF_ONEMETHOD, "Incorrect data type");
					name = member_type->get_name(member_type);
					mu_assert_streq(name, "Bar", "wrong member name");
				} else if (i == 2) {
					mu_assert_eq(member_type->leaf_type, eLF_METHOD, "Incorrect data type");
					name = member_type->get_name(member_type);
					mu_assert_streq(name, "CTest2", "wrong member name");
				}
				i++;
			}
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->vshape);
			mu_assert_eq(stype->tpi_idx, 0x11E8, "wrong class vshape");
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->derived);
			mu_assert_null(stype, "wrong class derived");
		} else if (type->tpi_idx == 0x2299) {
			mu_assert_eq(type_info->leaf_type, eLF_CLASS_19, "Incorrect data type");
			char *name;
			name = type_info->get_name(type_info);
			mu_assert_streq(name, "type_info", "wrong class name");
			RzList *members;
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 12, "wrong class member count");
			SType *stype = NULL;
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->vshape);
			mu_assert_notnull(stype, "wrong class vshape");
			stype = rz_bin_pdb_stype_by_index(((SLF_CLASS *)(type_info->type_info))->derived);
			mu_assert_null(stype, "wrong class derived");
		} else if (type->tpi_idx == 0x2147) {
			mu_assert_eq(type_info->leaf_type, eLF_BITFIELD, "Incorrect data type");
			SType *base_type = NULL;
			base_type = rz_bin_pdb_stype_by_index(((SLF_BITFIELD *)(type_info->type_info))->base_type);
			mu_assert_notnull(base_type, "Bitfield base type is NULL");
			char *type;
			type_info->get_print_type(type_info, &type);
			mu_assert_streq(type, "uint32_t /*bitfield:16*/", "Incorrect bitfield print type");
		} else if (type->tpi_idx == 0x2209) {
			mu_assert_eq(type_info->leaf_type, eLF_METHODLIST, "Incorrect data type");
			// Nothing from methodlist is currently being parsed
		} else if (type->tpi_idx == 0x224F) {
			mu_assert_eq(type_info->leaf_type, eLF_MFUNCTION, "Incorrect data type");
			SType *type;
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->return_type);
			mu_assert_eq(type->type_data.leaf_type, eLF_SIMPLE_TYPE, "Incorrect return type");
			SLF_SIMPLE_TYPE *simple_type = type->type_data.type_info;
			mu_assert_eq(simple_type->simple_type, eT_VOID, "Incorrect return type");
			mu_assert_eq(simple_type->size, 0, "Incorrect return type");
			mu_assert_streq(simple_type->type, "void", "Incorrect return type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->class_type);
			mu_assert_eq(type->tpi_idx, 0x2247, "incorrect mfunction class type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->this_type);
			mu_assert_eq(type->tpi_idx, 0x2248, "incorrect mfunction this type");
			type = rz_bin_pdb_stype_by_index(((SLF_MFUNCTION *)(type_info->type_info))->arglist);
			mu_assert_eq(type->tpi_idx, 0x224E, "incorrect mfunction arglist");
		} else if (type->tpi_idx == 0x239A) {
			mu_assert_eq(type_info->leaf_type, eLF_FIELDLIST, "Incorrect data type");
			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 5, "Incorrect members length");
			RzListIter *it_fieldlist;
			STypeInfo *type_info;
			int i = 0;
			rz_list_foreach (members, it_fieldlist, type_info) {
				if (i == 1) {
					mu_assert_eq(type_info->leaf_type, eLF_ONEMETHOD, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "Foo", "Wrong enum name");
				}
				if (i == 3) {
					mu_assert_eq(type_info->leaf_type, eLF_METHOD, "Incorrect data type");
					char *name = NULL;
					name = type_info->get_name(type_info);
					mu_assert_streq(name, "CTest1", "Wrong enum name");
				}
				i++;
			}
		} else if (type->tpi_idx == 0x2392) {
			mu_assert_eq(type_info->leaf_type, eLF_ARGLIST, "Incorrect data type");
		} else if (type->tpi_idx == 0x208F) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE, "Incorrect data type");
			char *name;
			name = type_info->get_name(&type->type_data);
			mu_assert_streq(name, "_s__RTTICompleteObjectLocator", "Wrong name");
			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_false(forward_ref, "Wrong fwdref");
			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 5, "Incorrect members count");
			RzListIter *it_structure;
			STypeInfo *type_structure;
			int i = 0;
			rz_list_foreach (members, it_structure, type_structure) {
				if (i == 0) {
					mu_assert_eq(type_structure->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_structure->get_name(type_structure);
					mu_assert_streq(name, "signature", "Wrong member name");
					char *type;
					type_structure->get_print_type(type_structure, &type);
					mu_assert_streq(type, "uint32_t", "Wrong member type");
				}
				if (i == 1) {
					mu_assert_eq(type_structure->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_structure->get_name(type_structure);
					mu_assert_streq(name, "offset", "Wrong member name");
					char *type;
					type_structure->get_print_type(type_structure, &type);
					mu_assert_streq(type, "uint32_t", "Wrong member type");
				}
				if (i == 4) {
					mu_assert_eq(type_structure->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_structure->get_name(type_structure);
					mu_assert_streq(name, "pClassDescriptor", "Wrong method name");
					char *type;
					type_structure->get_print_type(type_structure, &type);
					mu_assert_streq(type, "const struct _s__RTTIClassHierarchyDescriptor*", "Wrong method type");
				}
				i++;
			}
		} else if (type->tpi_idx == 0x2184) {
			mu_assert_eq(type_info->leaf_type, eLF_STRUCTURE_19, "Incorrect data type");
			char *name;
			name = type_info->get_name(&type->type_data);
			mu_assert_streq(name, "_RS5_IMAGE_LOAD_CONFIG_DIRECTORY32", "Wrong name");
			bool forward_ref;
			forward_ref = type->type_data.is_fwdref(type_info);
			mu_assert_false(forward_ref, "Wrong fwdref");
			;
			RzList *members = rz_list_new();
			members = type_info->get_members(type_info);
			mu_assert_eq(members->length, 48, "Incorrect members count");
			RzListIter *it_structure_19;
			STypeInfo *type_structure_19;
			int i = 0;
			rz_list_foreach (members, it_structure_19, type_structure_19) {
				if (i == 0) {
					mu_assert_eq(type_structure_19->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_structure_19->get_name(type_structure_19);
					mu_assert_streq(name, "Size", "Wrong member name");
					char *type;
					type_structure_19->get_print_type(type_structure_19, &type);
					mu_assert_streq(type, "uint32_t", "Wrong member type");
				}
				if (i == 1) {
					mu_assert_eq(type_structure_19->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_structure_19->get_name(type_structure_19);
					mu_assert_streq(name, "TimeDateStamp", "Wrong member name");
					char *type;
					type_structure_19->get_print_type(type_structure_19, &type);
					mu_assert_streq(type, "uint32_t", "Wrong member type");
				}
				if (i == 17) {
					mu_assert_eq(type_structure_19->leaf_type, eLF_MEMBER, "Incorrect data type");
					char *name = NULL;
					name = type_structure_19->get_name(type_structure_19);
					mu_assert_streq(name, "SecurityCookie", "Wrong method name");
					char *type;
					type_structure_19->get_print_type(type_structure_19, &type);
					mu_assert_streq(type, "uint32_t", "Wrong method type");
				}
				i++;
			}
		}
	}
	pdb.finish_pdb_parse(&pdb);
	mu_end;
}

bool all_tests() {
	mu_run_test(test_pdb_tpi_cpp);
	mu_run_test(test_pdb_tpi_rust);
	mu_run_test(test_pdb_type_save);
	mu_run_test(test_pdb_tpi_cpp_vs2019);
	return tests_passed != tests_run;
}

mu_main(all_tests)
