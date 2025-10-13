#include <asm/mmu.h>
#include <asm/pgtable.h>

uint64_t arch_pgtl_page_paddr(uint64_t pgtle)
{
	return pgtle & PFN_MASK;
}

/**
 * @brief Check whether specified page table entry is pointing to huge page.
 *
 * PS(Page Size) flag indicates whether maps a 1-GByte page or 2MByte page or references a page directory table. This function
 * checks this flag. This function is typically used in the context of setting up or modifying page tables where it's
 * necessary to distinguish between large and regular page mappings.
 *
 * It returns the value that bit 7 is 1 if the specified pte maps a 1-GByte or 2MByte page, and 0 if references a page table.
 *
 * @param[in] pgtle The page directory pointer table entry to check.
 *
 * @return The value of PS flag in the PDPTE.
 *
 * @retval PAGE_PSE indicating mapping to a 1-GByte or 2MByte page.
 * @retval 0 indicating reference to a page directory table.
 *
 * @pre N/A
 *
 * @post N/A
 */
uint64_t arch_pgtl_large(uint64_t pgtle)
{
	return pgtle & PAGE_PSE;
}
