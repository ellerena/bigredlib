/*	refer to the following documents:
	D2XX_Programmer's_Guide(FT_000071).pdf
	AN_130_FT2232H_Used_In_FT245 Synchronous FIFO Mode.pdf
*/

#include <stdlib.h>
#include "sona_usb.h"

#define	FT_MANUFACTURER_BUF		(32)
#define	FT_MANUFACTURERID_BUF	(16)
#define	FT_DDESCRIPTION_BUF		(64)
#define	FT_SERIALNUM_BUF		(16)
#define	BR_TX_PAGE_SIZE			(BR_TX_PAGE_LIMIT - GHOST_PADS)
#define	BR_TX_PAGE_LIMIT		(512)			/* max bytes to send at once by USB */

FT_HANDLE MyFtHandle;
uint8_t loc_TAG_header[8];

#ifdef BIGREDLIB_EXPORTS							/*** DLL Exports ***/
DLLEXPORT uint8_t *USB_buf = NULL;					/** data buffer pointer **/
DLLEXPORT uint8_t *TAG_header = loc_TAG_header;		/* tag header */
#else												/*** Static library ***/
uint8_t *TAG_header = loc_TAG_header;				/* tag header */
uint8_t *USB_buf = NULL;							/* USB data buffer pointer */
#endif



static DWORD USB_data_cnt;				/** general use data counter **/
tUSBinit *psUSB;

char USB_error_code[][32]=	{
							"FT_OK",							/*	5	0	*/\
							"FT_INVALID_HANDLE",				/*	17	5	*/\
							"FT_DEVICE_NOT_FOUND",				/*	19	22	*/\
							"FT_DEVICE_NOT_OPENED",				/*	20	41	*/\
							"FT_IO_ERROR",						/*	11	61	*/\
							"FT_INSUFFICIENT_RESOURCES",		/*	25	72	*/\
							"FT_INVALID_PARAMETER",				/*	20	97	*/\
							"FT_INVALID_BAUD_RATE",				/*	20	117	*/\
							"FT_DEVICE_NOT_OPENED_FOR_ERASE",	/*	30	137	*/\
							"FT_DEVICE_NOT_OPENED_FOR_WRITE",	/*	30	167	*/\
							"FT_FAILED_TO_WRITE_DEVICE",		/*	25	197	*/\
							"FT_EEPROM_READ_FAILED",			/*	21	222	*/\
							"FT_EEPROM_WRITE_FAILED",			/*	22	243	*/\
							"FT_EEPROM_ERASE_FAILED",			/*	22	265	*/\
							"FT_EEPROM_NOT_PRESENT",			/*	21	287	*/\
							"FT_EEPROM_NOT_PROGRAMMED",			/*	24	308	*/\
							"FT_INVALID_ARGS",					/*	15	332	*/\
							"FT_NOT_SUPPORTED",					/*	16	347	*/\
							"FT_OTHER_ERROR",					/*	14	363	*/\
							"FT_DEVICE_LIST_NOT_READ"			/*	24	377	*/
						};

/* Static functions */

#ifdef POL_FTDI_USE_DLL
#define	FTDI_DLL	("ftd2xx.dll")			/* FTDI DLL file name and path */

CREATE_DEVICE_INFO_LIST    CreateDeviceInfoList;
GET_DEVICE_INFO_LIST       GetDeviceInfoList;
GET_DEVICE_INFO_DETAIL     GetDeviceInfoDetail;

FT_GET_QUEUE_STATUS        FtGetQueueStatus;

FT_OPEN_EX                 Ftdi_OpenEx;
FT_OPEN                    Ftdi_Open;
FT_READ                    Ftdi_Read;
FT_WRITE                   Ftdi_Write;
FT_CLOSE                   Ftdi_Close;
FT_PURGE                   Ftdi_Purge;

GET_BIT_MODE               GetBitMode;
SET_BIT_MODE               SetBitMode;

GET_STATUS                 GetStatus;
CYCLE_PORT                 CyclePort;
RESET_PORT                 ResetPort;

