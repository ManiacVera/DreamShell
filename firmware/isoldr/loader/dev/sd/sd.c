/**
 * Copyright (c) 2011-2025 SWAT <http://www.dc-swat.ru>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <arch/types.h>
#include <arch/cache.h>
#include "spi.h"
#include "sd.h"
#include "sci.h"

//#define SD_DEBUG 1
#define DISCARD_CRC16 1

#ifdef SD_DEBUG
#include <main.h>
#endif

#define MAX_RETRIES     500000
#define READ_RETRIES    50000
#define WRITE_RETRIES   150000
#define MAX_RETRY       10


/* MMC/SD command (in SPI) */
#define CMD(n) (n | 0x40)
#define CMD0	(0x40+0)	/* GO_IDLE_STATE */
#define CMD1	(0x40+1)	/* SEND_OP_COND */
#define CMD8	(0x40+8)	/* SEND_IF_COND */
#define CMD9	(0x40+9)	/* SEND_CSD */
#define CMD10	(0x40+10)	/* SEND_CID */
#define CMD12	(0x40+12)	/* STOP_TRANSMISSION */
#define CMD16	(0x40+16)	/* SET_BLOCKLEN */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD18	(0x40+18)	/* READ_MULTIPLE_BLOCK */
#define CMD23	(0x40+23)	/* SET_BLOCK_COUNT */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD25	(0x40+25)	/* WRITE_MULTIPLE_BLOCK */
#define CMD41	(0x40+41)	/* SEND_OP_COND (ACMD) */
#define CMD55	(0x40+55)	/* APP_CMD */
#define CMD58	(0x40+58)	/* READ_OCR */
#define CMD59	(0x40+59)	/* CRC_ON_OFF */

/* Interface selection globals */
static sd_interface_t current_interface = SD_IF_SCIF;
static int check_crc = 1;
static int initted = 0;

/* Function pointers for unified interface */
static uint8 (*f_spi_sr_byte)(uint8 b) = NULL;
static uint8 (*f_spi_slow_sr_byte)(uint8 b) = NULL;
static uint8 (*f_spi_rec_byte)(void) = NULL;
static void (*f_spi_send_byte)(uint8 b) = NULL;
static void (*f_spi_rec_data)(uint8* buffer, uint16 buffer_len) = NULL;
#if _FS_READONLY == 0
static void (*f_spi_send_data)(const uint8* data, uint16 data_len) = NULL;
#endif
static void (*f_spi_cs_on)(void) = NULL;
static void (*f_spi_cs_off)(void) = NULL;

/* SCI interface implementation - use wrapper functions */
static uint8 sci_sr_byte(uint8 b) {
	uint8 r = 0;
	sci_spi_rw_byte(b, &r);
	return r;
}

static uint8 sci_slow_sr_byte(uint8 b) {
	uint8 r = 0;
	sci_spi_rw_byte(b, &r);
	return r;
}

static uint8 sci_rec_byte(void) {
	uint8 r = 0;
	sci_spi_read_byte(&r);
	return r;
}

static void sci_send_byte(uint8 b) {
	sci_spi_write_byte(b);
}

static void sci_rec_data(uint8* buffer, uint16 buffer_len) {
	if(buffer_len & 31) {
		sci_spi_read_data(buffer, buffer_len);
	}
	else {
		sci_spi_dma_read_data(buffer, buffer_len, NULL, NULL);
	}
}

#if _FS_READONLY == 0
static void sci_send_data(const uint8* data, uint16 data_len) {
	// if(data_len & 31) {
		sci_spi_write_data(data, data_len);
	// }
	// else {
	// 	sci_spi_dma_write_data(data, data_len, NULL, NULL);
	// }
}
#endif

static void sci_cs_on(void) {
	sci_spi_set_cs(1);
}

static void sci_cs_off(void) {
	sci_spi_set_cs(0);
}

/* Table/algorithm generated by pycrc. I really wanted to have a much smaller
   table here, but unfortunately, the code pycrc generated just did not work. */
