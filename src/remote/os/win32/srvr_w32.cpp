/*************  history ************
*
*       COMPONENT: REMOTE       MODULE: SRVR_W32.C
*       generated by Marion V2.5     2/6/90
*       from dev              db        on 26-JAN-1996
*****************************************************************
*
*       20927   klaus   26-JAN-1996
*       Call ICS_enter at start
*
*       20858   klaus   17-JAN-1996
*       Get rid of extraneous header file
*
*       20841   klaus   12-JAN-1996
*       Add interprocess comm under remote component
*
*       20804   RCURRY  9-JAN-1996 
*       Change priority for NP threads to normal
*
*       20768   klaus   20-DEC-1995
*       More xnet driver work
*
*       20729   klaus   8-DEC-1995 
*       Begin adding xnet protocol
*
*       20716   jmayer  6-DEC-1995 
*       Update to not show NamedPipes as supported on Win95.
*
*       20690   jmayer  4-DEC-1995 
*       Change to start the IPC protocol when running as a service.
*
*       20682   jmayer  3-DEC-1995 
*       Update to write to logfile and display msg box as a non-service.
*
*       20373   RCURRY  24-OCT-1995
*       Fix bug with license checking
*
*       20359   RMIDEKE 23-OCT-1995
*       add a semicollin
*
*       20356   rcurry  23-OCT-1995
*       Add more license file checking
*
*       20350   RCURRY  20-OCT-1995
*       add license file checking for remote protocols
*
*       20281   RCURRY  13-OCT-1995
*       fix multi thread scheduler problem
*
*       20198   RCURRY  27-SEP-1995
*       Make Windows95 and Windows NT have the same defaults
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
 * 2001.11.20: Claudio Valderrama: Honor -b in SS for high priority.
 *
*/


/*
 *      PROGRAM:        JRD Remote Server
 *      MODULE:         nt_server.c
 *      DESCRIPTION:    Windows NT remote server.
 *
 * copyright (c) 1993, 1996 by Borland International
 */

#include "firebird.h"
#include "../jrd/ib_stdio.h"
#include <stdlib.h>
#include <windows.h>
#include "../remote/remote.h"
#include "gen/codes.h"
#include "../jrd/license.h"
#include "../jrd/thd.h"
#include "../jrd/license.h"
#include "../utilities/install/install_nt.h"
#include "../remote/os/win32/cntl_proto.h"
#include "../remote/inet_proto.h"
#include "../remote/serve_proto.h"
#include "../remote/os/win32/window_proto.h"
#include "../remote/os/win32/wnet_proto.h"
#include "../remote/os/win32/window.rh"
#include "../remote/xnet_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/license.h"
#include "../jrd/sch_proto.h"
#include "../jrd/svc_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/os/isc_i_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../jrd/file_params.h"
#include "../common/config/config.h"

static void THREAD_ROUTINE inet_connect_wait_thread(void *);
static void THREAD_ROUTINE ipc_connect_wait_thread(void *);
static void THREAD_ROUTINE start_connections_thread(int);
static void THREAD_ROUTINE wnet_connect_wait_thread(void *);
static void THREAD_ROUTINE xnet_connect_wait_thread(void *);
static HANDLE parse_args(LPSTR, USHORT *);
static void service_connection(PORT);

static HINSTANCE hInst;

static TEXT protocol_inet[128];
static TEXT protocol_wnet[128];
static USHORT server_flag;

static SERVICE_TABLE_ENTRY service_table[] = {
	{REMOTE_SERVICE, (LPSERVICE_MAIN_FUNCTION) CNTL_main_thread},
	{NULL, NULL}
};

static const int SIGSHUT = 666;
static int shutdown_pid = 0;

/* put into ensure that we have a parent port for the XNET connections */
static int xnet_server_set = FALSE;

const char *FBCLIENTDLL = "fbclient.dll";