FT_EE_READ                 Ftdi_Ee_Read;
FT_SET_LATENCY_TIMER       SetLatencyTimer;
FT_SET_USB_PARAMETERS      SetUsbParameters;
FT_SET_FLOW_CONTROL        SetFlowControl;

HMODULE hftdi;

void UnloadFtdiDLL()
{
	FreeLibrary(hftdi);
}

int LoadFtdiDLL()
{
	hftdi = LoadLibrary(FTDI_DLL);
	if (hftdi == NULL) return 1;

	CreateDeviceInfoList = (CREATE_DEVICE_INFO_LIST)   GetProcAddress(hftdi, "FT_CreateDeviceInfoList");
	GetDeviceInfoList    = (GET_DEVICE_INFO_LIST)      GetProcAddress(hftdi, "FT_GetDeviceInfoList");
	GetDeviceInfoDetail  = (GET_DEVICE_INFO_DETAIL)    GetProcAddress(hftdi, "FT_GetDeviceInfoDetail");

	FtGetQueueStatus     = (FT_GET_QUEUE_STATUS)       GetProcAddress(hftdi, "FT_GetQueueStatus");

	Ftdi_OpenEx          = (FT_OPEN_EX)                GetProcAddress(hftdi, "FT_OpenEx");               // used in ftdi_init.cpp, analyzer_main
	Ftdi_Open            = (FT_OPEN)                   GetProcAddress(hftdi, "FT_Open");
	Ftdi_Read            = (FT_READ)                   GetProcAddress(hftdi, "FT_Read");                 // used in usb_wrupper.cpp
	Ftdi_Write           = (FT_WRITE)                  GetProcAddress(hftdi, "FT_Write");                // used in usb_wrupper.cpp
	Ftdi_Close           = (FT_CLOSE)                  GetProcAddress(hftdi, "FT_Close");                // used in analyzer_main
	Ftdi_Purge           = (FT_PURGE)                  GetProcAddress(hftdi, "FT_Purge");                // used in analyzer_main

	GetBitMode           = (GET_BIT_MODE)              GetProcAddress(hftdi, "FT_GetBitMode");
	SetBitMode           = (SET_BIT_MODE)              GetProcAddress(hftdi, "FT_SetBitMode");

	GetStatus            = (GET_STATUS)                GetProcAddress(hftdi, "FT_GetStatus");
	CyclePort            = (CYCLE_PORT)                GetProcAddress(hftdi, "FT_CyclePort");
	ResetPort            = (RESET_PORT)                GetProcAddress(hftdi, "FT_ResetPort");

	Ftdi_Ee_Read         = (FT_EE_READ)                GetProcAddress(hftdi, "FT_EE_Read");
	SetLatencyTimer      = (FT_SET_LATENCY_TIMER)      GetProcAddress(hftdi, "FT_SetLatencyTimer");
	SetUsbParameters     = (FT_SET_USB_PARAMETERS)     GetProcAddress(hftdi, "FT_SetUSBParameters");
	SetFlowControl       = (FT_SET_FLOW_CONTROL)       GetProcAddress(hftdi, "FT_SetFlowControl");

	atexit(UnloadFtdiDLL);

	return 0;
}
#endif