static const uint8 crc7_table[256] = {
    0x00, 0x12, 0x24, 0x36, 0x48, 0x5a, 0x6c, 0x7e,
    0x90, 0x82, 0xb4, 0xa6, 0xd8, 0xca, 0xfc, 0xee,
    0x32, 0x20, 0x16, 0x04, 0x7a, 0x68, 0x5e, 0x4c,
    0xa2, 0xb0, 0x86, 0x94, 0xea, 0xf8, 0xce, 0xdc,
    0x64, 0x76, 0x40, 0x52, 0x2c, 0x3e, 0x08, 0x1a,
    0xf4, 0xe6, 0xd0, 0xc2, 0xbc, 0xae, 0x98, 0x8a,
    0x56, 0x44, 0x72, 0x60, 0x1e, 0x0c, 0x3a, 0x28,
    0xc6, 0xd4, 0xe2, 0xf0, 0x8e, 0x9c, 0xaa, 0xb8,
    0xc8, 0xda, 0xec, 0xfe, 0x80, 0x92, 0xa4, 0xb6,
    0x58, 0x4a, 0x7c, 0x6e, 0x10, 0x02, 0x34, 0x26,
    0xfa, 0xe8, 0xde, 0xcc, 0xb2, 0xa0, 0x96, 0x84,
    0x6a, 0x78, 0x4e, 0x5c, 0x22, 0x30, 0x06, 0x14,
    0xac, 0xbe, 0x88, 0x9a, 0xe4, 0xf6, 0xc0, 0xd2,
    0x3c, 0x2e, 0x18, 0x0a, 0x74, 0x66, 0x50, 0x42,
    0x9e, 0x8c, 0xba, 0xa8, 0xd6, 0xc4, 0xf2, 0xe0,
    0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54, 0x62, 0x70,
    0x82, 0x90, 0xa6, 0xb4, 0xca, 0xd8, 0xee, 0xfc,
    0x12, 0x00, 0x36, 0x24, 0x5a, 0x48, 0x7e, 0x6c,
    0xb0, 0xa2, 0x94, 0x86, 0xf8, 0xea, 0xdc, 0xce,
    0x20, 0x32, 0x04, 0x16, 0x68, 0x7a, 0x4c, 0x5e,
    0xe6, 0xf4, 0xc2, 0xd0, 0xae, 0xbc, 0x8a, 0x98,
    0x76, 0x64, 0x52, 0x40, 0x3e, 0x2c, 0x1a, 0x08,
    0xd4, 0xc6, 0xf0, 0xe2, 0x9c, 0x8e, 0xb8, 0xaa,
    0x44, 0x56, 0x60, 0x72, 0x0c, 0x1e, 0x28, 0x3a,
    0x4a, 0x58, 0x6e, 0x7c, 0x02, 0x10, 0x26, 0x34,
    0xda, 0xc8, 0xfe, 0xec, 0x92, 0x80, 0xb6, 0xa4,
    0x78, 0x6a, 0x5c, 0x4e, 0x30, 0x22, 0x14, 0x06,
    0xe8, 0xfa, 0xcc, 0xde, 0xa0, 0xb2, 0x84, 0x96,
    0x2e, 0x3c, 0x0a, 0x18, 0x66, 0x74, 0x42, 0x50,
    0xbe, 0xac, 0x9a, 0x88, 0xf6, 0xe4, 0xd2, 0xc0,
    0x1c, 0x0e, 0x38, 0x2a, 0x54, 0x46, 0x70, 0x62,
    0x8c, 0x9e, 0xa8, 0xba, 0xc4, 0xd6, 0xe0, 0xf2
};


uint8 sd_crc7(const uint8 *data, int size, uint8 crc) {
    int tbl_idx;

    while(size--) {
        tbl_idx = (crc ^ *data) & 0xff;
        crc = (crc7_table[tbl_idx] ^ (crc << 7)) & (0x7f << 1);

        data++;
    }

    return crc & (0x7f << 1);
}

#if _FS_READONLY == 0 || !defined(DISCARD_CRC16)
uint16 sd_crc16(const uint8 *data, int size, uint16 start) {
    uint16 rv = start, tmp;

    while(size--) {
        tmp = (rv >> 8) ^ *data++;
        tmp ^= tmp >> 4;

        rv = (rv << 8) ^ (tmp << 12) ^ (tmp << 5) ^ tmp;
    }

    return rv;
}
#endif

