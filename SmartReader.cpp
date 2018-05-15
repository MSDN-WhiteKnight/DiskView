/*
Project: DiskView 
Author: MSDN.WhiteKnight (https://github.com/MSDN-WhiteKnight)
License: WTFPL
*/
#include "SmartReader.h"

//Based on: https://www.codeproject.com/Articles/16671/Hard-drive-information-using-S-M-A-R-T

/*Global variables*/
SMARTINFOMAP m_oSmartInfo; //map для хранения SMART-показателей
ST_DRIVE_INFO m_stDrivesInfo[32]; //массив с данными о жестких дисках

/*Functions*/

//преобразование идетификатора SMART-показателя в строку

char* SmartIndexToString(BYTE index)
{
	switch (index)
	{
case SMART_ATTRIB_RAW_READ_ERROR_RATE	: return "RAW_READ_ERROR_RATE";
case  SMART_ATTRIB_THROUGHPUT_PERFORMANCE	: return "THROUGHPUT_PERFORMANCE";
case  SMART_ATTRIB_SPIN_UP_TIME			: return "SPIN_UP_TIME";
case  SMART_ATTRIB_START_STOP_COUNT		: return "START_STOP_COUNT";
case  SMART_ATTRIB_START_REALLOCATION_SECTOR_COUNT		: return "START_REALLOCATION_SECTOR_COUNT";
case  SMART_ATTRIB_SEEK_ERROR_RATE						: return "SEEK_ERROR_RATE";
case  SMART_ATTRIB_POWER_ON_HOURS_COUNT	: return "POWER_ON_HOURS_COUNT";
case  SMART_ATTRIB_SPIN_RETRY_COUNT		: return "SPIN_RETRY_COUNT";
case  SMART_ATTRIB_RECALIBRATION_RETRIES	: return "RECALIBRATION_RETRIES";
case  SMART_ATTRIB_DEVICE_POWER_CYCLE_COUNT	: return "DEVICE_POWER_CYCLE_COUNT";
case  SMART_ATTRIB_SOFT_READ_ERROR_RATE		: return "SOFT_READ_ERROR_RATE";
case  SMART_ATTRIB_LOAD_UNLOAD_CYCLE_COUNT		: return "LOAD_UNLOAD_CYCLE_COUNT";
case  SMART_ATTRIB_TEMPERATURE					: return "TEMPERATURE";
case  SMART_ATTRIB_ECC_ON_THE_FLY_COUNT		: return "ECC_ON_THE_FLY_COUNT";
case  SMART_ATTRIB_REALLOCATION_EVENT_COUNT	: return "REALLOCATION_EVENT_COUNT";
case  SMART_ATTRIB_CURRENT_PENDING_SECTOR_COUNT: return "CURRENT_PENDING_SECTOR_COUNT";
case  SMART_ATTRIB_UNCORRECTABLE_SECTOR_COUNT	: return "UNCORRECTABLE_SECTOR_COUNT";
case  SMART_ATTRIB_ULTRA_DMA_CRC_ERROR_COUNT	: return "ULTRA_DMA_CRC_ERROR_COUNT";
case  SMART_ATTRIB_WRITE_ERROR_RATE			: return "WRITE_ERROR_RATE	";
case  SMART_ATTRIB_TA_COUNTER_INCREASED		: return "TA_COUNTER_INCREASED";
case  SMART_ATTRIB_GSENSE_ERROR_RATE			: return "GSENSE_ERROR_RATE";
case  SMART_ATTRIB_POWER_OFF_RETRACT_COUNT		: return "POWER_OFF_RETRACT_COUNT	";

	default:
		return "-";
	}
}

//получение значения показателя из m_oSmartInfo
ST_SMART_INFO * GetSMARTValue(BYTE ucDriveIndex,BYTE ucAttribIndex)
{
	SMARTINFOMAP::iterator pIt;
	ST_SMART_INFO *pRet=NULL;

	pIt=m_oSmartInfo.find(MAKELPARAM(ucAttribIndex,ucDriveIndex));
	if(pIt!=m_oSmartInfo.end())
		pRet=(ST_SMART_INFO *)pIt->second;
	return pRet;
}

ST_DRIVE_INFO * GetDriveInfo(BYTE ucDriveIndex)
{	
	if(ucDriveIndex>=32)return NULL;
	return &m_stDrivesInfo[ucDriveIndex];
}

