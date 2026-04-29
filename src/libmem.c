/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_area_struct *vma;
  struct vm_rg_struct *rg_node;

  if (mm == NULL || mm->mmap == NULL || rg_elmt == NULL)
    return -1;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  vma = get_vma_by_num(mm, rg_elmt->vmaid);
  if (vma == NULL)
    return -1;

  rg_node = vma->vm_freerg_list;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;
  else
    rg_elmt->rg_next = NULL;

  vma->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (mm == NULL)
    return NULL;

  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma;
  addr_t old_sbrk;

  pthread_mutex_lock(&mmvm_lock);

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL || alloc_addr == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ || size == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    caller->krnl->mm->symrgtbl[rgid].rg_next = NULL;
    caller->krnl->mm->symrgtbl[rgid].vmaid = vmaid;

    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  old_sbrk = cur_vma->sbrk;

  if (inc_vma_limit(caller, vmaid, size) < 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  caller->krnl->mm->symrgtbl[rgid].rg_next = NULL;
  caller->krnl->mm->symrgtbl[rgid].vmaid = vmaid;

  *alloc_addr = old_sbrk;
  cur_vma->sbrk = old_sbrk + size;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct *rgnode;
  struct vm_area_struct *cur_vma;
  struct vm_rg_struct *freerg_node;

  pthread_mutex_lock(&mmvm_lock);

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  rgnode = get_symrg_byid(caller->krnl->mm, rgid);

  if (cur_vma == NULL || rgnode == NULL || rgnode->rg_start >= rgnode->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  freerg_node = malloc(sizeof(struct vm_rg_struct));
  if (freerg_node == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;
  freerg_node->vmaid = vmaid;

  rgnode->rg_start = 0;
  rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  if (enlist_vm_freerg_list(caller->krnl->mm, freerg_node) < 0)
  {
    free(freerg_node);
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t addr;
  int val = __alloc(proc, 0, reg_index, size, &addr);

  if (val == -1)
    return -1;

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);

  if (val == -1)
    return -1;

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte;
  addr_t swpfpn;
  addr_t vicpgn;
  uint32_t vicpte;
  addr_t vicfpn;
  addr_t tgtfpn;
  addr_t newswpfpn;

  if (mm == NULL || caller == NULL || caller->krnl == NULL || fpn == NULL)
    return -1;

  pte = mm->pgd[pgn];

  if (pte == 0)
    return -1;

  if (PAGING_PAGE_PRESENT(pte) && !(pte & PAGING_PTE_SWAPPED_MASK))
  {
    *fpn = PAGING_FPN(pte);
    return 0;
  }

  if (!(pte & PAGING_PTE_SWAPPED_MASK))
    return -1;

  swpfpn = PAGING_SWP(pte);

  if (MEMPHY_get_freefp(caller->krnl->mram, &tgtfpn) == 0)
  {
    __swap_cp_page(caller->krnl->active_mswp, swpfpn, caller->krnl->mram, tgtfpn);
    pte_set_fpn(caller, pgn, tgtfpn);
    MEMPHY_put_freefp(caller->krnl->active_mswp, swpfpn);
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
    *fpn = tgtfpn;
    return 0;
  }

  if (find_victim_page(caller->krnl->mm, &vicpgn) == -1)
    return -1;

  vicpte = mm->pgd[vicpgn];
  if (!PAGING_PAGE_PRESENT(vicpte) || (vicpte & PAGING_PTE_SWAPPED_MASK))
    return -1;

  vicfpn = PAGING_FPN(vicpte);

  if (MEMPHY_get_freefp(caller->krnl->active_mswp, &newswpfpn) == -1)
    return -1;

  if (__mm_swap_page(caller, vicfpn, newswpfpn) < 0)
    return -1;

  pte_set_swap(caller, vicpgn, caller->krnl->active_mswp_id, newswpfpn);
  __swap_cp_page(caller->krnl->active_mswp, swpfpn, caller->krnl->mram, vicfpn);
  pte_set_fpn(caller, pgn, vicfpn);
  MEMPHY_put_freefp(caller->krnl->active_mswp, swpfpn);
  enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);

  *fpn = vicfpn;
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;
  int phyaddr;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  return MEMPHY_read(caller->krnl->mram, phyaddr, data);
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;
  int phyaddr;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  return MEMPHY_write(caller->krnl->mram, phyaddr, value);
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  struct vm_rg_struct *currg;
  struct vm_area_struct *cur_vma;

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL || data == NULL)
    return -1;

  currg = get_symrg_byid(caller->krnl->mm, rgid);
  cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (currg == NULL || cur_vma == NULL || currg->rg_start >= currg->rg_end)
    return -1;

  if (offset >= currg->rg_end - currg->rg_start)
    return -1;

  return pg_getval(caller->krnl->mm, currg->rg_start + offset, data, caller);
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc,
    uint32_t source,
    addr_t offset,
    uint32_t *destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  if (val == 0)
    *destination = data;

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  struct vm_rg_struct *currg;
  struct vm_area_struct *cur_vma;
  int ret;

  pthread_mutex_lock(&mmvm_lock);

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  currg = get_symrg_byid(caller->krnl->mm, rgid);
  cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (currg->rg_start >= currg->rg_end || offset >= currg->rg_end - currg->rg_start)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  ret = pg_setval(caller->krnl->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return ret;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,
    BYTE data,
    uint32_t destination,
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);

  if (val == -1)
    return -1;

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}


