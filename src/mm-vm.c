/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  if (mm == NULL || mm->mmap == NULL)
    return NULL;

  struct vm_area_struct *pvma = mm->mmap;

  while (pvma != NULL)
  {
    if ((int)pvma->vm_id == vmaid)
      return pvma;

    pvma = pvma->vm_next;
  }

  return NULL;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn, addr_t swpfpn)
{
  __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct *newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (cur_vma == NULL)
    return NULL;

  newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL)
    return NULL;

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = cur_vma->sbrk + size;
  newrg->rg_next = NULL;
  newrg->vmaid = vmaid;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  struct vm_area_struct *vma;
  struct vm_area_struct *cur_area;

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
    return -1;

  if (vmastart >= vmaend)
    return -1;

  vma = caller->krnl->mm->mmap;
  cur_area = get_vma_by_num(caller->krnl->mm, vmaid);

  if (cur_area == NULL)
    return -1;

  while (vma != NULL)
  {
    if (vma != cur_area)
    {
      if (!(vmaend <= vma->vm_start || vmastart >= vma->vm_end))
        return -1;
    }
    vma = vma->vm_next;
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  struct vm_rg_struct *newrg;
  struct vm_area_struct *cur_vma;
  addr_t inc_amt;
  addr_t old_end;
  int incnumpage;

  if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL)
    return -1;

  cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL)
    return -1;

  inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  if (inc_amt == 0)
    return 0;

  newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL)
    return -1;

  old_end = cur_vma->vm_end;
  incnumpage = inc_amt / PAGING_PAGESZ;

  newrg->rg_start = old_end;
  newrg->rg_end = old_end + inc_amt;
  newrg->rg_next = NULL;
  newrg->vmaid = vmaid;

  if (validate_overlap_vm_area(caller, vmaid, newrg->rg_start, newrg->rg_end) < 0)
  {
    free(newrg);
    return -1;
  }

  cur_vma->vm_end = newrg->rg_end;

  if (vm_map_ram(caller, newrg->rg_start, newrg->rg_end,
                 old_end, incnumpage, newrg) < 0)
  {
    cur_vma->vm_end = old_end;
    free(newrg);
    return -1;
  }

  free(newrg);
  return 0;
}

// #endif