#ifndef NO_SD_INIT
static int is_mmc = 0;
#endif

static int byte_mode = 0;


static struct {
	uint32 block;
	size_t rcount;
	size_t count;
	uint8 *buff;
} sds;

#define SELECT() f_spi_cs_on()
#define DESELECT() f_spi_cs_off()


static uint8 wait_ready (void) {
	int i;
	uint8 res;

	(void)f_spi_sr_byte(0xFF);
	i = 0;
	do {
		res = f_spi_sr_byte(0xFF);
		i++;
	} while ((res != 0xFF) && i < MAX_RETRIES);
	
	return res;
}


static uint8 send_cmd (
	uint8 cmd,		/* Command byte */
	uint32 arg		/* Argument */
)
{
	uint8 n, res;
	uint8 cb[6];

	if (wait_ready() != 0xFF) {
#ifdef SD_DEBUG
		LOGFF("CMD 0x%02x wait ready error\n", cmd);
#endif
		return 0xFF;
	}

	cb[0] = cmd;
	cb[1] = (uint8)(arg >> 24);
	cb[2] = (uint8)(arg >> 16);
	cb[3] = (uint8)(arg >> 8);
	cb[4] = (uint8)arg;
	cb[5] = sd_crc7(cb, 5, 0);
	/* Send command packet */

	f_spi_send_byte(cmd);		/* Command */
	f_spi_send_byte(cb[1]);		/* Argument[31..24] */
	f_spi_send_byte(cb[2]);		/* Argument[23..16] */
	f_spi_send_byte(cb[3]);		/* Argument[15..8] */
	f_spi_send_byte(cb[4]);		/* Argument[7..0] */
	f_spi_send_byte(cb[5]);		/* CRC7 */

	/* Receive command response */
	if (cmd == CMD12) 
		(void)f_spi_rec_byte();		/* Skip a stuff byte when stop reading */

	n = 20;						/* Wait for a valid response in timeout of 10 attempts */
	
	do {
		res = f_spi_rec_byte();
	} while ((res & 0x80) && --n);
	
#ifdef SD_DEBUG
	LOGFF("CMD 0x%02x response 0x%02x\n", cmd, res);
#endif

	return res;			/* Return with the response value */
}

#ifndef NO_SD_INIT
static uint8 send_slow_cmd (
	uint8 cmd,		/* Command byte */
	uint32 arg		/* Argument */
)
{
	uint8 n, res;
	uint8 cb[6];
	int i;

	(void)f_spi_slow_sr_byte(0xff);
	i = 0;
	
	do {
		
		res = f_spi_slow_sr_byte(0xff);
		i++;
		
	} while ((res != 0xFF) && i < 100000);
	
	if (res != 0xff) {
#ifdef SD_DEBUG
		LOGFF("CMD 0x%02x error\n", cmd);
#endif
		return(0xff);
	}

	cb[0] = cmd;
	cb[1] = (uint8)(arg >> 24);
	cb[2] = (uint8)(arg >> 16);
	cb[3] = (uint8)(arg >> 8);
	cb[4] = (uint8)arg;
	cb[5] = sd_crc7(cb, 5, 0);
	/* Send command packet */
	f_spi_slow_sr_byte(cmd);		/* Command */
	f_spi_slow_sr_byte(cb[1]);		/* Argument[31..24] */
	f_spi_slow_sr_byte(cb[2]);		/* Argument[23..16] */
	f_spi_slow_sr_byte(cb[3]);		/* Argument[15..8] */
	f_spi_slow_sr_byte(cb[4]);		/* Argument[7..0] */
	f_spi_slow_sr_byte(cb[5]);		// CRC7

	/* Receive command response */
	if (cmd == CMD12) 
		(void)f_spi_slow_sr_byte(0xff);/* Skip a stuff byte when stop reading */
		
	n = 20; /* Wait for a valid response in timeout of 10 attempts */
	
	do {
		res = f_spi_slow_sr_byte(0xff);
	} while ((res & 0x80) && --n);
	
#ifdef SD_DEBUG
	LOGFF("CMD 0x%02x response 0x%02x\n", cmd, res);
#endif
	
	return res; /* Return with the response value */
}


