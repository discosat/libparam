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
	if (stream == NULL) {
        stream = fopen(driver->filename, "w+");
		return;
    }
    
    fseek(stream, 0, SEEK_SET);
	fread(&driver->tail, sizeof(uint32_t), 1, stream);
	fread(&driver->head, sizeof(uint32_t), 1, stream);
    fread(driver->offsets, sizeof(uint32_t), driver->entries, stream);
	fclose(stream);
}

void vmem_ring_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t offset) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;

    uint32_t head = driver->head;
    uint32_t tail = driver->tail;
    uint32_t * offsets = (uint32_t *)driver->offsets;

    int offset_int = (int)offset;
    uint32_t read_from_index = offset_int < 0 // supports negative indexing from head
        ? (head + offset_int + driver->entries) % driver->entries
        : (tail + offset_int) % driver->entries;
    uint32_t read_to_index = (read_from_index + 1) % driver->entries;

    uint32_t read_from_offset = offsets[read_from_index];
    uint32_t read_to_offset = offsets[read_to_index];
    
    int wraparound = read_to_offset < read_from_offset; 

    FILE *stream = fopen(driver->filename, "r+");
    if (stream == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    uint32_t driver_meta_size = (driver->entries + 2) * sizeof(uint32_t); // offset with initial metadata
    fseek(stream, read_from_offset + driver_meta_size, SEEK_SET);
    if (wraparound) {
        uint32_t len_fst = driver->data_size - read_from_offset;
        uint32_t len_snd = read_to_offset;
        fread(dataout, len_fst, 1, stream); // read first part
        fseek(stream, driver_meta_size, SEEK_SET);
        fread((char *)dataout + len_fst, len_snd, 1, stream); // read second part
    } else {
        uint32_t len = read_to_offset - read_from_offset;
        fread(dataout, len, 1, stream);
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

    FILE *stream = fopen(driver->filename, "r+");
    if (stream == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    uint32_t driver_meta_size = (driver->entries + 2) * sizeof(uint32_t); // offset with initial metadata
    fseek(stream, insert_offset + driver_meta_size, SEEK_SET);
    if (wraparound) {
        uint32_t len_fst = driver->data_size - insert_offset;
        uint32_t len_snd = new_head_offset;
        fwrite(datain, len_fst, 1, stream); // write first part
        fseek(stream, driver_meta_size, SEEK_SET);
        fwrite((const char *)datain + len_fst, len_snd, 1, stream); // write second part
    } else {
        fwrite(datain, len, 1, stream);
    }

    /* Update driver values (tail, head, offsets) */
    driver->tail = new_tail;
    driver->head = new_head;
    offsets[new_head] = new_head_offset;
    
    fseek(stream, 0, SEEK_SET);
    fwrite(&new_tail, sizeof(uint32_t), 1, stream);
    fwrite(&new_head, sizeof(uint32_t), 1, stream);
    fwrite(offsets, sizeof(uint32_t), driver->entries, stream);
    fclose(stream);
}
