// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#ifndef _FDL_H_
#define _FDL_H_

#include <linux/types.h>
#include <part.h>
#include <hexdump.h>

typedef enum {
	FDL_LOGLEVEL_NONE = 0,
	FDL_LOGLEVEL_CRIT,
	FDL_LOGLEVEL_ERROR,
	FDL_LOGLEVEL_INFO,
	FDL_LOGLEVEL_DEBUG
} fdl_loglevel_t;

extern fdl_loglevel_t fdl_loglevel;
void fdl_set_loglevel(fdl_loglevel_t level);

#define FDL_LOG(level, fmt, ...)                                \
    do {                                                        \
        if (level <= fdl_loglevel) {                            \
            printf(fmt, ##__VA_ARGS__);                         \
        }                                                       \
    } while (0)

#define fdl_crit(fmt, ...)    FDL_LOG(FDL_LOGLEVEL_CRIT, fmt, ##__VA_ARGS__)
#define fdl_err(fmt, ...)     FDL_LOG(FDL_LOGLEVEL_ERROR, "[FDL ERROR] " fmt, ##__VA_ARGS__)
#define fdl_info(fmt, ...)    FDL_LOG(FDL_LOGLEVEL_INFO, "[FDL  INFO] " fmt, ##__VA_ARGS__)
#define fdl_debug(fmt, ...)   FDL_LOG(FDL_LOGLEVEL_DEBUG, "[FDL DEBUG] " fmt, ##__VA_ARGS__)

#if defined(CONFIG_FDL_VERBOSE)
#define FDL_DUMP(prefix, buffer, len)           \
	print_hex_dump(prefix, DUMP_PREFIX_OFFSET, 16, 64, buffer, len, false)
#else
#define FDL_DUMP(prefix, buffer, len)
#endif

#define FDL_HANDSHAKE_ROMCODE   "romcode"
#define FDL_HANDSHAKE_FDL1      "fdl1"
#define FDL_HANDSHAKE_FDL2      "fdl2"

#define FDL_VERSION             "v1.8"
#define FDL_MAGIC               0x5C6D8E9F

#define FDL_COMMAND_LEN         (64)
#define FDL_RESPONSE_LEN        (64)

#define FDL_PART_ID_MAX_LEN     (72)
#define FDL_PART_NAME_MAX_LEN   (FDL_PART_ID_MAX_LEN >> 1)
#define FDL_MAGIC_LEN           4
#if defined(CONFIG_FDL_RAW_DATA_DL) || defined(CONFIG_FDL_ROMCODE_PROTO)
#define FDL_SIZE_LEN            2
#define FDL_BYTECODE_LEN        2
#else
#define FDL_SIZE_LEN            4
#define FDL_BYTECODE_LEN        4
#endif
#define FDL_CHECKSUM_LEN        2

/*
 * FDL Protocol Format
 * -------------------------------------------------------
 * | magic | size | bytecode | payload | checksum |
 * -------------------------------------------------------
 *
 * magic:     4 bytes, magic number, fixed to 0x5C6D8E9F
 * size:      2 or 4 bytes, size of the payload
 * bytecode:  2 or 4 bytes, command or response bytecode
 * payload:   variable(0 ~ N), data or response payload
 * checksum:  2 bytes, checksum of the size + bytecode + payload
 *
 */
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

#define FDL_MAGIC_OFFSET             offsetof(struct fdl_header, magic)
#define FDL_SIZE_OFFSET              offsetof(struct fdl_header, size)
#define FDL_BYTECODE_OFFSET          offsetof(struct fdl_header, bytecode)
#define FDL_PAYLOAD_OFFSET           offsetof(struct fdl_packet, payload)
#define FDL_MAGIC_END                (FDL_MAGIC_OFFSET + FDL_MAGIC_LEN)
#define FDL_SIZE_END                 (FDL_SIZE_OFFSET + FDL_SIZE_LEN)
#define FDL_BYTECODE_END             (FDL_BYTECODE_OFFSET + FDL_BYTECODE_LEN)

#define FDL_PROTO_FIXED_LEN          (sizeof(struct fdl_packet) + FDL_CHECKSUM_LEN)
/* fixed length (size + bytecode) used to caculate the checksum */
#define FDL_CHECKSUM_FIXED_LEN       (FDL_SIZE_LEN + FDL_BYTECODE_LEN)
#define FDL_RESP_PLAYLOAD_MAX_LEN    (FDL_RESPONSE_LEN - FDL_PROTO_FIXED_LEN)

struct __packed fdl_info {
	struct fdl_header head;
	uint16_t checksum;
	char *payload_data;
};

/*
 * FDL TX Message Format
 * ROMCODE and FDL1 Protocol:
 * -------------------------------------------------------
 * | addr | size |
 * -------------------------------------------------------
 *
 * addr: 4 bytes, start address of the binary image to load
 * size: 4 bytes, size of the binary image to download
 *
 * FDL2 Protocol:
 * -------------------------------------------------------
 * | name | size | reserved | chksum |
 * -------------------------------------------------------
 *
 * name: 72 bytes, partition name
 * size: 8 bytes, partition size
 * reserved: 8 bytes, reserved
 * chksum: 4 bytes, checksum of the payload data
 */
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
	char name[FDL_PART_ID_MAX_LEN];
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
	char part_name[FDL_PART_ID_MAX_LEN];
	uint64_t size;
	uint64_t reserved;
	uint32_t chksum;
#endif
};
#endif


