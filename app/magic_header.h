#ifndef __MAGIC_HEADER_H__
#define __MAGIC_HEADER_H__

#include <stdint.h>
#include <stdbool.h>
typedef enum {
    MAGIC_HEADER_TYPE_APP = 0,//”¶”√≥Ã–Ú

} magic_header_type_t;

bool magic_header_validate(void);
magic_header_type_t magic_header_get_type(void);
uint32_t magic_header_get_offset(void);
uint32_t magic_header_get_address(void);
uint32_t magic_header_get_length(void);
uint32_t magic_header_get_crc32(void);
#endif /* __MAGIC_HEADER_H__ */
