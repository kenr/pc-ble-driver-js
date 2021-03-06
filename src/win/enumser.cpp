/*
Module : enumser.cpp
Purpose: Implementation for a class to enumerate the serial ports installed on a PC using a number
         of different approaches. 

Copyright (c) 1998 - 2013 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code. 

*/


/////////////////////////////////  Includes  //////////////////////////////////

#include "stdafx.h"
#include "enumser.h"
#include "AutoHModule.h"
#include "AutoHandle.h"
#include "AutoHeapAlloc.h"
#include <windows.h>

#ifndef NO_ENUMSERIAL_USING_WMI
#ifndef __ATLBASE_H__
  #include <atlbase.h>
  #pragma message("To avoid this message, please put atlbase.h in your pre compiled header (normally stdafx.h)")
#endif
#endif


static bool IsWindowsVistaOrGreater()
{
    OSVERSIONINFOEXW osvi = { };
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_VISTA);
    osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_VISTA);
    DWORDLONG conditoin = 0;
    VER_SET_CONDITION(conditoin, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditoin, VER_MINORVERSION, VER_GREATER_EQUAL);
    return !!::VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditoin);
}

/////////////////////////////// Macros / Defines //////////////////////////////

#if !defined(NO_ENUMSERIAL_USING_SETUPAPI1) || !defined(NO_ENUMSERIAL_USING_SETUPAPI2)
  #ifndef _INC_SETUPAPI
    #pragma message("To avoid this message, please put setupapi.h in your pre compiled header (normally stdafx.h)")
    #include <setupapi.h>
  #endif

  #ifndef GUID_DEVINTERFACE_COMPORT
    DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0x86E0D1E0L, 0x8089, 0x11D0, 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);
  #endif
  
  typedef HKEY (__stdcall SETUPDIOPENDEVREGKEY)(HDEVINFO, PSP_DEVINFO_DATA, DWORD, DWORD, DWORD, REGSAM);
  typedef BOOL (__stdcall SETUPDICLASSGUIDSFROMNAME)(LPCTSTR, LPGUID, DWORD, PDWORD);
  typedef BOOL (__stdcall SETUPDIDESTROYDEVICEINFOLIST)(HDEVINFO);
  typedef BOOL (__stdcall SETUPDIENUMDEVICEINFO)(HDEVINFO, DWORD, PSP_DEVINFO_DATA);
  typedef HDEVINFO (__stdcall SETUPDIGETCLASSDEVS)(LPGUID, LPCTSTR, HWND, DWORD);
  typedef BOOL (__stdcall SETUPDIGETDEVICEREGISTRYPROPERTY)(HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD, PDWORD);
#endif  

#ifndef NO_ENUMSERIAL_USING_ENUMPORTS
  #ifndef _WINSPOOL_
  #pragma message("To avoid this message, please put winspool.h in your pre compiled header (normally stdafx.h)")
  #include <winspool.h>
  #endif
#endif

#ifndef NO_ENUMSERIAL_USING_WMI
  #ifndef __IWbemLocator_FWD_DEFINED__
  #pragma message("To avoid this message, please put WBemCli.h in your pre compiled header (normally stdafx.h)")
  #include <WbemCli.h>
  #endif
  
  #ifndef _INC_COMDEF
  #pragma message("To avoid this message, please put comdef.h in your pre compiled header (normally stdafx.h)")
  #include <comdef.h>
  #endif

  #ifndef __ATLBASE_H__
  #pragma message("EnumSerialPorts as of v1.16 requires ATL support to implement its functionality. If your project is MFC only, then you need to update it to include ATL support")
  #endif
  
  //Automatically pull in the library WbemUuid.Lib since we need the WBem Guids
  #pragma comment(lib, "WbemUuid.Lib")
#endif

#ifndef NO_ENUMSERIAL_USING_COMDB
  #ifndef HCOMDB
    DECLARE_HANDLE(HCOMDB);
    typedef HCOMDB *PHCOMDB;
  #endif
  
  #ifndef CDB_REPORT_BYTES
    #define CDB_REPORT_BYTES 0x1  
  #endif
  
  typedef LONG (__stdcall COMDBOPEN)(PHCOMDB);
  typedef LONG (__stdcall COMDBCLOSE)(HCOMDB);
  typedef LONG (__stdcall COMDBGETCURRENTPORTUSAGE)(HCOMDB, PBYTE, DWORD, ULONG, LPDWORD);
#endif


///////////////////////////// Implementation //////////////////////////////////

#ifndef NO_ENUMSERIAL_USING_CREATEFILE
#if defined CENUMERATESERIAL_USE_STL
BOOL CEnumerateSerial::UsingCreateFile(std::vector<UINT>& ports)
#elif defined _AFX
BOOL CEnumerateSerial::UsingCreateFile(CUIntArray& ports)
#else
BOOL CEnumerateSerial::UsingCreateFile(CSimpleArray<UINT>& ports)
#endif
{
  //Make sure we clear out any elements which may already be in the array
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
#else
  ports.RemoveAll();
#endif  

  //Up to 255 COM ports are supported so we iterate through all of them seeing
  //if we can open them or if we fail to open them, get an access denied or general error error.
  //Both of these cases indicate that there is a COM port at that number. 
  for (UINT i=1; i<256; i++)
  {
    //Form the Raw device name
    CString sPort;
    sPort.Format(_T("\\\\.\\COM%u"), i);

    //Try to open the port
    BOOL bSuccess = FALSE;
    CAutoHandle port(CreateFile(sPort, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0));
    if (port == INVALID_HANDLE_VALUE)
    {
      DWORD dwError = GetLastError();

      //Check to see if the error was because some other app had the port open or a general failure
      if (dwError == ERROR_ACCESS_DENIED || dwError == ERROR_GEN_FAILURE || dwError == ERROR_SHARING_VIOLATION || dwError == ERROR_SEM_TIMEOUT)
        bSuccess = TRUE;
    }
    else
    {
      //The port was opened successfully
      bSuccess = TRUE;
    }

    //Add the port number to the array which will be returned
    if (bSuccess)
    {
    #if defined CENUMERATESERIAL_USE_STL
      ports.push_back(i);
    #else
      ports.Add(i);
    #endif  
    }
  }

  //Return the success indicator
  return TRUE;
}
#endif

