/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		file_params.h
 *	DESCRIPTION:	File parameter definitions
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
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "EPSON" define*
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 * 2002.10.30 Sean Leyne - Code Cleanup, removed obsolete "SUN3_3" port
 *
 */

#ifndef JRD_FILE_PARAMS_H
#define JRD_FILE_PARAMS_H

#ifdef NOHOSTNAME
static const char* EVENT_FILE	= "isc_event1";
static const char* LOCK_FILE	= "isc_lock1.gbl";
static const char* INIT_FILE	= "isc_init1";
static const char* GUARD_FILE	= "isc_guard1";
static const char* MONITOR_FILE	= "isc_monitor1";
static const char* SEM_FILE		= "isc_sem1";
#elif defined(WIN_NT)
static const char* EVENT_FILE	= "%s.evn";
static const char* LOCK_FILE	= "%s.lck";
static const char* INIT_FILE	= "%s.int";
static const char* GUARD_FILE	= "%s.grd";
static const char* MONITOR_FILE	= "%s.mon";
static const char* SEM_FILE		= "%s.sem";
#else
static const char* EVENT_FILE	= "isc_event1.%s";
static const char* LOCK_FILE	= "isc_lock1.%s";
static const char* INIT_FILE	= "isc_init1.%s";
static const char* GUARD_FILE	= "isc_guard1.%s";
static const char* MONITOR_FILE	= "isc_monitor1.%s";
static const char* SEM_FILE		= "isc_sem1.%s";
#endif

// CVC: Do we really need this information here after using autoconf?
#if defined(sun) || defined(LINUX) || defined(FREEBSD) || defined(NETBSD)
#include <sys/types.h>
#include <sys/ipc.h>
#endif

#ifdef DARWIN
#undef FB_PREFIX
#define FB_PREFIX		"/all/files/are/in/framework/resources"
#define DARWIN_GEN_DIR		"var"
#define DARWIN_FRAMEWORK_ID	"com.firebirdsql.Firebird"
#endif

/* keep MSG_FILE_LANG in sync with build_file.epp */
#ifdef WIN_NT
static const char* WORKFILE		= "c:\\temp\\";
static const char MSG_FILE_LANG[]= "intl\\%.10s.msg";
#else
static const char* WORKFILE		= "/tmp/";
static const char MSG_FILE_LANG[]= "intl/%.10s.msg";
#endif

static const char* LOGFILE		= "firebird.log";
static const char* MSG_FILE		= "firebird.msg";
// Keep in sync with MSG_FILE_LANG
const int LOCALE_MAX	= 10;

#endif /* JRD_FILE_PARAMS_H */
