/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
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
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <math.h>
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

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
	
/************External Declaration*****************************************/

/**************Implementation***********************************************/
#define MAXSIZE 8192
// 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096

#define MINSIZE 16
#define PAGENUM 91
typedef struct {
void * next_buffer;
}buffer_header;

typedef struct {
buffer_header* buffer;
int size;
}flist_header;

typedef struct{
kma_page_t* page_ptr;
void* addr;

int alloc_num;
unsigned char bitmap[64];
}page_header; 

typedef struct{
kma_page_t* head;
void* page_next;
int alloc_num;
int page_num;
flist_header free_list[10];
page_header page[PAGENUM];
}sys_header;

sys_header*  main_header = 0;
flist_header* splitBlock(flist_header* list, kma_size_t size);
flist_header* coalBlock(flist_header* list, page_header* page);

void*
kma_malloc(kma_size_t size){
//printf("test\n");

int round_size = 16,i;
while(round_size< size) round_size=round_size<<1;
//printf("%d\n", round_size);
//printf("alloc, size == %d, round size = %d\n", size, round_size);

if (!main_header){
    kma_page_t* page = get_page();
    main_header = (sys_header*)page->ptr;
    main_header->page_num=0;
    main_header->alloc_num = 0;
    main_header->page_next = 0;
    main_header->head=page;
    for (i=0;i<10;i++){
	main_header->free_list[i].buffer=0;
        main_header->free_list[i].size = 16<<i;
    }
    for(i=0;i<PAGENUM;i++){
        main_header->page[i].addr=0;
        main_header->page[i].page_ptr=0;
    }
}

int free_list_num=0;
for(i=0;i<10;i++){
   if(main_header->free_list[i].size>=round_size &&
          main_header->free_list[i].buffer!=0 )
    {free_list_num = i+1;
    break;}
}
//printf("%d\n", free_list_num);
if (free_list_num==0){
page_header* new_page;
page_header* ret_page=0;
sys_header *temp = main_header;
while(!ret_page){
    for(i=0; i < PAGENUM ; i++){
	if (temp->page[i].page_ptr==0){
             ret_page= & temp->page[i];
             break;
        }
    }
    if(ret_page || temp->page_next==0)  break;
    temp =(sys_header*) temp->page_next;
}
if (!ret_page){ // get another main_page->next 
kma_page_t* page_new = get_page();
sys_header* temp_ret = (sys_header*)page_new->ptr;
temp_ret->head = page_new;
temp_ret->page_next=0;
temp_ret->page_num=0;
temp_ret->alloc_num=0;
for(i=0;i<PAGENUM;i++){
   temp_ret->page[i].addr=0;
   temp_ret->page[i].page_ptr=0;
}
for(i=0;i<10;i++){
    temp_ret->free_list[i].buffer=0;
    temp_ret->free_list[i].size = 16 << i;
}
temp->page_next = temp_ret;
temp = temp->page_next;
ret_page = & temp->page[0];

kma_page_t* page = get_page();
ret_page->page_ptr = page;
ret_page->addr = page->ptr;
ret_page->alloc_num=0;
for(i=0;i<64;i++)
   ret_page->bitmap[i]=0;

buffer_header* buffer =(buffer_header*) ret_page->addr;
buffer_header * temp_buff = main_header->free_list[9].buffer;
main_header->free_list[9].buffer = buffer;
buffer->next_buffer = temp_buff;

temp->page_num++;
main_header->page_num++;


}
else{  // if there is a page to allocate
kma_page_t* page = get_page();
ret_page->page_ptr = page;
ret_page->addr = page->ptr;
for(i=0;i<64;i++)
   ret_page->bitmap[i]=0;
ret_page->alloc_num = 0;
buffer_header *buffer =(buffer_header*) ret_page->addr;
buffer_header * temp_buff = (buffer_header*)main_header->free_list[9].buffer;
main_header->free_list[9].buffer = buffer;
buffer->next_buffer = temp_buff;

temp->page_num++;
main_header->page_num++;
}
new_page = ret_page;
//printf("round size is %d\n", round_size);
//printf("free list num is %d\n", free_list_num);

flist_header* list = splitBlock(&main_header->free_list[9], round_size);
buffer_header* ret_buff = list->buffer;
list->buffer = ret_buff->next_buffer;
void * ret = ret_buff;
int start = (int)(ret - new_page->addr);
int end =  start + round_size;
for(i = start/16;i<end/16;i++)
     new_page->bitmap[i/8] |= (1<<(i%8));
new_page->alloc_num++;
main_header->alloc_num++;
return ret;
}
else{ // if free_list_num !=0
//int list_num = free_list_num - 1;
free_list_num --;
//printf("round size is %d\n", round_size);
//printf("free list num is %d\n", free_list_num);
flist_header* list = splitBlock(&main_header->free_list[free_list_num], round_size);
//printf("list->buffer=%p\n", list->buffer);
buffer_header* ret_buff = list->buffer;
list->buffer = ret_buff->next_buffer;
void * ret = ret_buff;
sys_header* temp = main_header;
page_header* new_page = 0;
void* addr = (void*)(((long int)(((long int)ret - (long int)main_header)/PAGESIZE)) * PAGESIZE + (long int)main_header);
while(!new_page){
    for(i=0; i< PAGENUM;i++){
      if (temp->page[i].addr == addr){
	    new_page = &temp->page[i]; break;
      }
    }
    if (new_page || temp->page_next == 0)  break;
    temp = temp->page_next;
}
//printf("test\n");

int start = (int)(ret - new_page->addr);
int end = start + round_size;
for(i = start/16;i<end/16;i++)
     new_page->bitmap[i/8] |= (1<<(i%8));
new_page->alloc_num++;
main_header->alloc_num++;

return ret;
}

return NULL;
}