BOOL ReadSMARTAttributes(HANDLE hDevice,UCHAR ucDriveIndex)
{
	SENDCMDINPARAMS stCIP={0};
	DWORD dwRet=0;
	BOOL bRet=FALSE;
	BYTE	szAttributes[sizeof(ST_ATAOUTPARAM) + READ_ATTRIBUTE_BUFFER_SIZE - 1];
	UCHAR ucT1;
	PBYTE pT1,pT3;PDWORD pT2;
	ST_SMART_INFO *pSmartValues;

	stCIP.cBufferSize=READ_ATTRIBUTE_BUFFER_SIZE;
	stCIP.bDriveNumber =ucDriveIndex;
	stCIP.irDriveRegs.bFeaturesReg=READ_ATTRIBUTES;
	stCIP.irDriveRegs.bSectorCountReg = 1;
	stCIP.irDriveRegs.bSectorNumberReg = 1;
	stCIP.irDriveRegs.bCylLowReg = SMART_CYL_LOW;
	stCIP.irDriveRegs.bCylHighReg = SMART_CYL_HI;
	stCIP.irDriveRegs.bDriveHeadReg = DRIVE_HEAD_REG;
	stCIP.irDriveRegs.bCommandReg = SMART_CMD;
	bRet=DeviceIoControl(hDevice,SMART_RCV_DRIVE_DATA,&stCIP,sizeof(stCIP),szAttributes,sizeof(ST_ATAOUTPARAM) + READ_ATTRIBUTE_BUFFER_SIZE - 1,&dwRet,NULL);
	if(bRet)
	{
		m_stDrivesInfo[ucDriveIndex].m_ucSmartValues=0;
		m_stDrivesInfo[ucDriveIndex].m_ucDriveIndex=ucDriveIndex;
		pT1=(PBYTE)(((ST_ATAOUTPARAM*)szAttributes)->bBuffer);
		for(ucT1=0;ucT1<30;++ucT1)
		{
			pT3=&pT1[2+ucT1*12];
			pT2=(PDWORD)&pT3[INDEX_ATTRIB_RAW];
			pT3[INDEX_ATTRIB_RAW+2]=pT3[INDEX_ATTRIB_RAW+3]=pT3[INDEX_ATTRIB_RAW+4]=pT3[INDEX_ATTRIB_RAW+5]=pT3[INDEX_ATTRIB_RAW+6]=0;
			if(pT3[INDEX_ATTRIB_INDEX]!=0)
			{
				pSmartValues=&m_stDrivesInfo[ucDriveIndex].m_stSmartInfo[m_stDrivesInfo[ucDriveIndex].m_ucSmartValues];
				pSmartValues->m_ucAttribIndex=pT3[INDEX_ATTRIB_INDEX];
				pSmartValues->m_ucValue=pT3[INDEX_ATTRIB_VALUE];
				pSmartValues->m_ucWorst=pT3[INDEX_ATTRIB_WORST];
				pSmartValues->m_dwAttribValue=pT2[0];
				pSmartValues->m_dwThreshold=MAXDWORD;
				m_oSmartInfo[MAKELPARAM(pSmartValues->m_ucAttribIndex,ucDriveIndex)]=pSmartValues;
				m_stDrivesInfo[ucDriveIndex].m_ucSmartValues++;
			}
		}
	}
	else
		dwRet=GetLastError();

	stCIP.irDriveRegs.bFeaturesReg=READ_THRESHOLDS;
	stCIP.cBufferSize=READ_THRESHOLD_BUFFER_SIZE; // Is same as attrib size
	bRet=DeviceIoControl(hDevice,SMART_RCV_DRIVE_DATA,&stCIP,sizeof(stCIP),szAttributes,sizeof(ST_ATAOUTPARAM) + READ_ATTRIBUTE_BUFFER_SIZE - 1,&dwRet,NULL);
	if(bRet)
	{
		pT1=(PBYTE)(((ST_ATAOUTPARAM*)szAttributes)->bBuffer);
		for(ucT1=0;ucT1<30;++ucT1)
		{
			pT2=(PDWORD)&pT1[2+ucT1*12+5];
			pT3=&pT1[2+ucT1*12];
			pT3[INDEX_ATTRIB_RAW+2]=pT3[INDEX_ATTRIB_RAW+3]=pT3[INDEX_ATTRIB_RAW+4]=pT3[INDEX_ATTRIB_RAW+5]=pT3[INDEX_ATTRIB_RAW+6]=0;
			if(pT3[0]!=0)
			{
				pSmartValues=GetSMARTValue(ucDriveIndex,pT3[0]);
				if(pSmartValues)
					pSmartValues->m_dwThreshold=pT2[0];
			}
		}
	}
	return bRet;
}

