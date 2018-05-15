/*
Project: DiskView 
Author: MSDN.WhiteKnight (https://github.com/MSDN-WhiteKnight)
License: WTFPL
*/

#include <stdio.h>
#include <tchar.h>
#include <WINDOWS.H>
#include <WinIoCtl.h>
#include <locale.h>
#include "SmartReader.h"


//вывод сообщения об ошибке
void ErrorMes(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    printf("%s failed with error %d: %s", 
        lpszFunction, dw, lpMsgBuf);     

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);

}

void SwapBytes(char* p, size_t len){
	
	if(len % 2 != 0)len--;	

	char t;
	for(int i=0;i<len-1;i+=2){
		t=p[i];
		p[i]=p[i+1];
		p[i+1]=t;
	}
}

#define BUF_LEN 4096

//вывод содержимого журнала USN для тома
bool PrintJournal(TCHAR* volume,WORD wYear,WORD wMonth, WORD wDay, WORD wHour,WORD wMin, UINT max_count){
   HANDLE hVol;
   CHAR Buffer[BUF_LEN];
   bool result = true;

   USN_JOURNAL_DATA JournalData;
   READ_USN_JOURNAL_DATA ReadData = {0, 0xFFFFFFFF, FALSE, 0, 0};
   PUSN_RECORD UsnRecord; 
   SYSTEMTIME st;
   int c=0;

   DWORD dwBytes;
   DWORD dwRetBytes;

   hVol = CreateFile( volume, 
               GENERIC_READ | GENERIC_WRITE, 
               FILE_SHARE_READ | FILE_SHARE_WRITE,
               NULL,
               OPEN_EXISTING,
               0,
               NULL);

   if( hVol == INVALID_HANDLE_VALUE )
   {
      printf("CreateFile failed (%d)\n", GetLastError());
	  result = false;
      goto End;
   }

   if( !DeviceIoControl( hVol, 
          FSCTL_QUERY_USN_JOURNAL, 
          NULL,
          0,
          &JournalData,
          sizeof(JournalData),
          &dwBytes,
          NULL) )
   {
      printf( "Query journal failed (%d)\n", GetLastError());
	  result = false;
      goto End;
   }

   ReadData.UsnJournalID = JournalData.UsnJournalID;

   printf( "Journal ID: 0x%I64x\n\n", JournalData.UsnJournalID );   

   while(true)
   {
      memset( Buffer, 0, BUF_LEN );

      if( !DeviceIoControl( hVol, 
            FSCTL_READ_USN_JOURNAL, 
            &ReadData,
            sizeof(ReadData),
            &Buffer,
            BUF_LEN,
            &dwBytes,
            NULL) )
      {
         printf( "Read journal failed (%d)\n", GetLastError());
		 result = false;
         goto End;
      }

      dwRetBytes = dwBytes - sizeof(USN);

      // Find the first record
      UsnRecord = (PUSN_RECORD)(((PUCHAR)Buffer) + sizeof(USN));  	     

      // This loop could go on for a long time, given the current buffer size.
      while( dwRetBytes > 0 )
      {
		  
          //получаем время записи...
         if(FileTimeToSystemTime((FILETIME*)&(UsnRecord->TimeStamp),&st)==false){
             printf( "\nError: Failed to get time\n");
			 result = false;
             goto End;
         }      

		 if( (st.wYear >= wYear && st.wMonth>=wMonth && st.wDay>=wDay && st.wHour>=wHour && st.wMinute>=wMin) == FALSE) 
			 goto Continue;

         //выводим данные
         printf( "%4d.%02d.%02d %2d:%02d\n",(int)st.wYear,(int)st.wMonth,(int)st.wDay,(int)st.wHour,(int)st.wMinute); //время записи
         printf("Reference number: 0x%I64x\n", UsnRecord->FileReferenceNumber ); //ID файла
         printf("File name: %.*S\n", UsnRecord->FileNameLength/2, UsnRecord->FileName ); //имя файла         

         printf( "Reason: 0x%x", UsnRecord->Reason ); //причина изменений
         if( (UsnRecord->Reason & USN_REASON_FILE_DELETE)>0)printf( " DeleteFile" );    
         if( (UsnRecord->Reason & USN_REASON_FILE_CREATE)>0)printf( " CreateFile" );
         if( (UsnRecord->Reason & USN_REASON_DATA_OVERWRITE)>0)printf( " DataOverwrite" );
         if( (UsnRecord->Reason & USN_REASON_DATA_EXTEND)>0)printf( " DataExtend" );
         printf( "\n\n" );       
         c++;
         if(c>=max_count) goto End;//если прочитано указанное количество записей, выходим

		 Continue:
         dwRetBytes -= UsnRecord->RecordLength;

         // Find the next record
         UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + 
                  UsnRecord->RecordLength); 
      }

	  if(ReadData.StartUsn == (*(USN *)&Buffer))goto End;

      // Update starting USN for next call
      ReadData.StartUsn = *(USN *)&Buffer; 
   }

   End:
   CloseHandle(hVol);  
   return result;
}


