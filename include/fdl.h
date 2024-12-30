// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#ifndef _FDL_H_
#define _FDL_H_

#include <linux/types.h>
#include <part.h>

#define FDL_HANDSHAKE_ROMCODE      "romcode"
#define FDL_HANDSHAKE_FDL1         "fdl1"
#define FDL_HANDSHAKE_FDL2         "fdl2"

#define FDL_VERSION      "v1.8"
#define FDL_MAGIC        0x5C6D8E9F

#define FDL_COMMAND_LEN         (64)
#define FDL_RESPONSE_LEN        (64)
#define FDL_RESP_PLAYLOAD_LEN   (FDL_RESPONSE_LEN - FDL_STRUCT_5THF_BUT4TH_LEN)

#define FDL_PART_NAME_LEN        (36)
#define FDL_MAGIC_LEN            4
#if defined(CONFIG_FDL_RAW_DATA_DL) || defined(CONFIG_FDL_ROMCODE_PROTO)
#define FDL_SIZE_LEN             2
#define FDL_BYTECODE_LEN         2
#else
#define FDL_SIZE_LEN             4
#define FDL_BYTECODE_LEN         4
#endif
#define FDL_CHECKSUM_LEN         2

#define FDL_STRUCT_1STF_LEN             (FDL_MAGIC_LEN)
#define FDL_STRUCT_2NDF_LEN             (FDL_STRUCT_1STF_LEN + FDL_SIZE_LEN)
#define FDL_STRUCT_3RDF_LEN             (FDL_STRUCT_2NDF_LEN + FDL_BYTECODE_LEN)
#define FDL_STRUCT_2NDF_AND_3RDF_LEN    (FDL_SIZE_LEN + FDL_BYTECODE_LEN)
#define FDL_STRUCT_5THF_BUT4TH_LEN      (FDL_STRUCT_3RDF_LEN + FDL_CHECKSUM_LEN)

struct __packed fdl_header {
	uint32_t magic;
#if defined (CONFIG_FDL_RAW_DATA_DL) || defined(CONFIG_FDL_ROMCODE_PROTO)
	uint16_t size;
	uint16_t bytecode;
#else
	uint32_t size;
	uint32_t bytecode;
#endif
};

struct __packed fdl_packet {
	struct fdl_header head;
	char payload[];
};

/*
 * fdl_struct layout
 *
 * 1st field: magic (uint32_t)
 * 2nd field: size
 * 3rd field: bytecode
 * 4th field: payload
 * 5th field: checksum
 */

struct __packed fdl_struct {
	struct fdl_header head;
	char *payload_data;
	uint16_t checksum;
};

#ifdef CONFIG_FDL_DEBUG
struct __packed fdl_tx_msg_romcode {
	uint32_t addr;
	uint32_t size;
};

struct __packed fdl_tx_msg_fdl1 {
	uint64_t addr;
	uint64_t size;
};

struct __packed fdl_tx_msg_fdl2 {
	char part_name[72];
	uint64_t size;
	uint64_t reserved;
	uint32_t chksum;
};
#else
struct __packed fdl_tx_msg {
#if defined(CONFIG_FDL_ROMCODE_PROTO)
	uint32_t addr;
	uint32_t size;
#elif defined(CONFIG_FDL_FDL1_PROTO)
	uint64_t addr;
	uint64_t size;
#elif defined(CONFIG_FDL_FDL2_PROTO)
	char part_name[72];
	uint64_t size;
	uint64_t reserved;
	uint32_t chksum;
#endif
};
#endif

struct __packed fdl_raw_tx {
	uint32_t len;
	uint32_t chksum_en;
	uint32_t chksum;
};

struct __packed fdl_erase {
	uint64_t flag;
	char name[72];
	uint64_t size;
};

enum {
	FDL_PART_UNIT_MiB,
	FDL_PART_UNIT_512KiB,
	FDL_PART_UNIT_KiB,
	FDL_PART_UNIT_BYTE,
	FDL_PART_UNIT_SECTOR,
};