/**
	@brief	Low level FTDI initialization
	@return	error code or '0' meaning Ok.
*/
static FT_STATUS Ftdi_Init(FT_HANDLE *pFtHandle)
{
	FT_STATUS ftStatus;
	int NumDev, i;
	FT_DEVICE_LIST_INFO_NODE *devInfo;

	ftStatus = Ftdi_CreateDeviceInfoList((LPDWORD)&NumDev);

	if(!ftStatus)																					/* FT_OK */
	{
		ftStatus = FT_DEVICE_NOT_FOUND;
		if(NumDev)
		{
			devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*NumDev);	/* allocate storage for list based on numDevs */
			Ftdi_GetDeviceInfoList(devInfo,(LPDWORD)&NumDev);										/* get the device information list */
			for(i = 0; i < NumDev; i++)
			{
				if(!strncmp(USB_DEV_NAME, devInfo[i].SerialNumber, 4))								/* check for first device with similar name */
				{
					if(!devInfo[i].ftHandle)
					{
						ftStatus = Ftdi_Open(i, pFtHandle);
					}
					else
					{
						MyFtHandle = devInfo[i].ftHandle;
						ftStatus = FT_OK;
					}
					memcpy(&(psUSB->sUSB_info), &devInfo[i], sizeof(FT_DEVICE_LIST_INFO_NODE));
					psUSB->sUSB_info.ftHandle = MyFtHandle;
					break;
				}
			}
			free(devInfo);
		}
	}

	if(ftStatus)			/* if no USB or FT devices present */
	{
		return ftStatus;
	}

	/*	 Mode values:
	0x0= Reset
	0x1= Asynchronous Bit Bang
	0x2= MPSSE (FT2232, FT2232H, FT4232H and FT232H devices only)
	0x4= Synchronous Bit Bang (FT232R, FT245R, FT2232, FT2232H, FT4232H and FT232H devices only)
	0x8= MCU Host Bus Emulation Mode (FT2232, FT2232H, FT4232H and FT232H devices only)
	0x10 = Fast Opto-Isolated Serial Mode (FT2232, FT2232H, FT4232H and FT232H devices only)
	0x20 = CBUS Bit Bang Mode (FT232R and FT232H devices only)
	0x40 = Single Channel Synchronous 245 FIFO Mode (FT2232H and FT232H devices only)
	*/
	Ftdi_SetBitMode(*pFtHandle, FIFO245_MASK, FIFO245_RST);	/* enable synchronous FIFO-245 */
	Sleep(1000);
	Ftdi_SetBitMode(*pFtHandle, FIFO245_MASK, FIFO245_MODE);	/* enable synchronous FIFO-245 */
	Ftdi_SetUsbParameters(*pFtHandle, FIFO245_USB_PARAM, FIFO245_USB_PARAM);	/* In, Out; (OUT - not supported) ?? */

	/*
	In the FT8U232AM and FT8U245AM devices, the receive buffer timeout that is used
	to flush remaining data from the receive buffer was fixed at 16 ms.
	In all other FTDI devices, this timeout is programmable and can be set
	at 1 ms intervals between 2ms and 255 ms. This allows the device to be
	better optimized for protocols requiring faster response times from short data packets.
	*/
	Ftdi_SetLatencyTimer(*pFtHandle, FIFO245_LATENCY);			/* 2-255; 16 default 2=best performance ?? */
	return Ftdi_SetFlowControl(*pFtHandle, FT_FLOW_RTS_CTS, 0, 0);		/* 0x11, 0x13); */
}

/**
	@brief	Reads the USB device's eeprom and prints the data
	@verbatim
		Read EEPROM Device 0
		128 Words; 256 bytes; 2048 bits
		0000: 0100 0304 1460 0009 A02D 0800 0101 A00A 
		0008: AA12 BC12 0000 0000 0000 0000 4400 5600 
		0010: 0000 0000 0000 0000 0000 0000 0000 0000 
		0018: 0000 0000 0000 0000 0000 0000 0000 0000 
		0020: 0000 0000 0000 0000 0000 0000 0000 0000 
		0028: 0000 0000 0000 0000 0000 0000 0000 0000 
		0030: 0000 0000 0000 0000 0000 0000 0000 0000 
		0038: 0000 0000 0000 0000 0000 0000 0000 0000 
		0040: 0000 0000 0000 0000 0000 4800 0000 0000 
		0048: 0000 0000 0000 0000 0000 0000 0000 0000 
		0050: 0A03 4600 5400 4400 4900 1203 5500 4D00 // _ F T D I _ U M
		0058: 3200 3300 3200 4800 2D00 4200 1203 5300 // 2 3 2 H - B _ S
		0060: 6F00 6E00 6100 3000 3000 3000 3000 0000 // o n a 0 0 0 0 _
		0068: 0000 0000 0000 0000 0000 0000 0000 0000 
		0070: 0000 0000 0000 0000 0000 0000 0000 0000 
		0078: 0000 0000 0000 0000 0000 0000 0000 D176 
	@endverbatim
*/
static FT_STATUS USB_ReadEeprom(void)
{
	char ManufacturerBuf[FT_MANUFACTURER_BUF];		// we must reserve enough room in buffers
	char ManufacturerIdBuf[FT_MANUFACTURERID_BUF];	// to read all four strings
	char DescriptionBuf[FT_DDESCRIPTION_BUF];		// including null_terminator
	char SerialNumberBuf[FT_SERIALNUM_BUF];
	PFT_PROGRAM_DATA pData;

	pData=(PFT_PROGRAM_DATA)(&psUSB->ft_Data);
	memset (pData, 0, sizeof(FT_PROGRAM_DATA));

	pData->Signature1 = 0;							// required by FTDI (ftd2xx.h)
	pData->Signature2 = 0xffffffff;					// required by FTDI (ftd2xx.h)
	pData->Version    = 5;
	pData->Manufacturer   = ManufacturerBuf;
	pData->ManufacturerId = ManufacturerIdBuf;
	pData->Description    = DescriptionBuf;
	pData->SerialNumber   = SerialNumberBuf;

	return Ftdi_Ee_Read(MyFtHandle, pData);
}

