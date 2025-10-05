#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Memory block header structure
typedef struct Block {
    struct Block *next;  // Pointer to next free block
    bool is_free;        // Flag to detect double-free
} Block;

// Memory pool structure
typedef struct {
    void *pool_start;    // Start of memory pool
    Block *free_list;    // Head of free list
    size_t block_size;   // Size of each block
    size_t num_blocks;   // Total number of blocks
    size_t blocks_used;  // Number of allocated blocks
} MemoryPool;

// Initialize memory pool
MemoryPool* pool_create(size_t block_size, size_t num_blocks) {
    if (block_size < sizeof(Block) || num_blocks == 0) {
        fprintf(stderr, "Invalid pool parameters\n");
        return NULL;
    }
    
    MemoryPool *pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    if (!pool) {
        fprintf(stderr, "Failed to allocate pool structure\n");
        return NULL;
    }
    
    // Align block size to pointer size for proper alignment
    block_size = (block_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    
    // Allocate the entire memory pool at once
    size_t pool_size = block_size * num_blocks;
    pool->pool_start = malloc(pool_size);
    if (!pool->pool_start) {
        fprintf(stderr, "Failed to allocate memory pool\n");
        free(pool);
        return NULL;
    }
    
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->blocks_used = 0;
    
    // Initialize free list - link all blocks
    pool->free_list = (Block*)pool->pool_start;
    Block *current = pool->free_list;
    
    for (size_t i = 0; i < num_blocks - 1; i++) {
        current->is_free = true;
        current->next = (Block*)((char*)current + block_size);
        current = current->next;
    }
    
    // Last block points to NULL
    current->is_free = true;
    current->next = NULL;
    
    return pool;
}

// Allocate a block from the pool
void* pool_alloc(MemoryPool *pool) {
    if (!pool || !pool->free_list) {
        fprintf(stderr, "Pool exhausted or invalid\n");
        return NULL;
    }
    
    // Get first free block
    Block *block = pool->free_list;
    pool->free_list = block->next;
    
    block->is_free = false;
    block->next = NULL;
    pool->blocks_used++;
    
    // Return pointer past the header
    return (void*)block;
}

// Free a block back to the pool
bool pool_free(MemoryPool *pool, void *ptr) {
    if (!pool || !ptr) {
        fprintf(stderr, "Invalid pool or pointer\n");
        return false;
    }
    
    // Check if pointer is within pool bounds
    uintptr_t pool_start = (uintptr_t)pool->pool_start;
    uintptr_t pool_end = pool_start + (pool->block_size * pool->num_blocks);
    uintptr_t ptr_addr = (uintptr_t)ptr;
    
    if (ptr_addr < pool_start || ptr_addr >= pool_end) {
        fprintf(stderr, "Pointer not from this pool\n");
        return false;
    }
    
    // Check if pointer is properly aligned
    if ((ptr_addr - pool_start) % pool->block_size != 0) {
        fprintf(stderr, "Misaligned pointer\n");
        return false;
    }
    
    Block *block = (Block*)ptr;
    
    // Detect double-free
    if (block->is_free) {
        fprintf(stderr, "Double-free detected!\n");
        return false;
    }
    
    // Add block back to free list
    block->is_free = true;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->blocks_used--;
    
    return true;
}

// Destroy memory pool
void pool_destroy(MemoryPool *pool) {
    if (pool) {
        free(pool->pool_start);
        free(pool);
    }
}

// Get pool statistics
void pool_stats(MemoryPool *pool) {
    if (!pool) return;
    
    printf("=== Memory Pool Statistics ===\n");
    printf("Block size: %zu bytes\n", pool->block_size);
    printf("Total blocks: %zu\n", pool->num_blocks);
    printf("Blocks used: %zu\n", pool->blocks_used);
    printf("Blocks free: %zu\n", pool->num_blocks - pool->blocks_used);
    printf("Memory usage: %.2f%%\n", 
           (pool->blocks_used * 100.0) / pool->num_blocks);
    printf("==============================\n");
}

// Test structure
typedef struct {
    int id;
    char name[32];
    double value;
} TestData;

// Demo program
int main() {
    printf("Memory Pool Allocator Demo\n\n");
    
    // Create pool for TestData structures
    MemoryPool *pool = pool_create(sizeof(TestData), 5);
    if (!pool) {
        return 1;
    }
    
    pool_stats(pool);
    
    // Allocate some blocks
    TestData *data1 = (TestData*)pool_alloc(pool);
    TestData *data2 = (TestData*)pool_alloc(pool);
    TestData *data3 = (TestData*)pool_alloc(pool);
    
    if (data1 && data2 && data3) {
        data1->id = 1;
        strcpy(data1->name, "First");
        data1->value = 3.14;
        
        data2->id = 2;
        strcpy(data2->name, "Second");
        data2->value = 2.71;
        
        data3->id = 3;
        strcpy(data3->name, "Third");
        data3->value = 1.41;
        
        printf("\nAllocated 3 blocks:\n");
        printf("Data1: id=%d, name=%s, value=%.2f\n", 
               data1->id, data1->name, data1->value);
        printf("Data2: id=%d, name=%s, value=%.2f\n", 
               data2->id, data2->name, data2->value);
        printf("Data3: id=%d, name=%s, value=%.2f\n\n", 
               data3->id, data3->name, data3->value);
    }
    
    pool_stats(pool);
    
    // Free a block
    printf("\nFreeing data2...\n");
    pool_free(pool, data2);
    pool_stats(pool);
    
    // Try double-free (should be detected)
    printf("\nAttempting double-free...\n");
    pool_free(pool, data2);
    
    // Allocate again (will reuse freed block)
    printf("\nAllocating new block (reuses freed memory)...\n");
    TestData *data4 = (TestData*)pool_alloc(pool);
    if (data4) {
        data4->id = 4;
        strcpy(data4->name, "Fourth");
        data4->value = 0.57;
    }
    pool_stats(pool);
    
    // Try to exhaust pool
    printf("\nAllocating remaining blocks...\n");
    TestData *data5 = (TestData*)pool_alloc(pool);
    TestData *data6 = (TestData*)pool_alloc(pool);
    TestData *data7 = (TestData*)pool_alloc(pool); // Should fail
    
    pool_stats(pool);
    
    // Cleanup
    pool_destroy(pool);
    printf("\nPool destroyed successfully!\n");
    
    return 0;
}