int sd_init(void) {
	sd_init_params_t params = {
		.interface = SD_IF_SCIF,
		.check_crc = 1
	};
	return sd_init_ex(&params);
}

int sd_init_ex(const sd_init_params_t *params) {
	int i;
	uint8 n, ty = 0, ocr[4];
	
	if (!params)
		return -1;
	
	is_mmc = 0;
	byte_mode = 0;
	current_interface = params->interface;
	check_crc = params->check_crc;
	
	/* Initialize the appropriate function pointers based on the interface */
	if (current_interface == SD_IF_SCIF) {
		/* Use the original SPI functions from spi.c */
		f_spi_sr_byte = spi_sr_byte;
		f_spi_slow_sr_byte = spi_slow_sr_byte;
		f_spi_rec_byte = spi_rec_byte;
		f_spi_send_byte = spi_send_byte;
		f_spi_rec_data = spi_rec_data;
#if _FS_READONLY == 0
		f_spi_send_data = spi_send_data;
#endif
		f_spi_cs_on = spi_cs_on;
		f_spi_cs_off = spi_cs_off;
		
		if(spi_init()) {
			return -1;
		}
	}
	else {
		/* Use SCI wrapper functions */
		f_spi_sr_byte = sci_sr_byte;
		f_spi_slow_sr_byte = sci_slow_sr_byte;
		f_spi_rec_byte = sci_rec_byte;
		f_spi_send_byte = sci_send_byte;
		f_spi_rec_data = sci_rec_data;
#if _FS_READONLY == 0
		f_spi_send_data = sci_send_data;
#endif
		f_spi_cs_on = sci_cs_on;
		f_spi_cs_off = sci_cs_off;
		
		if(sci_init(SCI_SPI_BAUD_INIT, SCI_MODE_SPI, SCI_CLK_INT)) {
			return -1;
		}
	}
	
	timer_spin_sleep_bios(20);
	SELECT();
	
	/* 80 dummy clocks */
	for (n = 10; n; n--) 
		(void)f_spi_slow_sr_byte(0xff);

	
	if (send_slow_cmd(CMD0, 0) == 1) {			/* Enter Idle state */

#ifdef SD_DEBUG
		LOGFF("Enter Idle state\n");
#endif
		timer_spin_sleep_bios(20);
		
		i = 0;
		
		if (send_slow_cmd(CMD8, 0x1AA) == 1) {	/* SDC Ver2+  */
		
			for (n = 0; n < 4; n++) 
				ocr[n] = f_spi_slow_sr_byte(0xff);
			
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) { /* The card can work at vdd range of 2.7-3.6V */
			
				do {
					
					/* ACMD41 with HCS bit */
					if (send_slow_cmd(CMD55, 0) <= 1 && send_slow_cmd(CMD41, 1UL << 30) == 0) 
						break;
						
					++i;
					
				} while (i < 300000);
				
				if (i < 300000 && send_slow_cmd(CMD58, 0) == 0) { /* Check CCS bit */
				
					for (n = 0; n < 4; n++) 
						ocr[n] = f_spi_slow_sr_byte(0xff);
						
					ty = (ocr[0] & 0x40) ? 6 : 2;
				}
			}
			
		} else { /* SDC Ver1 or MMC */
		
			ty = (send_slow_cmd(CMD55, 0) <= 1 && send_slow_cmd(CMD41, 0) <= 1) ? 2 : 1; /* SDC : MMC */
			
			do {
				
				if (ty == 2) {
					
					if (send_slow_cmd(CMD55, 0) <= 1 && send_slow_cmd(CMD41, 0) == 0) /* ACMD41 */
						break;
						
				} else {
					
					if (send_slow_cmd(CMD1, 0) == 0) { /* CMD1 */
						is_mmc = 1;
						break;
					}								
				}
				
				++i;
				
			} while (i < 300000);
			
			if (!(i < 300000) || send_slow_cmd(CMD16, 512) != 0)	/* Select R/W block length */
				ty = 0;
		}
	}
	
	send_slow_cmd(CMD59, 1);		// crc check
	
