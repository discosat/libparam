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
        fwrite(&driver->tail, sizeof(uint32_t), 1, stream);
        fwrite(&driver->head, sizeof(uint32_t), 1, stream);
        fwrite(driver->offsets, sizeof(uint32_t), driver->entries, stream);
		return;
    }
    
    fseek(stream, 0, SEEK_SET);
	fread(&driver->tail, sizeof(uint32_t), 1, stream);
	fread(&driver->head, sizeof(uint32_t), 1, stream);
    fread(driver->offsets, sizeof(uint32_t), driver->entries, stream);
	fclose(stream);
}

// Offset specifies the offset within the vmem ring buffer where the read should begin
void vmem_ring_read(vmem_t * vmem, uint32_t offset, void * dataout, uint32_t len) {

    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;

    int wraparound = (offset + len) > driver->data_size;

    FILE *stream = fopen(driver->filename, "r+");
    if (stream == NULL)
		return;
    
    /* Adjust read address and length in case of wraparound */
    uint32_t driver_meta_size = (driver->entries + 2) * sizeof(uint32_t); // offset with initial metadata
    fseek(stream, offset + driver_meta_size, SEEK_SET);
    if (wraparound) {
        uint32_t len_fst = driver->data_size - offset;
        uint32_t len_snd = (driver->data_size + offset) % driver->data_size;
        fread(dataout, len_fst, 1, stream); // read first part
        fseek(stream, driver_meta_size, SEEK_SET);
        fread((char *)dataout + len_fst, len_snd, 1, stream); // read second part
    } else {
        // This could also be fread(dataout, len, 1, stream), difference: https://stackoverflow.com/questions/295994/what-is-the-rationale-for-fread-fwrite-taking-size-and-count-as-arguments
        fread(dataout, 1, len, stream);
    }
    fclose(stream);
}

static int overtake(int before, int target, int after) 
{
    if (before < target && target < after) return 1; // Simple overtake
    if (target < before && target < after && after < before) return 1; // After wraparound overtake
    if (before < target && after < target && after < before) return 1; // Before wraparound overtake
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
