/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
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
#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

#define printf(arg...) ;

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct node
{
  int size;
  void* addr;
  struct node* next;
  struct node* prev;
}list;
typedef struct pageinfo
{
  kma_page_t* addr;
  struct pageinfo* next;
  struct pageinfo* prev;
}page_info;
typedef struct
{
  page_info* pagelist;
  list* freelist;
  list* emptylist;
  list* listtail;
  int freesize;
  int total_alloc;
  int total_free;
}mem_ctrl;
/************Global Variables*********************************************/
static kma_page_t* entry = NULL;
/************Function Prototypes******************************************/

void init_ctrl();
void* search_fit(int size);
void update_list(list* node,int size);
list* add_node(void* addr,int size,list* node);
list* set_new_page();
page_info* get_new_page();
//void insert_node();
list* search_list();
void merge_block();
void free_all();
void add_empty_list(list* node);
list* search_empty_list();
list* search_page(void* ptr);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void add_empty_list(list* node)
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  if(!controller->emptylist)
    {
	  controller->emptylist = node;
	  node->next = NULL;
	}
  else
	{
	  node->next = controller->emptylist;
	  controller->emptylist = node;
	}
}
list* search_empty_list()
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  if(controller->emptylist)
  {
	list* p = controller->emptylist;
	controller->emptylist = controller->emptylist->next;
	return p;
  }
  else
  {
	return NULL;
  }
}
void*
kma_malloc(kma_size_t size)
{
  //printf("------------------------------------Malloc----------------------------------------\n");
  //Judge if the request is larger than PAGESIZE
  if((size + sizeof(void*)) > PAGESIZE)
    return NULL;

  // Initialize the KMA control unit
  if(entry == NULL)
    init_ctrl();

  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));

  // Search for first fit
  void* fit = search_fit(size);

  // Add total alloc
  controller->total_alloc = controller->total_alloc + 1;
  //printf("----------------------------------Malloc Done-------------------------------------\n");
  return fit;
}

void
kma_free(void* ptr, kma_size_t size)
{
  //printf("-------------------------------------Free-----------------------------------------\n");
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  // Search same page free block
  list* same_page = search_page(ptr);
  
  // Merge or not 
  if(same_page)
  {
	merge_block(ptr,same_page,size);
  }
  else
  { 
	add_node(ptr,size,NULL);
  }

  // Add total free
  controller->total_free = controller->total_free + 1;

  // If done, free all pages
  if(controller->total_alloc == controller->total_free)
	free_all();
  
  //printf("-----------------------------------Free Done--------------------------------------\n");
  return; 
}

void init_ctrl()
{
  //Get the first page  
  kma_page_t* first_page = get_page();

  //Save the pointer to the head of first page;
  *((kma_page_t**)first_page->ptr) = first_page;
  //Init entry
  entry = first_page;
  
  //Alloc memory for controll unit;
  mem_ctrl* controller = first_page->ptr + sizeof(kma_page_t*);
  controller->pagelist = (page_info*)((char*)controller + sizeof(mem_ctrl));
  controller->freelist = (list*)((char*)controller->pagelist + sizeof(page_info));
  controller->emptylist = NULL;
  controller->total_alloc = 0;
  controller->total_free = 0;
 
  
  //Get new page
  kma_page_t* new_page = get_page();
  *((kma_page_t**)new_page->ptr) = new_page;
  page_info* new_page_info;
  new_page_info =(page_info*) (new_page->ptr + sizeof(kma_page_t*));
  new_page_info->prev = controller->pagelist;
  new_page_info->next = NULL;
  new_page_info->addr = (kma_page_t*)new_page->ptr;
  //Init pagelist and freelist
  controller->pagelist->addr = (kma_page_t*)first_page->ptr;
  controller->pagelist->next = new_page_info ;
  controller->pagelist->prev = NULL;

  controller->freelist->addr = (char*)new_page_info + sizeof(page_info);
  controller->freelist->size = PAGESIZE - sizeof(kma_page_t*) - sizeof(page_info);  
  controller->freelist->next = NULL;
  controller->freelist->prev = NULL;
  controller->listtail = controller->freelist;
  controller->freesize = PAGESIZE - sizeof(kma_page_t*) - sizeof(page_info) -sizeof(list);
  return;
}

void* search_fit(int size)
{
  
  // Search freelist
  list* fit = search_list(size);
  // Return result
  if(fit)
  {
    void* p = fit->addr;
	update_list(fit,size);
	return p;
  }
  else 
  {
	fit = set_new_page();
    void* p = fit->addr;
	update_list(fit,size);
	return p;
  }
}

list* search_list(int size)
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  //printf("*****************Search freelist for first fit*********************\n");
  //Search freelist
  list* p = controller->freelist;
  while(p)
    {
	  if(p->size < size)
	  {
		p = p->next;
	  }
	  else
	  {
	    //printf("******************************Gottcha******************************\n");
		return p;
	  }
	}
  //printf("*******************************Not find****************************\n");
  return NULL;
}

