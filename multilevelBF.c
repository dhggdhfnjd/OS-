#include<stdio.h>
#include<sys/mman.h>
#include<unistd.h> 
void* pool_start;
//這樣是可以先儲存通諭整標，可以指向任何類型的資料，因為記憶體回傳位置不知道是哪裡來的資料
int pool_base,initial=0;
void *malloc(size_t size);
void free(void *ptr);
struct header
{
    int status;
    size_t size;
    struct header *prev;      
    struct header *next;
};
struct header *p,*free_list[11];
int store_free_list_index(size_t size)
{
    int k=0;
    for(int i = 5 ; i < 16; i++ )
    {
        if(i==5)
        {
            if(size == 1 << 5)
            {
                return 0;
            }
        }
        if( size >= 1 << i && size <= 1 << (i + 1))
        {
            return i-5;
        }
    }
    return 0;
}

struct header *find_best_fit(int idx, size_t size)
{
    struct header *current = free_list[idx];
    struct header *best_fit = NULL;
    size_t min = (size_t)-1; 
    while (current != NULL)
    {
        if (current->size >= size && current->status == 0)
        {
            size_t diff = current->size - size;
            if (diff < min)
            {
                min= diff;
                best_fit = current;
                if (min== 0) 
                    break;
            }
        }
        current = current->next;
    }
    return best_fit;
}

void insert_to_free_list(int idx, struct header *node)
{
    if(free_list[idx] == NULL)
    {
        free_list[idx] = node;
        node->next = NULL;
    }
    else
    {
        struct header *current = free_list[idx];
        free_list[idx]=node;
        node->next = current;
    }
}

void delete_in_free_list(int idx , struct header *node)
{
    if(free_list[idx] == NULL)
    {
        return;
    }
    else
    {
        struct header *current = free_list[idx];
        struct header *prev = NULL,*next = NULL;
        while (current != NULL && current != node) {
            prev = current;
            current = current->next;
        }
        if (current == NULL) return;          
        
        if (prev == NULL) {
            free_list[idx] = node->next;
            node->next = NULL;                
        } else {
            prev->next = current->next;
            node->next = NULL;
        }
    }
}

struct header *find_mr_one(size_t size,int idx)
{
    struct header *mr_one;
    for(int i = idx; i < 11; i++)
    {
        mr_one = find_best_fit(i,size);
        if(!mr_one)
        continue;
        else
        return mr_one;
    }
    return NULL;
}
// 這個函式找free_list中從idx開始到11的最大size，並回傳該最大size
size_t find_largest_free_size(int idx)
{
    size_t max_size = 0;
    for(int i = idx; i <= 10; i++)  // free_list陣列大小為11, index 0~10
    {
        struct header *current = free_list[i];
        while(current)
        {
            if (current->status == 0 && current->size > max_size)
                max_size = current->size;
            current = current->next;
        }
    }
    return max_size;
}
void *malloc(size_t size)
{
    int idx,space;
    struct header *love;
    if(initial == 0)
    {
        pool_start = mmap(NULL, 20000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        p = (struct header *) pool_start;
        p->status = 0;
        p->size = 20000-32; 
        p->prev = NULL; 
        idx = store_free_list_index(p->size);
        insert_to_free_list(idx, p);  
        initial = 1;
    }
    if (size == 0) {
        char buf[64];
        size_t max_size = find_largest_free_size(0);
        const char *prefix = "Max Free Chunk Size = ";
        write(1, prefix, 23);
    
        char tmp[32];
        int t = 0;
        if (max_size == 0) {
            tmp[t++] = '0';
        } else {
            while (max_size > 0) {
                tmp[t++] = '0' + (max_size % 10);
                max_size /= 10;
            }
        }
        for (int i = t - 1; i >= 0; --i) buf[t - 1 - i] = tmp[i];
        write(1, buf, t);
        write(1, "\n", 1);
    
        munmap(pool_start, 20000);
        pool_start = NULL;
        initial = 0;
        for (int i = 0; i < 11; i++) free_list[i] = NULL;
        return NULL;
    }    
    if(size % 32 == 0)
    {
        space=size;
    }
    else
    {
        space = (size/32+1) * 32;
    }
    idx = store_free_list_index(space);
    love = find_mr_one(space, idx);       
    if (love == NULL) return NULL;

    if(love == NULL)
        return NULL;
    idx = store_free_list_index(love->size);
    delete_in_free_list(idx,love);
    size_t origin_space=love->size;
    struct header *k = (struct header *)love; 
    k->size = space;  
    k->status = 1;
    k->prev = love->prev;   
    k->next = NULL;  
    size_t leftover = origin_space - space;
    if (leftover >= sizeof(struct header) + 32) 
    {struct header *remain = (struct header *)((char*)k + sizeof(struct header) + space);
    remain->status = 0;
    remain->size = origin_space - space - sizeof(struct header);
    remain->prev = k;
    struct header *right = (struct header *)((char*)remain + sizeof(struct header) + remain->size);
    if((char*)right < (char*)pool_start + 20000)
    right->prev = remain;
    idx = store_free_list_index(remain->size);
    insert_to_free_list(idx,remain);
    }
    return (void*)((char*)k + sizeof(struct header));
}
struct header *merge_left(struct header *h)
{
    if (h->prev && h->prev->status == 0)
    {
        int idx;
        idx = store_free_list_index(h->prev->size);
        delete_in_free_list(idx,h->prev);
        h->prev->size += sizeof(struct header) + h->size;
        h = h->prev;
        struct header *right = (struct header *)((char*)h + sizeof(struct header) + h->size);
        if((char*)right < (char*)pool_start + 20000)
            right->prev = h;
        return merge_left(h);
    }
    else
    {
        return h;
    }
}
struct header *merge_right(struct header *h)
{
    struct header *next = (struct header *)((char*)h + sizeof(struct header) + h->size);
    if((char*)next < (char*)pool_start + 20000 && next->status == 0)
    {
        int idx;
        idx = store_free_list_index((next)->size);
        delete_in_free_list(idx,(next));
        h->size += sizeof(struct header) + next->size;
        struct header *right = (struct header *)((char*)h + sizeof(struct header) + h->size);
        if((char*)right < (char*)pool_start + 20000)
            right->prev = h;
        return merge_right(h);
    }
    else
    {
        return h;
    }
}
void free(void *ptr)
{
    int idx;
    if(ptr == NULL) return;
    struct header *h = (struct header*)((char*)ptr - sizeof(struct header));
    if(h->status != 1) return;  
   
    h->status = 0;
    h = merge_left(h);
    h = merge_right(h);
    
    idx = store_free_list_index(h->size);
    insert_to_free_list(idx,h);
}
