/*
 * vmem_file.c
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#include <string.h>
#include <stdio.h>
#include <vmem/vmem.h>
#include <vmem/vmem_ring.h>

void vmem_ring_read(vmem_t * vmem, void * dataout, int offset) {
    vmem_ring_driver_t * driver = (vmem_ring_driver_t *)vmem->driver;
    uint32_t head = param_get_uint32(driver->head_p);
    uint32_t tail = param_get_uint32(driver->tail_p);
    uint32_t read_index = offset < 0 
        ? (head + offset + driver->entries) % driver->entries
        : (tail + offset) % driver->entries;
    uint32_t read_from = param_get_uint32_array(driver->offsets_p, read_index);
    uint32_t read_to = param_get_uint32_array(driver->offsets_p, read_index+1);
    
    /* Adjust read address and length in case of wraparound */
    int wraparound = read_to < read_from; 

    FILE *fh = fopen(driver->filename, "r+");
    if (fh == NULL)
		return;
    
    if (wraparound) {
        uint32_t len_fst = driver->size - read_from;
        uint32_t len_snd = read_to;
        fseek(read_from, SEEK_SET);
        int read_fst = fread(dataout, 1, len_fst, fh); // read first part
        fseek(0, SEEK_SET);
        int read_snd = fread(dataout + len_fst, 1, len_snd, fh); // read second part
    } else {
        uint32_t len = read_to - read_from;
        fseek(read_from, SEEK_SET);
        int read = fread(dataout, 1, len, fh);
    }
    fclose(fh);
}

void vmem_ring_write(vmem_t * vmem, const void * datain, int len) {
}
