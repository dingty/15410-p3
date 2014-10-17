#include "cr.h"
 #include <malloc.h>
#include "simics.h"
#include "mem_internals.h"

static KF *frame_base;
/* Initialize the whole memory system */
KF *mm_init() {

	// allocate 4k memory for kernel page directory
	uint32_t *PD = (uint32_t *)smemalign(4096, 1024 * 4); // should mark as global
	set_cr3((uint32_t)PD);
	lprintf("the pd uint32_t is %u", (unsigned int)PD);
	lprintf("the pd  is %p", PD);
	lprintf("cr3 is :%u", ((unsigned int)get_cr3()));
	// allocate PTs
	int i =0;
	for (i = 0; i < 1024; ++i)
	{
		void *PTE = smemalign(4096, 1024*4);
		
		PD[i] = (uint32_t)PTE;	
		// lprintf("the page table is in %p", PTE);
		// lprintf("check we get the correct thign: %x", (unsigned int)*(PD + i));	
	}

	frame_base = (KF*)smemalign(4096, 8 * 65536); // bunch of pointers that points to pages

	for (i = 0; i < 65536; ++i)
	{
		frame_base[i].flag=0;

		frame_base[i].address = (void *)(0x0+i*4096);
	}

	lprintf("the address for frame is %p", frame_base);

	// maping kernel address spaces

	// map first 4 entries in page directory
	int j;
	for ( i = 0; i < 4; ++i)
	{	
		
		uint32_t PT = PD[i];
		PD[i] |= 0x107;
		lprintf("the pt is %x", (unsigned int)PT);
		for (j = 0; j < 1024; ++j)
		{
			uint32_t k = (uint32_t)(frame_base[1024*i+j].address);
			frame_base[1024*i+j].flag = 1;
			// lprintf("the page address is %x", (unsigned int)k);
			*((uint32_t*)(PT) + j) =  k| 0x107;
		}
	}



	// enable paging
	lprintf("the cr0 %u", (unsigned int)get_cr0);
	set_cr4(get_cr4() | CR4_PGE);
	set_cr0(get_cr0() | CR0_PG);
	// MAGIC_BREAK;
	return frame_base;
}

void kerelmap() {

}


/* Map a virtual memory to physical memory */
uint32_t virtual2physical(uint32_t virtual_addr) {

	return 0;
}

/* Map this virtual address to a physical page */
void allocate_page(uint32_t virtual_addr, size_t size) {
	int i ;
	lprintf("the address is %x", (unsigned int)virtual_addr);

	uint32_t pd = virtual_addr >> 22;
	lprintf("the pd is %x", (unsigned int)pd);
	uint32_t pt = (virtual_addr & 0x3ff000)>>12;
	lprintf("the pt is %x", (unsigned int)pt);

	uint32_t offset = (virtual_addr & 0xfff)+ (uint32_t)size;
	lprintf("the offset is %x", (unsigned int)offset);

	uint32_t times = offset % 4096 == 0 ? offset/4096 : offset/4096+1;
int j =0;
	for (i = 0; i < 65536; ++i)
	{
		if (frame_base[i].flag == 0) {
			lprintf("times to allocation a page: %u", (unsigned int)times);
			times--;
			frame_base[i].flag = 1;

			uint32_t *PD = (uint32_t*)get_cr3();
			uint32_t pde = PD[pd];
			lprintf("page table entry: %x", (unsigned int)pde);

			uint32_t *PT = (uint32_t *)(pde & 0xfffffffc);
			lprintf("page table entry: %x", (unsigned int)PT);

		 	PT[pt+j] = (uint32_t)frame_base[i].address|0x3;
		 	lprintf("this is adjusted address %x  ",(unsigned int)frame_base[i].address);
		 				pde |= 0x3;
		 				lprintf("pde: %x", (unsigned int)pde);
			PD[pd]=pde;
			j++;
		 	// break;
		 	if (times == 0)
		 	{
		 		break;
		 	}
		 	
		}
	}


}

// void allocate_page(uint32_t virtual_addr, size_t size) {
// 	int i ;
// 	uint32_t pd = virtual_addr >> 22;
// 	uint32_t pt = virtual_addr & 0x3ff000;
// 	// uint32_t offset = virtual_addr & 0x111;
// 	for (i = 0; i < 65536; ++i)
// 	{
// 		if (frame_base[i].flag == 0) {
// 			frame_base[i].flag = 1;
// 			uint32_t *page_table = (uint32_t*)get_cr3()+pd;
// 		 	*(page_table + pt) = (uint32_t)frame_base[i].address|0x107;
// 		 	//lprintf("so therefore %x, %d", (unsigned int)*(page_table + pt), i);
// 		 	break;
// 		}
// 	}


// }

/* The reverse for allocate, free the memory out of the memory system */
void freeff(uint32_t *addr) {

}