int WINAPI WinMain(HINSTANCE	hThisInst,
				   HINSTANCE	hPrevInst,
				   LPSTR		lpszArgs,
				   int			nWndMode)
{
/**************************************
 *
 *      W i n M a i n
 *
 **************************************
 *
 * Functional description
 *      Run the server with NT named
 *      pipes and/or TCP/IP sockets.
 *
 **************************************/
	ISC_STATUS_ARRAY status_vector;
	HANDLE connection_handle;
	PORT port;
	int nReturnValue = 0;

	hInst = hThisInst;

#ifdef SUPERSERVER
	server_flag = SRVR_multi_client;
#else
	server_flag = 0;
#endif

#ifdef SUPERSERVER
	if (ISC_is_WinNT()) {	/* True - NT, False - Win95 */

		/* CVC: This operating system call doesn't exist for W9x. */
		typedef BOOL (__stdcall *PSetProcessAffinityMask)(HANDLE, DWORD);
		PSetProcessAffinityMask SetProcessAffinityMask;

		SetProcessAffinityMask = (PSetProcessAffinityMask)
			GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "SetProcessAffinityMask");
		if (SetProcessAffinityMask) {
			/* Mike Nordell - 11 Jun 2001: CPU affinity. */
			(*SetProcessAffinityMask)(GetCurrentProcess(),
				static_cast<DWORD>(Config::getCpuAffinityMask()));
		}
	}
	else {
		server_flag |= SRVR_non_service;
	}
#endif

	if (server_flag & SRVR_multi_client) {
		gds__thread_enable(-1);
	}

	protocol_inet[0] = 0;
	protocol_wnet[0] = 0;

	connection_handle = parse_args(lpszArgs, &server_flag);

	if (shutdown_pid) {
		ISC_kill(shutdown_pid, SIGSHUT, 0);
		return 0;
	}

	if ((server_flag & (SRVR_inet | SRVR_wnet | SRVR_ipc | SRVR_xnet))==0) {

		if (ISC_is_WinNT())		/* True - NT, False - Win95 */
			server_flag |= SRVR_wnet;
		server_flag |= SRVR_inet;
		server_flag |= SRVR_xnet;
#ifdef SUPERSERVER
		server_flag |= SRVR_ipc;
#endif
	}

#ifdef SUPERSERVER
	// get priority class from the config file
	int priority = Config::getProcessPriorityLevel();

	// override it, if necessary
	if (server_flag & SRVR_high_priority) {
		priority = 1;
	}

	// set priority class
	if (priority > 0) {
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	}
	else if (priority < 0) {
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
	}
#endif

/* Initialize the service and
   Setup sig_mutex for the process
*/
	ISC_signal_init();
#ifdef SUPERSERVER
	ISC_enter();
#endif

	if (!ISC_is_WinNT()) {
		LoadLibrary(FBCLIENTDLL);
	}

	if (connection_handle != INVALID_HANDLE_VALUE) {
		THREAD_ENTER;
		if (server_flag & SRVR_inet)
			port = INET_reconnect(connection_handle, 0, status_vector);
		else if (server_flag & SRVR_wnet)
			port = WNET_reconnect(connection_handle, 0, status_vector);
		else if (server_flag & SRVR_xnet)
			port = XNET_reconnect((ULONG) connection_handle, 0, status_vector);
		THREAD_EXIT;
		if (port) {
			service_connection(port);
		}
	}
	else if (!(server_flag & SRVR_non_service)) {
		CNTL_init((FPTR_VOID) start_connections_thread, REMOTE_SERVICE);
		if (!StartServiceCtrlDispatcher(service_table)) {
			if (GetLastError() != ERROR_CALL_NOT_IMPLEMENTED) {
				CNTL_shutdown_service("StartServiceCtrlDispatcher failed");
			}
			server_flag |= SRVR_non_service;
		}
	}
	else {
		if (server_flag & SRVR_inet) {
			gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
							  (inet_connect_wait_thread), 0, THREAD_medium, 0,
							  0);
		}
		if (server_flag & SRVR_wnet) {
			gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
							  (wnet_connect_wait_thread), 0, THREAD_medium, 0,
							  0);
		}
		if (server_flag & SRVR_xnet) {
			gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
							  (xnet_connect_wait_thread), 0, THREAD_medium, 0,
							  0);
		}
		/* No need to waste a thread if we are running as a window.  Just start
		 * the IPC communication
		 */
		if (Config::getCreateInternalWindow()) {
			nReturnValue = WINDOW_main(hThisInst, nWndMode, server_flag);
		}
		else {
			HANDLE hEvent =
				ISC_make_signal(TRUE, TRUE, GetCurrentProcessId(), SIGSHUT);
			WaitForSingleObject(hEvent, INFINITE);
			THREAD_ENTER;
			JRD_shutdown_all();
		}
	}

