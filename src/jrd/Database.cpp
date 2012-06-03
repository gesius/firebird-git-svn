/*
 *      PROGRAM:        JRD access method
 *      MODULE:         Database.cpp
 *      DESCRIPTION:    Common descriptions
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
 * Sean Leyne
 * Claudio Valderrama C.
 */

#include "firebird.h"
// Definition of block types for data allocation in JRD
#include "../include/fb_blk.h"

#include "../jrd/ibase.h"
#include "../jrd/ods.h"
#include "../jrd/lck.h"
#include "../jrd/Database.h"
#include "../jrd/nbak.h"
#include "../jrd/tra.h"
#include "../jrd/lck_proto.h"
#include "../jrd/CryptoManager.h"
#include "../jrd/os/pio_proto.h"

// Thread data block
#include "../common/ThreadData.h"

// recursive mutexes
#include "../common/thd.h"

using namespace Firebird;

namespace Jrd
{
	bool Database::onRawDevice() const
	{
#ifdef SUPPORT_RAW_DEVICES
		return PIO_on_raw_device(dbb_filename);
#else
		return false;
#endif
	}

	string Database::getUniqueFileId() const
	{
		const PageSpace* const pageSpace = dbb_page_manager.findPageSpace(DB_PAGE_SPACE);

		UCharBuffer buffer;
		PIO_get_unique_file_id(pageSpace->file, buffer);

		string file_id;
		char* s = file_id.getBuffer(2 * buffer.getCount());
		for (size_t i = 0; i < buffer.getCount(); i++)
		{
			sprintf(s, "%02x", (int) buffer[i]);
			s += 2;
		}

		return file_id;
	}

	Database::~Database()
	{
		{
			SyncLockGuard guard(&dbb_pools_sync, SYNC_EXCLUSIVE, "Database::~Database");

			fb_assert(dbb_pools[0] == dbb_permanent);

			for (size_t i = 1; i < dbb_pools.getCount(); ++i)
				MemoryPool::deletePool(dbb_pools[i]);
		}

		delete dbb_monitoring_data;
		delete dbb_backup_manager;
		delete dbb_crypto_manager;

		//Checkout dcoHolder(this);

		// This line decrements the usage counter and may cause the destructor to be called.
		// It should happen with the dbb_sync unlocked.
		LockManager::destroy(dbb_lock_mgr);
		EventManager::destroy(dbb_event_mgr);
	}

	void Database::deletePool(MemoryPool* pool)
	{
		if (pool)
		{
			{
				SyncLockGuard guard(&dbb_pools_sync, SYNC_EXCLUSIVE, "Database::deletePool");
				size_t pos;

				if (dbb_pools.find(pool, pos))
					dbb_pools.remove(pos);
			}

			MemoryPool::deletePool(pool);
		}
	}

	Database::SharedCounter::SharedCounter()
	{
		memset(m_counters, 0, sizeof(m_counters));
	}

	Database::SharedCounter::~SharedCounter()
	{
		for (size_t i = 0; i < TOTAL_ITEMS; i++)
		{
			delete m_counters[i].lock;
		}
	}

	void Database::SharedCounter::shutdown(thread_db* tdbb)
	{
		for (size_t i = 0; i < TOTAL_ITEMS; i++)
		{
			if (m_counters[i].lock)
				LCK_release(tdbb, m_counters[i].lock);
		}
	}

	SLONG Database::SharedCounter::generate(thread_db* tdbb, ULONG space, ULONG prefetch)
	{
		fb_assert(space < TOTAL_ITEMS);
		ValueCache* const counter = &m_counters[space];
		Database* const dbb = tdbb->getDatabase();

		SyncLockGuard guard(&dbb->dbb_sh_counter_sync, SYNC_EXCLUSIVE, "Database::SharedCounter::generate");

		if (!counter->lock)
		{
			Lock* const lock = FB_NEW_RPT(*dbb->dbb_permanent, sizeof(SLONG))
				Lock(tdbb, LCK_shared_counter, counter, blockingAst);
			counter->lock = lock;
			lock->lck_length = sizeof(SLONG);
			lock->lck_key.lck_long = space;
			LCK_lock(tdbb, lock, LCK_PW, LCK_WAIT);

			counter->isProtected = true;
			counter->curVal = 1;
			counter->maxVal = 0;
		}

		if (counter->curVal > counter->maxVal)
		{
			if (!counter->isProtected)
			{
				LCK_convert(tdbb, counter->lock, LCK_PW, LCK_WAIT);
				counter->isProtected = true;
			}

			counter->curVal = LCK_read_data(tdbb, counter->lock);

			if (!counter->curVal)
			{
				// zero IDs are somewhat special, so let's better skip them
				counter->curVal = 1;
			}

			counter->maxVal = counter->curVal + prefetch - 1;
			LCK_write_data(tdbb, counter->lock, counter->maxVal + 1);
		}

		return counter->curVal++;
	}

	int Database::SharedCounter::blockingAst(void* ast_object)
	{
		ValueCache* const counter = static_cast<ValueCache*>(ast_object);
		fb_assert(counter && counter->lock);
		Database* const dbb = counter->lock->lck_dbb;

		try
		{
			AsyncContextHolder tdbb(dbb);

			SyncLockGuard guard(&dbb->dbb_sh_counter_sync, SYNC_EXCLUSIVE, "Database::blockingAstSharedCounter");

			LCK_downgrade(tdbb, counter->lock);
			counter->isProtected = false;
		}
		catch (const Exception&)
		{} // no-op

		return 0;
	}
} // namespace
