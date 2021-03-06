#include "ktsan.h"

#include <linux/list.h>
#include <linux/spinlock.h>

kt_tab_sync_t *kt_sync_ensure_created(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_sync_t *sync;
	bool created;
	uptr_t memblock_addr;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, &created, false);
	BUG_ON(sync == NULL); /* Ran out of memory. */

	if (created) {
		kt_clk_init(&sync->clk);
		sync->lock_tid = -1;
		INIT_LIST_HEAD(&sync->list);

		memblock_addr = kt_memblock_addr(addr);
		kt_memblock_add_sync(thr, memblock_addr, sync);

		kt_stat_inc(thr, kt_stat_sync_objects);
		kt_stat_inc(thr, kt_stat_sync_alloc);
	}

	return sync;
}

void kt_sync_destroy(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, true);
	BUG_ON(sync == NULL);

	spin_unlock(&sync->tab.lock);
	kt_cache_free(&kt_ctx.sync_tab.obj_cache, sync);

	kt_stat_dec(thr, kt_stat_sync_objects);
	kt_stat_inc(thr, kt_stat_sync_free);
}

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, addr);
	kt_clk_acquire(thr, &thr->clk, &sync->clk);
	spin_unlock(&sync->tab.lock);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, addr);
	kt_clk_acquire(thr, &sync->clk, &thr->clk);
	spin_unlock(&sync->tab.lock);
}