#if !defined(NO_ENUMSERIAL_USING_SETUPAPI1) || !defined(NO_ENUMSERIAL_USING_SETUPAPI2) || !defined(NO_ENUMSERIAL_USING_COMDB)
HMODULE CEnumerateSerial::LoadLibraryFromSystem32(LPCTSTR lpFileName)
{
  //Get the Windows System32 directory
  TCHAR szFullPath[_MAX_PATH];
  szFullPath[0] = _T('\0');
  if (GetSystemDirectory(szFullPath, _countof(szFullPath)) == 0)
    return NULL;

  //Setup the full path and delegate to LoadLibrary    
  _tcscat_s(szFullPath, _countof(szFullPath), _T("\\"));
  _tcscat_s(szFullPath, _countof(szFullPath), lpFileName);
  return LoadLibrary(szFullPath);
}
#endif

#if !defined(NO_ENUMSERIAL_USING_SETUPAPI1) || !defined(NO_ENUMSERIAL_USING_SETUPAPI2)
BOOL CEnumerateSerial::RegQueryValueString(HKEY kKey, LPCTSTR lpValueName, LPTSTR& pszValue)
{
  //Initialize the output parameter
  pszValue = NULL;

  //First query for the size of the registry value 
  DWORD dwType = 0;
  DWORD dwDataSize = 0;
  LONG nError = RegQueryValueEx(kKey, lpValueName, NULL, &dwType, NULL, &dwDataSize);
  if (nError != ERROR_SUCCESS)
  {
    SetLastError(nError);
    return FALSE;
  }

  //Ensure the value is a string
  if (dwType != REG_SZ)
  {
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }

  //Allocate enough bytes for the return value
  DWORD dwAllocatedSize = dwDataSize + sizeof(TCHAR); //+sizeof(TCHAR) is to allow us to NULL terminate the data if it is not null terminated in the registry
  pszValue = reinterpret_cast<LPTSTR>(LocalAlloc(LMEM_FIXED, dwAllocatedSize)); 
  if (pszValue == NULL)
    return FALSE;

  //Recall RegQueryValueEx to return the data
  pszValue[0] = _T('\0');
  DWORD dwReturnedSize = dwAllocatedSize;
  nError = RegQueryValueEx(kKey, lpValueName, NULL, &dwType, reinterpret_cast<LPBYTE>(pszValue), &dwReturnedSize);
  if (nError != ERROR_SUCCESS)
  {
    LocalFree(pszValue);
    pszValue = NULL;
    SetLastError(nError);
    return FALSE;
  }

  //Handle the case where the data just returned is the same size as the allocated size. This could occur where the data
  //has been updated in the registry with a non null terminator between the two calls to ReqQueryValueEx above. Rather than
  //return a potentially non-null terminated block of data, just fail the method call
  if (dwReturnedSize >= dwAllocatedSize)
  {
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }

  //NULL terminate the data if it was not returned NULL terminated because it is not stored null terminated in the registry
  if (pszValue[dwReturnedSize/sizeof(TCHAR) - 1] != _T('\0'))
    pszValue[dwReturnedSize/sizeof(TCHAR)] = _T('\0');

  return TRUE;
}

BOOL CEnumerateSerial::QueryRegistryPortName(HKEY hDeviceKey, int& nPort)
{
  //What will be the return value from the method (assume the worst)
  BOOL bAdded = FALSE;

  //Read in the name of the port
  LPTSTR pszPortName = NULL;
  if (RegQueryValueString(hDeviceKey, _T("PortName"), pszPortName))
  {
    //If it looks like "COMX" then
    //add it to the array which will be returned
    size_t nLen = _tcslen(pszPortName);
    if (nLen > 3)
    {
      if ((_tcsnicmp(pszPortName, _T("COM"), 3) == 0) && IsNumeric((pszPortName + 3), FALSE))
      {
        //Work out the port number
        nPort = _ttoi(pszPortName + 3);

        bAdded = TRUE;
      }
    }
    LocalFree(pszPortName);
  }

  return bAdded;
}
#endif //#if !defined(NO_ENUMSERIAL_USING_SETUPAPI1) || !defined(NO_ENUMSERIAL_USING_SETUPAPI2)

BOOL CEnumerateSerial::IsNumeric(LPCSTR pszString, BOOL bIgnoreColon)
{
  size_t nLen = strlen(pszString);
  if (nLen == 0)
    return FALSE;

  //What will be the return value from this function (assume the best)
  BOOL bNumeric = TRUE;

  for (size_t i=0; i<nLen && bNumeric; i++)
  {
    bNumeric = (isdigit(static_cast<int>(pszString[i])) != 0);
    if (bIgnoreColon && (pszString[i] == ':'))
      bNumeric = TRUE;
  }

  return bNumeric;
}