#ifdef DEBUG_GDS_ALLOC
/* In Debug mode - this will report all server-side memory leaks
 * due to remote access
 */
	//gds_alloc_report(0, __FILE__, __LINE__);
	char name[MAXPATHLEN];
	gds__prefix(name, "memdebug.log");
	FILE* file = fopen(name, "w+b");
	if (file) {
	  fprintf(file,"Global memory pool allocated objects\n");
	  getDefaultMemoryPool()->print_contents(file);
	  fclose(file);
	}
#endif

	return nReturnValue;
}


void THREAD_ROUTINE process_connection_thread( PORT port)
{
/**************************************
 *
 *      p r o c e s s _ c o n n e c t i o n _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	void *thread;

	if (!(server_flag & SRVR_non_service)) {
		thread = CNTL_insert_thread();
	}
	service_connection(port);
	if (!(server_flag & SRVR_non_service)) {
		CNTL_remove_thread(thread);
	}
}


static void THREAD_ROUTINE inet_connect_wait_thread( void *dummy)
{
/**************************************
 *
 *      i n e t _ c o n n e c t _ w a i t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	void *thread;
	ISC_STATUS_ARRAY status_vector;
	PORT port;

	if (!(server_flag & SRVR_non_service))
		thread = CNTL_insert_thread();

	THREAD_ENTER;
	port = INET_connect(protocol_inet, 0, status_vector, server_flag, 0, 0);
	THREAD_EXIT;
	if (port)
		SRVR_multi_thread(port, server_flag);
	else
		gds__log_status(0, status_vector);

	if (!(server_flag & SRVR_non_service))
		CNTL_remove_thread(thread);
}


static void THREAD_ROUTINE wnet_connect_wait_thread( void *dummy)
{
/**************************************
 *
 *      w n e t _ c o n n e c t _ w a i t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	void *thread;
	ISC_STATUS_ARRAY status_vector;

	if (!(server_flag & SRVR_non_service)) {
		thread = CNTL_insert_thread();
	}

	while (true)
	{
		THREAD_ENTER;
		PORT port = WNET_connect(protocol_wnet, 0, status_vector, server_flag);
		THREAD_EXIT;
		if (!port) {
			if (status_vector[1] != gds_io_error ||
				status_vector[6] != gds_arg_win32 ||
				status_vector[7] != ERROR_CALL_NOT_IMPLEMENTED) {
				gds__log_status(0, status_vector);
			}
			break;
		}
		gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
						  (process_connection_thread), port, THREAD_medium, 0,
						  0);
	}

	if (!(server_flag & SRVR_non_service)) {
		CNTL_remove_thread(thread);
	}
}


static void THREAD_ROUTINE ipc_connect_wait_thread( void *dummy)
{
/**************************************
 *
 *      i p c _ c o n n e c t _ w a i t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	void *thread;

	if (!(server_flag & SRVR_non_service))
		thread = CNTL_insert_thread();

	if (Config::getCreateInternalWindow()) {
		WINDOW_main(hInst, SW_NORMAL, server_flag);
	}

	if (!(server_flag & SRVR_non_service))
		CNTL_remove_thread(thread);
}


static void THREAD_ROUTINE xnet_connect_wait_thread(void *dummy)
{
/**************************************
 *
 *      x n e t _ c o n n e c t _ w a i t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *   Starts xnet server side interprocess thread
 *
 **************************************/
	void *thread;

	if (!(server_flag & SRVR_non_service))
		thread = CNTL_insert_thread();

	THREAD_ENTER;
	XNET_srv(server_flag);
	THREAD_EXIT;

	if (!(server_flag & SRVR_non_service))
		CNTL_remove_thread(thread);
}


