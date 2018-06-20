// bigredlib.cpp : Defines the exported functions for the DLL application.
//
#define  _CRT_SECURE_NO_WARNINGS

#include "POL_dbg.h"					/* STM Commands macro definitions */
#include "bigredlib.h"
#include "sona_usb.h"					/* add remote header from segalib_static (DLL mode) */
#include <Windows.h>
#include <Winuser.h>

/**
	Global vars and functions
*/
uint8_t twelve_2_eight[TWLVE2EIGHT_MAX];		/* 12-to-8 mapping table in cc ram */
tProcContext br_cntxt = INI_PROC_CNTX;			/* context structure initialized */
tDataEval data_eval = INI_DATA_EVAL;			/* data evaluation results */
tBlk_Pix blk_pix = INI_BLK_PIX;					/* block pixel structure used for signal scope */
uint8_t *bg_buffer = NULL;

void BR_Update_12_to_8_Lin_Table(void)
{
	uint32_t tw_2_8, x, div;								/* twelve_2_eight, Margen1/2, x_position */
	uint8_t *p;

	tw_2_8 = br_cntxt.twelve_2_8_typ;
	x = br_cntxt.Floor - TWLVE2EIGHT_M;
	p = twelve_2_eight;
	memset(p, (uint8_t)tw_2_8, x);
	tw_2_8 >>= 8;
	p += x;
	memset(p, (uint8_t)tw_2_8, TWLVE2EIGHT_M);
	p += TWLVE2EIGHT_M;
	div = br_cntxt.Ceiling - br_cntxt.Floor;
	x = div;
	while(x)
	{
		*p++ = ((uint8_t)tw_2_8 ? (255*x/div):(255 - 255*x/div));
		x--;
	}
	tw_2_8 >>= 8;
	memset(p, (uint8_t)tw_2_8, TWLVE2EIGHT_M);
	tw_2_8 >>= 8;
	p += TWLVE2EIGHT_M;
	x = ADC_MAX_VAL - br_cntxt.Ceiling - TWLVE2EIGHT_M;
	memset(p, (uint8_t)tw_2_8, x);
}

/**
	@param	fp_buffer, pointer to image location in ram.
	@param	data, pointer to begining of the RF block data source in ram.
	@param	len, block size in bytes.
	@filenum	code word containing block identification information.
*/
static void BR_write_RF_block_to_img(uint8_t* fp_buffer, uint32_t len, int32_t bg_delta16)
{
	uint32_t rx, s, filenum, errorcnt;
	uint32_t sam_wdw[BLK_W], *p_sam_wdw;
	uint8_t * pimg;
	int32_t sample, bg;
	int16_t *data;

	filenum = (uint32_t)TAG_header[TAG_FLG_OFFSET-GHOST_PADS];
    pimg = fp_buffer +\
		(((filenum & 0x7)-SEN_Y_GRP) * 2 * SEN_W * BLK_H * FP_BPP) +\
		((((filenum >> 4) & 0x7)-SEN_X_GRP) * FP_BPP * BLK_W );    /* jump start to block begin within the image */
	errorcnt = data_eval.err_cnt;
	data = (int16_t*)USB_buf;
	len = (uint32_t)data + len;											/* get RF block last memory location */
	while ((uint32_t)data < len)										/* process RF block data */
	{
		s = 0;															/* init sample counter */
		p_sam_wdw = sam_wdw;											/* point to 1st sample */
		memset((void*)p_sam_wdw, 0, sizeof(sam_wdw));					/* zero out our sample accumulation array */
		rx = 0;															/* rx counter */

		while (1)														/* loop through each RX channel and each sample */
		{
			if((rx++) >= BLK_W)											/* RX counter, reset vars at begin of each RX group */
			{
				p_sam_wdw = sam_wdw;									/* reinit to sample 1 */
				rx = 1;													/* reinit RX counter */
				s++;													/* increment sample window step */
			}

			if (s < br_cntxt.Block_Depth)								/* samples sum loop */
			{
				bg = bg_delta16 ? *(data + bg_delta16) : 0;
				sample = ((*data++) - bg);
				(*p_sam_wdw++) += abs(sample);							/* accumulate the abs value of each sample */
			}
			else if (s == br_cntxt.Block_Depth)							/* sample averaging and store loop */
			{
				int32_t pixel;

				pixel =  ((*p_sam_wdw++) / br_cntxt.Block_Depth);
				if(pixel <= 2048)										/* data corruption w/q being - end */
				{
					*pimg++ =twelve_2_eight[pixel];
				}
				else													/* data fail action */
				{
					*pimg++ = 255;										/* make it white */
					errorcnt++;
				}
			}
			else
			{
				break;
			}
		}
		pimg += (3 * FP_BPP * (SEN_W - BLK_W));							/* select next TX for this block in the image */
	}
	data_eval.err_cnt = errorcnt;
}