/** API functions **/
/**
	@brief	Initialized the USB interface. This function must be
			called before using any other function in this module.
			If the USB initialization succeeds, the function returns 0 and
			the USB_buf pointer is valid. In this case USB_buf data is filled
			with a structure of type tUSBinit containing details of the usb device.
			The only case where the function returns non-zero is if it can't malloc USB_buf
	@return	error code or '0' meaning Ok.
*/
int USB_Init(void)
{
	FT_STATUS ftStatus;

#ifdef POL_FTDI_USE_DLL
	if(LoadFtdiDLL())
	{
		return(E_USB_MISSING_DLL);
	}
#endif
	MyFtHandle = NULL;
	if(!USB_buf)												/* if 1st time initialization, create the USB buffer */
	{
		USB_buf = (uint8_t*)malloc(USB_BUF_SZ_W);					// USB Buffer 1 block = 32x32x128x2bytes + 1024 jic
		if(!USB_buf) return FT_INSUFFICIENT_RESOURCES;
	}
	psUSB = (tUSBinit*)USB_buf;
	memset(psUSB, 0, sizeof(tUSBinit));
	ftStatus = Ftdi_Init(&MyFtHandle);										// init USB 2.0 dongle
	if(ftStatus) return ftStatus;
	return USB_ReadEeprom();
}

/**
	@brief	sends data to the VC707 board via USB transfer
	@param	id, is the type of communication (defined in sona_link.cpp)
	@param	flags, msg flags to caracterize the request
	@param	*data, pointer to the data field location
	@param	count, size of the data field in words (4 bytes each)
	@return	FT_OK or FT_STATUS error code
*/
int USB_SendData(int id, int flags, int *data, int count)
{
	uint8_t *ptemp;

	ptemp = USB_buf;
	*(TAG_NUM_TYPE*)(ptemp + TAG_NUM_OFFSET) = (TAG_NUM_TYPE)id;
	*(TAG_FLG_TYPE*)(ptemp + TAG_FLG_OFFSET) = (TAG_FLG_TYPE)flags;
	count *= 4;
	*(TAG_LEN_TYPE*)(ptemp + TAG_LEN_OFFSET) = (TAG_LEN_TYPE)count;
	memcpy(ptemp + TAG_HEADER_SIZE, (void*)data, count);
	count += TAG_HEADER_SIZE;

	return(int)Ftdi_Write(MyFtHandle, (void*)ptemp, count, &USB_data_cnt);
}

