// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <env.h>
#include <fdl.h>
#include <hexdump.h>

#ifdef CONFIG_FDL_PACKAGE_DATA_DL
#error "Not support package data download"
#endif

/**
 * fdl_buf_addr - base address of the fdl download buffer
 */
void *fdl_buf_addr;

/**
 * fdl_buf_size - size of the fdl download buffer
 */
u32 fdl_buf_size;

uint16_t fdl_checksum(const char *buffer, int len)
{
	uint32_t sum = 0;

	while (len > 1) {
		sum += *(uint16_t*)buffer;
		buffer += sizeof(uint16_t);
		len -= sizeof(uint16_t);
	}

	if (len == 1)
		sum += *buffer;

	sum = (sum & 0xFFFF) + (sum >> 16);
	sum += (sum >> 16);

	return (uint16_t)(~sum);
}

uint32_t fdl_checksum32(uint32_t chksum, const char *buffer, uint32_t len)
{
	while (len--)
		chksum += *buffer++;

	return chksum;
}

uint32_t fdl_rawdata_checksum(const char *buffer, uint32_t len)
{
	uint32_t checksum;

	while (len > 3) {
		checksum += *(uint32_t*)buffer;
		buffer += sizeof(uint32_t);
		len -= sizeof(uint32_t);
	}

	while (len--)
		checksum += *buffer++;

	return checksum;
}

int fdl_packet_check(struct fdl_struct *fdl, const char *packet,
                        char *response)
{
	struct fdl_packet *pp = NULL;
	struct fdl_header *head = NULL;
	uint16_t chksum, calc_chksum;
	uint32_t magic, bytecode, size;
	int len = FDL_STRUCT_2NDF_AND_3RDF_LEN;

	if (unlikely(!packet) ||
	     unlikely(!fdl)) {
		fdl_fail_null(FDL_RESPONSE_INVAILD_CMD, response);
		return -EINVAL;
	}

	pp = (void *)packet;
	head = (void *)pp;
	magic = head->magic;
	bytecode = head->bytecode;
	size = head->size;
	debug("magic: 0x%X size: 0x%x, bytecode: 0x%X\n", magic, size, bytecode);

	chksum = *(uint16_t *)(pp->payload + head->size);
	calc_chksum = fdl_checksum((void *)&head->size, size + len);
	if (chksum != calc_chksum) {
		debug("calc_chksum: 0x%x mismatch chksum: 0x%x\n", calc_chksum, chksum);
		fdl_fail_null(FDL_RESPONSE_VERIFY_CHECKSUM_FAIL, response);
		return -EINVAL;
	}

	head = (void *)fdl;
	head->magic = FDL_MAGIC;
	head->bytecode = bytecode;
	head->size = size;
	fdl->payload_data = pp->payload;
	fdl->checksum = chksum;

	return 0;
}

int fdl_response_check(const char *respone)
{
	struct fdl_header *head = (void *)respone;

	if (head->magic != FDL_MAGIC)
		return -EINVAL;

	return 0;
}

int fdl_get_resp_size(const char *response)
{
	struct fdl_header *head = (void *)response;

	if (fdl_response_check(response))
		return 0;

	return head->size + FDL_STRUCT_5THF_BUT4TH_LEN;
}

/**
 * fdl_response() - Writes a response.
 *
 * @tag: identify type of the response
 * @response: Pointer to fdl response buffer
 * @format: printf style format string
 */
void fdl_response(int tag, char *response,
                    const char *format, ...)
{
	struct fdl_packet *pp = NULL;
	struct fdl_header *head = NULL;
	char *buffer = NULL;
	int len = FDL_STRUCT_2NDF_AND_3RDF_LEN;
	uint16_t chksum, *p;
	va_list args;

	pp = (void *)response;
	head = (void *)pp;
	head->magic = FDL_MAGIC;
	head->bytecode = fdl_get_resp_bytecode(tag);

	if (format) {
		va_start(args, format);
		vsnprintf(pp->payload,
		     FDL_RESP_PLAYLOAD_LEN - 1,
		     format, args);
		va_end(args);

		head->size = strlen(pp->payload);
	} else {
		head->size = 0;
	}

	buffer = (void *)&head->size;
	len += head->size;
	chksum = fdl_checksum(buffer, len);
	p = (uint16_t *)(pp->payload + head->size);
	*p = chksum;
}

