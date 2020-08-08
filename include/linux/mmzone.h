#ifndef _LINUX_MMZONE_H
#define _LINUX_MMZONE_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>

/*
 * Free memory management - zoned buddy allocator.
 */

#ifndef CONFIG_FORCE_MAX_ZONEORDER
#define MAX_ORDER 10
#else
#define MAX_ORDER CONFIG_FORCE_MAX_ZONEORDER
#endif

typedef struct free_area_struct {
	struct list_head	free_list;
	unsigned long		*map;
} free_area_t;

struct pglist_data;

/*
 * On machines where it is needed (eg PCs) we divide physical memory
 * into multiple physical zones. On a PC we have 3 zones:
 *
 * ZONE_DMA	  < 16 MB	ISA DMA capable memory
 * ZONE_NORMAL	16-896 MB	direct mapped by the kernel
 * ZONE_HIGHMEM	 > 896 MB	only page cache and user processes
 */
typedef struct zone_struct {
	/*
	 * Commonly accessed fields:
	 */
	spinlock_t		lock;
	unsigned long		free_pages;
	unsigned long		pages_min, pages_low, pages_high;
	int			need_balance;

	/*
	 * free areas of different sizes
	 */
	free_area_t		free_area[MAX_ORDER];

	/*
	 * wait_table		-- the array holding the hash table
	 * wait_table_size	-- the size of the hash table array
	 * wait_table_shift	-- wait_table_size
	 * 				== BITS_PER_LONG (1 << wait_table_bits)
	 *
	 * The purpose of all these is to keep track of the people
	 * waiting for a page to become available and make them
	 * runnable again when possible. The trouble is that this
	 * consumes a lot of space, especially when so few things
	 * wait on pages at a given time. So instead of using
	 * per-page waitqueues, we use a waitqueue hash table.
	 *
	 * The bucket discipline is to sleep on the same queue when
	 * colliding and wake all in that wait queue when removing.
	 * When something wakes, it must check to be sure its page is
	 * truly available, a la thundering herd. The cost of a
	 * collision is great, but given the expected load of the
	 * table, they should be so rare as to be outweighed by the
	 * benefits from the saved space.
	 *
	 * __wait_on_page() and unlock_page() in mm/filemap.c, are the
	 * primary users of these fields, and in mm/page_alloc.c
	 * free_area_init_core() performs the initialization of them.
	 */
	wait_queue_head_t	* wait_table;
	unsigned long		wait_table_size;
	unsigned long		wait_table_shift;

	/*
	 * Discontig memory support fields.
	 */
	struct pglist_data	*zone_pgdat;
	struct page		*zone_mem_map;
	unsigned long		zone_start_paddr;
	unsigned long		zone_start_mapnr;

	/*
	 * rarely used fields:
	 */
	char			*name;
	unsigned long		size;
} zone_t;

#define ZONE_DMA		0
#define ZONE_NORMAL		1
#define ZONE_HIGHMEM		2
#define MAX_NR_ZONES		3

/*
 * One allocation request operates on a zonelist. A zonelist
 * is a list of zones, the first one is the 'goal' of the
 * allocation, the other zones are fallback zones, in decreasing
 * priority.
 *
 * Right now a zonelist takes up less than a cacheline. We never
 * modify it apart from boot-up, and only a few indices are used,
 * so despite the zonelist table being relatively big, the cache
 * footprint of this construct is very small.
 */
typedef struct zonelist_struct {
	zone_t * zones [MAX_NR_ZONES+1]; // NULL delimited
} zonelist_t;

#define GFP_ZONEMASK	0x0f

/*
 * The pg_data_t structure is used in machines with CONFIG_DISCONTIGMEM
 * (mostly NUMA machines?) to denote a higher-level memory zone than the
 * zone_struct denotes.
 *
 * On NUMA machines, each NUMA node would have a pg_data_t to describe
 * it's memory layout.
 *
 * XXX: we need to move the global memory statistics (active_list, ...)
 *      into the pg_data_t to properly support NUMA.
 */
struct bootmem_data;