BOOL CollectDriveInfo(HANDLE hDevice,UCHAR ucDriveIndex)
{
	BOOL bRet=FALSE;
	SENDCMDINPARAMS stCIP={0};
	DWORD dwRet=0;
	#define OUT_BUFFER_SIZE IDENTIFY_BUFFER_SIZE+16
	char szOutput[OUT_BUFFER_SIZE]={0};

	stCIP.cBufferSize=IDENTIFY_BUFFER_SIZE;
	stCIP.bDriveNumber =ucDriveIndex;
	stCIP.irDriveRegs.bFeaturesReg=0;
	stCIP.irDriveRegs.bSectorCountReg = 1;
	stCIP.irDriveRegs.bSectorNumberReg = 1;
	stCIP.irDriveRegs.bCylLowReg = 0;
	stCIP.irDriveRegs.bCylHighReg = 0;
	stCIP.irDriveRegs.bDriveHeadReg = DRIVE_HEAD_REG;
	stCIP.irDriveRegs.bCommandReg = ID_CMD;

	bRet=DeviceIoControl(hDevice,SMART_RCV_DRIVE_DATA,&stCIP,sizeof(stCIP),szOutput,OUT_BUFFER_SIZE,&dwRet,NULL);
	if(bRet)
	{
		CopyMemory(&m_stDrivesInfo[ucDriveIndex].m_stInfo,szOutput+16,sizeof(ST_IDSECTOR));	
		;
	}
	else
		dwRet=GetLastError();
	return bRet;
}

BOOL IsSmartEnabled(HANDLE hDevice,UCHAR ucDriveIndex)
{
	SENDCMDINPARAMS stCIP={0};
	SENDCMDOUTPARAMS stCOP={0};
	DWORD dwRet=0;
	BOOL bRet=FALSE;

	stCIP.cBufferSize=0;
	stCIP.bDriveNumber =ucDriveIndex;
	stCIP.irDriveRegs.bFeaturesReg=ENABLE_SMART;
	stCIP.irDriveRegs.bSectorCountReg = 1;
	stCIP.irDriveRegs.bSectorNumberReg = 1;
	stCIP.irDriveRegs.bCylLowReg = SMART_CYL_LOW;
	stCIP.irDriveRegs.bCylHighReg = SMART_CYL_HI;
	stCIP.irDriveRegs.bDriveHeadReg = DRIVE_HEAD_REG;
	stCIP.irDriveRegs.bCommandReg = SMART_CMD;
	
	bRet=DeviceIoControl(hDevice,SMART_SEND_DRIVE_COMMAND,&stCIP,sizeof(stCIP),&stCOP,sizeof(stCOP),&dwRet,NULL);
	if(bRet)
	{

	}
	else
	{
		dwRet=GetLastError();
		sprintf(m_stDrivesInfo[ucDriveIndex].m_csErrorString,"Error %d in reading SMART Enabled flag",dwRet);		
	}
	return bRet;
}

//Считывает SMART-показатели для диска с указанным индексом
BOOL ReadSMARTInfo(BYTE ucDriveIndex)
{
	HANDLE hDevice=NULL;
	char szT1[MAX_PATH]={0};
	BOOL bRet=FALSE;
	DWORD dwRet=0;
	
	wsprintf(szT1,"\\\\.\\PHYSICALDRIVE%d",ucDriveIndex);
	hDevice=CreateFile(szT1,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM,NULL);
	if(hDevice!=INVALID_HANDLE_VALUE)
	{
		bRet=DeviceIoControl(hDevice,SMART_GET_VERSION,NULL,0,&m_stDrivesInfo[ucDriveIndex].m_stGVIP,sizeof(GETVERSIONINPARAMS),&dwRet,NULL);
		if(bRet)
		{			
			if((m_stDrivesInfo[ucDriveIndex].m_stGVIP.fCapabilities & CAP_SMART_CMD)==CAP_SMART_CMD)
			{
				if(IsSmartEnabled(hDevice,ucDriveIndex))
				{
					bRet=CollectDriveInfo(hDevice,ucDriveIndex);
					bRet=ReadSMARTAttributes(hDevice,ucDriveIndex);
				}
			}
		}
		CloseHandle(hDevice);
	}
	else ErrorMes("CreateFile");
	return bRet;
}