#ifdef SD_DEBUG
	LOGFF("card type = 0x%02x\n", ty & 0xff);
#endif

	if(!(ty & 4)) {
		byte_mode = 1;
	}

	DESELECT();
	(void)f_spi_slow_sr_byte(0xff); /* Idle (Release DO) */

	initted = ty ? 1 : 0;

	if (initted) { /* Initialization succeded */
		/* Switch to maximum speed after successful initialization */
		if (current_interface == SD_IF_SCI) {
			sci_shutdown();
			if(sci_init(SCI_SPI_BAUD_MAX, SCI_MODE_SPI, SCI_CLK_INT)) {
				initted = 0;
				return -1;
			}
		}
		return 0;
	}

	/* Initialization failed */
//	sd_shutdown();
	return -1;
}

#else

static int read_data(uint8 *buff, size_t len);

int sd_init(void) {
	sd_init_params_t params = {
		.interface = SD_IF_SCIF,
		.check_crc = 1
	};
	return sd_init_ex(&params);
}

int sd_init_ex(const sd_init_params_t *params) {
	uint8 csd[16];
	int rv = 0;
	
	if (!params)
		return -1;
	
	byte_mode = 0;
	current_interface = params->interface;
	check_crc = params->check_crc;
	
	/* Initialize the appropriate function pointers based on the interface */
	if (current_interface == SD_IF_SCIF) {
		/* Use the original SPI functions from spi.c */
		f_spi_sr_byte = spi_sr_byte;
		f_spi_slow_sr_byte = spi_slow_sr_byte;
		f_spi_rec_byte = spi_rec_byte;
		f_spi_send_byte = spi_send_byte;
		f_spi_rec_data = spi_rec_data;
#if _FS_READONLY == 0
		f_spi_send_data = spi_send_data;
#endif
		f_spi_cs_on = spi_cs_on;
		f_spi_cs_off = spi_cs_off;
		
		if(spi_init()) {
			return -1;
		}
	}
	else {
		/* Use SCI wrapper functions */
		f_spi_sr_byte = sci_sr_byte;
		f_spi_slow_sr_byte = sci_slow_sr_byte;
		f_spi_rec_byte = sci_rec_byte;
		f_spi_send_byte = sci_send_byte;
		f_spi_rec_data = sci_rec_data;
#if _FS_READONLY == 0
		f_spi_send_data = sci_send_data;
#endif
		f_spi_cs_on = sci_cs_on;
		f_spi_cs_off = sci_cs_off;
		
		if(sci_init(SCI_SPI_BAUD_MAX, SCI_MODE_SPI, SCI_CLK_INT)) {
			return -1;
		}
	}
	
	SELECT();
	
	if(send_cmd(CMD9, 0)) {
		rv = -1;
		goto out;
	}

	/* Read back the register */
	if(read_data(csd, 16)) {
		rv = -1;
		goto out;
	}
	
	if((csd[0] >> 6) == 0) {
		byte_mode = 1;
	} else {
		byte_mode = 0;
	}
	
	initted = 1;

out:
	DESELECT();
	(void)f_spi_rec_byte();	
	return rv;
}
	
#endif


int sd_shutdown(void) {
	int i = 0;
	uint8 rv;
	
	if(!initted)
		return -1;
	
	SELECT();
	
	f_spi_sr_byte(0xFF);
	do {
		rv = f_spi_sr_byte(0xFF);
		++i;
	} while(rv != 0xFF && i < MAX_RETRIES);
	
	DESELECT();
	(void)f_spi_rec_byte();
	
	if (current_interface == SD_IF_SCIF) {
		spi_shutdown();
	}
	else {
		sci_shutdown();
	}
	
	initted = 0;
	return 0;
}