/*Отображение информации о файле, расположенном в указанном кластере указанного тома*/
BOOL FindFileByClaster(TCHAR* volume,LONGLONG cluster){

    HANDLE hDevice = CreateFile(volume,      // drive to open
        GENERIC_READ ,                       // access to the drive
        FILE_SHARE_READ | FILE_SHARE_WRITE,  // share mode
        NULL,                                // default security attributes
        OPEN_EXISTING,                       // disposition
        FILE_FLAG_BACKUP_SEMANTICS,          // file attributes
        NULL);

    if(hDevice == INVALID_HANDLE_VALUE)
    {   
          ErrorMes("CreateFile");
          return FALSE;
    }

    //входные параметры
    LOOKUP_STREAM_FROM_CLUSTER_INPUT input={0};
    input.NumberOfClusters = 1;
    input.Cluster[0].QuadPart = cluster;        

    //буфер для результатов
    BYTE output[5000]={};
    DWORD dwRes=0;
    LOOKUP_STREAM_FROM_CLUSTER_OUTPUT result={0};   

    //пытаемся отправить IOCTL...
    BOOL bRes = DeviceIoControl( (HANDLE)       hDevice,   // handle to file, directory, or volume 
                 FSCTL_LOOKUP_STREAM_FROM_CLUSTER, // dwIoControlCode
                 (LPVOID)       &input,        // input buffer 
                 (DWORD)        sizeof(input),     // size of input buffer 
                 (LPVOID)       output,       // output buffer 
                 (DWORD)        5000,    // size of output buffer 
                 (LPDWORD)      &dwRes,   // number of bytes returned 
                 NULL );    // OVERLAPPED structure

    if(bRes == FALSE){      
          ErrorMes("DeviceIoControl");
          return FALSE;
    }

    //переносим результат из массива в структуру LOOKUP_STREAM_FROM_CLUSTER_OUTPUT
    memcpy(&result,output,sizeof(LOOKUP_STREAM_FROM_CLUSTER_OUTPUT));

    if(result.NumberOfMatches == 0){
        wprintf( L"Not found\n");
        return FALSE;
    }   
	   

    //переходим к адресу первой структуры LOOKUP_STREAM_FROM_CLUSTER_ENTRY
    BYTE* p = (BYTE*)output + result.Offset;
    LOOKUP_STREAM_FROM_CLUSTER_ENTRY* pentry = (LOOKUP_STREAM_FROM_CLUSTER_ENTRY*)p;    

    //выводим информацию
	wprintf( L"File: %s\n",pentry->FileName);
    wprintf( L"Flags: 0x%x ",(UINT)pentry->Flags);

    if((pentry->Flags & LOOKUP_STREAM_FROM_CLUSTER_ENTRY_FLAG_PAGE_FILE) > 0) wprintf(L"(Pagefile)");
    else if((pentry->Flags & LOOKUP_STREAM_FROM_CLUSTER_ENTRY_FLAG_FS_SYSTEM_FILE) > 0)  wprintf(L"(Internal filesystem file)");
    else if((pentry->Flags & LOOKUP_STREAM_FROM_CLUSTER_ENTRY_FLAG_TXF_SYSTEM_FILE) > 0) wprintf(L"(Internal TXF file)");
    else wprintf(L"(Normal file)"); 

	wprintf( L"\n");
     
    return TRUE;
}