static void BR_write_RF_block_to_img_4x(uint8_t* fp_buffer, uint32_t len, int32_t bg_delta16)
{
	uint32_t rx, s, filenum, err_cnt;
	uint32_t sam_wdw[BLK_W], *p_sam_wdw;
	uint8_t * pimg;
	int32_t sample, bg;
	int16_t *data;

	filenum = (uint32_t)TAG_header[TAG_FLG_OFFSET-GHOST_PADS];
    pimg = (uint8_t*)fp_buffer +\
		(((filenum & 0x7)-SEN_Y_GRP) * 2 * SEN_W * BLK_H * FP_BPP * 2) +\
		(((filenum & 0x8)>>3) * 2 * SEN_W * FP_BPP) +	\
		((((filenum >> 4) & 0x7)-SEN_X_GRP) * 2 * FP_BPP * BLK_W) + \
		(((filenum >> 7) & 0x1) * FP_BPP);    /* jump start to block begin within the image */
	err_cnt = data_eval.err_cnt;
	data = (int16_t*)USB_buf;
	len = (uint32_t)data + len;											/* get RF block last memory location */
	while ((uint32_t)data < len)										/* process RF block data */
	{
		s = 0;															/* init sample counter */
		p_sam_wdw = sam_wdw;											/* point to 1st sample */
		memset((void*)p_sam_wdw, 0, sizeof(sam_wdw));					/* zero out our sample accumulation array */
		rx = 0;															/* rx counter */

		while (1)														/* loop through each RX channel and each sample */
		{
			if((rx++) >= BLK_W)											/* RX counter, reset vars at begin of each RX group */
			{
				p_sam_wdw = sam_wdw;									/* reinit to sample 1 */
				rx = 1;													/* reinit RX counter */
				s++;													/* increment sample window step */
			}

			if (s < br_cntxt.Block_Depth)								/* samples sum loop */
			{
				bg = bg_delta16 ? *(data + bg_delta16) : 0;
				sample = ((*data++) - bg);
				(*p_sam_wdw++) += abs(sample);							/* accumulate the abs value of each sample */
			}
			else if (s == br_cntxt.Block_Depth)							/* sample averaging and store loop */
			{
				int32_t pixel;

				pixel =  ((*p_sam_wdw++) / br_cntxt.Block_Depth);
				if(pixel <= 2048)										/* data corruption w/q being - end */
				{
					*pimg =twelve_2_eight[pixel];
				}
				else													/* data fail action */
				{
					*pimg = 255;										/* make pixel white */
					err_cnt++;
				}
				pimg += 2;
			}
			else
			{
				break;
			}
		}
		pimg += (2 * 3 * FP_BPP * (SEN_W - BLK_W));						/* select next TX for this block in the image */
	}
	data_eval.err_cnt = err_cnt;
}

uint32_t BR_Get_RF_as_Image(uint8_t* fp_buffer)
{
	uint32_t u32Result, i, block_size;
	int32_t bg_delta, br_call;
	trf_to_img fn_fr_to_img;

	data_eval.err_cnt = 0;
	bg_delta = bg_buffer ? (int32_t)(bg_buffer - USB_buf) : 0;
	i = (F_CNT_RES_4X & br_cntxt.ContextFlags) ? SEN_BLKS_4X : SEN_BLKS_1X;
	fn_fr_to_img = (i == SEN_BLKS_4X) ? BR_write_RF_block_to_img_4x : BR_write_RF_block_to_img;
	br_call = (i == SEN_BLKS_4X) ? FN_BR_GETRFDATA_4X : FN_BR_GETRFDATA;
	USB_SendCommand(br_call, TAG_F_FB_REQST);
	block_size = br_cntxt.Block_Depth * br_cntxt.Block_Height * BLK_W * BLK_BPP;
	do
	{
		u32Result = USB_Rcvd_TAG();
		if(u32Result != block_size) return u32Result;
		fn_fr_to_img(fp_buffer ,u32Result, bg_delta/2);
		if (bg_buffer)
		{
			bg_delta += block_size;
		}
	} while (--i);
	USB_Rcvd_TAG();									/* get process results from FPGA */
	*(uint32_t*)&data_eval = *(uint32_t*)USB_buf;	/* copy min and max values to local buffer */
	return 0;
}