flist_header* splitBlock(flist_header* list, kma_size_t size){
//ret is the previous level list
flist_header* ret = (flist_header*)((long int)list - sizeof(flist_header));

if (list->size == size)
   {// printf("test\n");
return list;
}

buffer_header* temp = list->buffer;
list->buffer = temp->next_buffer;
buffer_header* new_buffer0 =  temp;
buffer_header* new_buffer1 = (buffer_header*)((long int)temp + ret->size);
buffer_header* temp1 = ret->buffer;
ret->buffer = new_buffer1;
new_buffer1->next_buffer = temp1;
buffer_header* temp2 = ret->buffer;
ret->buffer = new_buffer0;
new_buffer0->next_buffer = temp2;

if (ret->size >size)
    ret = splitBlock(ret, size);  

return ret;
} 
flist_header* coalBlock(flist_header* list, page_header* page){
// ret is the higher level list
flist_header* ret = (flist_header*)((long int)list + sizeof(flist_header));

buffer_header* temp1;
buffer_header* temp2;
int start = (int)((void*)list->buffer - page->addr);
int end = start + list->size;
int i;
unsigned char * bm = page->bitmap;
if (list->size == MAXSIZE)
  return list;
//printf("start = %d, end = %d\n", start, end);
//printf("list->size == %d \n", list->size);
if (start %(list->size*2) != 0){
  temp1 = (buffer_header*)((void*)list->buffer - list->size);
  temp2 = list->buffer;
  start -= list->size;
  end -= list->size;
 // printf("temp1= %p, temp2= %p\n",temp1, temp2);
  //printf("start = %d, end = %d\n", start, end);

  for( i = start/16; i < end/16; i++){
     
      if ( (1<< (i%8)) & bm[i/8])
         {// printf("i =%d , bm[i%8]=%x\n", i, bm[i%8]);
          // printf("list size = %d\n", list->size); 
          // printf("list buffer = %p\n", list->buffer);
           return list;
         }
   }

   buffer_header* ret_temp = 0;
  // printf("list buffer = %p\n", list->buffer) ;
   void* next_buff = list->buffer;
   while(next_buff){
	if (((buffer_header*)next_buff)->next_buffer == temp1){
	    ret_temp = temp1;
            ( (buffer_header*)next_buff )-> next_buffer = ret_temp->next_buffer;
            break;
        } 
        next_buff =((buffer_header*) next_buff) -> next_buffer;
   }
   buffer_header* ret_buff = list->buffer;
   list->buffer = ret_buff -> next_buffer;
   temp2 = ret_buff;

//printf("test\n");
}else{ // start%(list->size*2) == 0
   temp1 = list->buffer;
   temp2 = (buffer_header*)((void*)list->buffer + list->size);
   start += list->size;
   end += list->size;
   for( i = start/16; i < end/16; i++){
      if ( (1<< (i%8)) & bm[i/8])
         return list;
   }
   buffer_header* ret_temp = 0;
   void* next_buff = list->buffer;
   while(next_buff){
	if (((buffer_header*) next_buff)->next_buffer == temp2){
	    ret_temp = temp2;
           ((buffer_header*) next_buff) -> next_buffer = ret_temp->next_buffer;
            break;
        } 
        next_buff = ((buffer_header*)next_buff) -> next_buffer;
   }
   buffer_header* ret_buff = list->buffer;
   list->buffer = ret_buff -> next_buffer;
   temp1 = ret_buff;
}


buffer_header* temp_buff = ret->buffer;
ret->buffer = temp1;
temp1->next_buffer = temp_buff;

if (list->size < MAXSIZE)
    ret = coalBlock(ret, page);
return ret;
}