static int read_data (
	uint8 *buff,		/* Data buffer to store received data */
	size_t len			/* Byte count (must be even number) */
)
{
	uint8 token;
	int i = 0;
#ifndef DISCARD_CRC16
	uint16 crc, crc2;
#endif
	
	do {							/* Wait for data packet in timeout of 100ms */
		token = f_spi_rec_byte();
	} while (token == 0xFF && ++i < READ_RETRIES);

	if(token != 0xFE) {
#ifdef SD_DEBUG
		LOGFF("not valid data token: %02x\n", token);
#endif
		return -1;	/* If not valid data token, return with error */
	}

	f_spi_rec_data(buff, len);
	
#ifndef DISCARD_CRC16
	crc = (uint16)f_spi_rec_byte() << 8;
	crc |= (uint16)f_spi_rec_byte();

	if (check_crc) {
		crc2 = sd_crc16(buff, len, 0);

		if(crc != crc2) {
			//errno = EIO;
			return -1;
		}
	}
#else
	(void)f_spi_rec_byte();	
	(void)f_spi_rec_byte();
#endif

	return 0; /* Return with success */
}


int sd_read_blocks(uint32 block, size_t count, uint8 *buf, int blocked) {
	
#ifdef SD_DEBUG
	LOGFF("block=%ld count=%d\n", block, count);
#endif

	uint8 *p;
	int retry, cnt;
	
	if (byte_mode) block <<= 9;	/* Convert to byte address if needed */

	for (retry = 0; retry < MAX_RETRY; retry++) {
		p = buf;
		cnt = count;
		
		SELECT();

		if (cnt == 1) { /* Single block read */
		
			if ((send_cmd(CMD17, block) == 0) && !read_data(p, 512)) {
				cnt = 0;
			}
			
		} else { /* Multiple block read */
			if (send_cmd(CMD18, block) == 0) {	

				if(blocked) {
				
					do {
						if (read_data(p, 512)) 
							break;
							
						p += 512;
					} while (--cnt);
					
					send_cmd(CMD12, 0); /* STOP_TRANSMISSION */
					
				} else {
					
					sds.block = block;
					sds.count = sds.rcount = count;
					sds.buff = buf;
					DESELECT();
					return 0;
				}
			}
		}

		DESELECT();
		(void)f_spi_rec_byte();			/* Idle (Release DO) */
		if (cnt == 0) break;
	}
	
//#ifdef SD_DEBUG
//	LOGFF("retry = %d (MAX=%d) cnt = %d\n", retry, MAX_RETRY, cnt);
//#endif

	if((retry >= MAX_RETRY || cnt > 0)) {
		//errno = EIO;
		return -1;
	}

	return 0;
}


#if _FS_READONLY == 0

static int write_data (
	uint8 *buff,	/* 512 byte data block to be transmitted */
	uint8 token	/* Data/Stop token */
)
{
	uint8 resp;
	uint16 crc;

	if (wait_ready() != 0xFF) 
		return -1;

	f_spi_send_byte(token);	 /* Xmit data token */
	
	if (token != 0xFD) {	/* Is data token */

		crc = sd_crc16(buff, 512, 0);
		f_spi_send_data(buff, 512);
		f_spi_send_byte((uint8)(crc >> 8));
		f_spi_send_byte((uint8)crc);
		
		resp = f_spi_rec_byte();				/* Reсeive data response */
		
		if ((resp & 0x1F) != 0x05) {		/* If not accepted, return with error */
#ifdef SD_DEBUG
			LOGFF("not accepted: %02x\n", resp);
#endif
			//errno = EIO;
			return -1;
		}
	}
	
	return 0;
}


