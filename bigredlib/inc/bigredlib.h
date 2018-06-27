#ifndef __BIGREDLIB_HEADER__
#define	__BIGREDLIB_HEADER__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define	BRL_PRODUCT		0
#define	BRL_DESIGN		1
#ifdef _DEBUG
#define	BRL_RELEASE		5					/* release number (odd = debug) */
#else
#define	BRL_RELEASE		4					/* release number (even = release) */
#endif
#define	BRL_BUILD		0

#ifdef _M_IX86
#define BRL_ARCH		"32b"
#else
#define BRL_ARCH		"64b"
#endif

#define	BRL_VER			STR(BRL_PRODUCT) "." \
						STR(BRL_DESIGN) "." \
						STR(BRL_RELEASE) "." \
						STR(BRL_BUILD) " " \
						BRL_ARCH " " __DATE__ " " __TIME__

#define STR_HELPER(x)	#x
#define STR(x) STR_HELPER(x)
																/********* Data Block definitions *******/
#define	BLK_W			(32)									/* block width in pixels */
#define	BLK_H			(32)									/* block height in pixels */
#define	BLK_D			(256)									/* block depth (samples) */
#define	BLK_BPP			(2)										/* bytes per pixel for a block */
#define	BLK_PIX			(BLK_W * BLK_H * BLK_D)					/* pixels per block */
#define	BLK_B			(BLK_PIX * BLK_BPP)						/* block size in bytes */
																/********* Sensor Definitions *********/
#define SEN_X_GRP		(1)										/* sensor area location - x offset [0..3] */
#define	SEN_Y_GRP		(2)										/* sensor area location - y offset [0..5] */
#define	SEN_X_GRPS		(2)										/* width of sensor in blocks */
#define	SEN_Y_GRPS		(2)										/* width of sensor in blocks */
#define	SEN_BLKS		(SEN_X_GRPS + SEN_Y_GRPS)				/* total number of blocks covered by the sensor */
#define	SEN_W			(BLK_W * SEN_X_GRPS)					/* sensor width (full blocks) */
#define	SEN_H			(BLK_H * SEN_Y_GRPS)					/* sensor height (full blocks) */
#define	SEN_BLKS_1X		(SEN_BLKS)
#define	SEN_BLKS_4X		(4*SEN_BLKS)
																/********** Fingerprint definitions **********/
#define FP_W			(60)									/* actual sensor width in pix */
#define	FP_H			(60)									/* actual sensor height in pix */
#define	FP_BPP			(1)										/* bytes per pixel */
																/********* Initial Parameters *********/
#define	FREQ_MIN		(15000000)
#define	FREQ_MAX		(26550000)
#define	FREQ_STEP		(50000)
#define	INI_FREQ_INDX	(37)
#define	INI_BLKDEF		(0x8012)
#define	INI_CEIL		(ADC_MAX_VAL - TWLVE2EIGHT_M)			/* 12-to-8 mapping CEILING parameter */
#define	INI_FLOOR		(TWLVE2EIGHT_M)							/* 12-to-8 mapping FLOOR parameter */
#define	INI_DAC_VAL		(0x2a)
#define	INI_TXVH		(0x12u)
#define	INI_BURST_LEN	(64)
#define	ADC_MAX_VAL		(2048)
#define SAM_WDW_BOT		(0)
#define	SAM_WDW_LEN		(128)									/* max sampling window length */
#define	SAM_COL_DEPTH	(256)
#define	TWLVE_2_8_LLHH	(0xFFFF0000)
#define	TWLVE_2_8_HLHH	(0xFFFF00FF)
#define	TWLVE_2_8_HHLH	(0xFF00FFFF)
#define	TWLVE_2_8_HHLL	(0x0000FFFF)
#define	TWLVE2EIGHT_M	(0)
#define	TWLVE2EIGHT_MAX	(ADC_MAX_VAL+10)
#define	SEN_COORD_1X	(0x3c3c0002)
#define	SEN_COORD_4X	(2*SEN_COORD_1X)
#define	DATA_EVA_MIN_I	(2048)
#define	DATA_EVA_MAX_I	(0)
#define	INI_SCOPE_T		(128)
#define	INI_SCOPE_R		(68)
#define	SCOPE_PIX_T		(0)
#define	SCOPE_PIX_R		(0)
#define	SCOPE_PIX_S		(0)

#define	F_CNT_RES_4X	(SEN_BLKS_4X - 1)				/* Context flag: use 4x resolution */
#define	F_CNT_RES_1X	(SEN_BLKS_1X - 1)				/* Context flag: use 1x resolution */