struct __packed fdl_part_header {
	uint32_t magic;
	uint8_t version;
	uint8_t unit;
	uint16_t count;
};

struct __packed fdl_part {
	char name[72];
	int64_t size;
	int64_t gap;
};

struct __packed fdl_part_table {
	uint16_t number;
	struct disk_partition part[];
};

/**
 * All known commands to FDL
 */
enum {
	FDL_COMMAND_HANDSHAKE = 0,
	FDL_COMMAND_CONNECT,
	FDL_COMMAND_START_DATA,
	FDL_COMMAND_MIDST_DATA,
	FDL_COMMAND_END_DATA,
	FDL_COMMAND_EXECUTE,
	FDL_COMMAND_CHANGE_BAUD,
	FDL_COMMAND_REBOOT,
	FDL_COMMAND_POWER_OFF,
	FDL_COMMAND_READ_CHIP_ID,
	FDL_COMMAND_ERASE_FLASH,
	FDL_COMMAND_REPARTITION,
	FDL_COMMAND_READ_CHIP_UID,
	FDL_COMMAND_START_READ_FLASH,
	FDL_COMMAND_MIDST_READ_FLASH,
	FDL_COMMAND_END_READ_FLASH,

	FDL_COMMAND_COUNT
};

/**
 * All known respones to FDL
 */
enum {
	FDL_RESPONSE_ACK,
	FDL_RESPONSE_VERSION,
	FDL_RESPONSE_INVAILD_CMD,
	FDL_RESPONSE_UNKNOW_CMD,
	FDL_RESPONSE_OPERATION_FAIL,
	FDL_RESPONSE_SECUREBOOT_VERIFY_ERROR,
	FDL_RESPONSE_INVALID_DEST_ADDR,
	FDL_RESPONSE_INVALID_SIZE,
	FDL_RESPONSE_VERIFY_CHECKSUM_FAIL,
	FDL_RESPONSE_INVALID_PARTITION,
	FDL_RESPONSE_FLASH_DATA,
	FDL_RESPONSE_CHIP_ID,
	FDL_RESPONSE_CHIP_UID,
	FDL_RESPONSE_COMPATIBILITY_DATA,

	FDL_RESPONSE_COUNT
};


/**
 * fdl_buf_addr - base address of the fdl download buffer
 */
extern void *fdl_buf_addr;

/**
 * fdl_buf_size - size of the fdl download buffer
 */
extern uint32_t fdl_buf_size;

uint16_t fdl_checksum(const char *buffer, int len);
uint32_t fdl_checksum32(uint32_t chksum, const char *buffer, uint32_t len);
uint32_t fdl_rawdata_checksum(const char *buffer, uint32_t len);

void fdl_ack(char *response);
void fdl_okay_null(int tag, char *response);
void fdl_fail_null(int tag, char *response);
void fdl_okay(int tag, const char *reason, char *response);
void fdl_fail(int tag, const char *reason, char *response);
void fdl_response_data(int tag, char *response,
                    const char *data, unsigned int size);
int fdl_get_resp_size(const char *response);

void fdl_command(int tag, char *command);
uint16_t fdl_get_comm_bytecode(int tag);
uint16_t fdl_get_resp_bytecode(int tag);
bool fdl_compare_comm_bytecode(struct fdl_struct *fdl, int tag);

void fdl_init(void *buf_addr, u32 buf_size);
u32 fdl_data_remaining(void);
int fdl_data_complete(char *response);
int fdl_handle_command(struct fdl_struct *fdl, char *response);
int fdl_data_download(const void *fdl_data, unsigned int fdl_data_len,
                      char *response);
int fdl_packet_check(struct fdl_struct *fdl, const char *packet,
                           char *response);

/* block device operation functions */
int fdl_blk_write_data(const char *part_name, size_t image_size);
int fdl_blk_write_partition(struct fdl_part_table *part_tab);
int fdl_blk_erase(const char *part_name, size_t size);

#endif