uint32_t BR_Get_RF_Data(int16_t *buffer)
{
	uint32_t u32Result, i, block_size;
	uint8_t *oldUsb_buf;

	i = (F_CNT_RES_4X & br_cntxt.ContextFlags) ? SEN_BLKS_4X : SEN_BLKS_1X;
	USB_SendCommand((i == SEN_BLKS_4X) ? FN_BR_GETRFDATA_4X : FN_BR_GETRFDATA, TAG_F_ALL_OFF);
	oldUsb_buf = USB_buf;
	USB_buf = (uint8_t*)buffer;
	block_size = br_cntxt.Block_Depth * br_cntxt.Block_Height * BLK_W * BLK_BPP;
	do
	{
		u32Result = USB_Rcvd_TAG();
		if(u32Result != block_size)
		{
			USB_buf = oldUsb_buf;
			return u32Result;
		}
		USB_buf += block_size;
	} while (--i);
	USB_buf = oldUsb_buf;
	return 0;
}

uint32_t BR_Get_RF_Samples(int16_t *buffer, uint32_t flag)
{
	uint32_t u32Result, block_size;
	uint8_t *oldUsb_buf;

	USB_SendBytes(FN_BR_GET_RF_SAMPLES, TAG_F_FB_REQST, (uint8_t*)&flag, sizeof(uint8_t*));
	oldUsb_buf = USB_buf;
	USB_buf = (uint8_t*)buffer;
	block_size = br_cntxt.Block_Depth * br_cntxt.Block_Height * BLK_W * BLK_BPP;

	u32Result = USB_Rcvd_TAG();						/* get samples data from inner process */
	if(u32Result == block_size) u32Result = 0;

	USB_buf = (uint8_t*)&data_eval;					/* get process result (contains min/max) */
	USB_Rcvd_TAG();
	USB_buf = oldUsb_buf;
	return u32Result;
}

/**
	@brief	writes data from a ram location into a text file.
	@param	data: is a pointer to the data location (16 bit/word)
	@param	len: number of 16bit words to be written.
	@param	filenum: a number to append to the filename
*/
void BR_write_RF_block_to_file(uint32_t len)
{
	FILE *dst;
	char valut_text[10], space, filename[] = OUTPUT_FILEPATH OUTPUT_FILENAME;
	int k = 0;
	uint16_t * data;

	sprintf(filename+OUTPUT_FILENDX, "%02x.csv", TAG_header[TAG_FLG_OFFSET-GHOST_PADS]);
	fopen_s(&dst, filename, "w");

	data = (uint16_t*)USB_buf;
	len >>= 1;
	while (len--)
	{
		space = ((k < 31) ? ',' : '\n');
		k = ((k==31) ? 0 : (k+1));
		sprintf(valut_text, "\'%04x%c\0", *data++, space);
		fputs(valut_text, dst);
	}
	fclose(dst);
}

uint32_t BR_Get_RF_Blocks_To_Files(void)
{
	uint32_t u32Result, i, block_size;

	i = (F_CNT_RES_4X & br_cntxt.ContextFlags) ? SEN_BLKS_4X : SEN_BLKS_1X;
	USB_SendCommand((i == SEN_BLKS_4X) ? FN_BR_GETRFDATA_4X : FN_BR_GETRFDATA, TAG_F_ALL_OFF);
	block_size = br_cntxt.Block_Depth * br_cntxt.Block_Height * BLK_W * BLK_BPP;
	do
	{
		u32Result = USB_Rcvd_TAG();
		if(u32Result != block_size) return u32Result;
		BR_write_RF_block_to_file(u32Result);
	} while (--i);
	return 0;
}

/**
	@brief	Starts the USB session with the FTDI chip.
	@return	NULL if fail, otherwise a pointer to a USB_init structure.
*/
void* BR_USB_Initialize(void)
{
	int32_t i32Result;

	i32Result = USB_Init();
	if (i32Result == FT_INSUFFICIENT_RESOURCES)									/*  couldn't malloc USB_buf */
	{
		return NULL;
	}
	if (i32Result == FT_DEVICE_NOT_OPENED)										/* device already opened, try to re-use it */
	{
		if(((tUSBinit*)USB_buf)->sUSB_info.ftHandle)								/* if handle exists, re-use it */
		{
			i32Result = 0;
		}
		else																	/* otherwise, try once more to open it */
		{
			USB_De_Init();
			i32Result = USB_Init();
		}
	}
	if(i32Result)																/* definitely we can't open the USB device */
	{
		return NULL;
	}
	else
	{
		USB_Purge(FT_PURGE_RX_TX);
		return (void*)USB_buf;
	}
}