static void service_connection( PORT port)
{
/**************************************
 *
 *      s e r v i c e _ c o n n e c t i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	SRVR_main(port, (USHORT) (server_flag & ~SRVR_multi_client));
}


static void THREAD_ROUTINE start_connections_thread( int flag)
{
/**************************************
 *
 *      s t a r t _ c o n n e c t i o n s _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	HANDLE ipc_thread_handle = 0;

	if (server_flag & SRVR_inet) {
		gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
						  (inet_connect_wait_thread), 0, THREAD_medium, 0, 0);
	}
	if (server_flag & SRVR_wnet) {
		gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
						  (wnet_connect_wait_thread), 0, THREAD_medium, 0, 0);
	}
	if (server_flag & SRVR_xnet) {
		gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
						  (xnet_connect_wait_thread), 0, THREAD_medium, 0, 0);
	}
	if (server_flag & SRVR_ipc) {
		const int bFailed =
			gds__thread_start(reinterpret_cast<FPTR_INT_VOID_PTR>
							  (ipc_connect_wait_thread),
							  0,
							  THREAD_medium,
							  0,
							  &ipc_thread_handle);

		if (bFailed ||
			WaitForSingleObject(ipc_thread_handle, 2000) != WAIT_TIMEOUT) {
			/* If the IPC thread did not time out, then it must have finished. *
			 * the only way for it to have finished in 2 seconds is for it to  *
			 * not have succeeded.  IE. It already exists.                     */

			if (!bFailed && ipc_thread_handle) {
				CloseHandle(ipc_thread_handle);
			}
			CNTL_shutdown_service("Could not start service");
			return;
		}
	}
}


static HANDLE parse_args( LPSTR lpszArgs, USHORT * pserver_flag)
{
/**************************************
 *
 *      p a r s e _ a r g s
 *
 **************************************
 *
 * Functional description
 *      WinMain gives us a stupid command string, not
 *      a cool argv.  Parse through the string and
 *      set the options.
 * Returns
 *      a connection handle if one was passed in,
 *      INVALID_HANDLE_VALUE otherwise.
 *
 **************************************/
	TEXT *p, c;
	TEXT buffer[32];
	HANDLE connection_handle;

	connection_handle = INVALID_HANDLE_VALUE;

	p = lpszArgs;
	while (*p) {
		if (*p++ == '-')
			while ((*p) && (c = *p++) && (c != ' '))
				switch (UPPER(c)) {
				case 'A':
					*pserver_flag |= SRVR_non_service;
					break;

				case 'B':
					*pserver_flag |= SRVR_high_priority;
					break;

				case 'D':
					*pserver_flag |= (SRVR_debug | SRVR_non_service);
					break;

#ifndef SUPERSERVER
				case 'H':
					while (*p && *p == ' ')
						p++;
					if (*p) {
						char *pp = buffer;
						while (*p && *p != ' ') {
							*pp++ = *p++;
						}
						*pp++ = '\0';
						connection_handle = (HANDLE) atol(buffer);
					}
					break;
#endif

				case 'I':
					*pserver_flag |= SRVR_inet;
					break;

				case 'K':
					while (*p && *p == ' ')
						p++;
					if (*p) {
						char *pp = buffer;
						while (*p && *p != ' ') {
							*pp++ = *p++;
						}
						*pp++ = '\0';
						shutdown_pid = atoi(buffer);
					}
					break;

#ifdef SUPERSERVER
				case 'L':
					*pserver_flag |= SRVR_ipc;
					break;
#endif

				case 'N':
					*pserver_flag |= SRVR_no_icon;
					break;

				case 'P':		/* Specify a port or named pipe other than the default */
					while (*p && *p == ' ')
						p++;
					if (*p) {
						char *pi = protocol_inet, *pw = protocol_wnet;

						*pi++ = '/';
						*pw++ = '\\';
						*pw++ = '\\';
						*pw++ = '.';
						*pw++ = '@';
						while (*p && *p != ' ') {
							*pi++ = *p;
							*pw++ = *p++;
						}
						*pi++ = '\0';
						*pw++ = '\0';
					}
					break;

				case 'R':
					*pserver_flag &= ~SRVR_high_priority;
					break;

				case 'W':
					*pserver_flag |= SRVR_wnet;
					break;

				case 'X':
					*pserver_flag |= SRVR_xnet;
					break;

				case 'Z':
					ib_printf("Firebird remote server version %s\n",
							  FB_VERSION);
					exit(FINI_OK);

				default:
					/* In case of something unrecognized, just
					 * continue, since we have already taken it off
					 * of p. */
					break;
				}
	}
	return connection_handle;
}