void update_list(list* node,int size)
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  //printf("****************************update_list****************************\n");
  if(node->size < size)
  {
	perror("You made a mistake!");
    return;
  }

  // update list
  if(node->size == size)
  {
	if(node->prev)
	  node->prev->next = node->next;
	else
	  controller->freelist = node->next;
	if(node->next)
	  node->next->prev = node->prev;
    add_empty_list(node);
  }
  else
  {
    node->size = node->size - size;
	node->addr = node->addr + size;
  }
  //printf("****************************update complete*************************\n");
}

list* set_new_page()
{
  
  //printf("****************************Set new page*******************************\n");
  // Get new page
  page_info* new_page_info = get_new_page();
  
  int size = PAGESIZE - sizeof(kma_page_t*) - sizeof(page_info);
  list* q = add_node((void*)new_page_info + sizeof(page_info),size,NULL);
  //printf("**************************Set new page done****************************\n");
  return q;
}
list* add_node(void* addr,int size,list* pos)
{  
  //printf("****************************Add new node*******************************\n");
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  list* p = controller->freelist;
  
  // Search empty list
  list* t = search_empty_list();
  
  // Alloc memory for new node
  if(!t)
  {
	if(controller->freesize >= 10 * sizeof(list))
	{
	  t = (list*)((char*)controller->listtail + sizeof(list));
	  controller->listtail = t;
	  controller->freesize = controller->freesize - sizeof(list);
	}
	else
	{
	  page_info* new_page_info = get_new_page();
	  t = (list*)((char*)new_page_info + sizeof(page_info));
	  controller->listtail = t;
	  controller->freesize = PAGESIZE - sizeof(kma_page_t*) - sizeof(page_info) - sizeof(list);
	}
  }

  // Add node
  t->addr = addr;
  t->size = size;
  t->next = pos;
  if(pos)
  {
    if(pos->prev)
	{
	  pos->prev->next = t;
	  t->prev = pos->prev;
	  pos->prev = t;
	}
	else
	{ 
	  controller->freelist = t;
	  pos->prev = t;	
	  t->prev = NULL;
	}
  }
  else
  {
    if(!p)
	{
	  t->prev = NULL;
	  controller->freelist = t;
	}
	while(p)
	{
	  if(p->next)
	    p = p->next;
	  else
	  {
		p->next = t;
		t->prev = p;
		break;
	  }
	}
  }
  return t;
  //printf("****************************New Node add*******************************\n");
}
  
page_info* get_new_page()
{
  //printf("****************************Get New page*******************************\n");
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  // Get new page
  
  kma_page_t* new_page = get_page();
  *((kma_page_t**)new_page->ptr) = new_page;
  page_info* new_page_info;
  new_page_info =(page_info*) (new_page->ptr + sizeof(kma_page_t*));
  new_page_info->next = NULL;
  new_page_info->addr = (kma_page_t*)new_page->ptr;
  
  // Add to pagelist
  page_info* p = controller->pagelist;
  while(p)
  {
	if(!(p->next))
	  {
	    p->next = new_page_info;
		new_page_info->prev = p->next; 
	    break;
	  }
	else
	  {
		p = p->next;
	  }
  }
  //printf("****************************New page get*******************************\n");
  return new_page_info;
}

list* search_page(void* ptr)
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));
  
  //printf("************************Search for same_page**************************\n");

  // t is used to judge whether ptr and p are in same page
  void* t = (void*)((unsigned long)ptr & (~(PAGESIZE - 1)));
 
  //search the freelist
  list* p = controller->freelist;
  while(p)
  {
    void* m = p->addr;
	void* q =(void*)((unsigned long)m & (~(PAGESIZE - 1)));
	if(t == q)
	{
	  return p;
	}
	else
	  p = p->next;
  }
  return p;
}

void merge_block(void* ptr,list* same_page,int size)
{
  //printf("****************************Merge********************************\n");
  void* t = (void*)((unsigned long)ptr & (~(PAGESIZE - 1)));
  void* q = (void*)((unsigned long)same_page->addr & (~(PAGESIZE - 1)));
  // Search for blocks to merge
  while(t == q)
  {
	if(ptr < same_page->addr)
	{ 
	  if((ptr + size) == same_page->addr)
      {
	    same_page->addr = ptr;
	    same_page->size = same_page->size + size;
		//printf("****************************Merged********************************\n");
	    return;
	  }
	  else
	  {
	    add_node(ptr,size,same_page); 
        //printf("****************************Merged********************************\n");
		return;
	  }
	}
	else
	{
	  if((same_page->addr + same_page->size) == ptr)
	  {
		same_page->size = size + same_page->size;
        //printf("****************************Merge********************************\n");
	    return;
	  }
	  else
	  {
	    same_page = same_page->next;
        q = (void*)((unsigned long)same_page & (~(PAGESIZE - 1)));
	  }
	}
  }
  add_node(ptr,size,same_page);
  //printf("****************************Merge********************************\n");
  return;
}

void free_all()
{
  // Get controller
  mem_ctrl* controller = (mem_ctrl*)(entry->ptr + sizeof(kma_page_t*));

  page_info* p = controller->pagelist;
  page_info* q;
  while(p)
  {
	q = p->next;
	kma_page_t* page = *(kma_page_t**)p->addr;
	free_page(page);
	p = q;
    
  }
  entry = NULL;
}

#endif // KMA_RM
