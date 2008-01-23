//____________________________________________________________
//  
//		PROGRAM:	Alice (All Else) Utility
//		MODULE:		all.cpp
//		DESCRIPTION:	Block allocator
//  
//  The contents of this file are subject to the Interbase Public
//  License Version 1.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy
//  of the License at http://www.Inprise.com/IPL.html
//  
//  Software distributed under the License is distributed on an
//  "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
//  or implied. See the License for the specific language governing
//  rights and limitations under the License.
//  
//  The Original Code was created by Inprise Corporation
//  and its predecessors. Portions created by Inprise Corporation are
//  Copyright (C) Inprise Corporation.
//  
//  All Rights Reserved.
//  Contributor(s): ______________________________________.
//  
//
//____________________________________________________________
//
//	$Id: all.cpp,v 1.30 2008-01-23 15:51:31 alexpeshkoff Exp $
//

#include "firebird.h"
#include "../jrd/common.h"
#include "../alice/all.h"
#include "../alice/alice.h"
#include "../common/classes/alloc.h"

AliceMemoryPool* AliceMemoryPool::createPool() {
	AliceMemoryPool* result = (AliceMemoryPool*)internal_create(sizeof(AliceMemoryPool));
	return result;
}

void AliceMemoryPool::deletePool(AliceMemoryPool* pool) 
{
	MemoryPool::deletePool(pool);
}