void BR_USB_Purge(uint32_t mask)
{
	USB_Purge(mask);
}

uint32_t BR_Call_Fn(uint32_t fn, uint8_t *arg, uint32_t len, uint32_t flags)
{
	USB_SendBytes(fn, flags, arg, len);
	if(flags & (TAG_F_RB_RQST | TAG_F_FB_REQST))
	{
		return USB_Rcvd_TAG();
	}
	else
	{
		return 0;
	}
}

uint32_t BR_Call_Fn_1Byte(uint32_t fn, uint32_t data, uint32_t flags)
{
	USB_SendByte(fn, flags, data);
	if(flags & (TAG_F_RB_RQST | TAG_F_FB_REQST))
	{
		return USB_Rcvd_TAG();
	}
	else
	{
		return 0;
	}
}

void BR_USB_De_Initialize(void)
{
	USB_De_Init();				// this will close FT232H
}

void BR_Write_STM_Register(uint32_t regaddr, uint32_t regval)
{
	uint8_t data[3] = {POL_COM_STM_REG_WRITE};

	data[1] = regaddr;
	data[2] = regval;
	USB_SendBytes(FN_BR_STM_SEND_COMMAND, TAG_F_ALL_OFF, data, 3);
}

uint32_t BR_Get_Version(char * version)
{
	char brl_id[] = BRL_VER;
	memcpy(version, brl_id, sizeof(brl_id));			/* copy bigredlib.dll version */
	if(USB_buf)											/* copy ub-fw version */
	{
		uint32_t * ver = (uint32_t*)USB_buf;
		USB_SendCommand(FN_BR_GET_VER, TAG_F_ALL_OFF);
		USB_Rcvd_TAG();
		version += sizeof(brl_id)-1;
		sprintf(version, "\nBR_VER %08x\nTXTVER %08x\nDESVER %08x\nDCOVER %08x\nREGVER %08x\0", \
			ver[0], ver[1], ver[2], ver[3], ver[4]);
	}
	return sizeof(brl_id);
}

uint32_t BR_Test(int16_t *buffer, uint32_t flag)
{
	uint32_t u32Result;
	uint8_t *oldUsb_buf;

	USB_SendBytes(FN_TEST_FUNCTION, TAG_F_ALL_OFF, (uint8_t*)&flag, 4);
	oldUsb_buf = USB_buf;
	USB_buf = (uint8_t*)buffer;
	u32Result = USB_Rcvd_TAG();
	if(u32Result == *(uint32_t*)&TAG_header[TAG_LEN_OFFSET-GHOST_PADS]) u32Result = 0;
	USB_buf = oldUsb_buf;
	return u32Result;
}

/**
	@brief	this function allows to interface external applications
			by returning an array of pointers to the internal variables.
*/
void BR_Get_Vars(void)
{
	uint32_t *ptemp;

	ptemp = (uint32_t*)USB_buf;
	*ptemp++ = (uint32_t)&br_cntxt;
	*ptemp++ = sizeof(br_cntxt);
	*ptemp++ = (uint32_t)TAG_header;
	*ptemp++ = TAG_HEADER_SIZE;
	*ptemp++ = (uint32_t)&data_eval;
	*ptemp++ = sizeof(data_eval);
	*ptemp++ = (uint32_t)twelve_2_eight;
	*ptemp++ = sizeof(twelve_2_eight);
}

/**
	@brief	this function writes data from external sources to be
			written into local variable arrays

	@param	src: pointer to source data;
	@param	dst: pointer to destination location in ram;
	@param	len: size in bytes of the transfer
*/
void BR_Set_Vars(uint32_t *src, uint32_t *dst, int32_t len)
{
	while(len>0)
	{
		*dst++ = *src++;
		len -= 4;
	}
}

void BR_Set_Background(uint32_t state)
{
	if(bg_buffer)
	{
		free(bg_buffer);
		bg_buffer = NULL;
	}
	if (state)
	{
		bg_buffer = (uint8_t*)malloc(SEN_BLKS_4X * br_cntxt.Block_Depth * br_cntxt.Block_Height * BLK_W * BLK_BPP);
		BR_Get_RF_Data((int16_t*)bg_buffer);
	}
}