#define OUTPUT_FILEPATH	"c:\\"
#define	OUTPUT_FILENAME	"br_00.txt"
#define OUTPUT_FILENDX	(sizeof(OUTPUT_FILEPATH OUTPUT_FILENAME) - 7)

#define	INI_PROC_CNTX	{ BLK_W, BLK_H, BLK_D, TWLVE_2_8_LLHH, F_CNT_RES_4X, \
						INI_CEIL, INI_FLOOR, INI_BLKDEF, \
						SEN_COORD_4X}

#define INI_DATA_EVAL	{DATA_EVA_MIN_I, DATA_EVA_MAX_I, 0}

#define	INI_BLK_PIX		{SCOPE_PIX_R, SCOPE_PIX_T, SCOPE_PIX_S}

typedef struct sProcContext
{
	uint32_t	Block_Width;
	uint32_t	Block_Height;
	uint32_t	Block_Depth;
	uint32_t	twelve_2_8_typ;		/* twelve 2 eight conversion definition */
	uint32_t	ContextFlags;
	uint32_t	Ceiling;
	uint32_t	Floor;
	uint32_t	Blk_def;			/* block definition to retrieve sub blocks */
	uint32_t	sen_coordinates;
} tProcContext;

typedef struct sMSM_params
{
	uint32_t collection_depth;					/* MSM collection depth value mask [1..511] */
	uint32_t full_depth_capture;				/* keep sampling till full depth (1) or stop at window end (0) */
	uint16_t sample_window_start;				/* defines the start sample [0..255] */
	uint16_t sample_window_length;				/* defines the sampling window length [1..511] */
} tMSM_params;

typedef struct sDataEval						/* data analysis results */
{
	uint16_t min;								/* minimum value in data dynamic range */
	uint16_t max;								/* maximum value in data dynamic range */
	uint32_t err_cnt;							/* detected data errors */
} tDataEval;

typedef struct sBlk_Pix
{
	uint8_t r;
	uint8_t t;
	uint16_t s;
} tBlk_Pix;

typedef void (*trf_to_img)(uint8_t*, uint32_t, int32_t);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BIGREDLIB_EXPORTS
#define	BRL_DECLSPEC	__declspec(dllexport)
#else
#define	BRL_DECLSPEC	__declspec(dllimport)
#endif

extern BRL_DECLSPEC tBlk_Pix blk_pix;
extern BRL_DECLSPEC tDataEval data_eval;
extern BRL_DECLSPEC tProcContext br_cntxt;
extern BRL_DECLSPEC uint8_t twelve_2_eight[TWLVE2EIGHT_MAX];	/* 12-to-8 mapping table in cc ram */
extern BRL_DECLSPEC uint8_t *bg_buffer;							/* pointer to background data area */

BRL_DECLSPEC void BR_Update_12_to_8_Lin_Table(void);
BRL_DECLSPEC void* BR_USB_Initialize(void);
BRL_DECLSPEC void BR_USB_Purge(uint32_t mask);
BRL_DECLSPEC void BR_USB_De_Initialize(void);
BRL_DECLSPEC void BR_Write_STM_Register(uint32_t regaddr, uint32_t regval);
BRL_DECLSPEC void BR_write_RF_block_to_file(uint32_t len);
BRL_DECLSPEC void BR_Get_Vars(void);
BRL_DECLSPEC void BR_Set_Vars(uint32_t *src, uint32_t *dst, int32_t len);
BRL_DECLSPEC void BR_Set_Background(uint32_t state);
BRL_DECLSPEC uint32_t BR_Get_RF_Blocks_To_Files(void);
BRL_DECLSPEC uint32_t BR_Get_RF_Data(int16_t *buffer);
BRL_DECLSPEC uint32_t BR_Get_Version(char * version);
BRL_DECLSPEC uint32_t BR_Get_RF_as_Image(uint8_t* fp_buffer);
BRL_DECLSPEC uint32_t BR_Get_RF_Samples(int16_t *buffer, uint32_t flag);
BRL_DECLSPEC uint32_t BR_Call_Fn_1Byte(uint32_t fn, uint32_t data, uint32_t flags);
BRL_DECLSPEC uint32_t BR_Call_Fn(uint32_t fn, uint8_t *arg, uint32_t len, uint32_t flags);
BRL_DECLSPEC uint32_t BR_Test(int16_t *buffer, uint32_t flag);
BRL_DECLSPEC uint32_t BR_Get_Tag_Package(void);
#ifdef __cplusplus
}
#endif

#endif