/*libkmem_malloc- alloc region memory in kmem
 *@caller: caller
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 */

int libkmem_malloc(struct pcb_t *caller, uint32_t size, uint32_t reg_index)
{
  return 0;
}


/*kmalloc - alloc region memory in kmem
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 *@alloc_addr: allocated address
 */
addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  return 0;
}

/*libkmem_cache_pool_create - create cache pool in kmem
 *@caller: caller
 *@size: memory size
 *@align: alignment size of each cache slot (identical cache slot size)
 *@cache_pool_id: cache pool ID
 */
int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id)
{
  return 0;
}

/*libkmem_cache_alloc - allocate cache slot in cache pool, cache slot has identical size
 * the allocated size is embedded in pool management mechanism
 *@caller: caller
 *@cache_pool_id: cache pool ID
 *@reg_index: memory region index
 */
int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index)
{
  addr_t addr = 0;
  addr = __kmem_cache_alloc(proc, -1, reg_index, cache_pool_id, &addr);
  return 0;
}

/*kmem_cache_alloc - alloc region memory in kmem cache
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@cache_pool_id: cached pool ID
 *@alloc_addr: allocated address
 */

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr)
{
  return 0;
}


int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  return 0;
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  return 1;
}


/*__read_kernel_mem - read value in kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  return 0;
}

/*__write_kernel_mem - write a kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  return 0;
}

/*__read_user_mem - read value in user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  return 0;
}


/*__write_user_mem - write a user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  return 0;
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum;
  int fpn;
  uint32_t pte;

  pthread_mutex_lock(&mmvm_lock);

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->krnl->mm->pgd[pagenum];

    if (pte == 0)
      continue;

    if (PAGING_PAGE_PRESENT(pte) && !(pte & PAGING_PTE_SWAPPED_MASK))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else if (pte & PAGING_PTE_SWAPPED_MASK)
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg;
  struct pgn_t *prev = NULL;

  if (mm == NULL || retpgn == NULL)
    return -1;

  pg = mm->fifo_pgn;

  if (!pg)
    return -1;

  while (pg->pg_next)
  {
    prev = pg;
    pg = pg->pg_next;
  }

  *retpgn = pg->pgn;

  if (prev != NULL)
    prev->pg_next = NULL;
  else
    mm->fifo_pgn = NULL;

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma;
  struct vm_rg_struct *rgit;
  struct vm_rg_struct *prev_rg = NULL;

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL || newrg == NULL)
    return -1;

  cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL)
    return -1;

  rgit = cur_vma->vm_freerg_list;
  if (rgit == NULL)
    return -1;

  newrg->rg_start = newrg->rg_end = -1;
  newrg->rg_next = NULL;
  newrg->vmaid = vmaid;

  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    {
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      {
        if (prev_rg == NULL)
          cur_vma->vm_freerg_list = rgit->rg_next;
        else
          prev_rg->rg_next = rgit->rg_next;

        free(rgit);
      }
      break;
    }

    prev_rg = rgit;
    rgit = rgit->rg_next;
  }

  if (newrg->rg_start == -1)
    return -1;

  return 0;
}

// #endif
