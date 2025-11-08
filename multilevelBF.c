#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#define POOL_SIZE 20000
#define HDR_SIZE 32
#define ALIGN 32
#define LEVELS 11
#define NONE 0xFFFFFFFFu

typedef struct {
  uint32_t total;
  uint32_t prev;
  uint32_t next_free;
  uint32_t prev_free;
  uint32_t used;
  uint32_t pad[3];
} Header;

static void *pool_base = NULL;
static int initialized = 0;
static uint32_t free_head[LEVELS];
static uint32_t free_tail[LEVELS];

static inline uint32_t ptr2off(void *p){ return (uint32_t)((uintptr_t)p - (uintptr_t)pool_base); }
static inline Header* off2hdr(uint32_t o){ return (Header*)((uintptr_t)pool_base + o); }
static inline Header* right_of(Header* h){ return (Header*)((char*)h + h->total); }
static inline Header* left_of(Header* h){ return (h->prev==0)?NULL:(Header*)((char*)h - h->prev); }
static inline int within(void* p){ return p>=pool_base && (uintptr_t)p < (uintptr_t)pool_base + POOL_SIZE; }
static inline uint32_t round_up32(uint32_t x){ return (x + (ALIGN-1)) & ~(ALIGN-1); }

static int level_for_payload(uint32_t payload){
  int l=0; uint32_t v=payload;
  while(v>64 && l<LEVELS-1){ v>>=1; l++; }
  return l;
}

static void add_tail(int lvl, Header* h){
  h->next_free=NONE;
  h->prev_free=free_tail[lvl];
  if(free_tail[lvl]!=NONE) off2hdr(free_tail[lvl])->next_free=ptr2off(h);
  else free_head[lvl]=ptr2off(h);
  free_tail[lvl]=ptr2off(h);
}

static void remove_from_level(int lvl, Header* h){
  uint32_t off=ptr2off(h);
  uint32_t prev=h->prev_free,next=h->next_free;
  if(prev!=NONE) off2hdr(prev)->next_free=next; else free_head[lvl]=next;
  if(next!=NONE) off2hdr(next)->prev_free=prev; else free_tail[lvl]=prev;
  h->next_free=h->prev_free=NONE;
}

static void init_pool(){
  pool_base=mmap(NULL,POOL_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  for(int i=0;i<LEVELS;i++){ free_head[i]=free_tail[i]=NONE; }
  Header*h=(Header*)pool_base;
  h->total=POOL_SIZE; h->prev=0; h->next_free=NONE; h->prev_free=NONE; h->used=0;
  memset(h->pad,0,sizeof(h->pad));
  add_tail(level_for_payload(POOL_SIZE-HDR_SIZE),h);
  initialized=1;
}

static void coalesce_and_add(Header*h){
  Header*r=right_of(h);
  if(within(r)&&r->used==0){
    int lvl=level_for_payload(r->total-HDR_SIZE);
    remove_from_level(lvl,r);
    h->total+=r->total;
    Header*rr=right_of(h);
    if(within(rr)) rr->prev=h->total;
  }
  Header*l=left_of(h);
  if(l&&l->used==0){
    int lvl=level_for_payload(l->total-HDR_SIZE);
    remove_from_level(lvl,l);
    l->total+=h->total;
    Header*rr=right_of(l);
    if(within(rr)) rr->prev=l->total;
    h=l;
  }
  h->used=0;
  add_tail(level_for_payload(h->total-HDR_SIZE),h);
}

static Header* best_fit(int lvl,uint32_t need_total){
  Header*best=NULL; uint32_t best_size=0;
  for(uint32_t cur=free_head[lvl];cur!=NONE;cur=off2hdr(cur)->next_free){
    Header*c=off2hdr(cur);
    if(c->total>=need_total){
      if(!best||c->total<best_size){best=c;best_size=c->total;}
    }
  }
  return best;
}

static Header* find_block(uint32_t need_total){
  int lvl=level_for_payload(need_total-HDR_SIZE);
  for(int L=lvl;L<LEVELS;L++){
    Header*h=best_fit(L,need_total);
    if(h) return h;
  }
  return NULL;
}

static void split_and_alloc(Header*h,uint32_t need_total){
  uint32_t old=h->total;
  uint32_t rem=(old>need_total)?old-need_total:0;
  int lvl=level_for_payload(old-HDR_SIZE);
  remove_from_level(lvl,h);
  if(rem>=HDR_SIZE+ALIGN){
    h->total=need_total; h->used=1;
    Header*remh=(Header*)((char*)h+need_total);
    remh->total=rem; remh->prev=need_total; remh->used=0;
    remh->next_free=remh->prev_free=NONE;
    memset(remh->pad,0,sizeof(remh->pad));
    Header*right=right_of(remh);
    if(within(right)) right->prev=rem;
    add_tail(level_for_payload(rem-HDR_SIZE),remh);
  }else h->used=1;
}

void* malloc(size_t size){
  if(!initialized) init_pool();
  if(size==0){
    size_t maxp=0;
    for(int L=0;L<LEVELS;L++){
      for(uint32_t cur=free_head[L];cur!=NONE;cur=off2hdr(cur)->next_free){
        Header*h=off2hdr(cur);
        size_t p=h->total-HDR_SIZE;
        if(p>maxp) maxp=p;
      }
    }
    char buf[128];
    int n=snprintf(buf,sizeof(buf),"Max Free Chunk Size = %zu in bytes\n",maxp);
    if(n>0) write(1,buf,(size_t)n);
    munmap(pool_base,POOL_SIZE);
    pool_base=NULL; initialized=0;
    for(int i=0;i<LEVELS;i++){free_head[i]=free_tail[i]=NONE;}
    return NULL;
  }
  uint32_t need_payload=round_up32((uint32_t)size);
  uint32_t need_total=HDR_SIZE+need_payload;
  Header*h=find_block(need_total);
  if(!h) return NULL;
  split_and_alloc(h,need_total);
  return (char*)h+HDR_SIZE;
}

void free(void*ptr){
  if(!ptr||!pool_base) return;
  Header*h=(Header*)((char*)ptr-HDR_SIZE);
  if(h->used==0) return;
  h->used=0;
  coalesce_and_add(h);
}