bool PrintVolumeInfo(TCHAR* volume)
{
	TCHAR szVolumeName[100]    = "";
	TCHAR szFileSystemName[10] = "";
	DWORD dwSerialNumber       = 0;
	DWORD dwMaxFileNameLength  = 0;
	DWORD dwFileSystemFlags    = 0;

	if(::GetVolumeInformation(volume,
                            szVolumeName,
                            sizeof(szVolumeName),
                            &dwSerialNumber,
                            &dwMaxFileNameLength,
                            &dwFileSystemFlags,
                            szFileSystemName,
                            sizeof(szFileSystemName)) == TRUE)
	{
			printf("              Volume: %s\n",volume);
			printf("         Volume name: %s\n",szVolumeName);
			printf("       Serial number: 0x%x\n",dwSerialNumber);
			printf("Max. filename length: %u\n",dwMaxFileNameLength);
			printf("    File system name: %s\n",szFileSystemName);
			printf("   File system flags: 0x%x\n\n",dwFileSystemFlags);
					
			printf("Features: ");
			if( (dwFileSystemFlags & FILE_FILE_COMPRESSION) >0)printf("Supports file compression; ");
			if( (dwFileSystemFlags & FILE_READ_ONLY_VOLUME) >0)printf("Read-only; ");
			if( (dwFileSystemFlags & FILE_SUPPORTS_USN_JOURNAL) >0)printf("Supports USN journal; ");
			if( (dwFileSystemFlags & FILE_VOLUME_IS_COMPRESSED) >0)printf("Compressed; ");
			if( (dwFileSystemFlags & FILE_SUPPORTS_ENCRYPTION) >0)printf("Supports EFS; ");
			if( (dwFileSystemFlags & FILE_PERSISTENT_ACLS) >0)printf("Supports ACLs; ");
			printf("\n\n");

		DWORD dwSectorsPerCluster, dwBytesPerSector, dwNumberOfFreeClusters, dwTotalNumberOfClusters;
		BOOL bResult = GetDiskFreeSpace(volume, &dwSectorsPerCluster,
                   &dwBytesPerSector, &dwNumberOfFreeClusters,
                   &dwTotalNumberOfClusters);

		if (bResult != FALSE) {
			printf(" Sectors per cluster: %u\n",(UINT)dwSectorsPerCluster);
			printf("    Bytes per sector: %u\n",(UINT)dwBytesPerSector);
			printf("       Free clusters: %u\n",(UINT)dwNumberOfFreeClusters);
			printf("      Total clusters: %u\n",(UINT)dwTotalNumberOfClusters);
			printf("      Free space, MB: %.3f\n",(float)(dwSectorsPerCluster*dwBytesPerSector*(dwNumberOfFreeClusters/1000.0/1000.0)));
			printf("     Total space, MB: %.3f\n",(float)(dwSectorsPerCluster*dwBytesPerSector*(dwTotalNumberOfClusters/1000.0/1000.0)));
			printf("  Free space percent: %.1f\n",(float)(dwNumberOfFreeClusters*100.0f/dwTotalNumberOfClusters));
		}


			    return true;
         
	}
	else
	{
		ErrorMes("GetVolumeInformation");
		return false;
	}

}


bool PrintDiskInfo(bool fGeneral,bool fSMART, UINT index){
	BOOL res = ReadSMARTInfo(index);
	if(res==FALSE){return false;}

	char sbuf[50]="";
	char* pch = NULL;
	printf("Physical disk #%d\n\n",index);

	if(fGeneral){
	ST_DRIVE_INFO * pInfo = GetDriveInfo(index);
	
		if(pInfo != NULL){			
			ZeroMemory(sbuf,sizeof(sbuf));		
			strncpy_s(sbuf,sizeof(sbuf),(char*)pInfo->m_stInfo.sFirmwareRev,8);		
			SwapBytes(sbuf,8);		
			printf("        Firmware revision: %s\n",sbuf);

			ZeroMemory(sbuf,sizeof(sbuf));
			strncpy_s(sbuf,sizeof(sbuf),(char*)pInfo->m_stInfo.sModelNumber,39);
			SwapBytes(sbuf,38);
			printf("             Model number: %s\n",sbuf);

			ZeroMemory(sbuf,sizeof(sbuf));
			strncpy_s(sbuf,sizeof(sbuf),(char*)pInfo->m_stInfo.sSerialNumber,20);
			SwapBytes(sbuf,20);

			pch = &(sbuf[0]);
			while(true){
				if(*pch != ' ')break;
				if(pch >= &(sbuf[18]))break;
				pch++;
			}
			
			printf("            Serial number: %s\n",pch);
				
			printf("                Cylinders: %d\n",(int)(pInfo->m_stInfo.wNumCyls));
			printf("                    Heads: %d\n",(int)(pInfo->m_stInfo.wNumHeads));
			printf("Total addressable sectors: %d\n",(int)(pInfo->m_stInfo.ulTotalAddressableSectors));
			printf("        Sectors per track: %d\n",(int)(pInfo->m_stInfo.wSectorsPerTrack));
			printf("          Sector capacity: %d\n",(int)(pInfo->m_stInfo.ulCurrentSectorCapacity));

			printf("\n");
		}
	}
	
	if(fSMART){
		ST_SMART_INFO * pSmart;
		printf("     S.M.A.R.T attribute                 Current Worst\n");
	
		for(int i=0;i<MAX_ATTRIBUTES;i++)
		{
			pSmart = GetSMARTValue(index,i);
			if(pSmart == NULL)continue;
			printf("0x%02x %35s %5u   %5u\n", pSmart->m_ucAttribIndex,SmartIndexToString(pSmart->m_ucAttribIndex),
				(UINT)pSmart->m_ucValue,(UINT)pSmart->m_ucWorst);
		
		}
		printf("\n");
	}
	return true;
}



