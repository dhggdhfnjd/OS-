#include<stdio.h>
#include<sys/mman.h>
#include<unistd.h> 
#include<string.h>
#include<stdint.h>
void* pool_start;
int pool_base, initial = 0;
void *malloc(size_t size);
void free(void *ptr);

struct header
{
    int status; 
    size_t size;
    struct header *prev;      
    struct header *next;
};

struct header *p, *free_list[11];

int store_free_list_index(size_t size)
{
    for(int i = 5; i < 16; i++)
    {
        if(size < 32)  
        {
            return 0;
        }
        if(size >= (1 << i) && size < (1 << (i + 1)))
        {
            return i - 4;  
        }
    }
    return 10;  
}

struct header *find_best_fit(int idx, size_t size)
{
    struct header *current = free_list[idx];
    struct header *best_fit = NULL;
    size_t min_diff = (size_t)-1; 
    
    while (current != NULL)
    {
        if (current->size >= size && current->status == 0)
        {
            size_t diff = current->size - size;
            if (diff < min_diff)
            {
                min_diff = diff;
                best_fit = current;
                if (min_diff == 0) 
                    break;
            }
        }
        current = current->next;
    }
    return best_fit;
}

void insert_to_free_list(int idx, struct header *node)
{
    node->next = NULL;
    
    if(free_list[idx] == NULL)
    {
        free_list[idx] = node; 
    }
    else
    {
        struct header *current = free_list[idx];
        while(current->next != NULL)
        {
            current = current->next;
        }
        current->next = node;  
    }
}
void delete_in_free_list(int idx, struct header *node)
{
    if(free_list[idx] == NULL)
        return;
    
    struct header *current = free_list[idx];
    struct header *prev = NULL;
    
    while (current != NULL && current != node) 
    {
        prev = current;
        current = current->next;
    }
    
    if (current == NULL) return;
    
    if (prev == NULL) 
    {
        free_list[idx] = node->next;
    } 
    else 
    {
        prev->next = current->next;
    }
    node->next = NULL;
}

struct header *find_mr_one(size_t size, int idx)
{
    struct header *mr_one;
    for(int i = idx; i < 11; i++)
    {
        mr_one = find_best_fit(i, size);
        if(mr_one)
            return mr_one;
    }
    return NULL;
}

size_t find_largest_free_size(int idx)
{
    size_t max_size = 0;
    for(int i = idx; i <= 10; i++)
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

static inline int within(void *p) 
{
    return p >= pool_start && (char*)p < (char*)pool_start + 20000;
}

void *malloc(size_t size)
{
    write(1, "MALLOC CALLED\n", 14);
    
    if(initial == 0)
    {
        pool_start = mmap(NULL, 20000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (pool_start == MAP_FAILED)
            return NULL;
            
        p = (struct header *) pool_start;
        p->status = 0;
        p->size = 20000 - sizeof(struct header); 
        p->prev = NULL;
        p->next = NULL;
        
        int idx = store_free_list_index(p->size);
        insert_to_free_list(idx, p);  
        initial = 1;
    }
    
    if (size == 0)
    {
        size_t max_size = find_largest_free_size(0);
        const char *prefix = "Max Free Chunk Size = ";
        write(1, prefix, strlen(prefix));

        char tmp[32];
        int t = 0;
        if (max_size == 0) {
            tmp[t++] = '0';
        } else {
            size_t temp = max_size;
            while (temp > 0 && t < 32) {
                tmp[t++] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        
        char out[32];
        for (int i = 0; i < t; ++i) 
            out[i] = tmp[t - 1 - i];
        write(1, out, t);
        write(1, "\n", 1);

        munmap(pool_start, 20000);
        pool_start = NULL;
        initial = 0;
        for (int i = 0; i < 11; ++i) 
            free_list[i] = NULL;
        return NULL;
    }
    
    size_t space;
    if(size % 32 == 0)
        space = size;
    else
        space = ((size / 32) + 1) * 32;
    
    int idx = store_free_list_index(space);
    struct header *love = find_mr_one(space, idx);
    
    if (love == NULL) 
        return NULL;
    
    idx = store_free_list_index(love->size);
    delete_in_free_list(idx, love);
    
    size_t origin_space = love->size;
    size_t leftover = origin_space - space;
    
    if (leftover >= sizeof(struct header) + 32) 
    {
        love->size = space;
        love->status = 1;  
        love->next = NULL;
        
        struct header *remain = (struct header *)((char*)love + sizeof(struct header) + space);
        remain->status = 0;
        remain->size = leftover - sizeof(struct header);
        remain->prev = love;
        remain->next = NULL;
        
        struct header *right = (struct header *)((char*)remain + sizeof(struct header) + remain->size);
        if (within(right))
            right->prev = remain;
        
        int idx2 = store_free_list_index(remain->size);
        insert_to_free_list(idx2, remain);
    } 
    else 
    {
        love->size = origin_space;
        love->status = 1;  
        love->next = NULL;
    }
    
    return (void*)((char*)love + sizeof(struct header));
}

struct header *merge_left(struct header *h)
{
    if (h->prev && within(h->prev) && h->prev->status == 0) 
    {
        int idx = store_free_list_index(h->prev->size);
        delete_in_free_list(idx, h->prev);
        
        h->prev->size += sizeof(struct header) + h->size;
        h = h->prev;
        
        struct header *right = (struct header *)((char*)h + sizeof(struct header) + h->size);
        if (within(right)) 
            right->prev = h;
        
        return merge_left(h);
    }
    return h;
}

struct header *merge_right(struct header *h)
{
    struct header *next = (struct header *)((char*)h + sizeof(struct header) + h->size);
    
    if (within(next) && next->status == 0) 
    {
        int idx = store_free_list_index(next->size);
        delete_in_free_list(idx, next);
        
        h->size += sizeof(struct header) + next->size;
        
        struct header *right = (struct header *)((char*)h + sizeof(struct header) + h->size);
        if (within(right)) 
            right->prev = h;
        
        return merge_right(h);
    }
    return h;
}

void free(void *ptr)
{
    write(1, "FREE CALLED\n", 12);
    
    if (!ptr || !pool_start) 
        return;
    
    if ((char*)ptr <= (char*)pool_start ||
        (char*)ptr >= (char*)pool_start + 20000)
        return;
    
    struct header *h = (struct header*)((char*)ptr - sizeof(struct header));
    
    if (h->status != 1) 
        return;
    
    h->status = 0;
    h->next = NULL;
    
    h = merge_left(h);
    h = merge_right(h);
    
    int idx = store_free_list_index(h->size);
    insert_to_free_list(idx, h);
}