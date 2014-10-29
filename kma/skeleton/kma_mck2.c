/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_MCK2
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"
/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define SIZE (18)
#define OFFSET (3)
typedef struct node
{
  struct node* next;
}list_item;

typedef struct pageinfo
{
  kma_page_t* page;
  struct pageinfo* next;
  struct pageinfo* prev;
  int size; 
}page_item;

typedef struct list
{
  int size;
  list_item* next;
}list;


typedef struct
{
  page_item* pagelist;
  list freelist[SIZE];
  int total_free;
  int total_alloc;
}mem_ctrl;


/************Global Variables*********************************************/
static kma_page_t* entry;
/************Function Prototypes******************************************/
void init_ctrl();
void* search_fit(int size);
int roundup_pow2(int v); 
void delete_node(int i,mem_ctrl* controller);
//void* get_free_block(int size);
void* get_new_page();
void add_node(void* ptr,int size);
int get_index(int size); 
void free_all();
int find_size(void* ptr);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  //printf("---------------------------Malloc---------------------------\n");
  // Judge if the request is larger than PAGESIZE
  if ((size + sizeof(void*)) > PAGESIZE)
	  return NULL;

  // Initialize the KMA control unit
  if (entry == NULL)
	  init_ctrl();
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  
  // Search for freebloc
  void* fit = search_fit(size);

  // Add total alloc to controller
  controller->total_alloc = controller->total_alloc + 1;
  
  //printf("Total_alloc: %d\n", controller->total_alloc);
  //printf("------------------------Malloc Done-------------------------\n");
  return fit;
}

void
kma_free(void* ptr, kma_size_t size)
{
  //printf("----------------------------Free----------------------------\n");
  // Find size of the block
  int block_size = find_size(ptr);

  // Add node to freelist
  add_node(ptr,block_size);
  
  // Add total alloc to controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  //controller->total_alloc = controller->total_alloc - 1;
  controller->total_free = controller->total_free + 1;
  //printf("Total_free: %d\n", controller->total_free);
  //printf("-------------------------Free done--------------------------\n");
  if(controller->total_alloc == controller->total_free)
	free_all();
  return;
}
void init_ctrl()
{
  //printf("----------------------------Init----------------------------\n");
  // Get the first page
  entry = get_page();
  
  // Save the pointer to the head of first page;
  *((kma_page_t**)entry->ptr) = entry;
  
  // Alloc memory for control unit 
  mem_ctrl* controller = entry->ptr + sizeof(kma_page_t*);
  controller->pagelist = (page_item*)((char*)controller + sizeof(mem_ctrl));
  controller->pagelist->page = (kma_page_t*)entry->ptr;
  controller->pagelist->next = NULL;
  controller->pagelist->prev = NULL;
  controller->pagelist->size = 8;
  int i = 0;
  for (; i < SIZE; i++)
  {
	controller->freelist[i].size = 1 << (i + OFFSET);
	controller->freelist[i].next = NULL;
  }
  char* addr = (char*)controller->pagelist + sizeof(page_item);
  char* addr_end = entry->ptr + PAGESIZE;
  for(; addr + controller->pagelist->size < addr_end; addr += controller->pagelist->size)
  {
	add_node(addr,controller->pagelist->size);
  }
  controller->total_alloc = 0;
  controller->total_free = 0;
  //printf("--------------------------Init Done--------------------------\n");
  return; 
}
int roundup_pow2(int v) 
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}
int get_index(int size) 
{
  if (size == 8)
	return 0;
  if (8 < size && size <= 16)
	return 1;
  if (16 < size && size <= 32)
	return 2;
  if (32 < size && size <= 64)
	return 3;
  if (64 < size && size <= 128)
	return 4;
  if (128 < size && size <= 256)
    return 5;
  if (256 < size && size <= 512)
    return 6;
  if (512 < size && size <= 1024)
    return 7;
  if (1024 < size && size <= 2048)
    return 8;
  if (2048 < size && size <= 4096)
    return 9;
  if (size > 4096)
	return 10;
  return -1;
}
void* search_fit(int size)
{
  //printf("-------------------------Search fit--------------------------\n");
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));

  // Round up size to power of 2
  int size_pow2 = roundup_pow2(size);
  // Find the freelist
  int i = 0;
  for(; i < SIZE; i++)
  {
	if (controller->freelist[i].size == size_pow2)
	  break;
  }

  // Walk the freelist to find fit
  list_item* fit = NULL;
  list p = controller->freelist[i];
  if(p.next)
  {
	fit = p.next;			
    delete_node(i,controller);
  }
  else
  {
	fit = get_new_page(size);		
  }
  //printf("------------------------Search Done--------------------------\n");
  return fit; 
}
void delete_node(int i,mem_ctrl* controller)
{
  //printf("------------------------Delete Node--------------------------\n");
  controller->freelist[i].next = controller->freelist[i].next->next;
  //printf("------------------------Delete Done--------------------------\n");
}

void* get_new_page(int size)
{
  //printf("------------------------Get new page-------------------------\n");
  
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  
  // Get new page
  kma_page_t* new_page = get_page();
  *((kma_page_t**)new_page->ptr) = new_page;
  page_item* new_page_item = (page_item*)(new_page->ptr + sizeof(kma_page_t*));
  new_page_item->next = NULL;
  new_page_item->page = (kma_page_t*)(new_page->ptr);
  size = roundup_pow2(size);
  new_page_item->size = size;

  // Add to page list
  page_item* p = controller->pagelist;
  while(p)
  {
	
	if(!(p->next))
	{
	  new_page_item->prev = p;
	  p->next = new_page_item;
	  break;
	}
	else
	  p = p->next;
  }
  
  char* addr_return;
  // Add the remain to freelist
  if(size > 4096)
    return (void*)new_page_item + sizeof(page_item);
  else
  {
    char* addr = (char*)new_page_item + sizeof(page_item);
	char* addr_end = (char*)new_page->ptr + PAGESIZE;
    addr_return = addr;
	addr = addr + size;
	for(; (addr + size) < addr_end;addr += size)
	{
	  add_node(addr,size);
	}
  }

  
  //printf("------------------------New page get-------------------------\n");
  return (void*)addr_return; 
}
void add_node(void* ptr,int size)
{

  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  int i = get_index(size);
  ((list_item*)ptr)->next = controller->freelist[i].next;
  controller->freelist[i].next = (list_item*)ptr;
  return;
}
int find_size(void* ptr)
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  
  // find which page the block is on
  unsigned long t = (unsigned long)ptr & (~(PAGESIZE - 1));
  
  // search the pagelist
  page_item* p = controller->pagelist;
  while(p)
  {
	unsigned long q = (unsigned long)(p->page) & (~(PAGESIZE - 1));
	if (t == q)
	  return p->size;
	else
      p = p->next;  
  }
 return -1;
}
void free_all()
{
  //printf("------------------------Free all page------------------------\n");
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  page_item* p = controller->pagelist;
  page_item* q;
  while(p)
  {
	kma_page_t* page = *(kma_page_t**)p->page;
	q = p->next;
	free_page(page);
    p = q;
  } 
  entry = NULL;
  //printf("------------------------All page free------------------------\n");
}
#	endif // KMA_P2FL