/*
 * FDL TX Payload Data Format
 * -------------------------------------------------------
 * | len | chksum_en | chksum |
 * -------------------------------------------------------
 *
 * len: 4 bytes, length of the payload data
 * chksum_en: 4 bytes, checksum enable flag
 * chksum: 4 bytes, checksum of the payload data
 */
struct __packed fdl_tx_info {
	uint32_t len;
	uint32_t chksum_en;
	uint32_t chksum;
};

/*
 * FDL Erase Payload Data Format
 * -------------------------------------------------------
 * | flag | name | size |
 * -------------------------------------------------------
 *
 * flag: 8 bytes, 0x0 for the whole device to earse
 *                0x1 for erasing by partition
 * name: 72 bytes, partition name to erase
 * size: 8 bytes, partition size to erase
 */
struct __packed fdl_erase {
	uint64_t flag;
	char name[FDL_PART_ID_MAX_LEN];
	uint64_t size;
};

enum {
	FDL_PART_UNIT_MiB,
	FDL_PART_UNIT_512KiB,
	FDL_PART_UNIT_KiB,
	FDL_PART_UNIT_BYTE,
	FDL_PART_UNIT_SECTOR,
};


/*
 * FDL Partition Payload Data Header Format
 * -------------------------------------------------------
 * | magic | version | unit | count |
 * -------------------------------------------------------
 *
 * magic: 4 bytes, magic number, fixed to 0x5C6D8E9F
 * version: 1 byte, version number
 * unit: 1 byte, partition size unit (0 ~ 4)
 * count: 2 bytes, partition count
 */
struct __packed fdl_part_header {
	uint32_t magic;
	uint8_t version;
	uint8_t unit;
	uint16_t count;
};

/*
 * FDL Partition Payload Data Format
 * -------------------------------------------------------
 * | name | size | gap |
 * -------------------------------------------------------
 *
 * name: 72 bytes, partition name
 * size: 8 bytes, partition size
 * gap: 8 bytes, partition gap
 */
struct __packed fdl_part {
	char name[FDL_PART_ID_MAX_LEN];
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

/*
 * fdl_execute_received - number of FDL execute command received
 */
extern uint32_t fdl_execute_received;

/*
 * fdl_tx_payload_info - FDL TX payload information
 */
extern struct fdl_tx_info fdl_tx_payload_info;


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
const char *fdl_get_command(uint16_t bytecode);
bool fdl_compare_command_by_tag(struct fdl_info *fdl, int tag);

void fdl_init(void *buf_addr, uint32_t buf_size);
u32 fdl_data_remaining(void);
int fdl_data_complete(char *response);
int fdl_handle_command(struct fdl_info *fdl, char *response);
int fdl_packet_check(struct fdl_info *fdl, const char *packet,
                           char *response);
int fdl_data_download(const void *fdl_data, uint32_t fdl_data_len,
                           char *response);

/* block device operation functions */
int fdl_blk_write_data(const char *part_name, size_t image_size);
int fdl_blk_write_partition(struct fdl_part_table *part_tab);
int fdl_blk_erase(const char *part_name, size_t size);

#if defined(CONFIG_FDL_UART)
 int fdl_uart_download(int dev_idx, bool timeout);
#else
static inline int fdl_uart_download(int dev_idx, bool timeout) { return -ENOSYS; }
#endif

#if defined (CONFIG_FDL_USB)
 int fdl_usb_download(int dev_idx, bool timeout);
#else
static inline int fdl_usb_download(int dev_idx, bool timeout) { return -ENOSYS; }
#endif

#endif