int _tmain(int argc, _TCHAR* argv[])
{
	int code=0;
	int count=0;
	bool res=FALSE;
	TCHAR buf[50]="";
	LONGLONG inp=0;
	UINT year,month,day,hour,min,maxrecords;

	if(argc <= 1){
		printf("DiskView by MSDN.WhiteKnight (https://github.com/MSDN-WhiteKnight)\n");
		printf("License: WTFPL\n");
		printf("\n");
		printf("Displays hard disk or volume information\n\n");


		printf("Usage:\n");
		printf("  DiskView.exe -d (index)                 display disk general information\n");
		printf("  DiskView.exe -s (index)                 display disk S.M.A.R.T. attributes\n");
		printf("  DiskView.exe -v (letter):               display volume information\n");
		printf("  DiskView.exe -f (letter): (cluster)     find file by cluster number\n\n");

		printf("  DiskView.exe -usn (letter): (year)-(month)-(day) (hour)-(min) (maxrecords)\n\n");
		printf("Display up to (maxrecords) entries from volume's USN journal\nSearches records newer then specified date and time\n");
		
		goto End;
	}

	UINT index = 0;

	if(strcmp(argv[1],"-d")==0)
	{
		if(argc<=2){
			printf("Specify disk index\n");
			code = 1;
			goto End;
		}

		count = sscanf(argv[2],"%u",&index);
		if(count==0){
			printf("Disk index must be a number\n");
			code = 2;
			goto End;
		}
		res= PrintDiskInfo(true,false,index);

	}
	else if(strcmp(argv[1],"-s")==0)
	{
		if(argc<=2){
			printf("Specify disk index\n");
			code = 1;
			goto End;
		}

		count = sscanf(argv[2],"%u",&index);
		if(count==0){
			printf("Disk index must be a number\n");
			code = 2;
			goto End;
		}
		res= PrintDiskInfo(false,true,index);
	}
	else if(strcmp(argv[1],"-v")==0)
	{
		if(argc<=2){
			printf("Specify disk letter\n");
			code = 1;
			goto End;
		}

		sprintf(buf,"%s\\",argv[2]);
		res = PrintVolumeInfo(buf);
	}
	else if(strcmp(argv[1],"-f")==0)
	{
		if(argc<=2){
			printf("Specify disk letter\n");
			code = 1;
			goto End;
		}

		if(argc<=3){
			printf("Specify cluster number\n");
			code = 3;
			goto End;
		}

		count = sscanf(argv[3],"%llu",&inp);
		if(count==0){
			printf("Cluster number is wrong\n");
			code = 4;
			goto End;
		}
		sprintf(buf,"\\\\.\\%s",argv[2]);
		res = FindFileByClaster(buf,inp);
	}	
	else if(strcmp(argv[1],"-usn")==0)
	{
		if(argc<=2){
			printf("Specify disk letter\n");
			code = 1;
			goto End;
		}

		if(argc<6){
			printf("Too few arguments\n");
			code = 5;
			goto End;
		}

		count = sscanf(argv[3],"%u-%u-%u",&year,&month,&day);
		if(count<3){
			printf("Date is wrong\n");
			code = 4;
			goto End;
		}

		count = sscanf(argv[4],"%u-%u",&hour,&min);
		if(count<2){
			printf("Time is wrong\n");
			code = 4;
			goto End;
		}

		count = sscanf(argv[5],"%u",&maxrecords);
		if(count==0){
			printf("Wrong maxrecords value\n");
			code = 4;
			goto End;
		}
		
		sprintf(buf,"\\\\.\\%s",argv[2]);
		res = PrintJournal(buf,year,month,day,hour,min,maxrecords);
	}
	else {printf("Unknown command\n"); code = 6;}
	
	if(res==false)code=GetLastError();			

	End:	
	return code;
}