void fdl_response_data(int tag, char *response,
                    const char *data, unsigned int size)
{
	struct fdl_packet *pp = NULL;
	struct fdl_header *head = NULL;
	char *payload = NULL;
	uint32_t len = FDL_STRUCT_2NDF_AND_3RDF_LEN;
	uint16_t chksum, *p;

	if (unlikely(!data)) {
		fdl_fail_null(FDL_RESPONSE_INVAILD_CMD, response);
		return;
	}

	pp = (void *)response;
	head = (void *)pp;
	head->magic = FDL_MAGIC;
	head->bytecode = fdl_get_resp_bytecode(tag);
	head->size = size;

	memcpy(pp->payload, data, size);

	payload = (void *)&head->size;
	len += head->size;
	chksum = fdl_checksum(payload, len);
	p = (uint16_t *)(pp->payload + head->size);
	*p = chksum;
}

/**
 * fdl_fail() - Write a FAIL response.
 *
 * @tag: identify type of the response
 * @reason: Pointer to returned reason string
 * @response: Pointer to fdl response buffer
 */
void fdl_fail(int tag, const char *reason, char *response)
{
	fdl_response(tag, response, "%s", reason);
}


void fdl_fail_null(int tag, char *response)
{
	fdl_response(tag, response, NULL);
}

/**
 * fdl_okay() - Write an OKAY response.
 *
 * @tag: identify type of the response
 * @reason: Pointer to returned reason string, or NULL
 * @response: Pointer to fdl response buffer
 */
void fdl_okay(int tag, const char *reason, char *response)
{
	if (reason)
		fdl_response(tag, response, "%s", reason);
	else
		fdl_response(tag, response, NULL);
}

void fdl_okay_null(int tag, char *response)
{
	fdl_response(tag, response, NULL);
}

void fdl_ack(char *response)
{
	fdl_response(FDL_RESPONSE_ACK, response, NULL);
}

/**
 * fdl_version() - Write a VERSION response.
 *
 * @tag: identify type of the response
 * @version: Pointer to returned version string
 * @response: Pointer to fdl response buffer
 */
void fdl_version(int tag, const char *version, char *response)
{
	fdl_response(tag, response, "%s", version);
}

void fdl_command(int tag, char *command)
{
	struct fdl_packet *pp = NULL;
	struct fdl_header *head = NULL;
	char *buffer = NULL;
	int len = FDL_STRUCT_2NDF_AND_3RDF_LEN;
	uint16_t chksum, *p;

	pp = (void *)command;
	head = (void *)pp;
	head->magic = FDL_MAGIC;
	head->bytecode = fdl_get_comm_bytecode(tag);
	buffer = (void *)&head->size;
	len += head->size;
	chksum = fdl_checksum(buffer, len);
	p = (uint16_t *)(pp->payload + head->size);
	*p = chksum;
}

/*
 * fdl_init() - initialise new FDL protocol session
 *
 * @buf_addr: Pointer to download buffer, or NULL for default
 * @buf_size: Size of download buffer, or zero for default
 */
void fdl_init(void *buf_addr, u32 buf_size)
{
	fdl_buf_addr = buf_addr ? buf_addr : (void *)CONFIG_FDL_BUF_ADDR;
	fdl_buf_size = buf_size ? buf_size : CONFIG_FDL_BUF_SIZE;

#ifdef CONFIG_FDL_DEBUG
	exec_cmd_cnt = 0;
#endif
}