/* 描述物理内存节点的数据结构：pg_data_t */
/* pg_data_t由pglist_data定义 */
typedef struct pglist_data {
	/* 该节点的管理区，ZONE_DMA,ZONE_NORMAL,ZONE_HIGHMEM */
	zone_t node_zones[MAX_NR_ZONES];/* MAX_NR_ZNES=3 */
	/*
	 * zonelists是按照分配时的管理区顺序排列。在调用free_area_init_core()时，
	 * 通过mm/page_alloc.c中的build_zonlists()建立顺序。
	 * 如果在ZONE_HIGHMEM中分配失败，就有可能还原成ZONE_NORMAL或ZONE_DMA。
	 */
	zonelist_t node_zonelists[GFP_ZONEMASK+1];
	/* 
	 * nr_zones表示该节点中的管理区数目，在1-3之间，并不是所有的节点都有3个管理区。
	 * 例如：一个CPU簇就可能没有ZONE_DMA。
	 */
	int nr_zones;
	struct page *node_mem_map;/* 指向struct page数组的第一个页面 */
	unsigned long *valid_addr_bitmap;/* 一张描述内存节点空洞的位图，只在Sparc/Sparc64使用，其他体系结构可忽略 */
	struct bootmem_data *bdata;/* 指向内存引导程序 */
	/*
	 * node_start_paddr表示节点的起始物理地址。
	 * 无符号长整型并不是最佳的选择，因为它会在IA32上被物理地址扩展(PAE)以及一些PowerPC上的变量拆散。
	 * 一种更好的解决办法是用页面帧号(PFN, Page Frame Number)记录该节点的起始物理地址。
	 * 一个PFN仅是一个简单的物理内存索引，以页面大小为基础的单位计算。
	 * 物理地址的PFN一般定义为(page_phys_addr >> PAGE_SHIFT)。
	 */
	unsigned long node_start_paddr;
	/*
	 * node_start_mapnr指出该节点在全局mem_map中的页面偏移。
	 * 在free_area_init_core()中，通过计算mem_map与该节点的局部mem_map中称为lmem_map之间的页面数，从而得到页面偏移
	 */
	unsigned long node_start_mapnr;/* 指出该节点在全局mem_map中的页面偏移 */
	unsigned long node_size;/* 该节点的页面总数 */
	int node_id;/* 节点的ID号(NID)，从0开始 */
	struct pglist_data *node_next;/* 指向下一个节点，该链表以NULL结束 */
} pg_data_t;

extern int numnodes;

/* 
 * pgdat_list是维护节点的链表
 * 所有节点都由一个称为pgdat_list的链表维护，这些节点都放在该链表中，均由函数init_bootmem_core()初始化节点
 */
extern pg_data_t *pgdat_list;

#define memclass(pgzone, classzone)	(((pgzone)->zone_pgdat == (classzone)->zone_pgdat) \
			&& ((pgzone) <= (classzone)))

/*
 * The following two are not meant for general usage. They are here as
 * prototypes for the discontig memory code.
 */
struct page;
extern void show_free_areas_core(pg_data_t *pgdat);
extern void free_area_init_core(int nid, pg_data_t *pgdat, struct page **gmap,
  unsigned long *zones_size, unsigned long paddr, unsigned long *zholes_size,
  struct page *pmap);

extern pg_data_t contig_page_data;

/**
 * for_each_pgdat - helper macro to iterate over all nodes
 * @pgdat - pg_data_t * variable
 *
 * Meant to help with common loops of the form
 * pgdat = pgdat_list;
 * while(pgdat) {
 * 	...
 * 	pgdat = pgdat->node_next;
 * }
 */
#define for_each_pgdat(pgdat) \
	for (pgdat = pgdat_list; pgdat; pgdat = pgdat->node_next)


/*
 * next_zone - helper magic for for_each_zone()
 * Thanks to William Lee Irwin III for this piece of ingenuity.
 */
static inline zone_t *next_zone(zone_t *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone - pgdat->node_zones < MAX_NR_ZONES - 1)
		zone++;

	else if (pgdat->node_next) {
		pgdat = pgdat->node_next;
		zone = pgdat->node_zones;
	} else
		zone = NULL;

	return zone;
}

/**
 * for_each_zone - helper macro to iterate over all memory zones
 * @zone - zone_t * variable
 *
 * The user only needs to declare the zone variable, for_each_zone
 * fills it in. This basically means for_each_zone() is an
 * easier to read version of this piece of code:
 *
 * for(pgdat = pgdat_list; pgdat; pgdat = pgdat->node_next)
 * 	for(i = 0; i < MAX_NR_ZONES; ++i) {
 * 		zone_t * z = pgdat->node_zones + i;
 * 		...
 * 	}
 * }
 */
#define for_each_zone(zone) \
	for(zone = pgdat_list->node_zones; zone; zone = next_zone(zone))


#ifndef CONFIG_DISCONTIGMEM

#define NODE_DATA(nid)		(&contig_page_data)
#define NODE_MEM_MAP(nid)	mem_map
#define MAX_NR_NODES		1

#else /* !CONFIG_DISCONTIGMEM */

#include <asm/mmzone.h>

/* page->zone is currently 8 bits ... */
#ifndef MAX_NR_NODES
#define MAX_NR_NODES		(255 / MAX_NR_ZONES)
#endif

#endif /* !CONFIG_DISCONTIGMEM */

#define MAP_ALIGN(x)	((((x) % sizeof(mem_map_t)) == 0) ? (x) : ((x) + \
		sizeof(mem_map_t) - ((x) % sizeof(mem_map_t))))

#endif /* !__ASSEMBLY__ */
#endif /* __KERNEL__ */
#endif /* _LINUX_MMZONE_H */