BOOL CEnumerateSerial::IsNumeric(LPCWSTR pszString, BOOL bIgnoreColon)
{
  size_t nLen = wcslen(pszString);
  if (nLen == 0)
    return FALSE;

  //What will be the return value from this function (assume the best)
  BOOL bNumeric = TRUE;

  for (size_t i=0; i<nLen && bNumeric; i++)
  {
    bNumeric = (iswdigit(pszString[i]) != 0);
    if (bIgnoreColon && (pszString[i] == L':'))
      bNumeric = TRUE;
  }

  return bNumeric;
}

#ifndef NO_ENUMSERIAL_USING_QUERYDOSDEVICE
#if defined CENUMERATESERIAL_USE_STL
BOOL CEnumerateSerial::UsingQueryDosDevice(std::vector<UINT>& ports)
#elif defined _AFX
BOOL CEnumerateSerial::UsingQueryDosDevice(CUIntArray& ports)
#else
BOOL CEnumerateSerial::UsingQueryDosDevice(CSimpleArray<UINT>& ports)
#endif
{
  //What will be the return value from this function (assume the worst)
  BOOL bSuccess = FALSE;

  //Make sure we clear out any elements which may already be in the array
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
#else
  ports.RemoveAll();
#endif  

  //On NT use the QueryDosDevice API
  if (IsWindowsVistaOrGreater())
  {
    //Use QueryDosDevice to look for all devices of the form COMx. Since QueryDosDevice does
    //not consitently report the required size of buffer, lets start with a reasonable buffer size
    //of 4096 characters and go from there
    int nChars = 4096;
    BOOL bWantStop = FALSE;
    while (nChars && !bWantStop)
    {
      CAutoHeapAlloc devices;
      if (devices.Allocate(nChars * sizeof(TCHAR)))
      {
        LPTSTR pszDevices = static_cast<LPTSTR>(devices.m_pData);
        DWORD dwChars = QueryDosDevice(NULL, pszDevices, nChars);
        if (dwChars == 0)
        {
          DWORD dwError = GetLastError();
          if (dwError == ERROR_INSUFFICIENT_BUFFER)
          {
            //Expand the buffer and  loop around again
            nChars *= 2;
          }
          else
            bWantStop = TRUE;
        }
        else
        {
          bSuccess = TRUE;
          bWantStop = TRUE;
          size_t i=0;
          
          while (pszDevices[i] != _T('\0'))
          {
            //Get the current device name
            TCHAR* pszCurrentDevice = &(pszDevices[i]);

            //If it looks like "COMX" then
            //add it to the array which will be returned
            size_t nLen = _tcslen(pszCurrentDevice);
            if (nLen > 3)
            {
              if ((_tcsnicmp(pszCurrentDevice, _T("COM"), 3) == 0) && IsNumeric(&(pszCurrentDevice[3]), FALSE))
              {
                //Work out the port number
                int nPort = _ttoi(&pszCurrentDevice[3]);
              #if defined CENUMERATESERIAL_USE_STL
                ports.push_back(nPort);
              #else
                ports.Add(nPort);
              #endif  
              }
            }

            //Go to next device name
            i += (nLen + 1);
          }
        }
      }
      else
      {
        bWantStop = TRUE;
        SetLastError(ERROR_OUTOFMEMORY);        
      }
    }
  }
  else
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

  return bSuccess;
}
#endif

#ifndef NO_ENUMSERIAL_USING_GETDEFAULTCOMMCONFIG
#if defined CENUMERATESERIAL_USE_STL
BOOL CEnumerateSerial::UsingGetDefaultCommConfig(std::vector<UINT>& ports)
#elif defined _AFX
BOOL CEnumerateSerial::UsingGetDefaultCommConfig(CUIntArray& ports)
#else
BOOL CEnumerateSerial::UsingGetDefaultCommConfig(CSimpleArray<UINT>& ports)
#endif
{
  //Make sure we clear out any elements which may already be in the array
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
#else
  ports.RemoveAll();
#endif  

  //Up to 255 COM ports are supported so we iterate through all of them seeing
  //if we can get the default configuration
  for (UINT i=1; i<256; i++)
  {
    //Form the Raw device name
    CString sPort;
    sPort.Format(_T("COM%u"), i);

    COMMCONFIG cc;
    DWORD dwSize = sizeof(COMMCONFIG);
    if (GetDefaultCommConfig(sPort, &cc, &dwSize))
    {
    #if defined CENUMERATESERIAL_USE_STL
      ports.push_back(i);
    #else
      ports.Add(i);
    #endif  
    }
  }

  //Return the success indicator
  return TRUE;
}
#endif

