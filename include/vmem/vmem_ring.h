/*
 * vmem_ring.h
 *
 *  Created on: Jun 11, 2024
 *      Author: JDN
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_RING_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_RING_H_

#include <vmem/vmem.h>

#define OFFSETS_ID 100
#define TAIL_ID 101
#define HEAD_ID 102

typedef struct {
    uint32_t size;
    uint32_t entries;
    char * filename;
	param_t * offsets_p;
	param_t * tail_p;
	param_t * head_p;
} vmem_ring_driver_t;

void vmem_ring_read(vmem_t * vmem, void * dataout, int offset);
void vmem_ring_write(vmem_t * vmem, const void * datain, uint32_t len);

#define VMEM_DEFINE_RING(name_in, strname, filename_in, size_in, max_entries) \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_FILE, \
		.name = strname, \
		.size = size_in + (max_entries * sizeof(uint32_t)) + 8, \
		.read = vmem_file_read, \
		.write = vmem_file_write, \
		.ack_with_pull = 1, \
	}; \
    PARAM_DEFINE_STATIC_VMEM(OFFSETS_ID, offsets, PARAM_TYPE_INT32, max_entries, sizeof(uint32_t), PM_CONF, NULL, NULL, name_in, size_in, "Ring buffer image offsets"); \
    PARAM_DEFINE_STATIC_VMEM(TAIL_ID, tail, PARAM_TYPE_INT32, -1, 0, PM_CONF, NULL, NULL, name_in, size_in + (max_entries * sizeof(uint32_t)), "Start of oldest image index"); \
    PARAM_DEFINE_STATIC_VMEM(HEAD_ID, head, PARAM_TYPE_INT32, -1, 0, PM_CONF, NULL, NULL, name_in, size_in + (max_entries * sizeof(uint32_t)) + 4, "End of newest image index"); \
    static vmem_ring_driver_t vmem_##name_in##_params = { \
        .size = size_in, \
        .entries = max_entries, \
        .filename = filename_in, \
		.offsets_p = offsets, \
		.tail_p = tail, \
		.head_p = head, \
	}; \
    vmem_##name_in.driver = &vmem_##name_in##_params; \

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_RING_H_ */