/**
	@brief	sends 1 word (4 bytes) to the VC707 board via USB transfer
	@param	id, is the type of communication
	@param	flags, msg flags to caracterize the request
	@param	data, the word (4 bytes) data to be sent
	@return	FT_OK or FT_STATUS error code
*/
int USB_SendWord(int id, int flags, int data)
{
	uint8_t *ptemp;

	ptemp = USB_buf;
	*(TAG_CAT_TYPE*)(ptemp + TAG_CAT_OFFSET) = CMD_BIGRED;
	*(TAG_NUM_TYPE*)(ptemp + TAG_NUM_OFFSET) = (TAG_NUM_TYPE)id;
	*(TAG_FLG_TYPE*)(ptemp + TAG_FLG_OFFSET) = (TAG_FLG_TYPE)(flags);
	*(TAG_LEN_TYPE*)(ptemp + TAG_LEN_OFFSET) = (TAG_LEN_TYPE)4;
	*(uint32_t*)(ptemp + TAG_DATA_OFFSET) = data;

	return(int)Ftdi_Write(MyFtHandle, (void*)ptemp, (4 + TAG_HEADER_SIZE), &USB_data_cnt);
}

/**
	@brief	sends 1 byte to the VC707 board via USB transfer
	@param	id, is the type of communication
	@param	flags, msg flags to caracterize the request
	@param	data, the byte value to be sent
	@return	FT_OK or FT_STATUS error code
*/
extern int USB_SendByte(int id, int flags, int data)
{
	uint8_t *ptemp;

	ptemp = USB_buf;
	*(TAG_CAT_TYPE*)(ptemp + TAG_CAT_OFFSET) = CMD_BIGRED;
	*(TAG_NUM_TYPE*)(ptemp + TAG_NUM_OFFSET) = (TAG_NUM_TYPE)id;
	*(TAG_FLG_TYPE*)(ptemp + TAG_FLG_OFFSET) = (TAG_FLG_TYPE)(flags & ~TAG_F_1BYTE);	/* TODO: check */
	*(TAG_LEN_TYPE*)(ptemp + TAG_LEN_OFFSET) = (TAG_LEN_TYPE)1;
	*(ptemp + TAG_DATA_OFFSET) = data;

	return(int)Ftdi_Write(MyFtHandle, (void*)ptemp, (1 + TAG_HEADER_SIZE), &USB_data_cnt);
}

/**
	@brief	sends data to the VC707 board via USB transfer
	@param	id, is the type of communication (defined in sona_link.cpp)
	@param	flags, msg flags to caracterize the request
	@param	*data, pointer to the data field location
	@param	count, size of the data field in bytes
	@return	FT_OK or FT_STATUS error code
*/
extern int USB_SendBytes(int id, int flags, uint8_t *data, int count)
{
	uint8_t *ptemp;

	ptemp = USB_buf;
	*(TAG_CAT_TYPE*)(ptemp + TAG_CAT_OFFSET) = CMD_BIGRED;
	*(TAG_NUM_TYPE*)(ptemp + TAG_NUM_OFFSET) = (TAG_NUM_TYPE)id;
	*(TAG_FLG_TYPE*)(ptemp + TAG_FLG_OFFSET) = (TAG_FLG_TYPE)(flags);
	*(TAG_LEN_TYPE*)(ptemp + TAG_LEN_OFFSET) = (TAG_LEN_TYPE)count;
	memcpy(ptemp + TAG_DATA_OFFSET, (void*)data, count);
	count += TAG_HEADER_SIZE;

	while(count > BR_TX_PAGE_LIMIT)
	{
		Ftdi_Write(MyFtHandle, (void*)ptemp, BR_TX_PAGE_LIMIT, &USB_data_cnt);
		count -= BR_TX_PAGE_SIZE;
		ptemp += BR_TX_PAGE_SIZE;
	}
	return(int)Ftdi_Write(MyFtHandle, (void*)ptemp, count, &USB_data_cnt);
}