#ifndef NO_ENUMSERIAL_USING_SETUPAPI1
#if defined CENUMERATESERIAL_USE_STL
#if defined _UNICODE
BOOL CEnumerateSerial::UsingSetupAPI1(std::vector<UINT>& ports, std::vector<std::wstring>& friendlyNames)
#else
BOOL CEnumerateSerial::UsingSetupAPI1(std::vector<UINT>& ports, std::vector<std::string>& friendlyNames)
#endif
#elif defined _AFX
BOOL CEnumerateSerial::UsingSetupAPI1(CUIntArray& ports, CStringArray& friendlyNames)
#else
BOOL CEnumerateSerial::UsingSetupAPI1(CSimpleArray<UINT>& ports, CSimpleArray<CString>& friendlyNames)
#endif
{
  //Make sure we clear out any elements which may already be in the array(s)
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
  friendlyNames.clear();
#else
  ports.RemoveAll();
  friendlyNames.RemoveAll();
#endif  

  //Get the various function pointers we require from setupapi.dll
  CAutoHModule setupAPI(LoadLibraryFromSystem32(_T("SETUPAPI.DLL")));
  if (setupAPI == NULL)
    return FALSE;

  SETUPDIOPENDEVREGKEY* lpfnLPSETUPDIOPENDEVREGKEY = reinterpret_cast<SETUPDIOPENDEVREGKEY*>(GetProcAddress(setupAPI, "SetupDiOpenDevRegKey"));
#if defined _UNICODE
  SETUPDIGETCLASSDEVS* lpfnSETUPDIGETCLASSDEVS = reinterpret_cast<SETUPDIGETCLASSDEVS*>(GetProcAddress(setupAPI, "SetupDiGetClassDevsW"));
  SETUPDIGETDEVICEREGISTRYPROPERTY* lpfnSETUPDIGETDEVICEREGISTRYPROPERTY = reinterpret_cast<SETUPDIGETDEVICEREGISTRYPROPERTY*>(GetProcAddress(setupAPI, "SetupDiGetDeviceRegistryPropertyW"));
#else
  SETUPDIGETCLASSDEVS* lpfnSETUPDIGETCLASSDEVS = reinterpret_cast<SETUPDIGETCLASSDEVS*>(GetProcAddress(setupAPI, "SetupDiGetClassDevsA"));
  SETUPDIGETDEVICEREGISTRYPROPERTY* lpfnSETUPDIGETDEVICEREGISTRYPROPERTY = reinterpret_cast<SETUPDIGETDEVICEREGISTRYPROPERTY*>(GetProcAddress(setupAPI, "SetupDiGetDeviceRegistryPropertyA"));
#endif
  SETUPDIDESTROYDEVICEINFOLIST* lpfnSETUPDIDESTROYDEVICEINFOLIST = reinterpret_cast<SETUPDIDESTROYDEVICEINFOLIST*>(GetProcAddress(setupAPI, "SetupDiDestroyDeviceInfoList"));
  SETUPDIENUMDEVICEINFO* lpfnSETUPDIENUMDEVICEINFO = reinterpret_cast<SETUPDIENUMDEVICEINFO*>(GetProcAddress(setupAPI, "SetupDiEnumDeviceInfo"));

  if ((lpfnLPSETUPDIOPENDEVREGKEY == NULL) || (lpfnSETUPDIDESTROYDEVICEINFOLIST == NULL) ||
      (lpfnSETUPDIENUMDEVICEINFO == NULL) || (lpfnSETUPDIGETCLASSDEVS == NULL) || (lpfnSETUPDIGETDEVICEREGISTRYPROPERTY == NULL))
  {
    //Set the error to report
    setupAPI.m_dwError = ERROR_CALL_NOT_IMPLEMENTED;

    return FALSE;
  }
  
  //Now create a "device information set" which is required to enumerate all the ports
  GUID guid = GUID_DEVINTERFACE_COMPORT;
  HDEVINFO hDevInfoSet = lpfnSETUPDIGETCLASSDEVS(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (hDevInfoSet == INVALID_HANDLE_VALUE)
  {
    //Set the error to report
    setupAPI.m_dwError = GetLastError();
  
    return FALSE;
  }

  //Finally do the enumeration
  BOOL bMoreItems = TRUE;
  int nIndex = 0;
  SP_DEVINFO_DATA devInfo;
  while (bMoreItems)
  {
    //Enumerate the current device
    devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
    bMoreItems = lpfnSETUPDIENUMDEVICEINFO(hDevInfoSet, nIndex, &devInfo);
    if (bMoreItems)
    {
      //Did we find a serial port for this device
      BOOL bAdded = FALSE;

      //Get the registry key which stores the ports settings
      HKEY hDeviceKey = lpfnLPSETUPDIOPENDEVREGKEY(hDevInfoSet, &devInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
      if (hDeviceKey)
      {
        int nPort = 0;
        if (QueryRegistryPortName(hDeviceKey, nPort))
        {
        #if defined CENUMERATESERIAL_USE_STL
          ports.push_back(nPort);
        #else
          ports.Add(nPort);
        #endif  
          bAdded = TRUE;
        }

        //Close the key now that we are finished with it
        RegCloseKey(hDeviceKey);
      }

      //If the port was a serial port, then also try to get its friendly name
      if (bAdded)
      {
        TCHAR szFriendlyName[1024];
        szFriendlyName[0] = _T('\0');
        DWORD dwSize = sizeof(szFriendlyName);
        DWORD dwType = 0;
        if (lpfnSETUPDIGETDEVICEREGISTRYPROPERTY(hDevInfoSet, &devInfo, SPDRP_DEVICEDESC, &dwType, reinterpret_cast<PBYTE>(szFriendlyName), dwSize, &dwSize) && (dwType == REG_SZ))
        {
        #if defined CENUMERATESERIAL_USE_STL
          friendlyNames.push_back(szFriendlyName);
        #else
          friendlyNames.Add(szFriendlyName);
        #endif  
        }
        else
        {
        #if defined CENUMERATESERIAL_USE_STL
          friendlyNames.push_back(_T(""));
        #else
          friendlyNames.Add(_T(""));
        #endif  
        }
      }
    }

    ++nIndex;
  }

  //Free up the "device information set" now that we are finished with it
  lpfnSETUPDIDESTROYDEVICEINFOLIST(hDevInfoSet);

  //Return the success indicator
  return TRUE;
}
#endif

#ifndef NO_ENUMSERIAL_USING_SETUPAPI2
#if defined CENUMERATESERIAL_USE_STL
#if defined _UNICODE
BOOL CEnumerateSerial::UsingSetupAPI2(std::vector<UINT>& ports, std::vector<std::wstring>& friendlyNames)
#else
BOOL CEnumerateSerial::UsingSetupAPI2(std::vector<UINT>& ports, std::vector<std::string>& friendlyNames)
#endif
#elif defined _AFX
BOOL CEnumerateSerial::UsingSetupAPI2(CUIntArray& ports, CStringArray& friendlyNames)
#else
BOOL CEnumerateSerial::UsingSetupAPI2(CSimpleArray<UINT>& ports, CSimpleArray<CString>& friendlyNames)
#endif
{
  //Make sure we clear out any elements which may already be in the array(s)
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
  friendlyNames.clear();
#else
  ports.RemoveAll();
  friendlyNames.RemoveAll();
#endif  

  //Get the function pointers to "SetupDiGetClassDevs", "SetupDiGetClassDevs", "SetupDiEnumDeviceInfo", "SetupDiOpenDevRegKey" 
  //and "SetupDiDestroyDeviceInfoList" in setupapi.dll
  CAutoHModule setupAPI(LoadLibraryFromSystem32(_T("SETUPAPI.DLL")));
  if (setupAPI == NULL)
    return FALSE;

  SETUPDIOPENDEVREGKEY* lpfnLPSETUPDIOPENDEVREGKEY = reinterpret_cast<SETUPDIOPENDEVREGKEY*>(GetProcAddress(setupAPI, "SetupDiOpenDevRegKey"));
#if defined _UNICODE
  SETUPDICLASSGUIDSFROMNAME* lpfnSETUPDICLASSGUIDSFROMNAME = reinterpret_cast<SETUPDICLASSGUIDSFROMNAME*>(GetProcAddress(setupAPI, "SetupDiClassGuidsFromNameW"));
  SETUPDIGETCLASSDEVS* lpfnSETUPDIGETCLASSDEVS = reinterpret_cast<SETUPDIGETCLASSDEVS*>(GetProcAddress(setupAPI, "SetupDiGetClassDevsW"));
  SETUPDIGETDEVICEREGISTRYPROPERTY* lpfnSETUPDIGETDEVICEREGISTRYPROPERTY = reinterpret_cast<SETUPDIGETDEVICEREGISTRYPROPERTY*>(GetProcAddress(setupAPI, "SetupDiGetDeviceRegistryPropertyW"));
#else
  SETUPDICLASSGUIDSFROMNAME* lpfnSETUPDICLASSGUIDSFROMNAME = reinterpret_cast<SETUPDICLASSGUIDSFROMNAME*>(GetProcAddress(setupAPI, "SetupDiClassGuidsFromNameA"));
  SETUPDIGETCLASSDEVS* lpfnSETUPDIGETCLASSDEVS = reinterpret_cast<SETUPDIGETCLASSDEVS*>(GetProcAddress(setupAPI, "SetupDiGetClassDevsA"));
  SETUPDIGETDEVICEREGISTRYPROPERTY* lpfnSETUPDIGETDEVICEREGISTRYPROPERTY = reinterpret_cast<SETUPDIGETDEVICEREGISTRYPROPERTY*>(GetProcAddress(setupAPI, "SetupDiGetDeviceRegistryPropertyA"));
#endif
  SETUPDIDESTROYDEVICEINFOLIST* lpfnSETUPDIDESTROYDEVICEINFOLIST = reinterpret_cast<SETUPDIDESTROYDEVICEINFOLIST*>(GetProcAddress(setupAPI, "SetupDiDestroyDeviceInfoList"));
  SETUPDIENUMDEVICEINFO* lpfnSETUPDIENUMDEVICEINFO = reinterpret_cast<SETUPDIENUMDEVICEINFO*>(GetProcAddress(setupAPI, "SetupDiEnumDeviceInfo"));

  if ((lpfnLPSETUPDIOPENDEVREGKEY == NULL) || (lpfnSETUPDICLASSGUIDSFROMNAME == NULL) || (lpfnSETUPDIDESTROYDEVICEINFOLIST == NULL) ||
      (lpfnSETUPDIENUMDEVICEINFO == NULL) || (lpfnSETUPDIGETCLASSDEVS == NULL) || (lpfnSETUPDIGETDEVICEREGISTRYPROPERTY == NULL))
  {
    //Set the error to report
    setupAPI.m_dwError = ERROR_CALL_NOT_IMPLEMENTED;

    return FALSE;
  }
  
  //First need to convert the name "Ports" to a GUID using SetupDiClassGuidsFromName
  DWORD dwGuids = 0;
  lpfnSETUPDICLASSGUIDSFROMNAME(_T("Ports"), NULL, 0, &dwGuids);
  if (dwGuids == 0)
  {
    //Set the error to report
    setupAPI.m_dwError = GetLastError();

    return FALSE;
  }

  //Allocate the needed memory
  CAutoHeapAlloc guids;
  if (!guids.Allocate(dwGuids * sizeof(GUID)))
  {
    //Set the error to report
    setupAPI.m_dwError = ERROR_OUTOFMEMORY;

    return FALSE;
  }

  //Call the function again
  GUID* pGuids = static_cast<GUID*>(guids.m_pData);
  if (!lpfnSETUPDICLASSGUIDSFROMNAME(_T("Ports"), pGuids, dwGuids, &dwGuids))
  {
    //Set the error to report
    setupAPI.m_dwError = GetLastError();

    return FALSE;
  }

  //Now create a "device information set" which is required to enumerate all the ports
  HDEVINFO hDevInfoSet = lpfnSETUPDIGETCLASSDEVS(pGuids, NULL, NULL, DIGCF_PRESENT);
  if (hDevInfoSet == INVALID_HANDLE_VALUE)
  {
    //Set the error to report
    setupAPI.m_dwError = GetLastError();

    return FALSE;
  }

  //Finally do the enumeration
  BOOL bMoreItems = TRUE;
  int nIndex = 0;
  SP_DEVINFO_DATA devInfo;
  while (bMoreItems)
  {
    //Enumerate the current device
    devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
    bMoreItems = lpfnSETUPDIENUMDEVICEINFO(hDevInfoSet, nIndex, &devInfo);
    if (bMoreItems)
    {
      //Did we find a serial port for this device
      BOOL bAdded = FALSE;

      //Get the registry key which stores the ports settings
      HKEY hDeviceKey = lpfnLPSETUPDIOPENDEVREGKEY(hDevInfoSet, &devInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
      if (hDeviceKey)
      {
        int nPort = 0;
        if (QueryRegistryPortName(hDeviceKey, nPort))
        {
        #if defined CENUMERATESERIAL_USE_STL
          ports.push_back(nPort);
        #else
          ports.Add(nPort);
        #endif  
          bAdded = TRUE;
        }

        //Close the key now that we are finished with it
        RegCloseKey(hDeviceKey);
      }

      //If the port was a serial port, then also try to get its friendly name
      if (bAdded)
      {
        TCHAR szFriendlyName[1024];
        szFriendlyName[0] = _T('\0');
        DWORD dwSize = sizeof(szFriendlyName);
        DWORD dwType = 0;
        if (lpfnSETUPDIGETDEVICEREGISTRYPROPERTY(hDevInfoSet, &devInfo, SPDRP_DEVICEDESC, &dwType, reinterpret_cast<PBYTE>(szFriendlyName), dwSize, &dwSize) && (dwType == REG_SZ))
        {
        #if defined CENUMERATESERIAL_USE_STL
          friendlyNames.push_back(szFriendlyName);
        #else
          friendlyNames.Add(szFriendlyName);
        #endif  
        }
        else
        {
        #if defined CENUMERATESERIAL_USE_STL
          friendlyNames.push_back(_T(""));
        #else
          friendlyNames.Add(_T(""));
        #endif  
        }
      }
    }

    ++nIndex;
  }

  //Free up the "device information set" now that we are finished with it
  lpfnSETUPDIDESTROYDEVICEINFOLIST(hDevInfoSet);

  //Return the success indicator
  return TRUE;
}
#endif

#ifndef NO_ENUMSERIAL_USING_ENUMPORTS
#if defined CENUMERATESERIAL_USE_STL
BOOL CEnumerateSerial::UsingEnumPorts(std::vector<UINT>& ports)
#elif defined _AFX
BOOL CEnumerateSerial::UsingEnumPorts(CUIntArray& ports)
#else
BOOL CEnumerateSerial::UsingEnumPorts(CSimpleArray<UINT>& ports)
#endif
{
  //Make sure we clear out any elements which may already be in the array
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
#else
  ports.RemoveAll();
#endif  

  //Call the first time to determine the size of the buffer to allocate
  DWORD cbNeeded = 0;
  DWORD dwPorts = 0;
  EnumPorts(NULL, 1, NULL, 0, &cbNeeded, &dwPorts);

  //What will be the return value
  BOOL bSuccess = FALSE;

  //Allocate the buffer and recall
  CAutoHeapAlloc portsBuffer;
  if (portsBuffer.Allocate(cbNeeded))
  {
    BYTE* pPorts = static_cast<BYTE*>(portsBuffer.m_pData);
    bSuccess = EnumPorts(NULL, 1, pPorts, cbNeeded, &cbNeeded, &dwPorts);
    if (bSuccess)
    {
      PORT_INFO_1* pPortInfo = reinterpret_cast<PORT_INFO_1*>(pPorts);
      for (DWORD i=0; i<dwPorts; i++)
      {
        //If it looks like "COMX" then
        //add it to the array which will be returned
        size_t nLen = _tcslen(pPortInfo->pName);
        if (nLen > 3)
        {
          if ((_tcsnicmp(pPortInfo->pName, _T("COM"), 3) == 0) && IsNumeric(&(pPortInfo->pName[3]), TRUE))
          {
            //Work out the port number
            int nPort = _ttoi(&(pPortInfo->pName[3]));
          #if defined CENUMERATESERIAL_USE_STL
            ports.push_back(nPort);
          #else
            ports.Add(nPort);
          #endif  
          }
        }

        pPortInfo++;
      }
    }
  }
  else
    SetLastError(ERROR_OUTOFMEMORY);        
  
  return bSuccess;
}
#endif

#ifndef NO_ENUMSERIAL_USING_WMI
#if defined CENUMERATESERIAL_USE_STL
#if defined _UNICODE
BOOL CEnumerateSerial::UsingWMI(std::vector<UINT>& ports, std::vector<std::wstring>& friendlyNames)
#else
BOOL CEnumerateSerial::UsingWMI(std::vector<UINT>& ports, std::vector<std::string>& friendlyNames)
#endif
#elif defined _AFX
BOOL CEnumerateSerial::UsingWMI(CUIntArray& ports, CStringArray& friendlyNames)
#else
BOOL CEnumerateSerial::UsingWMI(CSimpleArray<UINT>& ports, CSimpleArray<CString>& friendlyNames)
#endif
{
  //Make sure we clear out any elements which may already be in the array(s)
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
  friendlyNames.clear();
#else
  ports.RemoveAll();
  friendlyNames.RemoveAll();
#endif  

  //What will be the return value
  BOOL bSuccess = FALSE;

  //Create the WBEM locator
  ATL::CComPtr<IWbemLocator> locator;
  HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
  if (SUCCEEDED(hr))
  {
    ATL::CComPtr<IWbemServices> services;
    hr = locator->ConnectServer(_bstr_t("\\\\.\\root\\cimv2"), NULL, NULL, NULL, 0, NULL, NULL, &services);
    if (SUCCEEDED(hr))
    {
      //Execute the query
      ATL::CComPtr<IEnumWbemClassObject> classObject;
      hr = services->CreateInstanceEnum(_bstr_t("Win32_SerialPort"), WBEM_FLAG_RETURN_WBEM_COMPLETE, NULL, &classObject);
      if (SUCCEEDED(hr))
      {
        bSuccess = TRUE;

        //Now enumerate all the ports
        hr = WBEM_S_NO_ERROR;

        //Final Next will return WBEM_S_FALSE
        while (hr == WBEM_S_NO_ERROR)
        {
          ULONG uReturned = 0;
          ATL::CComPtr<IWbemClassObject> apObj[10];
          hr = classObject->Next(WBEM_INFINITE, 10, reinterpret_cast<IWbemClassObject**>(apObj), &uReturned);
          if (SUCCEEDED(hr))
          {
            for (ULONG n=0; n<uReturned; n++)
            {
              ATL::CComVariant varProperty1;
              HRESULT hrGet = apObj[n]->Get(L"DeviceID", 0, &varProperty1, NULL, NULL);
              if (SUCCEEDED(hrGet) && (varProperty1.vt == VT_BSTR) && (wcslen(varProperty1.bstrVal) > 3))
              {
                //If it looks like "COMX" then add it to the array which will be returned
                if ((_wcsnicmp(varProperty1.bstrVal, L"COM", 3) == 0) && IsNumeric(&(varProperty1.bstrVal[3]), TRUE))
                {
                  //Work out the port number
                  int nPort = _wtoi(&(varProperty1.bstrVal[3]));
                #if defined CENUMERATESERIAL_USE_STL
                  ports.push_back(nPort);
                #else
                  ports.Add(nPort);
                #endif

                  //Also get the friendly name of the port
                  ATL::CComVariant varProperty2;
                  if (SUCCEEDED(apObj[n]->Get(L"Name", 0, &varProperty2, NULL, NULL)) && (varProperty2.vt == VT_BSTR))
                  {  
                #if defined CENUMERATESERIAL_USE_STL
                  #if defined _UNICODE  
                    std::wstring szName(varProperty2.bstrVal);
                  #else
                    std::string szName(ATL::CW2A(varProperty2.bstrVal));
                  #endif
                    friendlyNames.push_back(szName);
                  #else
                    friendlyNames.Add(CString(varProperty2.bstrVal));    
                  #endif
                  }
                  else
                  {
                  #if defined CENUMERATESERIAL_USE_STL
                    friendlyNames.push_back(_T(""));
                  #else
                    friendlyNames.Add(_T(""));
                  #endif  
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  
  return bSuccess;
}
#endif

#ifndef NO_ENUMSERIAL_USING_COMDB
#if defined CENUMERATESERIAL_USE_STL
BOOL CEnumerateSerial::UsingComDB(std::vector<UINT>& ports)
#elif defined _AFX
BOOL CEnumerateSerial::UsingComDB(CUIntArray& ports)
#else
BOOL CEnumerateSerial::UsingComDB(CSimpleArray<UINT>& ports)
#endif
{
  //Make sure we clear out any elements which may already be in the array(s)
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
#else
  ports.RemoveAll();
#endif  

  //What will be the return value from this function (assume the worst)
  BOOL bSuccess = FALSE;
  
  //Get the function pointers to "ComDBOpen", "ComDBClose" & "ComDBGetCurrentPortUsage" in msports.dll
  CAutoHModule msPorts(LoadLibraryFromSystem32(_T("MSPORTS.DLL")));
  if (msPorts == NULL)
    return FALSE;

  COMDBOPEN* lpfnLPCOMDBOPEN = reinterpret_cast<COMDBOPEN*>(GetProcAddress(msPorts, "ComDBOpen"));
  COMDBCLOSE* lpfnLPCOMDBCLOSE = reinterpret_cast<COMDBCLOSE*>(GetProcAddress(msPorts, "ComDBClose"));
  COMDBGETCURRENTPORTUSAGE* lpfnCOMDBGETCURRENTPORTUSAGE = reinterpret_cast<COMDBGETCURRENTPORTUSAGE*>(GetProcAddress(msPorts, "ComDBGetCurrentPortUsage"));
  if ((lpfnLPCOMDBOPEN != NULL) && (lpfnLPCOMDBCLOSE != NULL) && (lpfnCOMDBGETCURRENTPORTUSAGE != NULL))
  {
    //First need to open up the DB
    HCOMDB hComDB;
    DWORD dwComOpen = lpfnLPCOMDBOPEN(&hComDB);
    if (dwComOpen == ERROR_SUCCESS)
    {
      //Work out the size of the buffer required
      DWORD dwMaxPortsReported = 0;
      DWORD dwPortUsage = lpfnCOMDBGETCURRENTPORTUSAGE(hComDB, NULL, 0, CDB_REPORT_BYTES, &dwMaxPortsReported);
      if (dwPortUsage == ERROR_SUCCESS)
      {
        //Allocate some heap space and recall the function
        CAutoHeapAlloc portBytes;
        if (portBytes.Allocate(dwMaxPortsReported))
        {
          bSuccess = TRUE;

          PBYTE pPortBytes = static_cast<PBYTE>(portBytes.m_pData);
          if (lpfnCOMDBGETCURRENTPORTUSAGE(hComDB, pPortBytes, dwMaxPortsReported, CDB_REPORT_BYTES, &dwMaxPortsReported) == ERROR_SUCCESS)
          {
            //Work thro the byte bit array for ports which are in use
            for (DWORD i=0; i<dwMaxPortsReported; i++)
            {
              if (pPortBytes[i])
              {
              #if defined CENUMERATESERIAL_USE_STL
                ports.push_back(i + 1);
              #else
                ports.Add(i + 1);
              #endif
              }
            }
          }
        }
        else
          msPorts.m_dwError = ERROR_OUTOFMEMORY;
      }
      else
        msPorts.m_dwError = dwPortUsage;
    
      //Close the DB
      lpfnLPCOMDBCLOSE(hComDB);
    }
    else
      msPorts.m_dwError = dwComOpen;
  }
  else
    msPorts.m_dwError = ERROR_CALL_NOT_IMPLEMENTED;

  return bSuccess;
}
#endif

#ifndef NO_ENUMSERIAL_USING_REGISTRY
#if defined CENUMERATESERIAL_USE_STL
#if defined _UNICODE
BOOL CEnumerateSerial::UsingRegistry(std::vector<std::wstring>& ports)
#else
BOOL CEnumerateSerial::UsingRegistry(std::vector<std::string>& ports)
#endif
#elif defined _AFX
BOOL CEnumerateSerial::UsingRegistry(CStringArray& ports)
#else
BOOL CEnumerateSerial::UsingRegistry(CSimpleArray<CString>& ports)
#endif
{
  //Make sure we clear out any elements which may already be in the array(s)
#if defined CENUMERATESERIAL_USE_STL
  ports.clear();
#else
  ports.RemoveAll();
#endif  

  //What will be the return value from this function (assume the worst)
  BOOL bSuccess = FALSE;

  HKEY hSERIALCOMM;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0, KEY_QUERY_VALUE, &hSERIALCOMM) == ERROR_SUCCESS)
  {
    //Get the max value name and max value lengths
    DWORD dwMaxValueNameLen;
    DWORD dwMaxValueLen;
    DWORD dwQueryInfo = RegQueryInfoKey(hSERIALCOMM, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &dwMaxValueNameLen, &dwMaxValueLen, NULL, NULL);
    if (dwQueryInfo == ERROR_SUCCESS)
    {
      DWORD dwMaxValueNameSizeInChars = dwMaxValueNameLen + 1; //Include space for the NULL terminator
      DWORD dwMaxValueNameSizeInBytes = dwMaxValueNameSizeInChars * sizeof(TCHAR);
      DWORD dwMaxValueDataSizeInChars = dwMaxValueLen/sizeof(TCHAR) + 1; //Include space for the NULL terminator
      DWORD dwMaxValueDataSizeInBytes = dwMaxValueDataSizeInChars * sizeof(TCHAR);
    
      //Allocate some space for the value name and value data			
      CAutoHeapAlloc valueName;
      CAutoHeapAlloc valueData;
      if (valueName.Allocate(dwMaxValueNameSizeInBytes) && valueData.Allocate(dwMaxValueDataSizeInBytes))
      {
        bSuccess = TRUE;

        //Enumerate all the values underneath HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
        DWORD dwIndex = 0;
        DWORD dwType;
        DWORD dwValueNameSize = dwMaxValueNameSizeInChars;
        DWORD dwDataSize = dwMaxValueDataSizeInBytes;
        memset(valueName.m_pData, 0, dwMaxValueNameSizeInBytes);
        memset(valueData.m_pData, 0, dwMaxValueDataSizeInBytes);
        TCHAR* szValueName = static_cast<TCHAR*>(valueName.m_pData);
        BYTE* byValue = static_cast<BYTE*>(valueData.m_pData);
        LONG nEnum = RegEnumValue(hSERIALCOMM, dwIndex, szValueName, &dwValueNameSize, NULL, &dwType, byValue, &dwDataSize);
        while (nEnum == ERROR_SUCCESS)
        {
          //If the value is of the correct type, then add it to the array
          if (dwType == REG_SZ)
          {
            TCHAR* szPort = reinterpret_cast<TCHAR*>(byValue);
          #if defined CENUMERATESERIAL_USE_STL
            ports.push_back(szPort);
          #else
            ports.Add(szPort);
          #endif  						
          }

          //Prepare for the next time around
          dwValueNameSize = dwMaxValueNameSizeInChars;
          dwDataSize = dwMaxValueDataSizeInBytes;
          memset(valueName.m_pData, 0, dwMaxValueNameSizeInBytes);
          memset(valueData.m_pData, 0, dwMaxValueDataSizeInBytes);
          ++dwIndex;
          nEnum = RegEnumValue(hSERIALCOMM, dwIndex, szValueName, &dwValueNameSize, NULL, &dwType, byValue, &dwDataSize);
        }
      }
      else
        SetLastError(ERROR_OUTOFMEMORY);
    }
    
    //Close the registry key now that we are finished with it    
    RegCloseKey(hSERIALCOMM);
    
    if (dwQueryInfo != ERROR_SUCCESS)
      SetLastError(dwQueryInfo);
  }
  
  return bSuccess;
}
#endif