void 
kma_free(void* ptr, kma_size_t size)
{
int round_size = 16, i;
while(round_size < size)
     round_size = round_size <<1;
page_header* ret_page = 0;
sys_header* temp = main_header;
sys_header* prev = 0;
void * base_addr = (void*)((long int)main_header + (((long int)ptr -(long int) main_header)/ PAGESIZE) * PAGESIZE);
//printf("free, size == %d, round size = %d\n", size, round_size);
//printf("base_addr = %p\n", base_addr);
while(!ret_page){
  for (i=0; i< PAGENUM; i++){
	if (temp->page[i].addr == base_addr){
		ret_page = &temp->page[i];break;
        }
  }
  if (ret_page || temp->page_next == 0) break;
   prev = temp;
   temp = (sys_header*)temp->page_next;
}
//printf("page base addr is %p\n", ret_page);
flist_header* old_list = 0;
for(i=0;i<10;i++){
     // printf("free list i. size = %d\n", main_header->free_list[i].size);
       //printf("free list i. buffer = %p\n", main_header->free_list[i].buffer);

   if (main_header->free_list[i].size == round_size){
         //    printf("to insert a buffer at free list %d\n", i);
       old_list =& main_header->free_list[i];
       }     
}
// insert a piece of buffer


buffer_header* temp_buff = old_list->buffer;
old_list->buffer =(buffer_header*) ptr;
old_list->buffer->next_buffer = temp_buff;
//reset bitmap
int start = (int)(ptr - ret_page->addr);
int end = start + round_size;
for (i = start/16; i< end/16; i++)
      ret_page->bitmap[i/8] &=( ~(1<<(i%8)));

ret_page->alloc_num--;
main_header->alloc_num--;


flist_header* new_list = 0;
new_list = coalBlock(old_list, ret_page);
//printf("test\n");
//printf("alloc num is %d\n", ret_page->alloc_num);
//printf("page num is %d\n", main_header->page_num);



// free pages
if (ret_page->alloc_num == 0){
//  printf("test\n");
  new_list->buffer = new_list->buffer->next_buffer;
  free_page(ret_page->page_ptr);
  ret_page->addr = 0;
  ret_page->page_ptr = 0;
  main_header->page_num--;
  temp -> page_num--;
}
if (prev != 0 && temp->page_num == 0){
  // printf("test\n");
   prev->page_next = temp->page_next;
   free_page(temp->head);
   temp = 0;
}
if (main_header->page_num == 0){
   //printf("test\n");
   free_page(main_header->head);
   main_header = 0;
}


}
#endif // KMA_BUD
