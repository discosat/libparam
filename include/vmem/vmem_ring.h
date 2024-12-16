/*
 * vmem_ring.h
 *
 *  Created on: Jun 11, 2024
 *      Author: JDN
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_RING_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_RING_H_

#include <vmem/vmem.h>

typedef struct {
    uint32_t data_size;
    uint32_t entries;
    char * filename;
	void * offsets;
	uint32_t tail;
	uint32_t head;
} vmem_ring_driver_t;

void vmem_ring_init(vmem_t * vmem);
void vmem_ring_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t offset);
void vmem_ring_write(vmem_t * vmem, uint32_t addr, const void * datain, uint32_t len);
uint32_t vmem_ring_offset(vmem_t * vmem, uint32_t index, uint32_t offset);
uint32_t vmem_ring_element_size(vmem_t * vmem, uint32_t index);
int vmem_ring_is_valid_index(vmem_t * vmem, uint32_t index);
uint32_t vmem_ring_get_amount_of_elements(vmem_t * vmem);

#define VMEM_DEFINE_RING(name_in, strname, filename_in, size_in, entries_in) \
	uint32_t vmem_##name_in##_offsets[entries_in] = {0}; \
    static vmem_ring_driver_t vmem_##name_in##_driver = { \
        .data_size = size_in, \
        .entries = entries_in, \
        .filename = filename_in, \
		.offsets = vmem_##name_in##_offsets, \
		.tail = 0, \
		.head = 0, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_FILE, \
		.name = strname, \
		.size = size_in + ((entries_in + 2) * sizeof(uint32_t)), \
		.read = vmem_ring_read, \
		.write = vmem_ring_write, \
		.driver = &vmem_##name_in##_driver, \
		.ack_with_pull = 1, \
	}; \

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_RING_H_ */
