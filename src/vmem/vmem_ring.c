/*
 * vmem_ring.c
 *
 *  Created on: Jun 11, 2024
 *      Author: JDN
 */

#include <string.h>
#include <stdio.h>
#include <vmem/vmem.h>
#include <vmem/vmem_ring.h>

void vmem_ring_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t offset) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;
    uint32_t head = param_get_uint32(driver->head_p);
    uint32_t tail = param_get_uint32(driver->tail_p);
    uint32_t read_index = offset < 0 // supports negative indexing from head (requires int for offset)
        ? (head + offset + driver->entries) % driver->entries
        : (tail + offset) % driver->entries;
    uint32_t read_from = param_get_uint32_array(driver->offsets_p, read_index);
    uint32_t read_to = param_get_uint32_array(driver->offsets_p, read_index+1);
    
    int wraparound = read_to < read_from; 

    FILE *fh = fopen(driver->filename, "r");
    if (fh == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    if (wraparound) {
        uint32_t len_fst = driver->size - read_from;
        uint32_t len_snd = read_to;
        fseek(read_from, SEEK_SET);
        size_t read_fst = fread(dataout, 1, len_fst, fh); // read first part
        fseek(0, SEEK_SET);
        size_t read_snd = fread(dataout + len_fst, 1, len_snd, fh); // read second part
    } else {
        uint32_t len = read_to - read_from;
        fseek(read_from, SEEK_SET);
        size_t read = fread(dataout, 1, len, fh);
    }
    fclose(fh);
}

static int overtake(int old, int target, int new) 
{
    if (old < target && target < new) return 1; // Simple overtake
    if (target < old && target < new && new < old) return 1; // Wraparound overtake
    return 0;
}

static int next(int current) 
{ 
    return (current + 1) % BUFFER_LIST_SIZE;
}

void vmem_ring_write(vmem_t * vmem, uint32_t addr, const void * datain, int len) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;
    uint32_t head = param_get_uint32(driver->head_p);
    uint32_t tail = param_get_uint32(driver->tail_p);
    uint32_t head_offset = param_get_uint32_array(driver->offsets_p, head);
    uint32_t tail_offset = param_get_uint32_array(driver->offsets_p, tail);
    
    /* Detect and react to wraparound read */
    int wraparound = head_offset + len > driver->size;
    uint32_t insert_offset = head_offset;
    uint32_t new_head_offset = (head_offset + len) % driver->size;

    if (overtake(head_offset, tail_offset, new_head_offset)) 
    {
        uint32_t next_tail = next(tail);
        uint32_t next_tail_offset = param_get_uint32_array(driver->offsets_p, next_tail);

        while (!overtake(tail_offset, new_head_offset, next_tail_offset))
        {
            tail_offset = next_tail_offset;
            next_tail = next(next_tail);
            next_tail_offset = param_get_uint32_array(driver->offsets_p, next_tail);
        }
        
        tail = next_tail;
    }

    uint32_t new_head = next(head);
    uint32_t new_tail = tail == new_head ? next(tail) : tail;

    FILE *fh = fopen(driver->filename, "a");
    if (fh == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    if (wraparound) {
        uint32_t len_fst = driver->size - insert_offset;
        uint32_t len_snd = new_head_offset;
        fseek(insert_offset, SEEK_SET);
        size_t wrote_fst = fwrite(datain, 1, len_fst, fh); // write first part
        fseek(0, SEEK_SET);
        size_t read_snd = fwrite(datain + len_fst, 1, len_snd, fh); // write second part
    } else {
        fseek(read_from, SEEK_SET);
        size_t wrote = fwrite(datain, 1, len, fh);
    }
    fclose(fh);
}
