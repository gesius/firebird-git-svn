/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_proto.h
 *	DESCRIPTION:	Prototype header file for met.cpp
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_MET_PROTO_H
#define JRD_MET_PROTO_H

#include "../jrd/exe.h"
#include "../jrd/blob_filter.h"

class jrd_tra;
class jrd_req;
class jrd_prc;
class fmt;
class jrd_rel;
class Csb;

void		MET_activate_shadow(TDBB);
ULONG		MET_align(const struct dsc*, USHORT);
void		MET_change_fields(TDBB, jrd_tra *, struct dsc *);
fmt*		MET_current(TDBB, jrd_rel*);
void		MET_delete_dependencies(TDBB, const TEXT*, USHORT);
void		MET_delete_shadow(TDBB, USHORT);
void		MET_error(const TEXT*, ...);
fmt*		MET_format(TDBB, jrd_rel*, USHORT);
BOOLEAN		MET_get_char_subtype(TDBB, SSHORT*, const UCHAR*, USHORT);
struct jrd_nod*	MET_get_dependencies(TDBB, jrd_rel*, const TEXT*,
								Csb*, struct bid*, jrd_req**,
								Csb**, const TEXT*, USHORT);
class jrd_fld*	MET_get_field(jrd_rel*, USHORT);
void		MET_get_shadow_files(TDBB, bool);
void		MET_load_trigger(TDBB, jrd_rel*, const TEXT*, trig_vec**);
void		MET_lookup_cnstrt_for_index(TDBB, TEXT* constraint, const TEXT* index_name);
void		MET_lookup_cnstrt_for_trigger(TDBB, TEXT*, TEXT*, const TEXT*);
void		MET_lookup_exception(TDBB, SLONG, /* INOUT */ TEXT*, /* INOUT */ TEXT*);
SLONG		MET_lookup_exception_number(TDBB, const TEXT*);
int			MET_lookup_field(TDBB, jrd_rel*, const TEXT*, const TEXT*);
BLF			MET_lookup_filter(TDBB, SSHORT, SSHORT);
SLONG		MET_lookup_generator(TDBB, const TEXT*);
void		MET_lookup_generator_id(TDBB, SLONG, TEXT *);
void		MET_lookup_index(TDBB, TEXT*, const TEXT*, USHORT);
SLONG		MET_lookup_index_name(TDBB, const TEXT*, SLONG*, SSHORT*);
bool		MET_lookup_partner(TDBB, jrd_rel*, struct idx*, const TEXT*);
jrd_prc*	MET_lookup_procedure(TDBB, SCHAR *, bool);
jrd_prc*	MET_lookup_procedure_id(TDBB, SSHORT, bool, bool, USHORT);
jrd_rel*	MET_lookup_relation(TDBB, const char*);
jrd_rel*	MET_lookup_relation_id(TDBB, SLONG, bool);
struct jrd_nod*	MET_parse_blob(TDBB, jrd_rel*, struct bid*, Csb**,
								  jrd_req**, BOOLEAN, BOOLEAN);
void		MET_parse_sys_trigger(TDBB, jrd_rel*);
int			MET_post_existence(TDBB, jrd_rel*);
void		MET_prepare(TDBB, jrd_tra*, USHORT, const UCHAR*);
jrd_prc*	MET_procedure(TDBB, int, bool, USHORT);
jrd_rel*	MET_relation(TDBB, USHORT);
bool		MET_relation_owns_trigger (TDBB, const TEXT*, const TEXT*);
bool		MET_relation_default_class (TDBB, const TEXT*, const TEXT*);
void		MET_release_existence(jrd_rel*);
void		MET_release_triggers(TDBB, trig_vec**);
#ifdef DEV_BUILD
void		MET_verify_cache(TDBB);
#endif
bool		MET_clear_cache(TDBB, jrd_prc*);
bool		MET_procedure_in_use(TDBB, jrd_prc*);
void		MET_remove_procedure(TDBB, int, jrd_prc*);
void		MET_revoke(TDBB, jrd_tra*, const TEXT*, const TEXT*, const TEXT*);
TEXT*		MET_save_name(TDBB, const TEXT*);
void		MET_scan_relation(TDBB, jrd_rel*);
const TEXT* MET_trigger_msg(TDBB, const TEXT*, USHORT);
void		MET_update_shadow(TDBB, class Shadow*, USHORT);
void		MET_update_transaction(TDBB, jrd_tra*, const bool);
void		MET_update_partners(TDBB);

#endif // JRD_MET_PROTO_H