extern int USB_SendCommand(int id, int flags)
{
	uint8_t *ptemp;

	ptemp = USB_buf;
	*(TAG_CAT_TYPE*)(ptemp + TAG_CAT_OFFSET) = CMD_BIGRED;
	*(TAG_NUM_TYPE*)(ptemp + TAG_NUM_OFFSET) = (TAG_NUM_TYPE)id;
	*(TAG_FLG_TYPE*)(ptemp + TAG_FLG_OFFSET) = (TAG_FLG_TYPE)(flags);
	*(TAG_LEN_TYPE*)(ptemp + TAG_LEN_OFFSET) = (TAG_LEN_TYPE)0;

	return(int)Ftdi_Write(MyFtHandle, (void*)ptemp, (int)TAG_HEADER_SIZE, &USB_data_cnt);
}

/**
	@brief	receives a data stream from the VC707 vis USB transfer
	@param	*data, location where the received data will be stored
	@param	count, number of data bytes requested.
	@return	on sucess: number of bytes received, 0 otherwise.
*/
int USB_ReadData(unsigned char *data, int count)
{
	FT_STATUS ftStatus;

	ftStatus = Ftdi_Read(MyFtHandle, (void *)data, count, &USB_data_cnt);

	return (ftStatus ? 0 : (int)USB_data_cnt);		/* on sucess return number of bytes received, 0 otherwise. */
}

/**
	@brief	this function retrieves a block of data from the USB buffer.
	@param	count: number of bytes desired to be read
	@return	positive: number of bytes read - NOTE: always stored @ USB_buf
			-12: time out occured while waiting for data
			-13: FTDI reported error
			
*/
int USB_ReceiveData(uint32_t count)
{
	DWORD RxBytes;
	int32_t fail_counter;
	FT_STATUS ftStatus;
	uint8_t *data;

	data = USB_buf;
	while(count)
	{
		fail_counter = 0xffff;
		while(1)
		{
			ftStatus = Ftdi_GetQueueStatus(MyFtHandle, &RxBytes);
			if(ftStatus) return -1;
			// delay function here
			if( --fail_counter<0) return -12;
			if(RxBytes) break;
		}
		if(RxBytes > count) RxBytes = count;
		ftStatus = Ftdi_Read(MyFtHandle, (void *)data, RxBytes, &USB_data_cnt);
		if(ftStatus) return -13;
		data += USB_data_cnt;
		count -= USB_data_cnt;
	}

	count = (uint32_t)(data - (uint8_t*)USB_buf);
	return (int)count;				/* on sucess return number of bytes received */
}

/**
	@brief this function receives a USB Tagged packed, including the data asociated with it.
	@param	data is a pointer to a ram location where data belonging to the tag will be stored
	@return Positive: number of bytes of asociated data read
			0: Tag read ok but it has no data asociated.
			-11: timeout happened with no complete data received
			Tag header is stored in global TAG_header
*/
uint32_t USB_Rcvd_TAG(void)
{
	int32_t fail_counter;
	uint32_t RxBytes, count;

	fail_counter = 0xfffff;
	memset((void*)TAG_header, 0, sizeof(loc_TAG_header));
	while(1)
	{
		Ftdi_GetQueueStatus(MyFtHandle, (DWORD*)&RxBytes);
		if(RxBytes >= 8) break;
		// delay function here
		if( --fail_counter<0) return -11;					/* return negative indicates time out error */
	}

	Ftdi_Read(MyFtHandle, (void *)TAG_header, 8, &USB_data_cnt);
	count = *(uint32_t*)(TAG_header+4);

	if(count) return USB_ReceiveData(count);					/* if data follows, pick it up */
	else return count;											/* return 0 otherwise. */
}

/**
	@brief: purges the USB fifo pipe resquested by <mask>
*/
void USB_Purge(int mask)
{
	Ftdi_Purge(MyFtHandle, mask);		// purge requested FIFO
}

/**
	@brief	closes the USB connection and releases all related resources
	@return	Ok status (0) or error code
*/
int USB_De_Init(void)
{
	free(USB_buf);
	USB_buf = NULL;
	return (int)Ftdi_Close(MyFtHandle);				// this will close FT232H
}

uint32_t USB_Rx_Queue(void)
{
	DWORD Rx_Cnt;
	Ftdi_GetQueueStatus(MyFtHandle, &Rx_Cnt);
	return Rx_Cnt;
}