int sd_write_blocks(uint32 block, size_t count, const uint8 *buf, int blocked) {

#ifdef SD_DEBUG
	LOGFF("block=%ld count=%d\n", block, count);
#endif

	(void)blocked;
	
	uint8 cnt, *p;
	int retry;

	if (byte_mode) block <<= 9;	/* Convert to byte address if needed */

	for (retry = 0; retry < MAX_RETRY; retry++) {
		
		p = (uint8 *)buf;
		cnt = count;
		
		SELECT();			/* CS = L */

		if (count == 1) {	/* Single block write */
			if ((send_cmd(CMD24, block) == 0) && !write_data(p, 0xFE))
				cnt = 0;
		}
		else {	/* Multiple block write */
//			if (!is_mmc) {
				send_cmd(CMD55, 0); 
				send_cmd(CMD23, cnt);	/* ACMD23 */
//			}
			if (send_cmd(CMD25, block) == 0) {
				
				do {
					if (write_data(p, 0xFC)) break;
					p += 512;
				} while (--cnt);
				
				if (write_data(0, 0xFD))	/* STOP_TRAN token */
					cnt = 1;
			}
		}

		DESELECT();			/* CS = H */
		(void)f_spi_rec_byte();			/* Idle (Release DO) */
		if (cnt == 0) break;
	}
	
//#ifdef SD_DEBUG
//	LOGFF("retry = %d (MAX=%d) cnt = %d\n", retry, MAX_RETRY, cnt);
//#endif
	
	if((retry >= MAX_RETRY || cnt > 0)) {
		//errno = EIO;
		return -1;
	}
	
	return 0;
}

#endif


static void sd_stop_trans() {
	send_cmd(CMD12, 0);   /* STOP_TRANSMISSION */
	DESELECT();
	(void)f_spi_rec_byte();	 /* Idle (Release DO) */
}


/* For now only read supported (really write is not need) */
int sd_poll(size_t blocks) {
	
	if(sds.count > 0) {
		
		int cnt = sds.count > blocks ? blocks : sds.count;
		sds.count -= cnt;
		SELECT();
		
		do {

			if(read_data(sds.buff, 512)) {
				
				DESELECT();
				
				/* Restart the transfer from last position */
				int rcnt = sds.rcount;
				sds.count += cnt;
				sds.block += (sds.rcount - sds.count);
				
				if(!sd_read_blocks(sds.block, sds.count, sds.buff, 0)) {
					sds.rcount = rcnt;
					return sds.rcount - sds.count;
				} else {
					sds.count = 0;
					sd_stop_trans();
					return -1;
				}
			}
			
			sds.buff += 512;
			
		} while (--cnt);
		
		if(sds.count <= 0) {
			
			sd_stop_trans();
			
		} else {
			DESELECT();
			return sds.rcount - sds.count;
		}
	}
	
	return 0;
}


int sd_abort() {
	
	if(sds.count > 0) {
		SELECT();
		sd_stop_trans();
	} 
	
	sds.count = 0;
	return 0;
}


#if _USE_MKFS && !_FS_READONLY

uint64 sd_get_size(void) {
	
	uint8 csd[16];
	int exponent;
	uint64 rv = 0;
	
	SELECT();
	
	if(send_cmd(CMD9, 0)) {
		rv = (uint64)-1;
		//errno = EIO;
		goto out;
	}

	/* Read back the register */
	if(read_data(csd, 16)) {
		rv = (uint64)-1;
		//errno = EIO;
		goto out;
	}

	/* Figure out what version of the CSD register we're looking at */
	switch(csd[0] >> 6) {
		case 0:
			/* CSD version 1.0 (SD)
			   C_SIZE is bits 62-73 of the CSD, C_SIZE_MULT is bits 47-49,
			   READ_BL_LEN is bits 80-83.
			   Card size is calculated as follows:
			   (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) * 2^(READ_BL_LEN) */
			exponent = (csd[5] & 0x0F) + ((csd[9] & 0x03) << 1) +
				(csd[10] >> 7) + 2;
			rv = ((csd[8] >> 6) | (csd[7] << 2) | ((csd[6] & 0x03) << 10)) + 1;
			rv <<= exponent;
			break;

		case 1:
			/* CSD version 2.0 (SDHC/SDXC)
			   C_SIZE is bits 48-69 of the CSD, card size is calculated as
			   (C_SIZE + 1) * 512KiB */
			rv = ((((uint64)csd[9]) | (uint64)(csd[8] << 8) |
				   ((uint64)(csd[7] & 0x3F) << 16)) + 1) << 19;
			break;

		default:
			/* Unknown version, punt. */
			rv = (uint64)-1;
			//errno = ENODEV;
			goto out;
	}

out:
	DESELECT();
	(void)f_spi_rec_byte();	
    return rv;
}

#endif
