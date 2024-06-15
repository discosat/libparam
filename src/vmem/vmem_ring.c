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

void vmem_ring_init(vmem_t * vmem) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *) vmem->driver;

	FILE * stream = fopen(driver->filename, "r+");
	if (stream == NULL)
		return;
    
    fseek(stream, driver->data_size, SEEK_SET);
	fread(driver->tail, 1, sizeof(uint32_t), stream);
	fread(driver->head, 1, sizeof(uint32_t), stream);
    fread(driver->offsets, 1, driver->entries * sizeof(uint32_t), stream);
	fclose(stream);
}

void vmem_ring_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t offset) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;

    uint32_t head = driver->head;
    uint32_t tail = driver->tail;
    uint32_t * offsets = (uint32_t *)driver->offsets;

    // uint32_t read_from_index = offset < 0 // supports negative indexing from head (requires int for offset)
    //     ? (head + offset + driver->entries) % driver->entries
    //     : (tail + offset) % driver->entries;
    uint32_t read_from_index = (tail + offset) % driver->entries;
    uint32_t read_to_index = (read_from_index + 1) % driver->entries;

    uint32_t read_from_offset = offsets[read_from_index];
    uint32_t read_to_offset = offsets[read_to_index];
    
    int wraparound = read_to_offset < read_from_offset; 

    FILE *stream = fopen(driver->filename, "r");
    if (stream == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    if (wraparound) {
        uint32_t len_fst = driver->data_size - read_from_offset;
        uint32_t len_snd = read_to_offset;
        fseek(stream, read_from_offset, SEEK_SET);
        size_t read_fst = fread(dataout, 1, len_fst, stream); // read first part
        fseek(stream, 0, SEEK_SET);
        size_t read_snd = fread(dataout + len_fst, 1, len_snd, stream); // read second part
    } else {
        uint32_t len = read_to_offset - read_from_offset;
        fseek(stream, read_from_offset, SEEK_SET);
        size_t read = fread(dataout, 1, len, stream);
    }
    fclose(stream);
}

static int overtake(int old, int target, int new) 
{
    if (old < target && target < new) return 1; // Simple overtake
    if (target < old && target < new && new < old) return 1; // Wraparound overtake
    return 0;
}

static int next(int current, int max) 
{ 
    return (current + 1) % max;
}

void vmem_ring_write(vmem_t * vmem, uint32_t addr, const void * datain, uint32_t len) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;
    
    uint32_t head = driver->head;
    uint32_t tail = driver->tail;
    uint32_t * offsets = (uint32_t *)driver->offsets;
    uint32_t head_offset = offsets[head];
    uint32_t tail_offset = offsets[tail];
    
    /* Detect and react to wraparound read */
    int wraparound = head_offset + len > driver->data_size;
    uint32_t insert_offset = head_offset;
    uint32_t new_head_offset = (head_offset + len) % driver->data_size;

    if (overtake(head_offset, tail_offset, new_head_offset)) 
    {
        uint32_t next_tail = next(tail, driver->entries);
        uint32_t next_tail_offset = offsets[next_tail];

        while (!overtake(tail_offset, new_head_offset, next_tail_offset))
        {
            tail_offset = next_tail_offset;
            next_tail = next(next_tail, driver->entries);
            next_tail_offset = offsets[next_tail];
        }
        
        tail = next_tail;
    }

    uint32_t new_head = next(head, driver->entries);
    uint32_t new_tail = tail == new_head ? next(tail, driver->entries) : tail;

    FILE *stream = fopen(driver->filename, "a");
    if (stream == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    if (wraparound) {
        uint32_t len_fst = driver->data_size - insert_offset;
        uint32_t len_snd = new_head_offset;
        fseek(stream, insert_offset, SEEK_SET);
        size_t wrote_fst = fwrite(datain, 1, len_fst, stream); // write first part
        fseek(stream, 0, SEEK_SET);
        size_t wrote_snd = fwrite(datain + len_fst, 1, len_snd, stream); // write second part
    } else {
        fseek(stream, insert_offset, SEEK_SET);
        size_t wrote = fwrite(datain, 1, len, stream);
    }

    /* Update driver values (tail, head, offsets) */
    driver->tail = new_tail;
    driver->head = new_head;
    offsets[new_head] = new_head_offset;
    
    fseek(stream, driver->data_size, SEEK_SET);
    fwrite(&new_tail, 1, sizeof(uint32_t), stream);
    fwrite(&new_head, 1, sizeof(uint32_t), stream);
    fwrite(offsets, 1, driver->entries * sizeof(uint32_t), stream);
    fclose(stream);
}
