/***********************************************************

win32apimodule.cpp -- module for interface into Win32' API


Note that this source file contains embedded documentation.
This documentation consists of marked up text inside the
C comments, and is prefixed with an '@' symbol.  The source
files are processed by a tool called "autoduck" which
generates Windows .hlp files.
@doc

******************************************************************/

#include "windows.h"

#define Py_USE_NEW_NAMES


#include "Python.h"
#include "PyWinTypes.h"
#include "PyWinObjects.h"

#include "malloc.h"

#include "math.h" // for some of the date stuff...

#define DllExport   _declspec(dllexport)

// Identical to PyW32_BEGIN_ALLOW_THREADS except no script "{" !!!
// means variables can be declared between the blocks
#define PyW32_BEGIN_ALLOW_THREADS PyThreadState *_save = PyEval_SaveThread();
#define PyW32_END_ALLOW_THREADS PyEval_RestoreThread(_save);
#define PyW32_BLOCK_THREADS Py_BLOCK_THREADS

/* error helper */
PyObject *ReturnError(char *msg, char *fnName = NULL)
{
	PyObject *v = Py_BuildValue("(izs)", 0, fnName, msg);
	if (v != NULL) {
		PyErr_SetObject(PyWinExc_ApiError, v);
		Py_DECREF(v);
	}
	return NULL;
}
/* error helper - GetLastError() is provided, but this is for exceptions */
PyObject *ReturnAPIError(char *fnName, long err = 0)
{
	return PyWin_SetAPIError(fnName, err);
}
// @pymethod |win32api|Beep|Generates simple tones on the speaker.
static PyObject *
PyBeep( PyObject *self, PyObject *args )
{
	DWORD freq;
	DWORD dur;

	if (!PyArg_ParseTuple(args, "ii:Beep", 
	          &freq,  // @pyparm int|freq||Specifies the frequency, in hertz, of the sound. This parameter must be in the range 37 through 32,767 (0x25 through 0x7FFF).
	          &dur)) // @pyparm int|dur||Specifies the duration, in milliseconds, of the sound.~
	                // One value has a special meaning: If dwDuration is  - 1, the function 
	                // operates asynchronously and produces sound until called again. 
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::Beep(freq,dur);
	PyW32_END_ALLOW_THREADS
	if (!ok) // @pyseeapi Beep
		return ReturnAPIError("Beep");		
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|GetStdHandle|Returns a handle for the standard input, standard output, or standard error device
static PyObject* PyGetStdHandle (PyObject *self, PyObject *args)
{
  DWORD nStdHandle;

  if (!PyArg_ParseTuple(args, "i:GetStdHandle",
			&nStdHandle)) // @pyparm int|handle||input, output, or error device
    return NULL;
  return Py_BuildValue("i", ::GetStdHandle (nStdHandle));
}

// @pymethod |win32api|SetStdHandle|Set the handle for the standard input, standard output, or standard error device
static PyObject* PySetStdHandle (PyObject *self, PyObject *args)
{
  DWORD nStdHandle;
  HANDLE hHandle;
  PyObject *obHandle;

  if (!PyArg_ParseTuple(args, "iO:SetStdHandle",
			&nStdHandle, // @pyparm int|handle||input, output, or error device
			&obHandle)) // @pyparm <o PyHANDLE>/int|handle||A previously opened handle to be a standard handle
    return NULL;

  if (!PyWinObject_AsHANDLE(obHandle, &hHandle))
    return NULL;

  if (!::SetStdHandle(nStdHandle, hHandle))
    return ReturnAPIError ("SetStdHandle");

  Py_INCREF(Py_None);
  return Py_None;
}

// @pymethod |win32api|CloseHandle|Closes an open handle.
static PyObject *PyCloseHandle(PyObject *self, PyObject *args)
{
	PyObject *obHandle;
	if (!PyArg_ParseTuple(args, "O:CloseHandle",
			&obHandle)) // @pyparm <o PyHANDLE>/int|handle||A previously opened handle.
		return NULL;
	if (!PyWinObject_CloseHANDLE(obHandle))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod <o PyHANDLE>|win32api|DuplicateHandle|Duplicates a handle.
static PyObject *PyDuplicateHandle(PyObject *self, PyObject *args)
{
	HANDLE hSourceProcess, hSource, hTarget, hResult;
	PyObject *obSourceProcess, *obSource, *obTarget;
	BOOL bInherit;
	DWORD options, access;
	if (!PyArg_ParseTuple(args, "OOOiii:DuplicateHandle",
			&obSourceProcess, // @pyparm <o PyHANDLE>|hSourceProcess||Identifies the process containing the handle to duplicate.
			&obSource, // @pyparm <o PyHANDLE>|hSource||Identifies the handle to duplicate. This is an open object handle that is valid in the context of the source process.
			&obTarget, // @pyparm <o PyHANDLE>|hTargetProcessHandle||Identifies the process that is to receive the duplicated handle. The handle must have PROCESS_DUP_HANDLE access. 
			&access, // @pyparm int|desiredAccess||Specifies the access requested for the new handle. This parameter is ignored if the dwOptions parameter specifies the DUPLICATE_SAME_ACCESS flag. Otherwise, the flags that can be specified depend on the type of object whose handle is being duplicated. For the flags that can be specified for each object type, see the following Remarks section. Note that the new handle can have more access than the original handle. 
			&bInherit, // @pyparm int|bInheritHandle||Indicates whether the handle is inheritable. If TRUE, the duplicate handle can be inherited by new processes created by the target process. If FALSE, the new handle cannot be inherited. 
			&options)) // @pyparm int|options||Specifies optional actions. This parameter can be zero, or any combination of the following flags
			// @flag DUPLICATE_CLOSE_SOURCE|loses the source handle. This occurs regardless of any error status returned.
			// @flag DUPLICATE_SAME_ACCESS|Ignores the dwDesiredAccess parameter. The duplicate handle has the same access as the source handle.
		return NULL;
	if (!PyWinObject_AsHANDLE(obSourceProcess, &hSourceProcess))
		return NULL;
	if (!PyWinObject_AsHANDLE(obSource, &hSource))
		return NULL;
	if (!PyWinObject_AsHANDLE(obTarget, &hTarget))
		return NULL;
	if (!DuplicateHandle(hSourceProcess, hSource, hTarget, &hResult, access, bInherit, options))
		return ReturnAPIError("DuplicateHandle");
	return PyWinObject_FromHANDLE(hResult);
}

// @pymethod |win32api|CopyFile|Copies an existing file to a new file
static PyObject *
PyCopyFile( PyObject *self, PyObject *args )
{
	BOOL failOnExist = FALSE;
	PyObject *obSrc, *obDest;
	if (!PyArg_ParseTuple(args, "OO|i:CopyFile", 
	          &obSrc,  // @pyparm string<o PyUnicode>|src||Name of an existing file.
	          &obDest, // @pyparm string/<o PyUnicode>|dest||Name of file to copy to.
	          &failOnExist)) // @pyparm int|bFailOnExist|0|Indicates if the operation should fail if the file exists.
		return NULL;
	char *src, *dest;
	if (!PyWinObject_AsTCHAR(obSrc, &src, FALSE))
		return NULL;
	if (!PyWinObject_AsTCHAR(obDest, &dest, FALSE)) {
		PyWinObject_FreeTCHAR(src);
		return NULL;
	}
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::CopyFile(src, dest, failOnExist);
	PyW32_END_ALLOW_THREADS
	PyWinObject_FreeTCHAR(src);
	PyWinObject_FreeTCHAR(dest);
	if (!ok) // @pyseeapi CopyFile
		return ReturnAPIError("CopyFile");		
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|DebugBreak|Breaks into the C debugger
static PyObject *
PyDebugBreak(PyObject * self, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ":DebugBreak"))
		return NULL;
	// @pyseeapi DebugBreak
	PyW32_BEGIN_ALLOW_THREADS
	DebugBreak();
	PyW32_END_ALLOW_THREADS
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|DeleteFile|Deletes the specified file.
static PyObject *
PyDeleteFile(PyObject * self, PyObject * args)
{
	PyObject *obPath;
	// @pyparm string/<o PyUnicode>|fileName||File to delete.
	if (!PyArg_ParseTuple(args, "O:DeleteFile", &obPath))
		return NULL;
	TCHAR *szPath;
	if (!PyWinObject_AsTCHAR(obPath, &szPath, FALSE))
		return NULL;
	// @pyseeapi DeleteFile
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = DeleteFile(szPath);
	PyW32_END_ALLOW_THREADS
		PyWinObject_FreeTCHAR(szPath);
	if (!ok)
		return ReturnAPIError("DeleteFile");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod string/int|win32api|DragQueryFile|Retrieves the file names of dropped files.
static PyObject *
PyDragQueryFile( PyObject *self, PyObject *args )
{
	char buf[MAX_PATH];
	HDROP hDrop;
	int iFileNum = 0xFFFFFFFF;
	if (!PyArg_ParseTuple(args, "i|i:DragQueryFile", 
	           &hDrop, // @pyparm int|hDrop||Handle identifying the structure containing the file names.
	           &iFileNum)) // @pyparm int|fileNum|0xFFFFFFFF|Specifies the index of the file to query.
		return NULL;
	if (iFileNum<0)
		return Py_BuildValue("i", ::DragQueryFile( hDrop, (UINT)-1, NULL, 0));
	else { // @pyseeapi DragQueryFile
		PyW32_BEGIN_ALLOW_THREADS
		int ret = ::DragQueryFile( hDrop, iFileNum, buf, sizeof(buf));
		PyW32_END_ALLOW_THREADS
		if (ret <=0)
			return ReturnAPIError("DragQueryFile");
		else
			return Py_BuildValue("s", buf);
	}
// @rdesc If the fileNum parameter is 0xFFFFFFFF (the default) then the return value
// is an integer with the count of files dropped.  If fileNum is between 0 and Count, 
// the return value is a string containing the filename.<nl>
// If an error occurs, and exception is raised.
}
// @pymethod |win32api|DragFinish|Releases the memory stored by Windows for the filenames.
static PyObject *
PyDragFinish( PyObject *self, PyObject *args )
{
	HDROP hDrop;
	// @pyparm int|hDrop||Handle identifying the structure containing the file names.
	if (!PyArg_ParseTuple(args, "i:DragFinish", &hDrop))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	::DragFinish( hDrop); // @pyseeapi DragFinish
	PyW32_END_ALLOW_THREADS
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod string|win32api|GetEnvironmentVariable|Retrieves the value of an environment variable.
static PyObject *
PyGetEnvironmentVariable( PyObject *self, PyObject *args )
{
	char *szVar;
	if (!PyArg_ParseTuple(args, "s:GetEnvironmentVariable", 
	           &szVar)) // @pyparm string|variable||The variable to get
		return NULL;
	// @pyseeapi GetEnvironmentVariable
	PyW32_BEGIN_ALLOW_THREADS
	DWORD size = GetEnvironmentVariable(szVar, NULL, 0);
	char *pResult = NULL;
	if (size) {
		pResult = (char *)malloc(sizeof(char) * size);
		GetEnvironmentVariable(szVar, pResult, size);
	}
	PyW32_END_ALLOW_THREADS
	PyObject *ret;
	if (pResult==NULL) {
		Py_INCREF(Py_None);
		ret = Py_None;
	} else
		ret = PyString_FromString(pResult);
	if (pResult)
		free(pResult);
	return ret;
}

// @pymethod string|win32api|ExpandEnvironmentStrings|Expands environment-variable strings and replaces them with their defined values. 
static PyObject *
PyExpandEnvironmentStrings( PyObject *self, PyObject *args )
{
	char *in;
	if (!PyArg_ParseTuple(args, "s:ExpandEnvironmentStrings", 
	           &in)) // @pyparm string|in||String to expand
		return NULL;
	// @pyseeapi ExpandEnvironmentStrings
	DWORD size;
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	// @comm On Windows 95, the string is limited to 1024 bytes.
	// On other platforms, there is no (practical) limit
	if (osvi.dwPlatformId==VER_PLATFORM_WIN32_WINDOWS && osvi.dwMinorVersion==0)
		 /* Default to 1024 for Windows 95, as the size request fails! */
		size = 1024;
	else
		size = ExpandEnvironmentStrings(in, NULL, 0);
	char *result = (char *)malloc(size);
	PyObject *rc;
	PyW32_BEGIN_ALLOW_THREADS
	long lrc = ExpandEnvironmentStrings(in, result, size);
	PyW32_END_ALLOW_THREADS
	if (lrc==0) {
		rc = ReturnAPIError("ExpandEnvironmentStrings");
	} else {
		rc = PyString_FromString(result);
	}
	free(result);
	return rc;
}

// @pymethod (int, string)|win32api|FindExecutable|Retrieves the name and handle of the executable (.EXE) file associated with the specified filename.
static PyObject *
PyFindExecutable( PyObject *self, PyObject *args )
{
	char *file, *dir="";
	char res[MAX_PATH];

	if (!PyArg_ParseTuple(args, "s|s:FindExecutable", 
	           &file, // @pyparm string|filename||A file name.  This can be either a document or executable file.
	           &dir)) // @pyparm string|dir||The default directory.
		return NULL;
	HINSTANCE rc;
	// @pyseeapi FixedExecutable
	PyW32_BEGIN_ALLOW_THREADS
	rc=::FindExecutable(file, dir, res);
	PyW32_END_ALLOW_THREADS
	if (rc<=(HINSTANCE)32) {
		if ((int)rc==31) 
			return ReturnError("There is no association for the file");
		return ReturnAPIError("FindExecutable", (int)rc );
	}
	return Py_BuildValue("(is)", rc, res );
	// @rdesc The return value is a tuple of (integer, string)<nl>
	// The integer is the instance handle of the executable file associated
	// with the specified filename. (This handle could also be the handle of
	// a dynamic data exchange [DDE] server application.)<nl>
	// The may contain the path to the DDE server started if no server responds to a request to initiate a DDE conversation.
	// @comm The function will raise an exception if it fails.
}

// @pymethod list|win32api|FindFiles|Retrieves a list of matching filenames.  An interface to the API FindFirstFile/FindNextFile/Find close functions.
static PyObject *
PyFindFiles(PyObject *self, PyObject *args)
{
	char *fileSpec;
	// @pyparm string|fileSpec||A string that specifies a valid directory or path and filename, which can contain wildcard characters (* and ?).

	// !!! NOT!!! comm This function detects a '\\?' in the first 3 characters, and automatically calls
	// FindFirstFileW.  This allows fileSpec to be greater than 256 characters, and
	// also the specifications "\\\\?\\C:\\python\\lib" is seen as "C:\\python\\lib", and "\\\\?\\UNC\\\\skippy\\hotstuff\\grail" is seen as "\\\\skippy\\hotstuff\\grail".
	if (!PyArg_ParseTuple (args, "s:FindFiles", &fileSpec))
		return NULL;
	WIN32_FIND_DATA findData;
	// @pyseeapi FindFirstFile
	HANDLE hFind;
	/* 	if (strcmp(fileSpec, "\\\\?")==0) */
	hFind =  ::FindFirstFile(fileSpec, &findData);
	/* else {
		int len=strlen(fileSpec);
		WCHAR *pBuf = new WCHAR[len+1];
		if (0==MultiByteToWideChar( CP_ACP, 0, fileSpec, len, pBuf, sizeof(WCHAR)*(len+1)))
			return ReturnAPIError("MultiByteToWideChar")
		hFind =  ::FindFirstFileW(pBuf, &findData);
		delete [] pBuf;
	} */
	if (hFind==INVALID_HANDLE_VALUE) {
		if (::GetLastError()==ERROR_FILE_NOT_FOUND) {	// this is OK
			return PyList_New(0);
		}
		return ReturnAPIError("FindFirstFile");
	}
	PyObject *retList = PyList_New(0);
	if (!retList) {
		::FindClose(hFind);
		return NULL;
	}
	BOOL ok = TRUE;
	while (ok) {
		PyObject *obCreateTime = PyWinObject_FromFILETIME(findData.ftCreationTime);
		PyObject *obAccessTime = PyWinObject_FromFILETIME(findData.ftLastAccessTime);
		PyObject *obWriteTime = PyWinObject_FromFILETIME(findData.ftLastWriteTime);
		if (obCreateTime==NULL || obAccessTime==NULL || obWriteTime==NULL) {
			Py_XDECREF(obCreateTime);
			Py_XDECREF(obAccessTime);
			Py_XDECREF(obWriteTime);
			Py_DECREF(retList);
			::FindClose(hFind);
			return NULL;
		}
		PyObject *newItem = Py_BuildValue("lOOOllllss",
		// @rdesc The return value is a list of tuples, in the same format as the WIN32_FIND_DATA structure:
			findData.dwFileAttributes, // @tupleitem 0|int|attributes|File Attributes.  A combination of the win32com.FILE_ATTRIBUTE_* flags.
			obCreateTime, // @tupleitem 1|<o PyTime>|createTime|File creation time.
    		obAccessTime, // @tupleitem 2|<o PyTime>|accessTime|File access time.
    		obWriteTime, // @tupleitem 3|<o PyTime>|writeTime|Time of last file write
    		findData.nFileSizeHigh, // @tupleitem 4|int|nFileSizeHigh|high order word of file size.
    		findData.nFileSizeLow,	// @tupleitem 5|int|nFileSizeLow|low order word of file size.
    		findData.dwReserved0,	// @tupleitem 6|int|reserved0|Reserved.
    		findData.dwReserved1,   // @tupleitem 7|int|reserved1|Reserved.
    		findData.cFileName,		// @tupleitem 8|string|fileName|The name of the file.
    		findData.cAlternateFileName );
		if (newItem!=NULL) {
			PyList_Append(retList, newItem); // @tupleitem 9|string|alternateFilename|Alternative name of the file, expressed in 8.3 format.
			Py_DECREF(newItem);
		}
		// @pyseeapi FindNextFile
		Py_DECREF(obCreateTime);
		Py_DECREF(obAccessTime);
		Py_DECREF(obWriteTime);
		ok=::FindNextFile(hFind, &findData);
	}
	ok = (GetLastError()==ERROR_NO_MORE_FILES);
	// @pyseeapi CloseHandle
	::FindClose(hFind);
	if (!ok) {
		Py_DECREF(retList);
		return ReturnAPIError("FindNextFile");
	}
	return retList;
}

// @pymethod int|win32api|FindFirstChangeNotification|Creates a change notification handle and sets up initial change notification filter conditions.
// @rdesc Although the result is a handle, the handle can not be closed via CloseHandle() - therefore a PyHandle object is not used.
static PyObject *
PyFindFirstChangeNotification(PyObject *self, PyObject *args)
{
	DWORD dwFilter;
	BOOL subDirs;
	PyObject *obPathName;
	// @pyparm string|pathName||Specifies the path of the directory to watch. 
	// @pyparm int|bSubDirs||Specifies whether the function will monitor the directory or the directory tree. If this parameter is TRUE, the function monitors the directory tree rooted at the specified directory; if it is FALSE, it monitors only the specified directory
	// @pyparm int|filter||Specifies the filter conditions that satisfy a change notification wait. This parameter can be one or more of the following values:
	// @flagh Value|Meaning
	// @flag FILE_NOTIFY_CHANGE_FILE_NAME|Any file name change in the watched directory or subtree causes a change notification wait operation to return. Changes include renaming, creating, or deleting a file name. 
	// @flag FILE_NOTIFY_CHANGE_DIR_NAME|Any directory-name change in the watched directory or subtree causes a change notification wait operation to return. Changes include creating or deleting a directory. 
	// @flag FILE_NOTIFY_CHANGE_ATTRIBUTES|Any attribute change in the watched directory or subtree causes a change notification wait operation to return. 
	// @flag FILE_NOTIFY_CHANGE_SIZE|Any file-size change in the watched directory or subtree causes a change notification wait operation to return. The operating system detects a change in file size only when the file is written to the disk. For operating systems that use extensive caching, detection occurs only when the cache is sufficiently flushed. 
	// @flag FILE_NOTIFY_CHANGE_LAST_WRITE|Any change to the last write-time of files in the watched directory or subtree causes a change notification wait operation to return. The operating system detects a change to the last write-time only when the file is written to the disk. For operating systems that use extensive caching, detection occurs only when the cache is sufficiently flushed. 
	// @flag FILE_NOTIFY_CHANGE_SECURITY|Any security-descriptor change in the watched directory or subtree causes a change notification wait operation to return 
	if (!PyArg_ParseTuple(args, "Oil", &obPathName, &subDirs, &dwFilter))
		return NULL;
	TCHAR *pathName;
	if (!PyWinObject_AsTCHAR(obPathName, &pathName, FALSE))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	HANDLE h = FindFirstChangeNotification(pathName, subDirs, dwFilter);
	PyW32_END_ALLOW_THREADS
	PyWinObject_FreeTCHAR(pathName);
	if (h==NULL || h==INVALID_HANDLE_VALUE)
		return ReturnAPIError("FindFirstChangeNotification");
	return PyInt_FromLong((long)h);
}

// @pymethod |win32api|FindNextChangeNotification|Requests that the operating system signal a change notification handle the next time it detects an appropriate change.
static PyObject *
PyFindNextChangeNotification(PyObject *self, PyObject *args)
{
	HANDLE h;
	// @pyparm int|handle||The handle returned from <om win32api.FindFirstChangeNotification>
	if (!PyArg_ParseTuple(args, "l", &h))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = FindNextChangeNotification(h);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("FindNextChangeNotification");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|FindCloseChangeNotification|Closes the change notification handle.
static PyObject *
PyFindCloseChangeNotification(PyObject *self, PyObject *args)
{
	HANDLE h;
	// @pyparm int|handle||The handle returned from <om win32api.FindFirstChangeNotification>
	if (!PyArg_ParseTuple(args, "l", &h))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = FindCloseChangeNotification(h);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("FindCloseChangeNotification");
	Py_INCREF(Py_None);
	return Py_None;
}


// @pymethod string|win32api|FormatMessage|Returns an error message from the system error file.
static PyObject *
PyFormatMessage (PyObject *self, PyObject *args)
{
	int errCode=0;
	// @pyparm int|errCode|0|The error code to return the message for,  If this value is 0, then GetLastError() is called to determine the error code.
	if (PyArg_ParseTuple (args, "|i:FormatMessage", &errCode)) {
		if (errCode==0)
			// @pyseeapi GetLastError
			errCode = GetLastError();
		const int bufSize = 512;
		char buf[bufSize];
		// @pyseeapi FormatMessage
		if (::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errCode, 0, buf, bufSize, NULL )<=0)
			return ReturnAPIError("FormatMessage");
		return Py_BuildValue("s", buf);
	}
	PyErr_Clear();
	// Support for "full" argument list
	//
	// @pyparmalt1 int|flags||Flags for the call.  Note that FORMAT_MESSAGE_ALLOCATE_BUFFER and FORMAT_MESSAGE_ARGUMENT_ARRAY will always be added.
	// @pyparmalt1 int/string|source||The source object.  If flags contain FORMAT_MESSAGE_FROM_HMODULE it should be an int, if flags contain FORMAT_MESSAGE_FROM_STRING, otherwise it is ignored.
	// @pyparmalt1 int|messageId||The message ID.
	// @pyparmalt1 int|languageID||The language ID.
	// @pyparmalt1 [string,...]/None|inserts||The string inserts to insert.
	// @pyparmalt1 int|bufSize|1024|
	DWORD  flags, msgId, langId;
	PyObject *obSource;
	PyObject *obInserts;
	HANDLE hSource;
	char *szSource;
	char **pInserts;
	void *pSource;
	if (!PyArg_ParseTuple (args, "iOiiO:FormatMessage", &flags, &obSource, &msgId, &langId, &obInserts))
		return NULL;
	if (flags & FORMAT_MESSAGE_FROM_HMODULE) {
		if (!PyInt_Check(obSource)) {
			PyErr_SetString(PyExc_TypeError, "Flags has FORMAT_MESSAGE_FROM_HMODULE, but object not an integer");
			return NULL;
		}
		hSource = (HANDLE)PyInt_AsLong(obSource);
		pSource = (void *)hSource;
	}
	else if (flags & FORMAT_MESSAGE_FROM_STRING) {
		if (!PyString_Check(obSource)) {
			PyErr_SetString(PyExc_TypeError, "Flags has FORMAT_MESSAGE_FROM_STRING, but object not a string");
			return NULL;
		}
		szSource = PyString_AsString(obSource);
		pSource = (void *)szSource;
	} else
		pSource = NULL;
	if (obInserts==NULL || obInserts==Py_None) {
		pInserts = NULL;
	} else if (PySequence_Check(obInserts)) {
		int len = PySequence_Length(obInserts);
		pInserts = (char **)malloc(sizeof(char *) * (len+1));
		if (pInserts==NULL) {
			PyErr_SetString(PyExc_MemoryError, "Allocating buffer for inserts");
			return NULL;
		}
		for (int i=0;i<len;i++) {
			PyObject *subObject = PySequence_GetItem(obInserts, i);
			if (subObject==NULL) {
				free(pInserts);
				return NULL;
			}
			if (!PyString_Check(subObject)) {
				free(pInserts);
				PyErr_SetString(PyExc_TypeError, "Inserts must be sequence of strings");
				return NULL;
			}
			if ((pInserts[i] = PyString_AsString(subObject))==NULL) {
				free(pInserts);
				return NULL;
			}
			Py_DECREF(subObject);
		}
		pInserts[i] = NULL;
	} else {
			PyErr_SetString(PyExc_TypeError, "Inserts must be sequence or None");
			return NULL;
	}
	char *buf;
	flags |= (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY);
	PyW32_BEGIN_ALLOW_THREADS
	long lrc = ::FormatMessage(flags, pSource, msgId, langId, (LPTSTR)&buf, 0, pInserts );
	PyW32_END_ALLOW_THREADS
	if (lrc<=0)
			return ReturnAPIError("FormatMessage");
	PyObject *rc = PyString_FromString(buf);
	LocalFree(buf);
	if (pInserts)
		free(pInserts);
	return rc;
}

// @pymethod <o PyUnicode>|win32api|FormatMessageW|Returns an error message from the system error file.
static PyObject *
PyFormatMessageW(PyObject *self, PyObject *args)
{
	// Only support for "full" argument list
	//
	// @pyparmalt1 int|flags||Flags for the call.  Note that FORMAT_MESSAGE_ALLOCATE_BUFFER and FORMAT_MESSAGE_ARGUMENT_ARRAY will always be added.
	// @pyparmalt1 int/<o PyUnicode>|source||The source object.  If flags contain FORMAT_MESSAGE_FROM_HMODULE it should be an int, if flags contain FORMAT_MESSAGE_FROM_STRING, otherwise it is ignored.
	// @pyparmalt1 int|messageId||The message ID.
	// @pyparmalt1 int|languageID||The language ID.
	// @pyparmalt1 [<o PyUnicode>,...]/None|inserts||The string inserts to insert.
	// @pyparmalt1 int|bufSize|1024|
	DWORD  flags, msgId, langId;
	PyObject *obSource;
	PyObject *obInserts;
	HANDLE hSource;
	WCHAR *szSource = NULL;
	WCHAR **pInserts = NULL;
	int numInserts = 0;
	void *pSource;
	PyObject *rc = NULL;
	WCHAR *resultBuf;
	int i;
	long lrc;
	if (!PyArg_ParseTuple (args, "iOiiO:FormatMessageW", &flags, &obSource, &msgId, &langId, &obInserts))
		goto cleanup;
	if (flags & FORMAT_MESSAGE_FROM_HMODULE) {
		if (!PyInt_Check(obSource)) {
			PyErr_SetString(PyExc_TypeError, "Flags has FORMAT_MESSAGE_FROM_HMODULE, but object not an integer");
			goto cleanup;
		}
		hSource = (HANDLE)PyInt_AsLong(obSource);
		pSource = (void *)hSource;
	}
	else if (flags & FORMAT_MESSAGE_FROM_STRING) {
		if (!PyUnicode_Check(obSource)) {
			PyErr_SetString(PyExc_TypeError, "Flags has FORMAT_MESSAGE_FROM_STRING, but object not a Unicode object");
			goto cleanup;
		}
		if (!PyWinObject_AsBstr(obSource, &szSource))
			goto cleanup;
		pSource = (void *)szSource;
	} else
		pSource = NULL;
	if (obInserts==NULL || obInserts==Py_None) {
		; // do nothing - already NULL
	} else if (PySequence_Check(obInserts)) {
		numInserts = PySequence_Length(obInserts);
		pInserts = (WCHAR **)malloc(sizeof(WCHAR *) * (numInserts+1));
		if (pInserts==NULL) {
			PyErr_SetString(PyExc_MemoryError, "Allocating buffer for inserts");
			goto cleanup;
		}
		for (i=0;i<numInserts;i++)	// Make sure clean for cleanup
			pInserts[i] = NULL;
		for (i=0;i<numInserts;i++) {
			PyObject *subObject = PySequence_GetItem(obInserts, i);
			if (subObject==NULL) {
				goto cleanup;
			}
			if (!PyUnicode_Check(subObject)) {
				PyErr_SetString(PyExc_TypeError, "Inserts must be sequence of UnicodeObjects");
				goto cleanup;
			}
			if (!PyWinObject_AsBstr(subObject, pInserts+i)) {
				goto cleanup;
			}
			Py_DECREF(subObject);
		}
		pInserts[i] = NULL;	// One beyond end - seems necessary if inserts dont match. 
	} else {
			PyErr_SetString(PyExc_TypeError, "Inserts must be sequence or None");
			goto cleanup;
	}
	flags |= (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY);
	{
	PyW32_BEGIN_ALLOW_THREADS
	lrc = ::FormatMessageW(flags, pSource, msgId, langId, (LPWSTR)&resultBuf, 0, (va_list *)pInserts );
	PyW32_END_ALLOW_THREADS
	}
	if (lrc<=0) {
			ReturnAPIError("FormatMessage");
			goto cleanup;
	}
	rc = PyWinObject_FromWCHAR(resultBuf);
cleanup:
	if (pInserts) {
		for (i=0;i<numInserts;i++)
			SysFreeString(pInserts[i]);
		free(pInserts);
	}
	if (resultBuf)
		LocalFree(resultBuf);
	return rc;
}

// @pymethod int|win32api|GetLogicalDrives|Returns a bitmask representing the currently available disk drives.
static PyObject *
PyGetLogicalDrives (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetLogicalDrives"))
		return NULL;
	// @pyseeapi GetLogicalDrives
	DWORD rc = GetLogicalDrives();
	if (rc==0)
		return ReturnAPIError("GetLogicalDrives");
	return PyInt_FromLong(rc);
}

// @pymethod string|win32api|GetConsoleTitle|Returns the title for the current console.
static PyObject *
PyGetConsoleTitle (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetConsoleTitle"))
		return NULL;
	char title[128];
	// @pyseeapi GetConsoleTitle
	if (GetConsoleTitle(title, sizeof(title))==0)
		return ReturnAPIError("GetConsoleTitle");
	return Py_BuildValue("s", title);
	// @rdesc The title for the current console window.  This function will
	// raise an exception if the current application does not have a console.
}


// @pymethod string|win32api|GetComputerName|Returns the local computer name
static PyObject *
PyGetComputerName (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetComputerName"))
		return NULL;
	// @pyseeapi GetComputerName
	char buf[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD size = sizeof(buf);
	if (GetComputerName(buf, &size)==0)
		return ReturnAPIError("GetComputerName");
	return Py_BuildValue("s", buf);
}

// @pymethod string|win32api|GetUserName|Returns the current user name
static PyObject *
PyGetUserName (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetUserName"))
		return NULL;
	// @pyseeapi GetUserName
	char buf[MAX_PATH + 1];
	DWORD size = sizeof(buf);
	if (GetUserName(buf, &size)==0)
		return ReturnAPIError("GetUserName");
	return Py_BuildValue("s", buf);
}

// @pymethod string|win32api|GetDomainName|Returns the current domain name
static PyObject *
PyGetDomainName (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetUserName"))
		return NULL;
    HANDLE hToken;
    #define MY_BUFSIZE 512  // highly unlikely to exceed 512 bytes
    UCHAR InfoBuffer[ MY_BUFSIZE ];
    DWORD cbInfoBuffer = MY_BUFSIZE;
    SID_NAME_USE snu;

    BOOL bSuccess;

    if(!OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&hToken)) {
        if(GetLastError() == ERROR_NO_TOKEN) {
            // attempt to open the process token, since no thread token
            // exists
            if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
                return ReturnAPIError("OpenProcessToken");
        } else {
        	return ReturnAPIError("OpenThreadToken");
        }
    }
    bSuccess = GetTokenInformation(hToken, TokenUser,
        InfoBuffer,
        cbInfoBuffer,
        &cbInfoBuffer
        );

    CloseHandle(hToken);

    if(!bSuccess)
       	return ReturnAPIError("GetTokenInformation");

	char UserName[256];
	DWORD cchUserName = sizeof(UserName);
	char DomainName[256];
	DWORD cchDomainName = sizeof(DomainName);
    if (!LookupAccountSid(
        NULL,
        ((PTOKEN_USER)InfoBuffer)->User.Sid,
        UserName,
        &cchUserName,
        DomainName,
        &cchDomainName,
        &snu
        ))
       	return ReturnAPIError("LookupAccountSid");
    return PyString_FromString(DomainName);
}

// @pymethod int|win32api|GetCurrentThread|Returns a pseudohandle for the current thread.
static PyObject *
PyGetCurrentThread (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetCurrentThread"))
		return NULL;
	// @pyseeapi GetCurrentThread
	return PyInt_FromLong((long)::GetCurrentThread());
	// @comm A pseudohandle is a special constant that is interpreted as the current thread handle. The calling thread can use this handle to specify itself whenever a thread handle is required. Pseudohandles are not inherited by child processes.
	// The method <om win32api.DuplicateHandle> can be used to create a handle that other threads and processes can use.
	// As this handle can not be closed, and integer is returned rather than
	// a <o PyHANDLE> object, which would attempt to automatically close the handle.

}

// @pymethod int|win32api|GetCurrentThreadId|Returns the thread ID for the current thread.
static PyObject *
PyGetCurrentThreadId (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetCurrentThreadId"))
		return NULL;
	// @pyseeapi GetCurrentThreadId
	return Py_BuildValue("i", ::GetCurrentThreadId());
}

// @pymethod int|win32api|GetCurrentProcess|Returns a pseudohandle for the current process.
static PyObject *
PyGetCurrentProcess (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetCurrentProcess"))
		return NULL;
	// @pyseeapi GetCurrentProcess
	return PyInt_FromLong((long)::GetCurrentProcess());
	// @comm A pseudohandle is a special constant that is interpreted as the current thread handle. The calling thread can use this handle to specify itself whenever a thread handle is required. Pseudohandles are not inherited by child processes.
	// The method <om win32api.DuplicateHandle> can be used to create a handle that other threads and processes can use.
	// As this handle can not be closed, and integer is returned rather than
	// a <o PyHANDLE> object, which would attempt to automatically close the handle.
}

// @pymethod int|win32api|GetCurrentProcessId|Returns the thread ID for the current process.
static PyObject *
PyGetCurrentProcessId (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetCurrentProcessId"))
		return NULL;
	// @pyseeapi GetCurrentProcessId
	return Py_BuildValue("i", ::GetCurrentProcessId());
}

// @pymethod int|win32api|GetFocus|Retrieves the handle of the keyboard focus window associated with the thread that called the method. 
static PyObject *
PyGetFocus (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetFocus"))
		return NULL;
	// @pyseeapi GetFocus
	PyW32_BEGIN_ALLOW_THREADS
	HWND rc = GetFocus();
	PyW32_END_ALLOW_THREADS
	if (rc==NULL)
		return ReturnError("No window has the focus");
	return Py_BuildValue("i", rc);
	// @rdesc The method raises an exception if no window with the focus exists.
}

// @pymethod |win32api|ClipCursor|Confines the cursor to a rectangular area on the screen.
static PyObject *
PyClipCursor( PyObject *self, PyObject *args )
{
	RECT r;
	RECT *pRect;
	// @pyparm (int, int, int, int)|left, top, right, bottom||contains the screen coordinates of the upper-left and lower-right corners of the confining rectangle. If this parameter is omitted or (0,0,0,0), the cursor is free to move anywhere on the screen. 
	if (!PyArg_ParseTuple(args, "|(iiii):ClipCursor", &r.left, &r.top, &r.right, &r.bottom))
		return NULL;
	if (r.left == r.top == r.right == r.bottom == 0)
		pRect = NULL;
	else
		pRect = &r;
	// @pyseeapi ClipCursor
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::ClipCursor(pRect);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("ClipCursor");
	Py_INCREF(Py_None);
	return Py_None;
}


// @pymethod int, int|win32api|GetCursorPos|Returns the position of the cursor, in screen co-ordinates.
static PyObject *
PyGetCursorPos (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetCursorPos"))
		return NULL;
	POINT pt;
	// @pyseeapi GetCursorPos
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = GetCursorPos(&pt);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("GetCursorPos");
	return Py_BuildValue("ii", pt.x, pt.y);
}

// @pymethod int|win32api|SetCursor|Set the cursor to the HCURSOR object.
static PyObject *
PySetCursor( PyObject *self, PyObject *args )
{
	long hCursor;
	if (!PyArg_ParseTuple(args,"l:SetCursor",
		&hCursor)) // @pyparm long|hCursor||The new cursor.
		return NULL;
	// @pyseeapi SetCursor
	PyW32_BEGIN_ALLOW_THREADS
	HCURSOR ret = ::SetCursor((HCURSOR)hCursor); 
	PyW32_END_ALLOW_THREADS
	return PyLong_FromLong((long)ret);
	// @rdesc The result is the previous cursor if there was one.
}

// @pymethod int|win32api|LoadCursor|Loads a cursor.
static PyObject *
PyLoadCursor( PyObject *self, PyObject *args )
{
	long hInstance;
	long id;
	if (!PyArg_ParseTuple(args,"ll:LoadCursor",
		&hInstance, // @pyparm int|hInstance||Handle to the instance to load the resource from.
		&id)) // @pyparm int|cursorid||The ID of the cursor.
		return NULL;
	// @pyseeapi LoadCursor
	PyW32_BEGIN_ALLOW_THREADS
	HCURSOR ret = ::LoadCursor((HINSTANCE)hInstance, MAKEINTRESOURCE(id));
	PyW32_END_ALLOW_THREADS
	if (ret==NULL) ReturnAPIError("LoadCursor");
	return PyLong_FromLong((long)ret);
}

// @pymethod string|win32api|GetCommandLine|Retrieves the current application's command line.
static PyObject *
PyGetCommandLine (PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple (args, ":GetCommandLine"))
		return NULL;
	return Py_BuildValue("s", ::GetCommandLine());
	// @pyseeapi GetCommandLine
}

// @pymethod tuple|win32api|GetDiskFreeSpace|Retrieves information about the specified disk, including the amount of free space available.
PyObject *PyGetDiskFreeSpace (PyObject *self, PyObject *args)
{
  char *path = NULL;
  // @pyparm string|rootPath||Specifies the root directory of the disk to return information about. If rootPath is None, the method uses the root of the current directory. 
  if (!PyArg_ParseTuple (args, "|z:GetDiskFreeSpace", &path))
	return NULL;
  DWORD spc, bps, fc, c;
  // @pyseeapi GetDiskFreeSpace
  PyW32_BEGIN_ALLOW_THREADS
  BOOL ok = ::GetDiskFreeSpace(path, &spc, &bps, &fc, &c);
  PyW32_END_ALLOW_THREADS
  if (!ok)
	return ReturnAPIError("GetDiskSpaceFree");
  return Py_BuildValue ("(iiii)",  spc, bps, fc, c);
  // @rdesc The return value is a tuple of 4 integers, containing
  // the number of sectors per cluster, the number of bytes per sector,
  // the total number of free clusters on the disk and the total number of clusters on the disk.
  // <nl>If the function fails, an error is returned.
}
// @pymethod int|win32api|GetAsyncKeyState|Retrieves the status of the specified key.
static PyObject *
PyGetAsyncKeyState(PyObject * self, PyObject * args)
{
	int key;
	// @pyparm int|key||Specifies one of 256 possible virtual-key codes.
	if (!PyArg_ParseTuple(args, "i:GetAsyncKeyState", &key))
		return (NULL);
	int ret;
	PyW32_BEGIN_ALLOW_THREADS
	// @pyseeapi GetAsyncKeyState
	ret = GetAsyncKeyState(key);
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("i",ret);
	// @rdesc The return value specifies whether the key was pressed since the last
	// call to GetAsyncKeyState, and whether the key is currently up or down. If
	// the most significant bit is set, the key is down, and if the least significant
	// bit is set, the key was pressed after the previous call to GetAsyncKeyState.
	// <nl>The return value is zero if a window in another thread or process currently has the
	// keyboard focus.
	// @comm An application can use the virtual-key code constants win32con.VK_SHIFT,
	// win32con.VK_CONTROL, and win32con.VK_MENU as values for the key parameter.
	// This gives the state of the SHIFT, CTRL, or ALT keys without distinguishing
	// between left and right. An application can also use the following virtual-key
	// code constants as values for key to distinguish between the left and
	// right instances of those keys:
	// <nl>win32con.VK_LSHIFT
	// <nl>win32con.VK_RSHIFT
	// <nl>win32con.VK_LCONTROL
	// <nl>win32con.VK_RCONTROL
	// <nl>win32con.VK_LMENU
	// <nl>win32con.VK_RMENU
	// <nl>The GetAsyncKeyState method works with mouse buttons. However, it checks on
	// the state of the physical mouse buttons, not on the logical mouse buttons that
	// the physical buttons are mapped to.
}

// @pymethod int|win32api|GetFileAttributes|Retrieves the attributes for the named file.
static PyObject *
PyGetFileAttributes (PyObject *self, PyObject *args)
{
	char *pathName;
	// @pyparm string|pathName||The name of the file whose attributes are to be returned.
	if (!PyArg_ParseTuple (args, "s:GetFileAttributes", &pathName))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	DWORD rc = ::GetFileAttributes(pathName);
	PyW32_END_ALLOW_THREADS
	if (rc==(DWORD)0xFFFFFFFF)
		return ReturnAPIError("GetFileAttributes");
	return Py_BuildValue("i", rc);
	// @pyseeapi GetFileAttributes
	// @rdesc The return value is a combination of the win32con.FILE_ATTRIBUTE_* constants.
	// <nl>An exception is raised on failure.
}

// @pymethod int|win32api|GetKeyState|Retrieves the status of the specified key.
static PyObject *
PyGetKeyState(PyObject * self, PyObject * args)
{
	int key;
	// @pyparm int|key||Specifies a virtual key. If the desired virtual key is a letter or digit (A through Z, a through z, or 0 through 9), key must be set to the ASCII value of that character. For other keys, it must be a virtual-key code.
	if (!PyArg_ParseTuple(args, "i:GetKeyState", &key))
		return (NULL);
	int ret;
	PyW32_BEGIN_ALLOW_THREADS
	// @pyseeapi GetKeyState
	ret = GetKeyState(key);
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("i",ret);
	// @rdesc The return value specifies the status of
	// the given virtual key. If the high-order bit is 1, the key is down;
	// otherwise, it is up. If the low-order bit is 1, the key is toggled. 
	// A key, such as the CAPS LOCK key, is toggled if it is turned on.
	// The key is off and untoggled if the low-order bit is 0. A toggle key's
	// indicator light (if any) on the keyboard will be on when the key is
	// toggled, and off when the key is untoggled.
	// @comm The key status returned from this function changes as a given thread
	// reads key messages from its message queue. The status does not reflect the
	// interrupt-level state associated with the hardware. Use the <om win32api.GetAsyncKeyState> method to retrieve that information.
}
// @pymethod int|win32api|GetLastError|Retrieves the calling threads last error code value.
static PyObject *
PyGetLastError(PyObject * self, PyObject * args)
{
	// @pyseeapi GetLastError
	return Py_BuildValue("i",::GetLastError());
}

// @pymethod string|win32api|GetLogicalDriveStrings|Returns a string with all logical drives currently mapped.
static PyObject * PyGetLogicalDriveStrings (PyObject * self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetLogicalDriveStrings"))
		return (NULL);
	// @pyseeapi GetLogicalDriveStrings
	int result;
	// first, find out how big our string needs to be.
	result = ::GetLogicalDriveStrings(0, NULL);
	if (!result) {
		return ReturnAPIError("GetLogicalDriveStrings");
	} else {
		char * buffer = new char[result];
		result = ::GetLogicalDriveStrings (result, buffer);
		if (!result) {
			return ReturnAPIError("GetLogicalDriveStrings");
		} else {
			PyObject * retval = Py_BuildValue ("s#", buffer, result);
			delete [] buffer;
			return (retval);
		}
	}
	// @rdesc The return value is a single string, with each drive
	// letter NULL terminated.
	// <nl>Use "string.splitfields (s, '\\000')" to split into components.
}

// @pymethod string|win32api|GetModuleFileName|Retrieves the filename of the specified module.
static PyObject *
PyGetModuleFileName(PyObject * self, PyObject * args)
{
	int iMod;
	char buf[_MAX_PATH];
	// @pyparm int|hModule||Specifies the handle to the module.
	if (!PyArg_ParseTuple(args, "i:GetModuleFileName", &iMod))
		return (NULL);
	// @pyseeapi GetModuleFileName
	PyW32_BEGIN_ALLOW_THREADS
	long rc = ::GetModuleFileName( (HMODULE)iMod, buf, sizeof(buf));
	PyW32_END_ALLOW_THREADS
	if (rc==0)
		return ReturnAPIError("GetModuleFileName");
	return Py_BuildValue("s",buf);
}

// @pymethod int|win32api|GetModuleHandle|Returns the handle of an already loaded DLL.
static PyObject *
PyGetModuleHandle(PyObject * self, PyObject * args)
{
	char *fname = NULL;
	// @pyparm string|fileName|None|Specifies the file name of the module to load.
	if (!PyArg_ParseTuple(args, "|z:GetModuleHandle", &fname))
		return (NULL);
	// @pyseeapi GetModuleHandle
	HINSTANCE hInst = ::GetModuleHandle(fname);
	if (hInst==NULL)
		return ReturnAPIError("GetModuleHandle");
	return Py_BuildValue("i",hInst);
}

// @pymethod int|win32api|GetUserDefaultLCID|Retrieves the user default locale identifier.
static PyObject *
PyGetUserDefaultLCID(PyObject * self, PyObject * args)
{
	// @pyseeapi GetUserDefaultLCID
	return Py_BuildValue("i",::GetUserDefaultLCID());
}

// @pymethod int|win32api|GetUserDefaultLangID|Retrieves the user default language identifier. 
static PyObject *
PyGetUserDefaultLangID(PyObject * self, PyObject * args)
{
	// @pyseeapi GetUserDefaultLangID
	return Py_BuildValue("i",::GetUserDefaultLangID());
}

// @pymethod int|win32api|GetSystemDefaultLCID|Retrieves the system default locale identifier.
static PyObject *
PyGetSystemDefaultLCID(PyObject * self, PyObject * args)
{
	// @pyseeapi GetSystemDefaultLCID
	return Py_BuildValue("i",::GetSystemDefaultLCID());
}

// @pymethod int|win32api|GetSystemDefaultLangID|Retrieves the system default language identifier. 
static PyObject *
PyGetSystemDefaultLangID(PyObject * self, PyObject * args)
{
	// @pyseeapi GetSystemDefaultLangID
	return Py_BuildValue("i",::GetSystemDefaultLangID());
}

// @pymethod |win32api|AbortSystemShutdown|Aborts a system shutdown
static PyObject *
PyAbortSystemShutdown(PyObject * self, PyObject * args)
{
	// @pyparm string/<o PyUnicode>|computerName||Specifies the name of the computer where the shutdown is to be stopped.
	PyObject *obName;
	if (!PyArg_ParseTuple(args, "O:AbortSystemShutdown", &obName))
		return NULL;
	TCHAR *cname;
	if (!PyWinObject_AsTCHAR(obName, &cname, TRUE))
		return NULL;
	// @pyseeapi AbortSystemShutdown
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = AbortSystemShutdown(cname);
	PyW32_END_ALLOW_THREADS
	PyWinObject_FreeTCHAR(cname);
	if (!ok)
		return ReturnAPIError("AbortSystemShutdown");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|InitiateSystemShutdown|Initiates a shutdown and optional restart of the specified computer. 
static PyObject *
PyInitiateSystemShutdown(PyObject * self, PyObject * args)
{
	PyObject *obName;
	PyObject *obMessage;
	DWORD dwTimeOut;
	BOOL bForceClosed;
	BOOL bRebootAfter;
	// @pyparm string/<o PyUnicode>|computerName||Specifies the name of the computer to shut-down, or None
	// @pyparm string/<o PyUnicode>|message||Message to display in a dialog box
	// @pyparm int|timeOut||Specifies the time (in seconds) that the dialog box should be displayed. While this dialog box is displayed, the shutdown can be stopped by the AbortSystemShutdown function. 
	// If dwTimeout is zero, the computer shuts down without displaying the dialog box, and the shutdown cannot be stopped by AbortSystemShutdown.
	// @pyparm int|bForceClose||Specifies whether applications with unsaved changes are to be forcibly closed. If this parameter is TRUE, such applications are closed. If this parameter is FALSE, a dialog box is displayed prompting the user to close the applications.
	// @pyparm int|bRebootAfterShutdown||Specifies whether the computer is to restart immediately after shutting down. If this parameter is TRUE, the computer is to restart. If this parameter is FALSE, the system flushes all caches to disk, clears the screen, and displays a message indicating that it is safe to power down.
	if (!PyArg_ParseTuple(args, "OOlll:InitiateSystemShutdown", &obName, &obMessage, &dwTimeOut, &bForceClosed, &bRebootAfter))
		return (NULL);
	char *cname;
	char *message;
	if (!PyWinObject_AsTCHAR(obName, &cname, TRUE))
		return NULL;
	if (!PyWinObject_AsTCHAR(obMessage, &message, TRUE)) {
		PyWinObject_FreeTCHAR(cname);
		return NULL;
	}
	// @pyseeapi InitiateSystemShutdown
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = InitiateSystemShutdown(cname, message, dwTimeOut, bForceClosed, bRebootAfter);
	PyW32_END_ALLOW_THREADS
	PyWinObject_FreeTCHAR(cname);
	PyWinObject_FreeTCHAR(message);
	if (!ok)
		return ReturnAPIError("InitiateSystemShutdown");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|ExitWindows|Logs off the current user
static PyObject *
PyExitWindows(PyObject * self, PyObject * args)
{
	// @pyparm int|reserved1|0|
	// @pyparm int|reserved2|0|
	DWORD dwReserved = 0;
	ULONG ulReserved = 0;
	if (!PyArg_ParseTuple(args, "|ll:ExitWindows", &dwReserved, &ulReserved))
		return NULL;
	// @pyseeapi AbortSystemShutdown
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ExitWindows(dwReserved, ulReserved);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("ExitWindows");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|ExitWindowsEx|either logs off the current user, shuts down the system, or shuts down and restarts the system.
static PyObject *
PyExitWindowsEx(PyObject * self, PyObject * args)
{
	// @comm It sends the WM_QUERYENDSESSION message to all applications to determine if they can be terminated. 

	// @pyparm int|flags||The shutdown operation
	// @pyparm int|reserved|0|
	UINT flags;
	DWORD reserved = 0;
	if (!PyArg_ParseTuple(args, "l|l:ExitWindowsEx", &flags, &reserved))
		return NULL;
	// @pyseeapi AbortSystemShutdown
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ExitWindowsEx(flags, reserved);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("ExitWindowsEx");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod int|win32api|LoadLibrary|Loads the specified DLL, and returns the handle.
static PyObject *
PyLoadLibrary(PyObject * self, PyObject * args)
{
	char *fname;
	// @pyparm string|fileName||Specifies the file name of the module to load.
	if (!PyArg_ParseTuple(args, "s:LoadLibrary", &fname))
		return (NULL);
	// @pyseeapi LoadLibrary
	PyW32_BEGIN_ALLOW_THREADS
	HINSTANCE hInst = ::LoadLibrary(fname);
	PyW32_END_ALLOW_THREADS
	if (hInst==NULL)
		return ReturnAPIError("LoadLibrary");
	return Py_BuildValue("i",hInst);
}

// @pymethod int|win32api|LoadLibraryEx|Loads the specified DLL, and returns the handle.
static PyObject *
PyLoadLibraryEx(PyObject * self, PyObject * args)
{
	char *fname;
	HANDLE handle;
	DWORD flags;
	// @pyparm string|fileName||Specifies the file name of the module to load.
	// @pyparm int|handle||Reserved - must be zero
	// @pyparm flags|handle||Specifies the action to take when loading the module.
	if (!PyArg_ParseTuple(args, "sll:LoadLibraryEx", &fname, &handle, &flags))
		return (NULL);
	// @pyseeapi LoadLibraryEx
	PyW32_BEGIN_ALLOW_THREADS
	HINSTANCE hInst = ::LoadLibraryEx(fname, handle, flags);
	PyW32_END_ALLOW_THREADS
	if (hInst==NULL)
		return ReturnAPIError("LoadLibraryEx");
	return Py_BuildValue("i",hInst);
}

// @pymethod |win32api|FreeLibrary|Decrements the reference count of the loaded dynamic-link library (DLL) module.
static PyObject *
PyFreeLibrary(PyObject * self, PyObject * args)
{
	HINSTANCE handle;
	// @pyparm int|hModule||Specifies the handle to the module.
	if (!PyArg_ParseTuple(args, "i:FreeLibrary", &handle))
		return (NULL);
	// @pyseeapi FreeLibrary
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::FreeLibrary(handle);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("FreeLibrary");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod int|win32api|GetProcAddress|Returns the address of the specified exported dynamic-link library (DLL) function. 
static PyObject *
PyGetProcAddress(PyObject * self, PyObject * args)
{
	HINSTANCE handle;
	char *fnName;
	// @pyparm int|hModule||Specifies the handle to the module.
	// @pyparm string|functionName||Specifies the name of the procedure.
	if (!PyArg_ParseTuple(args, "is:GetProcAddress", &handle, &fnName))
		return (NULL);
	FARPROC proc = ::GetProcAddress(handle, fnName);
	if (proc==NULL)
		return ReturnAPIError("GetProcAddress");
	// @pyseeapi GetProcAddress
	return Py_BuildValue("i",(int)proc);
}

// @pymethod int/string|win32api|GetProfileVal|Retrieves entries from a windows INI file.  This method encapsulates GetProfileString, GetProfileInt, GetPrivateProfileString and GetPrivateProfileInt.
static PyObject *
PyGetProfileVal(PyObject *self, PyObject *args)
{
	char *sect, *entry, *strDef, *iniName=NULL;
	int intDef;
	BOOL bHaveInt = TRUE;
	if (!PyArg_ParseTuple(args, "ssi|s", 
	          &sect,  // @pyparm string|section||The section in the INI file to retrieve a value for.
	          &entry, // @pyparm string|entry||The entry within the section in the INI file to retrieve a value for.
	          &intDef, // @pyparm int/string|defValue||The default value.  The type of this parameter determines the methods return type.
	          &iniName)) { // @pyparm string|iniName|None|The name of the INI file.  If None, the system INI file is used.
		bHaveInt = FALSE;
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, "sss|z", &sect, &entry, &strDef, &iniName)) {
			// set my own error
			PyErr_Clear();
			PyErr_SetString(PyExc_TypeError, "GetProfileVal must have format (ssi|s) or (sss|s)");
			return NULL;
		}
	}

	if (iniName) {	
		if (bHaveInt)
			return Py_BuildValue("i",::GetPrivateProfileInt(sect, entry, intDef, iniName));
		else {
			char resBuf[2046];
			PyW32_BEGIN_ALLOW_THREADS
			::GetPrivateProfileString(sect, entry, strDef, resBuf, sizeof(resBuf), iniName);
			PyW32_END_ALLOW_THREADS
			return Py_BuildValue("s",resBuf);
		}
	}
	else {
		if (bHaveInt)
			return Py_BuildValue("i",::GetProfileInt(sect, entry, intDef));
		else {
			char resBuf[2046];
			PyW32_BEGIN_ALLOW_THREADS
			::GetProfileString(sect, entry, strDef, resBuf, sizeof(resBuf));
			PyW32_END_ALLOW_THREADS
			return Py_BuildValue("s",resBuf);
		}
	}
	// @pyseeapi GetProfileString
	// @pyseeapi GetProfileInt
	// @pyseeapi GetPrivateProfileString
	// @pyseeapi GetPrivateProfileInt
	// @rdesc The return value is the same type as the default parameter.
}
// @pymethod list|win32api|GetProfileSection|Retrieves all entries from a section in an INI file.
static PyObject *
PyGetProfileSection(PyObject * self, PyObject * args)
{
	char *szSection;
	char *iniName = NULL;
	// @pyparm string|section||The section in the INI file to retrieve a entries for.
	// @pyparm string|iniName|None|The name of the INI file.  If None, the system INI file is used.
	if (!PyArg_ParseTuple(args, "s|z:GetProfileSection", &szSection, &iniName))
		return (NULL);
	int size=0;
	int retVal = 0;
	char *szRetBuf = NULL;
	while (retVal >= size-2) {
		if (szRetBuf)
			delete szRetBuf;
		size=size?size*2:256;
		szRetBuf = new char[size]; /* cant fail - may raise exception */
		if (szRetBuf==NULL) {
			PyErr_SetString(PyExc_MemoryError, "Error allocating space for return buffer");
			return NULL;
		}
		PyW32_BEGIN_ALLOW_THREADS
		if (iniName)
			retVal = GetPrivateProfileSection(szSection, szRetBuf, size, iniName);
		else
			retVal = GetProfileSection(szSection, szRetBuf, size);
		PyW32_END_ALLOW_THREADS
	}
	PyObject *retList = PyList_New(0);
	char *sz = szRetBuf;
	char *szLast = szRetBuf;
	while (*szLast!='\0') {
		for (;*sz;sz++)
			;
		PyObject *newItem = Py_BuildValue("s", szLast);
		PyList_Append(retList, newItem );
		sz++;
		szLast = sz;
	}
	// @pyseeapi GetProfileString
	// @pyseeapi GetProfileInt
	// @pyseeapi GetPrivateProfileString
	// @pyseeapi GetPrivateProfileInt
	delete szRetBuf;
	return retList;
	// @rdesc The return value is a list of strings.
}

// @pymethod list|win32api|WriteProfileSection|Writes a complete section to an INI file or registry.
static PyObject *
PyWriteProfileSection(PyObject * self, PyObject * args)
{
	char *szSection;
	char *data;
	int dataSize;
	// @pyparm string|section||The section in the INI file to retrieve a entries for.
	// @pyparm string|data||The data to write.  This must be string, with each entry terminated with '\0', followed by another terminating '\0'
	if (!PyArg_ParseTuple(args, "ss#:WriteProfileSection", &szSection, &data, &dataSize))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = WriteProfileSection(szSection, data);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("GetTempPath");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod tuple|win32api|GetSystemInfo|Retrieves information about the current system.
static PyObject *
PyGetSystemInfo(PyObject * self, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemInfo"))
		return NULL;
	// @pyseeapi GetSystemInfo
	SYSTEM_INFO info;
	GetSystemInfo( &info );
	return Py_BuildValue("iiiiiiii(ii)", info.dwOemId, info.dwPageSize, 
                         info.lpMinimumApplicationAddress, info.lpMaximumApplicationAddress,
                         info.dwActiveProcessorMask, info.dwNumberOfProcessors,
                         info.dwProcessorType, info.dwAllocationGranularity,
						 info.wProcessorLevel, info.wProcessorRevision);
	// @rdesc The return value is a tuple of 9 values, which corresponds
	// to the Win32 SYSTEM_INFO structure.  The element names are:
	// <nl>dwOemId<nl>dwPageSize<nl>lpMinimumApplicationAddress<nl>lpMaximumApplicationAddress<nl>,
    // dwActiveProcessorMask<nl>dwNumberOfProcessors<nl>
	// dwProcessorType<nl>dwAllocationGranularity<nl>(wProcessorLevel,wProcessorRevision)
}

// @pymethod int|win32api|GetSystemMetrics|Retrieves various system metrics and system configuration settings. 
static PyObject *
PyGetSystemMetrics(PyObject * self, PyObject * args)
{
	int which;
	// @pyparm int|index||Which metric is being requested.  See the API documentation for a fill list.
	if (!PyArg_ParseTuple(args, "i:GetSystemMetrics", &which))
		return NULL;
	// @pyseeapi GetSystemMetrics
	int rc = ::GetSystemMetrics(which);
	return Py_BuildValue("i",rc);
}

// @pymethod string|win32api|GetShortPathName|Obtains the short path form of the specified path.
static PyObject *
PyGetShortPathName(PyObject * self, PyObject * args)
{
	char *path;
	if (!PyArg_ParseTuple(args, "s:GetShortPathName", &path))
		return NULL;
	char szOutPath[_MAX_PATH];
	// @pyseeapi GetShortPathName
	PyW32_BEGIN_ALLOW_THREADS
	DWORD rc = GetShortPathName(path, szOutPath, sizeof(szOutPath));
	PyW32_END_ALLOW_THREADS
	if (rc==0)
		return ReturnAPIError("GetShortPathName");
	if (rc>=sizeof(szOutPath))
		return ReturnError("The pathname would be too big!!!");
	return Py_BuildValue("s", szOutPath);
	// @comm The short path name is an 8.3 compatible file name.  As the input path does
	// not need to be absolute, the returned name may be longer than the input path.
}

// @pymethod string|win32api|GetTickCount|Returns the number of milliseconds since windows started.
static PyObject *
PyGetTickCount(PyObject * self, PyObject * args)
{
	if (!PyArg_ParseTuple (args, ":PyGetTickCount"))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	DWORD count = GetTickCount();
	PyW32_END_ALLOW_THREADS

	return Py_BuildValue("l",(long)count);
}

// @pymethod string|win32api|GetTempPath|Retrieves the path of the directory designated for temporary files.
static PyObject *
PyGetTempPath(PyObject * self, PyObject * args)
{
	char buf[MAX_PATH];
	if (!PyArg_ParseTuple (args, ":GetTempPath"))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = GetTempPath(sizeof(buf), buf);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("GetTempPath");
	return Py_BuildValue("s",buf);
}
// @pymethod tuple|win32api|GetTempFileName|Returns creates a temporary filename of the following form: path\\preuuuu.tmp.
static PyObject *
PyGetTempFileName(PyObject * self, PyObject * args)
{
	char *path, *prefix;
	int n = 0;
	if (!PyArg_ParseTuple(args,"ss|i:GetTempFileName", 
	          &path,  // @pyparm string|path||Specifies the path where the method creates the temporary filename.
	                  // Applications typically specify a period (.) or the result of the GetTempPath function for this parameter.
	          &prefix,// @pyparm string|prefix||Specifies the temporary filename prefix.
	          &n))    // @pyparm int|nUnique||Specifies an nteger used in creating the temporary filename.
	                  // If this parameter is nonzero, it is appended to the temporary filename.
	                  // If this parameter is zero, Windows uses the current system time to create a number to append to the filename.
		return NULL;

    char buf[MAX_PATH];
	PyW32_BEGIN_ALLOW_THREADS
	UINT rc=GetTempFileName(path, prefix, n, buf);
	PyW32_END_ALLOW_THREADS
    if (!rc) // @pyseeapi GetTempFileName
		return ReturnAPIError("GetTempFileName");
	return Py_BuildValue("(si)",buf,rc);
	// @rdesc The return value is a tuple of (string, int), where string is the
	// filename, and rc is the unique number used to generate the filename.
}
// @pymethod tuple|win32api|GetTimeZoneInformation|Retrieves the system time-zone information.
static PyObject *
PyGetTimeZoneInformation(PyObject * self, PyObject * args)
{
	if (!PyArg_ParseTuple (args, ":GetTimeZoneInformation"))
		return NULL;
	TIME_ZONE_INFORMATION tzinfo;
	if (GetTimeZoneInformation(&tzinfo)==0xFFFFFFFF)
		return ReturnAPIError("GetTimeZoneInformation");
	return Py_BuildValue("ls(iiiiiiii)ls(iiiiiiii)l",
	              tzinfo.Bias,
				  tzinfo.StandardName,
				    tzinfo.StandardDate.wYear, tzinfo.StandardDate.wMonth, 
				    tzinfo.StandardDate.wDayOfWeek, tzinfo.StandardDate.wDay, 
				    tzinfo.StandardDate.wHour, tzinfo.StandardDate.wMinute, 
				    tzinfo.StandardDate.wSecond, tzinfo.StandardDate.wMilliseconds,
				  tzinfo.StandardBias,
				  tzinfo.DaylightName,
				    tzinfo.DaylightDate.wYear, tzinfo.DaylightDate.wMonth, 
				    tzinfo.DaylightDate.wDayOfWeek, tzinfo.DaylightDate.wDay, 
				    tzinfo.DaylightDate.wHour, tzinfo.DaylightDate.wMinute, 
				    tzinfo.DaylightDate.wSecond, tzinfo.DaylightDate.wMilliseconds,
				  tzinfo.DaylightBias );
	// @rdesc The return value is a tuple of format "ls(iiiiiiii)ls(iiiiiiii)l",
	// which contains the information in a win32api TIME_ZONE_INFORMATION
	// structure. 
}

// @pymethod int|win32api|GetSysColor|Returns the current system color for the specified element.
static PyObject *
PyGetSysColor (PyObject *self, PyObject *args)
{
  int color_id, color_rgb;
  // @pyparm int|index||The Id of the element to return.  See the API for full details.
  if (!PyArg_ParseTuple (args, "i:GetSysColor", &color_id))
	return NULL;
  color_rgb = GetSysColor (color_id);
  // @pyseeapi GetSysColor
  return Py_BuildValue ("i", color_rgb);
  // @rdesc The return value is a windows RGB color representation.
}

// @pymethod string|win32api|GetSystemDirectory|Returns the path of the Windows system directory.
static PyObject *
PyGetSystemDirectory (PyObject *self, PyObject *args)
{
	char buf[MAX_PATH];
	if (!PyArg_ParseTuple (args, ":GetSystemDirectory"))
	// @pyseeapi GetSystemDirectory
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	::GetSystemDirectory(buf, sizeof(buf));
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("s", buf);
}

// @pymethod int|win32api|GetVersion|Returns the current version of Windows, and information about the environment.
static PyObject *
PyGetVersion(PyObject * self, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ":GetVersion"))
		return NULL;
	return Py_BuildValue("i",::GetVersion());
	// @rdesc The return value's low word is the major/minor version of Windows.  The high
	// word is 0 if the platform is Windows NT, or 1 if Win32s on Windows 3.1
}

// @pymethod (int,int,int,int,string)|win32api|GetVersionEx|Returns the current version of Windows, and information about the environment.
static PyObject *
PyGetVersionEx(PyObject * self, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ":GetVersionEx"))
		return NULL;
	OSVERSIONINFO ver;
	ver.dwOSVersionInfoSize = sizeof(ver);
	if (!::GetVersionEx(&ver))
		return ReturnAPIError("GetVersionEx");
	return Py_BuildValue("iiiis",
	// @rdesc The return value is a tuple with the following information.<nl>
		         ver.dwMajorVersion, // @tupleitem 0|int|majorVersion|Identifies the major version number of the operating system.<nl>
				 ver.dwMinorVersion, //	@tupleitem 1|int|minorVersion|Identifies the minor version number of the operating system.<nl>
				 ver.dwBuildNumber,  //	@tupleitem 2|int|buildNumber|Identifies the build number of the operating system in the low-order word. (The high-order word contains the major and minor version numbers.)<nl>
				 ver.dwPlatformId, // @tupleitem 3|int|platformId|Identifies the platform supported by the operating system.  May be one of VER_PLATFORM_WIN32s, VER_PLATFORM_WIN32_WINDOWS or VER_PLATFORM_WIN32_NT<nl>
				 ver.szCSDVersion); // @tupleitem 4|string|version|Contains arbitrary additional information about the operating system.
}

// @pymethod tuple|win32api|GetVolumeInformation|Returns information about a file system and colume whose root directory is specified.
static PyObject *
PyGetVolumeInformation(PyObject * self, PyObject * args)
{
	char *pathName;
	// @pyparm string|path||The root path of the volume on which information is being requested.
	if (!PyArg_ParseTuple(args, "s:GetVolumeInformation", &pathName))
		return NULL;
	char szVolName[_MAX_PATH];
	DWORD serialNo;
	DWORD maxCompLength;
	DWORD sysFlags;
	char szSysName[50];
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::GetVolumeInformation( pathName, szVolName, sizeof(szVolName), &serialNo, &maxCompLength, &sysFlags, szSysName, sizeof(szSysName));
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("GetVolumeInformation");
	return Py_BuildValue("sllls", szVolName, (long)serialNo, (long)maxCompLength, (long)sysFlags, szSysName );
	// @rdesc The return is a tuple of:
	// <nl>string - Volume Name
	// <nl>long - Volume serial number.
	// <nl>long - Maximum Component Length of a file name.
	// <nl>long - Sys Flags - other flags specific to the file system.  See the api for details.
	// <nl>string - File System Name
}

// @pymethod string|win32api|GetFullPathName|Returns the full path of a (possibly relative) path
static PyObject *
PyGetFullPathName (PyObject *self, PyObject *args)
{
	char pathBuf[MAX_PATH];
	char *fileName;
	// @pyparm string|fileName||The file name.
	if (!PyArg_ParseTuple (args, "s:GetFullPathName", &fileName))
		return NULL;
	char *temp;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = GetFullPathName(fileName, sizeof(pathBuf), pathBuf, &temp);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("GetFullPathName");
	return Py_BuildValue("s", pathBuf);
}

// @pymethod string|win32api|GetWindowsDirectory|Returns the path of the Windows directory.
static PyObject *
PyGetWindowsDirectory (PyObject *self, PyObject *args)
{
	char buf[MAX_PATH];
	if (!PyArg_ParseTuple (args, ":GetWindowsDirectory"))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	::GetWindowsDirectory(buf, sizeof(buf));
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("s", buf);
}
// @pymethod |win32api|MoveFile|Renames a file, or a directory (including its children).
static PyObject *
PyMoveFile( PyObject *self, PyObject *args )
{
	char *src, *dest;
	// @pyparm string|srcName||The name of the source file.
	// @pyparm string|destName||The name of the destination file.
	// @comm This method can not move files across volumes.
	if (!PyArg_ParseTuple(args, "ss:MoveFile", &src, &dest))
		return NULL;
	// @pyseeapi MoveFile.
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::MoveFile(src, dest);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("MoveFile");
	Py_INCREF(Py_None);
	return Py_None;
}
// @pymethod |win32api|MoveFileEx|Renames a file.
static PyObject *
PyMoveFileEx( PyObject *self, PyObject *args )
{
	int flags;
	char *src, *dest;
	// @pyparm string|srcName||The name of the source file.
	// @pyparm string|destName||The name of the destination file.  May be None.
	// @pyparm int|flag||Flags indicating how the move is to be performed.  See the API for full details.
	// @comm This method can move files across volumes.<nl>
	// If destName is None, and flags contains win32con.MOVEFILE_DELAY_UNTIL_REBOOT, the
	// file will be deleted next reboot.
	if (!PyArg_ParseTuple(args, "szi:MoveFileEx", &src, &dest, &flags))
		return NULL;
	// @pyseeapi MoveFileEx
	PyW32_BEGIN_ALLOW_THREADS
		BOOL ok = ::MoveFileEx(src, dest, flags);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("MoveFileEx");
	Py_INCREF(Py_None);
	return Py_None;
}
// @pymethod |win32api|PostMessage|Post a message to a window.
PyObject *PyPostMessage(PyObject *self, PyObject *args)
{
	HWND hwnd;
	UINT message;
	WPARAM wParam=0;
	LPARAM lParam=0;
	if (!PyArg_ParseTuple(args, "ii|ii:PostMessage", 
	          &hwnd,    // @pyparm int|hwnd||The hWnd of the window to receive the message.
	          &message, // @pyparm int|idMessage||The ID of the message to post.
	          &wParam,  // @pyparm int|wParam||The wParam for the message
	          &lParam)) // @pyparm int|lParam||The lParam for the message
		return NULL;
	// @pyseeapi PostMessage
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::PostMessage(hwnd, message, wParam, lParam);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("PostMessage");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|PostThreadMessage|Post a message to the specified thread.
PyObject *PyPostThreadMessage(PyObject *self, PyObject *args)
{
	DWORD threadId;
	UINT message;
	WPARAM wParam=0;
	LPARAM lParam=0;
	if (!PyArg_ParseTuple(args, "ii|ii:PostThreadMessage", 
	          &threadId,    // @pyparm int|hwnd||The hWnd of the window to receive the message.
	          &message, // @pyparm int|idMessage||The ID of the message to post.
	          &wParam,  // @pyparm int|wParam||The wParam for the message
	          &lParam)) // @pyparm int|lParam||The lParam for the message
		return NULL;
	// @pyseeapi PostThreadMessage
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::PostThreadMessage(threadId, message, wParam, lParam);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("PostThreadMessage");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|PostQuitMessage|Post a quit message to an app.
PyObject *PyPostQuitMessage(PyObject *self, PyObject *args)
{
	DWORD exitCode = 0;
	if (!PyArg_ParseTuple(args, "|i:PostQuitMessage", 
	          &exitCode))    // @pyparm int|exitCode|0|The exit code
		return NULL;
	// @pyseeapi PostQuitMessage
	PyW32_BEGIN_ALLOW_THREADS
	::PostQuitMessage(exitCode);
	PyW32_END_ALLOW_THREADS
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|RegCloseKey|Closes a previously opened registry key.
static PyObject *
PyRegCloseKey( PyObject *self, PyObject *args )
{
	PyObject *obKey;
	// @pyparm <o PyHKEY>/int|key||The key to be closed.
	if (!PyArg_ParseTuple(args, "O:RegCloseKey", &obKey))
		return NULL;
	// @pyseeapi RegCloseKey
	if (!PyWinObject_CloseHKEY(obKey))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}
// @pymethod int|win32api|RegConnectRegistry|Establishes a connection to a predefined registry handle on another computer.
static PyObject *
PyRegConnectRegistry( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *szCompName = NULL;
	HKEY retKey;
	long rc;
	// @pyparm string|computerName||The name of the remote computer, of the form \\\\computername.  If None, the local computer is used.
	// @pyparm int|key||The predefined handle.  May be win32con.HKEY_LOCAL_MACHINE or win32con.HKEY_USERS.
	if (!PyArg_ParseTuple(args, "zO:RegConnectRegistry", &szCompName, &obKey))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegConnectRegistry
	rc=RegConnectRegistry(szCompName, hKey, &retKey);
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegConnectRegistry", rc);
	return PyWinObject_FromHKEY(retKey);
	// @rdesc The return value is the handle of the opened key. 
	// If the function fails, an exception is raised.
}
// @pymethod <o PyHKEY>|win32api|RegCreateKey|Creates the specified key, or opens the key if it already exists.
static PyObject *
PyRegCreateKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *subKey;
	HKEY retKey;
	long rc;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|subKey||The name of a key that this method opens or creates.
	// This key must be a subkey of the key identified by the key parameter.
	// If key is one of the predefined keys, subKey may be None. In that case,
	// the handle returned is the same hkey handle passed in to the function.
	if (!PyArg_ParseTuple(args, "Oz:RegCreateKey", &obKey, &subKey ))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegCreateKey
	rc=RegCreateKey(hKey, subKey, &retKey);
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegCreateKey", rc);
	return PyWinObject_FromHKEY(retKey);
	// @rdesc The return value is the handle of the opened key.
	// If the function fails, an exception is raised.
}
// @pymethod |win32api|RegDeleteKey|Deletes the specified key.  This method can not delete keys with subkeys.
static PyObject *
PyRegDeleteKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *subKey;
	long rc;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|subKey||The name of the key to delete.
	// This key must be a subkey of the key identified by the key parameter.
	// This value must not be None, and the key may not have subkeys.
	if (!PyArg_ParseTuple(args, "Os:RegDeleteKey", &obKey, &subKey))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegDeleteKey
	rc=RegDeleteKey(hKey, subKey );
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegDeleteKey", rc);
	Py_INCREF(Py_None);
	return Py_None;
	// @comm If the method succeeds, the entire key, including all of its values, is removed.
	// If the method fails, and exception is raised.
}
// @pymethod |win32api|RegDeleteValue|Removes a named value from the specified registry key.
static PyObject *
PyRegDeleteValue( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *subKey;
	long rc;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|value||The name of the value to remove.
	if (!PyArg_ParseTuple(args, "Oz:RegDeleteValue", &obKey, &subKey))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegDeleteValue
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegDeleteValue(hKey, subKey);
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegDeleteValue", rc);
	Py_INCREF(Py_None);
	return Py_None;
}
// @pymethod string|win32api|RegEnumKey|Enumerates subkeys of the specified open registry key. The function retrieves the name of one subkey each time it is called.
static PyObject *
PyRegEnumKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	int index;
	long rc;
	char *retBuf;
    DWORD len;
    
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm int|index||The index of the key to retrieve.
	if (!PyArg_ParseTuple(args, "Oi:RegEnumKey", &obKey, &index))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;

	if ((rc=RegQueryInfoKey( hKey, NULL, NULL, NULL, NULL, &len, 
		                           NULL, NULL, NULL, NULL, NULL, NULL))!=ERROR_SUCCESS)
		return ReturnAPIError("RegQueryInfoKey", rc);
	++len;	// include null terminator
	retBuf=(char *)alloca(len);

	// @pyseeapi RegEnumKey
	if ((rc=RegEnumKey(hKey, index, retBuf, len))!=ERROR_SUCCESS)
		return ReturnAPIError("RegEnumKey", rc);
	return Py_BuildValue("s", retBuf);
}

// Note that fixupMultiSZ and countString have both had changes
// made to support "incorrect strings".  The registry specification
// calls for strings to be terminated with 2 null bytes.  It seems
// some commercial packages install strings whcich dont conform,
// causing this code to fail - however, "regedit" etc still work 
// with these strings (ie only we dont!).
static void
fixupMultiSZ(char **str, char *data, int len)
{
	char *P;
	int i;
	char    *Q;


	Q = data + len;
	for(P=data, i=0; P < Q && *P!='\0'; P++, i++)
	{
		str[i]=P;
		for(; *P!='\0'; P++)
			;
	}
}

static int
countStrings(char *data, int len)
{
	int strings;
	char *P;
	char *Q = data + len;

	for (P=data, strings=0; P < Q && *P!='\0'; P++, strings++)
		for(; P < Q && *P!='\0'; P++)
			;

	return strings;
}

/* Convert PyObject into Registry data. 
   Allocates space as needed. */
static int
Py2Reg(PyObject *value, DWORD typ, BYTE **retDataBuf, DWORD *retDataSize)
{
	int i,j;
	switch (typ) {
		case REG_DWORD:
			if (value!=Py_None && !PyInt_Check(value))
				return 0;
			*retDataBuf=(BYTE *)PyMem_NEW(DWORD, 1);
			*retDataSize=sizeof(DWORD);
			if (value==Py_None) {
				DWORD zero = 0;
				memcpy(*retDataBuf, &zero, sizeof(DWORD));
			} 
			else
				memcpy(*retDataBuf, &PyInt_AS_LONG((PyIntObject *)value), sizeof(DWORD));
			break;
		case REG_SZ:
		case REG_EXPAND_SZ:
			if (value==Py_None)
				*retDataSize=1;
			else {
				if (!PyString_Check(value))
					return 0;
				*retDataSize=strlen(PyString_AS_STRING((PyStringObject *)value))+1;
			}
			*retDataBuf=(BYTE *)PyMem_NEW(DWORD, *retDataSize);
			if (value==Py_None)
				strcpy((char *)*retDataBuf, "");
			else
				strcpy((char *)*retDataBuf, PyString_AS_STRING((PyStringObject *)value));
			break;
		case REG_MULTI_SZ:
			{
				DWORD size=0;
				PyObject *t;

				if (value==Py_None)
					i = 0;
				else {
					if (!PyList_Check(value))
						return 0;
					i=PyList_Size(value);
				}
				for(j=0; j<i; j++)
				{
					t=PyList_GET_ITEM((PyListObject *)value,j);
					if (!PyString_Check(t))
						return 0;
					size=size+strlen(PyString_AS_STRING((PyStringObject *)t))+1;
				}
			
				*retDataSize=size+1;
				*retDataBuf=(BYTE *)PyMem_NEW(char, *retDataSize);
				char *P=(char *)*retDataBuf;

				for(j=0; j<i; j++)
				{
					t=PyList_GET_ITEM((PyListObject *)value,j);
					strcpy(P,PyString_AS_STRING((PyStringObject *)t));
					P=P+strlen(PyString_AS_STRING((PyStringObject *)t))+1;
				}
				// And doubly-terminate the list...
				*P = '\0';
				break;
			}
		case REG_BINARY:
		// ALSO handle ALL unknown data types here.  Even if we cant support
		// it natively, we should handle the bits.
		default: 
			if (value==Py_None)
				*retDataSize = 0;
			else {
				if (!PyString_Check(value))
					return 0;
				*retDataSize=PyString_Size(value);
				*retDataBuf=(BYTE *)PyMem_NEW(char, *retDataSize);
				memcpy(*retDataBuf,PyString_AS_STRING((PyStringObject *)value),*retDataSize);
			}
			break;
	}

	return 1;

}

/* Convert Registry data into PyObject*/
static PyObject *
Reg2Py(char *retDataBuf, DWORD retDataSize, DWORD typ)
{
	PyObject *obData;

	switch (typ) {
		case REG_DWORD:
			if (retDataSize==0)
				obData = Py_BuildValue("i", 0);
			else
				obData = Py_BuildValue("i", *(int *)retDataBuf);
			break;
		case REG_SZ:
		case REG_EXPAND_SZ:
			// retDataBuf may or may not have a trailing NULL in
			// the buffer.
			if (retDataSize && retDataBuf[retDataSize-1]=='\0')
				--retDataSize;
			if (retDataSize==0)
				retDataBuf = "";
			obData = PyString_FromStringAndSize(retDataBuf, retDataSize);
			break;
		case REG_MULTI_SZ:
			if (retDataSize==0)
				obData = PyList_New(0);
			else
			{
				int index=0;
				int s=countStrings(retDataBuf, retDataSize);
				char **str=(char **)alloca(sizeof(char *)*s);

				fixupMultiSZ(str, retDataBuf, retDataSize);
				obData = PyList_New(s);
				for(index=0; index < s; index++)
				{
					PyList_SetItem(obData, index, Py_BuildValue("s", (char *)str[index]));
				}
		
				break;
			}
		case REG_BINARY:
		// ALSO handle ALL unknown data types here.  Even if we cant support
		// it natively, we should handle the bits.
		default:
			if (retDataSize==0) {
				Py_INCREF(Py_None);
				obData = Py_None;
			} else
				obData = Py_BuildValue("s#", (char *)retDataBuf, retDataSize);
			break;
	}
	if (obData==NULL)
		return NULL;
	else
		return obData;
}

// @pymethod (string,object,type)|win32api|RegEnumValue|Enumerates values of the specified open registry key. The function retrieves the name of one subkey each time it is called.
static PyObject *
PyRegEnumValue( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	int index;
	long rc;
	char *retValueBuf;
	char *retDataBuf;
	DWORD retValueSize;
	DWORD retDataSize;
	DWORD typ;

	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm int|index||The index of the key to retrieve.

	if (!PyArg_ParseTuple(args, "Oi:PyRegEnumValue", &obKey, &index))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;

	if ((rc=RegQueryInfoKey( hKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		             &retValueSize, &retDataSize, NULL, NULL))!=ERROR_SUCCESS)
		return ReturnAPIError("RegQueryInfoKey", rc);
	++retValueSize;	// include null terminators
	++retDataSize;
	retValueBuf=(char *)alloca(retValueSize);
	retDataBuf=(char *)alloca(retDataSize);

	// @pyseeapi PyRegEnumValue
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegEnumValue(hKey, index, retValueBuf, &retValueSize, NULL, &typ, (BYTE *)retDataBuf, &retDataSize);
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("PyRegEnumValue", rc);
	PyObject *obData=Reg2Py(retDataBuf, retDataSize, typ);
	if (obData==NULL)
		return NULL;
	return Py_BuildValue("sOi", retValueBuf, obData, typ);
	// @comm This function is typically called repeatedly, until an exception is raised, indicating no more values.
}

// @pymethod |win32api|RegFlushKey|Writes all the attributes of the specified key to the registry.
static PyObject *
PyRegFlushKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	long rc;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	if (!PyArg_ParseTuple(args, "O:RegFlushKey", &obKey))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegFlushKey
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegFlushKey(hKey);
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegFlushKey", rc);
	Py_INCREF(Py_None);
	return Py_None;
	// @comm It is not necessary to call RegFlushKey to change a key.
	// Registry changes are flushed to disk by the registry using its lazy flusher.
	// Registry changes are also flushed to disk at system shutdown.
	// <nl>Unlike <om win32api.RegCloseKey>, the RegFlushKey method returns only when all the data has been written to the registry.
	// <nl>An application should only call RegFlushKey if it requires absolute certainty that registry changes are on disk. If you don't know whether a RegFlushKey call is required, it probably isn't.
}
// @pymethod |win32api|RegLoadKey|The RegLoadKey method creates a subkey under HKEY_USER or HKEY_LOCAL_MACHINE
// and stores registration information from a specified file into that subkey.
static PyObject *
PyRegLoadKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *subKey;
	char *fileName;

	long rc;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|subKey||The name of the key to delete.
	// This key must be a subkey of the key identified by the key parameter.
	// This value must not be None, and the key may not have subkeys.
	// @pyparm string|filename||The name of the file to load registry data from.
	//  This file must have been created with the <om win32api.RegSaveKey> function.
	// Under the file allocation table (FAT) file system, the filename may not have an extension.
	if (!PyArg_ParseTuple(args, "Oss:RegLoadKey", &obKey, &subKey, &fileName))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegLoadKey
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegLoadKey(hKey, subKey, fileName );
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegLoadKey", rc);
	Py_INCREF(Py_None);
	return Py_None;
	// @comm A call to RegLoadKey fails if the calling process does not have the SE_RESTORE_PRIVILEGE privilege.
	// <nl>If hkey is a handle returned by <om win32api.RegConnectRegistry>, then the path specified in fileName is relative to the remote computer. 
}
// @pymethod <o PyHKEY>|win32api|RegOpenKey|Opens the specified key.
// @comm This funcion is implemented using <om win32api.RegOpenKeyEx>, by taking advantage
// of default parameters.  See <om win32api.RegOpenKeyEx> for more details.
// @pymethod <o PyHKEY>|win32api|RegOpenKeyEx|Opens the specified key.
static PyObject *
PyRegOpenKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;

	char *subKey;
	int res = 0;
	HKEY retKey;
	long rc;
	REGSAM sam = KEY_READ;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|subKey||The name of a key that this method opens.
	// This key must be a subkey of the key identified by the key parameter.
	// If key is one of the predefined keys, subKey may be None. In that case,
	// the handle returned is the same key handle passed in to the function.
	// @pyparm int|reserved|0|Reserved.  Must be zero.
	// @pyparm int|sam|KEY_READ|Specifies an access mask that describes the desired security access for the new key. This parameter can be a combination of the following win32con constants:
	// <nl>KEY_ALL_ACCESS<nl>KEY_CREATE_LINK<nl>KEY_CREATE_SUB_KEY<nl>KEY_ENUMERATE_SUB_KEYS<nl>KEY_EXECUTE<nl>KEY_NOTIFY<nl>KEY_QUERY_VALUE<nl>KEY_READ<nl>KEY_SET_VALUE<nl>KEY_WRITE<nl>
	if (!PyArg_ParseTuple(args, "Oz|ii:RegOpenKey", &obKey, &subKey, &res, &sam ))
		return NULL;
	// @pyseeapi RegOpenKeyEx
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;

	PyW32_BEGIN_ALLOW_THREADS
	rc=RegOpenKeyEx(hKey, subKey, res, sam, &retKey);
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegOpenKeyEx", rc);
	return PyWinObject_FromHKEY(retKey);

	// @rdesc The return value is the handle of the opened key.
	// If the function fails, an exception is raised.
}

static double LI2double(LARGE_INTEGER *li)
{
  double d=li->LowPart;
  d=d+pow(2.0,32.0)*li->HighPart;
  return d;
}

// @pymethod (int, int, long)|win32api|RegQueryInfoKey|Returns the number of 
// subkeys, the number of values a key has, 
// and if available the last time the key was modified as
// 100's of nanoseconds since Jan 1, 1600.
static PyObject *
PyRegQueryInfoKey( PyObject *self, PyObject *args)
{
  HKEY hKey;
  PyObject *obKey;
  long rc;
  DWORD nSubKeys, nValues;
  FILETIME ft;
  LARGE_INTEGER li;
  PyObject *l;

  // @pyparm <o PyHKEY>/int|key||An already open key, or or any one of the following win32con
  // constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
  if (!PyArg_ParseTuple(args, "O:RegQueryInfoKey", &obKey))
    return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
  // @pyseeapi RegQueryInfoKey
  if ((rc=RegQueryInfoKey(hKey, NULL, NULL, 0, &nSubKeys, NULL, NULL, &nValues,
    NULL,
    NULL,
    NULL,
    &ft)
       )!=ERROR_SUCCESS)
    return ReturnAPIError("RegQueryInfoKey", rc);
  li.LowPart=ft.dwLowDateTime;
  li.HighPart=ft.dwHighDateTime;
  if (!(l=PyLong_FromDouble(LI2double(&li))))
      return NULL;
  return Py_BuildValue("iiO",nSubKeys,nValues,l);
}

// @pymethod string|win32api|RegQueryValue|The RegQueryValue method retrieves the value associated with
// the unnamed value for a specified key in the registry.
static PyObject *
PyRegQueryValue( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *subKey;

	long rc;
	char *retBuf;
	long bufSize = 0;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|subKey||The name of the subkey with which the value is associated.
	// If this parameter is None or empty, the function retrieves the value set by the <om win32api.RegSetValue> method for the key identified by key. 
	if (!PyArg_ParseTuple(args, "Oz:RegQueryValue", &obKey, &subKey))
		return NULL;
	// @pyseeapi RegQueryValue
	
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	if ((rc=RegQueryValue(hKey, subKey, NULL, &bufSize))!=ERROR_SUCCESS)
		return ReturnAPIError("RegQueryValue", rc);
	retBuf=(char *)alloca(bufSize);
	if ((rc=RegQueryValue(hKey, subKey, retBuf, &bufSize))!=ERROR_SUCCESS)
		return ReturnAPIError("RegQueryValue", rc);
	return Py_BuildValue("s", retBuf);
	// @comm Values in the registry have name, type, and data components. This method
	// retrieves the data for a key's first value that has a NULL name.
	// But the underlying API call doesn't return the type, Lame Lame Lame, DONT USE THIS!!!
}

// @pymethod (object,type)|win32api|RegQueryValueEx|Retrieves the type and data for a specified value name associated with an open registry key. 
static PyObject *
PyRegQueryValueEx( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *valueName;

	long rc;
	char *retBuf;
	DWORD bufSize = 0;
	DWORD typ;

	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|valueName||The name of the value to query.
	if (!PyArg_ParseTuple(args, "Oz:RegQueryValueEx", &obKey, &valueName))
		return NULL;
	// @pyseeapi RegQueryValueEx
	
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	if ((rc=RegQueryValueEx(hKey, valueName, NULL, NULL, NULL, &bufSize))!=ERROR_SUCCESS)
		return ReturnAPIError("RegQueryValueEx", rc);
	retBuf=(char *)alloca(bufSize);
	if ((rc=RegQueryValueEx(hKey, valueName, NULL, &typ, (BYTE *)retBuf, &bufSize))!=ERROR_SUCCESS)
		return ReturnAPIError("RegQueryValueEx", rc);
	PyObject *obData=Reg2Py(retBuf, bufSize, typ);
	if (obData==NULL)
		return NULL;
	PyObject *result = Py_BuildValue("Oi", obData, typ);
	Py_DECREF(obData);
	return result;
	// @comm Values in the registry have name, type, and data components. This method
	// retrieves the data for the given value.
}



// @pymethod |win32api|RegSaveKey|The RegSaveKey method saves the specified key, and all its subkeys to the specified file.
static PyObject *
PyRegSaveKey( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	PyObject *obSA = Py_None;
	char *fileName;

	long rc;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|filename||The name of the file to save registry data to.
	// This file cannot already exist. If this filename includes an extension, it cannot be used on file allocation table (FAT) file systems by the <om win32api.RegLoadKey>, <om win32api.RegReplaceKey>, or <om win32api.RegRestoreKey> methods. 
	// @pyparm <o PySECURITY_ATTRIBUTES>|sa||The security attributes of the created file.
	if (!PyArg_ParseTuple(args, "Os|O:RegSaveKey", &obKey, &fileName, &obSA))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	LPSECURITY_ATTRIBUTES pSA;
	if (!PyWinObject_AsSECURITY_ATTRIBUTES(obSA, &pSA, TRUE))
		return NULL;
	// @pyseeapi RegSaveKey
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegSaveKey(hKey, fileName, pSA );
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegSaveKey", rc);
	Py_INCREF(Py_None);
	return Py_None;
	// @comm If key represents a key on a remote computer, the path described by fileName is relative to the remote computer.
	// <nl>The caller of this method must possess the SeBackupPrivilege security privilege.
	// <nl>Currently, this function passes NULL for security_attributes to the API.
}
// @pymethod |win32api|RegSetValue|Associates a value with a specified key.  Currently, only strings are supported.
static PyObject *
PyRegSetValue( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *subKey;
	char *str;
	DWORD typ;
	DWORD len;
	long rc;
	PyObject *obStrVal;
	PyObject *obSubKey;
	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|subKey||The name of the subkey with which the value is associated.
	// This parameter can be None or empty, in which case the value will be added to the key identified by the key parameter. 
	// @pyparm int|type||Type of data. Must be win32con.REG_SZ
	// @pyparm string|value||The value to associate with the key.
	if (!PyArg_ParseTuple(args, "OOiO:RegSetValue", &obKey, &obSubKey, &typ, &obStrVal))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	if (typ!=REG_SZ) {
		PyErr_SetString(PyExc_TypeError, "Type must be win32con.REG_SZ");
		return NULL;
	}
	if (!PyWinObject_AsString(obStrVal, &str, FALSE, &len))
		return NULL;
	if (!PyWinObject_AsString(obSubKey, &subKey, TRUE)) {
		PyWinObject_FreeString(str);
		return NULL;
	}

	// @pyseeapi RegSetValue
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegSetValue(hKey, subKey, REG_SZ, str, len+1 );
	PyW32_END_ALLOW_THREADS
	PyWinObject_FreeString(str);
	PyWinObject_FreeString(subKey);
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegSetValue", rc);
	Py_INCREF(Py_None);
	return Py_None;
	// @comm If the key specified by the lpszSubKey parameter does not exist, the RegSetValue function creates it.
	// <nl>Value lengths are limited by available memory. Long values (more than 2048 bytes) should be stored as files with the filenames stored in the configuration registry. This helps the registry perform efficiently.
	// <nl>The key identified by the key parameter must have been opened with KEY_SET_VALUE access.
}

// @pymethod |win32api|RegSetValueEx|Stores data in the value field of an open registry key.
static PyObject *
PyRegSetValueEx( PyObject *self, PyObject *args )
{
	HKEY hKey;
	PyObject *obKey;
	char *valueName;
	PyObject *obRes;
	PyObject *value;
	BYTE *data;
	DWORD len;
	DWORD typ;

	DWORD rc;

	// @pyparm <o PyHKEY>/int|key||An already open key, or any one of the following win32con constants:<nl>HKEY_CLASSES_ROOT<nl>HKEY_CURRENT_USER<nl>HKEY_LOCAL_MACHINE<nl>HKEY_USERS
	// @pyparm string|valueName||The name of the value to set.
	// If a value with this name is not already present in the key, the method adds it to the key.
	// <nl>If this parameter is None or an empty string and the type parameter is the win32api.REG_SZ type, this function sets the same value the <om win32api.RegSetValue> method would set.
	// @pyparm any|reserved||Place holder for reserved argument.  Zero will always be passed to the API function.
	// @pyparm int|type||Type of data. 
	// @flagh Value|Meaning 
	// @flag REG_BINARY|Binary data in any form. 
	// @flag REG_DWORD|A 32-bit number. 
	// @flag REG_DWORD_LITTLE_ENDIAN|A 32-bit number in little-endian format. This is equivalent to REG_DWORD.<nl>In little-endian format, a multi-byte value is stored in memory from the lowest byte (the little end) to the highest byte. For example, the value 0x12345678 is stored as (0x78 0x56 0x34 0x12) in little-endian format. 
	// Windows NT and Windows 95 are designed to run on little-endian computer architectures. A user may connect to computers that have big-endian architectures, such as some UNIX systems. 
	// @flag REG_DWORD_BIG_ENDIAN|A 32-bit number in big-endian format.
	// In big-endian format, a multi-byte value is stored in memory from the highest byte (the big end) to the lowest byte. For example, the value 0x12345678 is stored as (0x12 0x34 0x56 0x78) in big-endian format. 
	// @flag REG_EXPAND_SZ|A null-terminated string that contains unexpanded references to environment variables (for example, %PATH%). It will be a Unicode or ANSI string depending on whether you use the Unicode or ANSI functions. 
	// @flag REG_LINK|A Unicode symbolic link. 
	// @flag REG_MULTI_SZ|An array of null-terminated strings, terminated by two null characters. 
	// @flag REG_NONE|No defined value type.
	// @flag REG_RESOURCE_LIST|A device-driver resource list. 
	// @flag REG_SZ|A null-terminated string. It will be a Unicode or ANSI string depending on whether you use the Unicode or ANSI functions
 

	// @pyparm registry data|value||The value to be stored with the specified value name.
	if (!PyArg_ParseTuple(args, "OzOiO:RegSetValueEx", &obKey, &valueName, &obRes, &typ, &value))
		return NULL;
	if (!PyWinObject_AsHKEY(obKey, &hKey))
		return NULL;
	// @pyseeapi RegSetValueEx
	if (!Py2Reg(value, typ, &data, &len))
	{
		PyErr_SetObject(PyExc_ValueError, Py_BuildValue("sO","Data didn't match Registry Type", data));
		return NULL;
	}
	PyW32_BEGIN_ALLOW_THREADS
	rc=RegSetValueEx(hKey, valueName, NULL, typ, data, len );
	PyW32_END_ALLOW_THREADS
	if (rc!=ERROR_SUCCESS)
		return ReturnAPIError("RegSetValueEx", rc);
	Py_INCREF(Py_None);
	return Py_None;
	// @comm  This method can also set additional value and type information for the specified key.
	// <nl>The key identified by the key parameter must have been opened with KEY_SET_VALUE access.
	// To open the key, use the <om win32api.RegCreateKeyEx> or <om win32api.RegOpenKeyEx> methods.
	// <nl>Value lengths are limited by available memory. 
	// Long values (more than 2048 bytes) should be stored as files with the filenames stored in the configuration registry.
	// This helps the registry perform efficiently.
	// <nl>The key identified by the key parameter must have been opened with KEY_SET_VALUE access.
}

// @pymethod |win32api|RegisterWindowMessage|The RegisterWindowMessage method, given a string, returns a system wide unique
// message ID, suitable for sending messages between applications who both register the same string.
static PyObject *
PyRegisterWindowMessage( PyObject *self, PyObject *args )
{
	char *msgString;
	UINT msgID;

	// @pyparm string|msgString||The name of the message to register.
	// All applications that register this message string will get the same message.
	// ID back.  It will be unique in the system and suitable for applications to 
	// use to exchange messages.
	if (!PyArg_ParseTuple(args, "s:RegisterWindowMessage", &msgString))
		return NULL;
	// @pyseeapi RegisterWindowMessage
	PyW32_BEGIN_ALLOW_THREADS
	msgID=RegisterWindowMessage(msgString);
	PyW32_END_ALLOW_THREADS
	if (msgID==0)
		return ReturnAPIError("RegisterWindowMessage", msgID);
	return Py_BuildValue("i",msgID);
	// @comm Only use RegisterWindowMessage when more than one application must process the 
	// <nl> same message. For sending private messages within a window class, an application
	// <nl> can use any integer in the range WM_USER through 0x7FFF. (Messages in this range
	// <nl> are private to a window class, not to an application. For example, predefined 
	// <nl> control classes such as BUTTON, EDIT, LISTBOX, and COMBOBOX may use values in
	// <nl> this range.) 
}

// @pymethod int|win32api|SearchPath|Searches a path for the specified file.
static PyObject *
PySearchPath (PyObject *self, PyObject *args)
{
	char *szPath, *szFileName, *szExt = NULL;
	char retBuf[512], *szBase;
	// @pyparm string|path||The path to search.  If None, searches the standard paths.
	// @pyparm string|fileName||The name of the file to search for.
	// @pyparm string|fileExt|None|specifies an extension to be added to the filename when searching for the file.
	// The first character of the filename extension must be a period (.).
	// The extension is added only if the specified filename does not end with an extension.
	// If a filename extension is not required or if the filename contains an extension, this parameter can be None.
	if (!PyArg_ParseTuple (args, "zs|z:SearchPath", &szPath, &szFileName, &szExt))
		return NULL;
	DWORD rc;
	PyW32_BEGIN_ALLOW_THREADS
	// @pyseeapi SearchPath
	rc = ::SearchPath(szPath, szFileName, szExt, sizeof(retBuf), retBuf, &szBase );
	PyW32_END_ALLOW_THREADS
	if (rc==0)
		return ReturnAPIError("SearchPath");
	return Py_BuildValue("si", retBuf, (szBase-retBuf) );
	// @rdesc The return value is a tuple of (string, int).  string is the full
	// path name located.  int is the offset in the string of the base name
	// of the file.
}
// @pymethod |win32api|SendMessage|Send a message to a window.
PyObject *PySendMessage(PyObject *self, PyObject *args)
{
	HWND hwnd;
	int message;
	int wParam=0;
	int lParam=0;
	if (!PyArg_ParseTuple(args, "ii|ii:SendMessage",
	          &hwnd,    // @pyparm int|hwnd||The hWnd of the window to receive the message.
		      &message, // @pyparm int|idMessage||The ID of the message to send.
	          &wParam,  // @pyparm int|wParam||The wParam for the message
	          &lParam)) // @pyparm int|lParam||The lParam for the message

		return NULL;
	int rc;
	// @pyseeapi SendMessage
	PyW32_BEGIN_ALLOW_THREADS
	rc = ::SendMessage(hwnd, message, wParam, lParam);
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("i",rc);
}

// @pymethod |win32api|SetConsoleTitle|Sets the title for the current console.
static PyObject *
PySetConsoleTitle( PyObject *self, PyObject *args )
{
	char *title;
	// @pyparm string|title||The new title
	if (!PyArg_ParseTuple(args, "s:SetConsoleTitle", &title))
		return NULL;
	// @pyseeapi SetConsoleTitle
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::SetConsoleTitle(title);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("SetConsoleTitle");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|SetCursorPos|The SetCursorPos function moves the cursor to the specified screen coordinates.
static PyObject *
PySetCursorPos( PyObject *self, PyObject *args )
{
	int x,y;
	// @pyparm (int, int)|x,y||The new position.
	if (!PyArg_ParseTuple(args, "(ii):SetCursorPos", &x, &y))
		return NULL;
	// @pyseeapi SetCursorPos
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::SetCursorPos(x,y);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("SetCursorPos");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod int|win32api|SetErrorMode|Controls whether the system will handle the specified types of serious errors, or whether the process will handle them.
static PyObject *
PySetErrorMode( PyObject *self, PyObject *args )
{
	int mode;
	// @pyparm int|errorMode||A set of bit flags that specify the process error mode
	if (!PyArg_ParseTuple(args, "i:SetErrorMode", &mode))
		return NULL;
	// @pyseeapi SetErrorMode
	PyW32_BEGIN_ALLOW_THREADS
	UINT ret = ::SetErrorMode(mode);
	PyW32_END_ALLOW_THREADS
	// @rdesc The result is an integer containing the old error flags.
	return PyInt_FromLong(ret);
}

// @pymethod int|win32api|ShowCursor|The ShowCursor method displays or hides the cursor. 
static PyObject *
PyShowCursor( PyObject *self, PyObject *args )
{
	BOOL bShow;
	// @pyparm int|show||Visiblilty flag
	if (!PyArg_ParseTuple(args, "i:ShowCursor", &bShow))
		return NULL;
	// @pyseeapi ShowCursor
	PyW32_BEGIN_ALLOW_THREADS
	int rc = ::ShowCursor(bShow);
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("i", rc);
	// @rdesc The return value specifies the new display counter
	// @comm This function sets an internal display counter that 
	// determines whether the cursor should be displayed. The 
	// cursor is displayed only if the display count is greater 
	// than or equal to 0. If a mouse is installed, the initial display 
	// count is 0. If no mouse is installed, the display count is -1. 
}


// @pymethod int|win32api|ShellExecute|Opens or prints a file.
static PyObject *
PyShellExecute( PyObject *self, PyObject *args )
{
	HWND hwnd;
	char *op, *file, *params, *dir;
	int show;
	if (!PyArg_ParseTuple(args, "izszzi:ShellExecute", 
		      &hwnd, // @pyparm int|hwnd||The handle of the parent window, or 0 for no parent.  This window receives any message boxes an application produces (for example, for error reporting).
		      &op,   // @pyparm string|op||The operation to perform.  May be "open", "print", or None, which defaults to "open".
		      &file, // @pyparm string|file||The name of the file to open.
		      &params,// @pyparm string|params||The parameters to pass, if the file name contains an executable.  Should be None for a document file.
		      &dir,  // @pyparm string|dir||The initial directory for the application.
		      &show))// @pyparm int|bShow||Specifies whether the application is shown when it is opened. If the lpszFile parameter specifies a document file, this parameter is zero.
		return NULL;
	if (dir==NULL)
		dir="";
	PyW32_BEGIN_ALLOW_THREADS
	HINSTANCE rc=::ShellExecute(hwnd, op, file, params, dir, show);
	PyW32_END_ALLOW_THREADS
	// @pyseeapi ShellExecute
	if ((rc) <= (HINSTANCE)32) {
		return ReturnAPIError("ShellExecute", (int)rc );
	}
	return Py_BuildValue("i", rc );
	// @rdesc The instance handle of the application that was run. (This handle could also be the handle of a dynamic data exchange [DDE] server application.)
	// If there is an error, the method raises an exception.
}
// @pymethod int|win32api|Sleep|Suspends execution of the current thread for the specified time.
static PyObject *
PySleep (PyObject *self, PyObject *args)
{
	BOOL bAlertable = FALSE;
	int time;
	// @pyparm int|time||The number of milli-seconds to sleep for,
	// @pyparm int|bAlterable|0|Specifies whether the function may terminate early due to an I/O completion callback function.
	if (!PyArg_ParseTuple (args, "i|i:Sleep", &time, &bAlertable))
		return NULL;
	DWORD rc;
	PyW32_BEGIN_ALLOW_THREADS
	// @pyseeapi Sleep
	// @pyseeapi SleepEx
	rc = ::SleepEx(time, bAlertable);
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("i", rc );
	// @rdesc The return value is zero if the specified time interval expired. 
}
// @pymethod |win32api|WinExec|Runs the specified application.
static PyObject *
PyWinExec( PyObject *self, PyObject *args )
{
	char *cmd;
	int style = SW_SHOWNORMAL;
	// @pyparm string|cmdLine||The command line to execute.
	// @pyparm int|show|win32con.SW_SHOWNORMAL|The initial state of the applications window.
	if (!PyArg_ParseTuple(args, "s|i:WinExec", &cmd, &style))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	int rc=::WinExec(cmd, style);
	PyW32_END_ALLOW_THREADS
	if ((rc)<=32) // @pyseeapi WinExec
		return ReturnAPIError("WinExec", rc );
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|WinHelp|Invokes the Windows Help system.
static PyObject *
PyWinHelp( PyObject *self, PyObject *args )
{
	HWND hwnd;
	char *hlpFile;
	UINT cmd;
	PyObject *dataOb = Py_None;
	DWORD data;
	if (!PyArg_ParseTuple(args, "isi|O:WinHelp",
		      &hwnd,   // @pyparm int|hwnd||The handle of the window requesting help.
			  &hlpFile,// @pyparm string|hlpFile||The name of the help file.
			  &cmd,    // @pyparm int|cmd||The type of help.  See the api for full details.
			  &dataOb))   // @pyparm int/string|data|0|Additional data specific to the help call.
		return NULL;
	if (dataOb==Py_None)
		data = 0;
	else if (PyString_Check(dataOb))
		data = (DWORD)PyString_AsString(dataOb);
	else if (PyInt_Check(dataOb))
		data = (DWORD)PyInt_AsLong(dataOb);
	else {
		PyErr_SetString(PyExc_TypeError, "4th argument must be a None, string or an integer.");
		return NULL;
	}
		
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = ::WinHelp(hwnd, hlpFile, cmd, data);
	PyW32_END_ALLOW_THREADS
	if (!ok) // @pyseeapi WinHelp
		return ReturnAPIError("WinHelp");
	Py_INCREF(Py_None);
	return Py_None;
	// @rdesc The method raises an exception if an error occurs.
}


// @pymethod |win32api|WriteProfileVal|Writes a value to a Windows INI file.
static PyObject *
PyWriteProfileVal(PyObject *self, PyObject *args)
{
	char *sect, *entry, *strVal, *iniFile=NULL;
	int intVal;
	BOOL bHaveInt = TRUE;

	if (!PyArg_ParseTuple(args, "ssi|s:WriteProfileVal", 
	          &sect,  // @pyparm string|section||The section in the INI file to write to.
	          &entry, // @pyparm string|entry||The entry within the section in the INI file to write to.
	          &intVal, // @pyparm int/string|value||The value to write.
	          &iniFile)) { // @pyparm string|iniName|None|The name of the INI file.  If None, the system INI file is used.
		bHaveInt = FALSE;
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, "ssz|s", &sect, &entry, &strVal, &iniFile)) {
			PyErr_Clear();
			PyErr_SetString(PyExc_TypeError, "WriteProfileVal must have format (ssi|s) or (ssz|s)");
			return NULL;
		}
	}
	BOOL rc;
	char intBuf[20];
	if (bHaveInt) {
		itoa( intVal, intBuf, 10 );
		strVal = intBuf;
	}

	// @pyseeapi WritePrivateProfileString
	// @pyseeapi WriteProfileString
	PyW32_BEGIN_ALLOW_THREADS
	if (iniFile)
		rc = ::WritePrivateProfileString( sect, entry, strVal, iniFile );
	else
		rc = ::WriteProfileString( sect, entry, strVal );
	PyW32_END_ALLOW_THREADS

	if (!rc)
		return ReturnAPIError("Write[Private]ProfileString");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod int|win32api|MessageBeep|Plays a predefined waveform sound.
static PyObject *
PyMessageBeep(PyObject *self, PyObject *args)
{
	// @comm The waveform sound for each sound type is identified by an entry in the [sounds] section of the registry.
	int val = MB_OK;

	if (!PyArg_ParseTuple(args, "|i:MessageBeep", 
	          &val)) // @pyparm int|type|win32con.MB_OK|Specifies the sound type, as
	          // identified by an entry in the [sounds] section of the
	          // registry. This parameter can be one of MB_ICONASTERISK,
	          // MB_ICONEXCLAMATION, MB_ICONHAND, MB_ICONQUESTION or MB_OK.
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = MessageBeep(val);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("MessageBeep");
	Py_INCREF(Py_None);
	return Py_None;
}
// @pymethod int|win32api|MessageBox|Display a message box.
static PyObject *
PyMessageBox(PyObject * self, PyObject * args)
{
  char *message;
  long style = MB_OK;
  const char *title = NULL;
  HWND hwnd = NULL;
  WORD langId = MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT);
  // @pyparm int|hwnd||The handle of the parent window.  See the comments section.
  // @pyparm string|message||The message to be displayed in the message box.
  // @pyparm string/None|title||The title for the message box.  If None, the applications title will be used.
  // @pyparm int|style|win32con.MB_OK|The style of the message box.
  // @pyparm int|language|win32api.MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT)|The language ID to use.

  // @comm Normally, a program in a GUI environment will use one of the MessageBox
  // methods supplied by the GUI (eg, <om win32ui.MessageBox> or <om PyCWnd.MessageBox>)
  if (!PyArg_ParseTuple(args, "is|zli:MessageBox(Ex)", &hwnd, &message, &title, &style, &langId))
    return NULL;
  PyW32_BEGIN_ALLOW_THREADS
  int rc = ::MessageBoxEx(hwnd, message, title, style, langId);
  PyW32_END_ALLOW_THREADS
  return Py_BuildValue("i",rc);
  // @rdesc An integer identifying the button pressed to dismiss the dialog.
}


// @pymethod int|win32api|SetFileAttributes|Sets the named file's attributes.
static PyObject *
PySetFileAttributes(PyObject * self, PyObject * args)
{
	char *pathName;
	int attrs;
	// @pyparm string|pathName||The name of the file.
	// @pyparm int|attrs||The attributes to set.  Must be a combination of the win32con.FILE_ATTRIBUTE_* constants.
	if (!PyArg_ParseTuple(args, "si:SetFileAttributes", &pathName, &attrs))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = SetFileAttributes(pathName, attrs);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("SetFileAttributes");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod int|win32api|GetWindowLong|Retrieves a long value at the specified offset into the extra window memory of the given window.
static PyObject *
PyGetWindowLong(PyObject * self, PyObject * args)
{
	int hwnd;
	int offset;
	// @pyparm int|hwnd||The handle to the window.
	// @pyparm int|offset||Specifies the zero-based byte offset of the value to change. Valid values are in the range zero through the number of bytes of extra window memory, minus four (for example, if 12 or more bytes of extra memory were specified, a value of 8 would be an index to the third long integer), or one of the GWL_ constants.
	if (!PyArg_ParseTuple(args, "ii:GetWindowLong", &hwnd, &offset))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	long rc = ::GetWindowLong( (HWND)hwnd, offset );
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("l", rc);
}

// @pymethod int|win32api|SetWindowLong|Places a long value at the specified offset into the extra window memory of the given window.
static PyObject *
PySetWindowLong(PyObject * self, PyObject * args)
{
	int hwnd;
	int offset;
	long newVal;
	// @pyparm int|hwnd||The handle to the window.
	// @pyparm int|offset||Specifies the zero-based byte offset of the value to change. Valid values are in the range zero through the number of bytes of extra window memory, minus four (for example, if 12 or more bytes of extra memory were specified, a value of 8 would be an index to the third long integer), or one of the GWL_ constants.
	// @pyparm int|val||Specifies the long value to place in the window's reserved memory.
	if (!PyArg_ParseTuple(args, "iil:SetWindowLong", &hwnd, &offset, &newVal))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	long rc = ::SetWindowLong( (HWND)hwnd, offset, newVal ) ;
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("l", rc);
}
// @pymethod int|win32api|SetWindowWord|
static PyObject *
PySetWindowWord(PyObject * self, PyObject * args)
{
	int hwnd;
	int offset;
	WORD newVal;
	// @pyparm int|hwnd||The handle to the window.
	// @pyparm int|offset||Specifies the zero-based byte offset of the value to change. Valid values are in the range zero through the number of bytes of extra window memory, minus four (for example, if 12 or more bytes of extra memory were specified, a value of 8 would be an index to the third long integer), or one of the GWL_ constants.
	// @pyparm int|val||Specifies the long value to place in the window's reserved memory.
	if (!PyArg_ParseTuple(args, "iii:SetWindowWord", &hwnd, &offset, (int *)(&newVal)))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	long rc = ::SetWindowWord( (HWND)hwnd, offset, newVal );
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("l", rc);
}

// @pymethod int|win32api|SetClassLong|Replaces the specified 32-bit (long) value at the specified offset into the extra class memory for the window.
static PyObject *
PySetClassLong(PyObject * self, PyObject * args)
{
	int hwnd;
	int offset;
	long newVal;
	// @pyparm int|hwnd||The handle to the window.
	// @pyparm int|offset||Specifies the zero-based byte offset of the value to change. Valid values are in the range zero through the number of bytes of extra window memory, minus four (for example, if 12 or more bytes of extra memory were specified, a value of 8 would be an index to the third long integer), or one of the GWL_ constants.
	// @pyparm int|val||Specifies the long value to place in the window's reserved memory.
	if (!PyArg_ParseTuple(args, "iil:SetClassLong", &hwnd, &offset, &newVal))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	long rc = ::SetClassLong( (HWND)hwnd, offset, newVal );
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("l", rc);
}
// @pymethod int|win32api|SetClassWord|
static PyObject *
PySetClassWord(PyObject * self, PyObject * args)
{
	int hwnd;
	int offset;
	WORD newVal;
	// @pyparm int|hwnd||The handle to the window.
	// @pyparm int|offset||Specifies the zero-based byte offset of the value to change. Valid values are in the range zero through the number of bytes of extra window memory, minus four (for example, if 12 or more bytes of extra memory were specified, a value of 8 would be an index to the third long integer), or one of the GWL_ constants.
	// @pyparm int|val||Specifies the long value to place in the window's reserved memory.
	if (!PyArg_ParseTuple(args, "iii:SetClassWord", &hwnd, &offset, (int *)(&newVal)))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	long rc = ::SetClassWord( (HWND)hwnd, offset, newVal );
	PyW32_END_ALLOW_THREADS
	return Py_BuildValue("l", rc);
}

/***** SOME "MACROS" ******/
// @pymethod int|win32api|MAKELANGID|Creates a language identifier from a primary language identifier and a sublanguage identifier.
static PyObject *
PyMAKELANGID(PyObject *self, PyObject *args)
{
	int lang, sublang;
	if (!PyArg_ParseTuple(args, "ii:MAKELANGID", 
	          &lang,  // @pyparm int|PrimaryLanguage||Primary language identifier

	          &sublang)) // @pyparm int|SubLanguage||The sublanguage identifier
		return NULL;
	return Py_BuildValue("i", (int)MAKELANGID(lang, sublang));
	// @comm This is simply a wrapper to a C++ macro.
}

// @pymethod int|win32api|HIWORD|An interface to the win32api HIWORD macro.
static PyObject *
PyHIWORD(PyObject *self, PyObject *args)
{
	int val;

	if (!PyArg_ParseTuple(args, "i:HIWORD", 
	          &val)) // @pyparm int|val||The value to retrieve the HIWORD from.
		return NULL;
	return Py_BuildValue("i", (int)HIWORD(val));
	// @comm This is simply a wrapper to a C++ macro.
}
// @pymethod int|win32api|LOWORD|An interface to the win32api LOWORD macro.
static PyObject *
PyLOWORD(PyObject *self, PyObject *args)
{
	int val;

	if (!PyArg_ParseTuple(args, "i:LOWORD", 
	          &val)) // @pyparm int|val||The value to retrieve the LOWORD from.
		return NULL;
	return Py_BuildValue("i", (int)LOWORD(val));
	// @comm This is simply a wrapper to a C++ macro.
}
// @pymethod int|win32api|HIBYTE|An interface to the win32api HIBYTE macro.
static PyObject *
PyHIBYTE(PyObject *self, PyObject *args)
{
	int val;

	if (!PyArg_ParseTuple(args, "i:HIBYTE", 
	          &val)) // @pyparm int|val||The value to retrieve the HIBYTE from.
		return NULL;
	return Py_BuildValue("i", (int)HIBYTE(val));
	// @comm This is simply a wrapper to a C++ macro.
}
// @pymethod int|win32api|LOBYTE|An interface to the win32api LOBYTE macro.
static PyObject *
PyLOBYTE(PyObject *self, PyObject *args)
{
	int val;

	if (!PyArg_ParseTuple(args, "i:LOBYTE", 
	          &val)) // @pyparm int|val||The value to retrieve the LOBYTE from.
		return NULL;
	return Py_BuildValue("i", (int)LOBYTE(val));
	// @comm This is simply a wrapper to a C++ macro.
}

// @pymethod int|win32api|RGB|An interface to the win32api RGB macro.
static PyObject *
PyRGB(PyObject *self, PyObject *args)
{
	int r,g,b;
	// @pyparm int|red||The red value
	// @pyparm int|green||The green value
	// @pyparm int|blue||The blue value
	if (!PyArg_ParseTuple(args, "iii:RGB", 
	          &r, &g, &b)) 
		return NULL;
	return Py_BuildValue("i", (int)RGB(r,g,b));
	// @comm This is simply a wrapper to a C++ macro.
}

// @pymethod tuple|win32api|GetSystemTime|Returns the current system time
static PyObject *
PyGetSystemTime (PyObject * self, PyObject * args)
{
  SYSTEMTIME t;
  if (!PyArg_ParseTuple (args, "")) {
 return NULL;
  } else {
 // GetSystemTime is a void function
 PyW32_BEGIN_ALLOW_THREADS
 GetSystemTime(&t);
 PyW32_END_ALLOW_THREADS;
 return Py_BuildValue ("(iiiiiiii)",
        t.wYear,
        t.wMonth,
        t.wDayOfWeek,
        t.wDay,
        t.wHour,
        t.wMinute,
        t.wSecond,
        t.wMilliseconds
        );
  }
}						  

// @pymethod int|win32api|SetSystemTime|Returns the current system time
static PyObject *
PySetSystemTime (PyObject * self, PyObject * args)
{
  SYSTEMTIME t;
  int result;

  if (!PyArg_ParseTuple (args,
       "hhhhhhhh",
       &t.wYear,   	// @pyparm int|year||
       &t.wMonth, // @pyparm int|month||
       &t.wDayOfWeek, // @pyparm int|dayOfWeek||
       &t.wDay, // @pyparm int|day||
       &t.wHour,// @pyparm int|hour||
       &t.wMinute,// @pyparm int|minute||
       &t.wSecond,// @pyparm int|second||
       &t.wMilliseconds // @pyparm int|millseconds||
       ))
 return NULL;
 PyW32_BEGIN_ALLOW_THREADS
 result = ::SetSystemTime (&t);
 PyW32_END_ALLOW_THREADS

 if (! result ) {
 return ReturnAPIError ("SetSystemTime");
  } else {
 return Py_BuildValue ("i", result);
  }
}
// @pymethod |win32api|OutputDebugString|Sends a string to the Windows debugging device.
static PyObject *
PyOutputDebugString(PyObject *self, PyObject *args)
{
	char *msg;
	// @pyparm string|msg||The string to write.
	if (!PyArg_ParseTuple(args, "s:OutputDebugString", &msg))
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	OutputDebugString(msg);
	PyW32_END_ALLOW_THREADS;
	Py_INCREF(Py_None);
	return Py_None;
}

// Process stuff

// @pymethod <o PyHANDLE>|win32api|OpenProcess|Retrieves a handle to an existing process
static PyObject *PyOpenProcess(PyObject *self, PyObject *args)
{
	DWORD pid, reqdAccess;
	BOOL inherit;
	if (!PyArg_ParseTuple(args, "iii:OpenProcess",
			&reqdAccess, // @pyparm int|reqdAccess||The required access.
			&inherit,    // @pyparm int|bInherit||Specifies whether the returned handle can be inherited by a new process created by the current process. If TRUE, the handle is inheritable.
	        &pid)) // @pyparm int|pid||The process ID
		return NULL;
	PyW32_BEGIN_ALLOW_THREADS
	HANDLE handle = OpenProcess(reqdAccess, inherit, pid);
	PyW32_END_ALLOW_THREADS;
	if (handle==NULL)
		return ReturnAPIError("OpenProcess");
	return PyWinObject_FromHANDLE(handle);
}

// @pymethod |win32api|TerminateProcess|Kills a process
static PyObject *PyTerminateProcess(PyObject *self, PyObject *args)
{
	PyObject *obHandle;
	HANDLE handle;
	UINT exitCode;
	if (!PyArg_ParseTuple(args, "Oi:TerminateProcess",
			&obHandle, // @pyparm <o PyHANDLE>|handle||The handle of the process to terminate.
	        &exitCode)) // @pyparm int|exitCode||The exit code for the process.
		return NULL;
	if (!PyWinObject_AsHANDLE(obHandle, &handle))
		return NULL;
	// @comm See also <om win32api.OpenProcess>
	PyW32_BEGIN_ALLOW_THREADS
	BOOL ok = TerminateProcess(handle, exitCode);
	PyW32_END_ALLOW_THREADS
	if (!ok)
		return ReturnAPIError("TerminateProcess");
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod <o PyUnicode>|win32api|LoadString|Loads a string from a resource file.
static PyObject * PyLoadString(PyObject *self, PyObject *args)
{
	HMODULE hModule;
	int numChars = 1024;
	UINT stringId;
	if ( !PyArg_ParseTuple(args, "ii|i",
						   &hModule, // @pyparm int|handle||The handle of the module containing the resource.
						   &stringId, // @pyparm int|stringId||The ID of the string to load.
						   &numChars)) // @pyparm int|numChars|1024|Number of characters to allocate for the return buffer.
		return NULL;
	UINT numBytes = sizeof(WCHAR) * numChars;
	WCHAR *buffer = (WCHAR *)malloc(numBytes);
	if (buffer==NULL) {
		PyErr_SetString(PyExc_MemoryError, "Allocating buffer for LoadString");
		return NULL;
	}
	int gotBytes = LoadStringW(hModule, stringId, buffer, numBytes);
	PyObject *rc;
	if (gotBytes==0)
		rc = ReturnAPIError("LoadString");
	else
		rc = PyWinObject_FromWCHAR(buffer, gotBytes);
	free(buffer);
	return rc;
}


// @pymethod string|win32api|LoadResource|Finds and loads a resource from a PE file.
static PyObject * PyLoadResource(PyObject *self, PyObject *args)
{
	HMODULE hModule;
	PyObject *obType;
	PyObject *obName;
	WORD wLanguage = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

	if ( !PyArg_ParseTuple(args, "iOO|i",
						   &hModule, // @pyparm int|handle||The handle of the module containing the resource.
						   &obType, // @pyparm object|type||The type of resource to load.
						   &obName, // @pyparm object|name||The name of the resource to load.
						   &wLanguage // @pyparm int|language|NEUTRAL|Language to use, defaults to LANG_NEUTRAL.
		) )
		return NULL;


	BOOL bFreeType = FALSE, bFreeName = FALSE;
	LPTSTR lpType;
	if ( PyInt_Check(obType) )
		lpType = MAKEINTRESOURCE(PyInt_AS_LONG((PyIntObject *)obType));
	else if (PyWinObject_AsTCHAR(obType, &lpType))
		bFreeType = TRUE;
	else
		return ReturnError("Bad type for resource type.", "LoadResource");

	LPTSTR lpName;
	if ( PyInt_Check(obName) )
		lpName = MAKEINTRESOURCE(PyInt_AS_LONG((PyIntObject *)obName));
	else if (PyWinObject_AsTCHAR(obName, &lpName))
		bFreeName = TRUE;
	else {
		if (bFreeType) PyWinObject_FreeTCHAR(lpType);
		return ReturnError("Bad type for resource name.", "LoadResource");
	}

	HRSRC hrsrc = FindResourceEx(hModule, lpType, lpName, wLanguage);
	if (bFreeType) PyWinObject_FreeTCHAR(lpType);
	if (bFreeName) PyWinObject_FreeTCHAR(lpName);
	if ( hrsrc == NULL )
		return ReturnAPIError("LoadResource");

	DWORD size = SizeofResource(hModule, hrsrc);
	if ( size == 0 )
		return ReturnAPIError("LoadResource");

	HGLOBAL hglob = LoadResource(hModule, hrsrc);
	if ( hglob == NULL )
		return ReturnAPIError("LoadResource");

	LPVOID p = LockResource(hglob);
	if ( p == NULL )
		return ReturnAPIError("LoadResource");

	return PyString_FromStringAndSize((char *)p, size);
}

// @pymethod |win32api|BeginUpdateResource|Begins an update cycle for a PE file.
static PyObject * PyBeginUpdateResource(PyObject *self, PyObject *args)
{
	const char *szFileName;
	int bDeleteExistingResources;

	if ( !PyArg_ParseTuple(args, "si",
						   &szFileName, // @pyparm string|filename||File in which to update resources.
						   &bDeleteExistingResources // @pyparm int|delete||Flag to indicate that all existing resources should be deleted.
		) )
		return NULL;

	HANDLE h = BeginUpdateResource(szFileName, bDeleteExistingResources);
	if ( h == NULL )
		return ReturnAPIError("BeginUpdateResource");

	return PyInt_FromLong((int)h);
}

// @pymethod |win32api|UpdateResource|Updates a resource in a PE file.
static PyObject * PyUpdateResource(PyObject *self, PyObject *args)
{
	HMODULE hUpdate;
	PyObject *obType;
	PyObject *obName;
	LPVOID lpData;
	DWORD cbData;
	WORD wLanguage = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

	if ( !PyArg_ParseTuple(args, "iOOs#|i",
						   &hUpdate, // @pyparm int|handle||The update-file handle.
						   &obType, // @pyparm object|type||The type of resource to load.
						   &obName, // @pyparm object|name||The name of the resource to load.
						   &lpData, // @pyparm string|data||The data to place into the resource.
						   &cbData,
						   &wLanguage // @pyparm int|language|NEUTRAL|Language to use, defaults to LANG_NEUTRAL.
		) )
		return NULL;

	BOOL bFreeType = FALSE, bFreeName = FALSE;
	LPWSTR lpType;
	if ( PyInt_Check(obType) )
		lpType = MAKEINTRESOURCEW(PyInt_AS_LONG((PyIntObject *)obType));
	else if (PyWinObject_AsBstr(obType, &lpType) )
		bFreeType = TRUE;
	else
		return ReturnError("Bad type for resource type.", "UpdateResource");

	LPWSTR lpName;
	if ( PyInt_Check(obName) )
		lpName = MAKEINTRESOURCEW(PyInt_AS_LONG((PyIntObject *)obName));
	else if ( PyWinObject_AsBstr(obName, &lpName) )
		bFreeName = TRUE;
	else {
		if (bFreeType) PyWinObject_FreeBstr(lpType);
		return ReturnError("Bad type for resource name.", "UpdateResource");
	}

	BOOL ok = UpdateResourceW(hUpdate, lpType, lpName, wLanguage, lpData, cbData);
	if (bFreeType) PyWinObject_FreeBstr(lpType);
	if (bFreeName) PyWinObject_FreeBstr(lpName);
	if ( !ok )
		return ReturnAPIError("UpdateResource");

	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |win32api|EndUpdateResource|Ends a resource update cycle of a PE file.
static PyObject * PyEndUpdateResource(PyObject *self, PyObject *args)
{
	HMODULE hUpdate;
	int fDiscard;

	if ( !PyArg_ParseTuple(args, "ii",
						   &hUpdate, // @pyparm int|handle||The update-file handle.
						   &fDiscard // @pyparm int|discard||Flag to discard all writes.
		) )
		return NULL;

	if ( !EndUpdateResource(hUpdate, fDiscard) )
		return ReturnAPIError("EndUpdateResource");

	Py_INCREF(Py_None);
	return Py_None;
}

BOOL CALLBACK EnumResProc(HMODULE module, LPCSTR type, LPSTR name, PyObject
*param)
{
	PyObject *pyname;
	if (HIWORD(name) == 0)
	{
		pyname = PyInt_FromLong(reinterpret_cast<long>(name));
	}
	else if (name[0] == '#')
	{
		pyname = PyInt_FromLong(_ttoi(name + 1));
	}
	else
	{
		pyname = PyString_FromString(name);
	}
	PyList_Append(param, pyname);
	return TRUE;
}

// @pymethod [string, ...]|win32api|EnumResourceNames|Enumerates all the resources of the specified type from the nominated file.
PyObject *PyEnumResourceNames(PyObject *, PyObject *args)
{
	HMODULE hmodule;
	LPCSTR restype;
	char buf[20];
	// NOTE:  MH can't make the string version of the param
	// return anything useful, so I undocumented its use!
	// pyparm int|hmodule||The handle to the module to enumerate.
	// pyparm string|resType||The type of resource to enumerate as a string (eg, 'RT_DIALOG')
	if (!PyArg_ParseTuple(args, "is", &hmodule, &restype))
	{
		PyErr_Clear();
		int restypeint;
		// @pyparm int|hmodule||The handle to the module to enumerate.
		// @pyparm int|resType||The type of resource to enumerate as an integer (eg, win32con.RT_DIALOG)
		if (!PyArg_ParseTuple(args, "ii", &hmodule, &restypeint))
		{
			return NULL;
		}
		sprintf(buf, "#%d", restypeint);
		restype = buf;
	}
	// @rdesc The result is a list of string or integers, one for each resource enumerated.
	PyObject *result = PyList_New(0);
	EnumResourceNames(
		hmodule,
		restype,
		reinterpret_cast<ENUMRESNAMEPROC>(EnumResProc),
		reinterpret_cast<LONG>(result));

	return result;
}


// @pymethod <o PyUnicode>|win32api|Unicode|Creates a new Unicode object
PYWINTYPES_EXPORT PyObject *PyWin_NewUnicode(PyObject *self, PyObject *args);

/****** Now uses pywintypes version!
static PyObject *Py_Unicode(PyObject *self, PyObject *args)
{
	PyObject *obString;
	// @pyparm string|str||The string to convert.
	if (!PyArg_ParseTuple(args, "O", &obString))
		return NULL;

	PyUnicode *result = new PyUnicode(obString);
	if ( result->m_bstrValue )
		return result;
	Py_DECREF(result);
	// an error should have been raised 
	return NULL;
}
*****/
///////////////////
//
// Win32 Exception Handler.
//
// A recursive routine called by the exception handler!
// (I hope this doesnt wind too far on a stack overflow :-)
// Limited testing indicates it doesnt, and this can handle
// a stack overflow fine.
PyObject *MakeExceptionRecord( PEXCEPTION_RECORD pExceptionRecord )
{
	if (pExceptionRecord==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	} else {
		PyObject *obExcRecord = MakeExceptionRecord(pExceptionRecord->ExceptionRecord);
		PyObject *ret = Py_BuildValue("llOlO", 
			pExceptionRecord->ExceptionCode,
			pExceptionRecord->ExceptionFlags,
			obExcRecord,
			pExceptionRecord->ExceptionAddress,
			Py_None);
		Py_XDECREF(obExcRecord);
		return ret;
	}
}
int PyApplyExceptionFilter( 
	DWORD ExceptionCode,
	PEXCEPTION_POINTERS ExceptionInfo,
	PyObject *obHandler,
	PyObject **ppExcType,
	PyObject **ppExcValue)
{

	PyThreadState *stateSave = PyThreadState_Swap(NULL);
	PyThreadState_Swap(stateSave);
	BOOL createdThreadState = FALSE;
	BOOL acquiredThreadLock = FALSE;
	if (stateSave==NULL) { // Need to create one!
		// Copied from CEnterLeavePython!
		createdThreadState = PyWinThreadState_Ensure();
#ifdef PYCOM_USE_FREE_THREAD
#error Please revisit the thread requirements here!
#endif
		acquiredThreadLock = TRUE;
		PyWinInterpreterLock_Acquire();
	}

	PyObject *obRecord = MakeExceptionRecord(ExceptionInfo->ExceptionRecord);
	PyObject *args = Py_BuildValue("i(OO)", ExceptionCode, obRecord, Py_None);
	Py_XDECREF(obRecord);
	PyObject *obRet = PyObject_CallObject(obHandler, args);
	Py_DECREF(args);
	int ret = EXCEPTION_CONTINUE_SEARCH;
	if (obRet) {
		// Simple integer return code
		if (PyInt_Check(obRet)) {
			ret = PyInt_AsLong(obRet);
		// Exception instance to be raised.
		} else if (PyInstance_Check(obRet)) {
			*ppExcType = obRet;
			Py_INCREF(obRet);
			*ppExcValue = NULL;
			ret = EXCEPTION_EXECUTE_HANDLER;
		}
		// (exc_type, exc_value) to be raised.
		// Sequence checking MUST COME LAST!
		else if (PySequence_Check(obRet)) {
			*ppExcType = PySequence_GetItem(obRet, 0);
			*ppExcValue = PySequence_GetItem(obRet, 1);
			ret = EXCEPTION_EXECUTE_HANDLER;
		// else default == not handled.
		}
	}
	Py_XDECREF(obRet);

	if (acquiredThreadLock)
		PyWinInterpreterLock_Release();

	if ( createdThreadState )
		PyWinThreadState_Free();

	return ret;
}

// @pymethod object|win32api|Apply|Calls a Python function, but traps Win32 exceptions.
static PyObject *PyApply(PyObject *self, PyObject *args)
{
	PyObject *ret, *obHandler, *obFunc, *obArgs;
	PyObject *exc_type = NULL, *exc_value = NULL;
	if (!PyArg_ParseTuple(args, "OOO", 
		&obHandler, // @pyparm object|exceptionHandler||An object which will be called when a win32 exception occurs.
		&obFunc, // @pyparm object|func||The function call call under the protection of the Win32 exception handler.
		&obArgs)) // @pyparm tuple|args||Args for the function.
		return NULL;

	if (!PyCallable_Check(obHandler)) {
		PyErr_SetString(PyExc_TypeError, "First argument must be an exception handler which accepts 2 arguments.");
		return NULL;
	}
	PyThreadState *stateSave = PyThreadState_Swap(NULL);
	PyThreadState_Swap(stateSave);
	_try {
		ret = PyObject_CallObject(obFunc, obArgs);
	}
	_except( PyApplyExceptionFilter( GetExceptionCode(),
	                                 GetExceptionInformation(),
	                                 obHandler,
									 &exc_type,
									 &exc_value) ) {
		// Do my best to restore the thread state to a sane spot.
		PyThreadState *stateCur = PyThreadState_Swap(NULL);
		if (stateCur == NULL) stateCur = stateSave;
		PyThreadState_Swap(stateCur);
		if (PyInstance_Check(exc_type)) {
			if (exc_value != NULL)
				PyErr_SetString(PyExc_TypeError, "instance exception returned from exception handler may not have a separate value");
			else {
				// Normalize to class, instance
				exc_value = exc_type;
				exc_type = (PyObject*) ((PyInstanceObject*)exc_type)->in_class;
				Py_INCREF(exc_type);
				PyErr_SetObject(exc_type, exc_value);
			}
		} else if (exc_type==NULL || exc_value==NULL)
			PyErr_SetString(PyExc_TypeError, "exception handler must return a valid object which can be raised as an exception (eg (exc_type, exc_value) or exc_class_instance)");
		else
			PyErr_SetObject(exc_type, exc_value);
		Py_XDECREF(exc_type);
		Py_XDECREF(exc_value);
		ret = NULL;
	}
	return ret;
// @comm Calls the specified function in a manner similar to 
// the built-in function apply(), but allows Win32 exceptions
// to be handled by Python.  If a Win32 exception occurs calling
// the function, the specified exceptionHandler is called, and its
// return value determines the action taken.
// @flagh Return value|Description
// @flag Tuple of (exc_type, exc_value)|This exception is raised to the 
// Python caller of Apply() - This is conceptually similar to 
// "raise exc_type, exc_value", although exception handlers must not
// themselves raise exceptions (see below).
// @flag Integer|Must be one of the win32 exception constants, and this
// value is returned to Win32.  See the Win32 documentation for details.
// @flag None|The exception is considered not handled (ie, it is as if no 
// exception handler exists).  If a Python exception occurs in the Win32 
// exception handler, it is as if None were returned (ie, no tracebacks 
// or other diagnostics are printed)
}

/* List of functions exported by this module */
// @module win32api|A module, encapsulating the Windows Win32 API.
static struct PyMethodDef win32api_functions[] = {
	{"AbortSystemShutdown",	PyAbortSystemShutdown,1},     // @pymeth AbortSystemShutdown|Aborts a system shutdown
	{"Apply",               PyApply, 1}, // @pymeth Apply|Calls a Python function, but traps Win32 exceptions.
	{"Beep",				PyBeep,         1},     // @pymeth Beep|Generates a simple tone on the speaker.
	{"BeginUpdateResource", PyBeginUpdateResource, 1 }, // @pymeth BeginUpdateResource|Begins an update cycle for a PE file.
	{"ClipCursor",			PyClipCursor,       1}, // @pymeth ClipCursor|Confines the cursor to a rectangular area on the screen.
	{"CloseHandle",		    PyCloseHandle,     1},  // @pymeth CloseHandle|Closes an open handle.
	{"CopyFile",			PyCopyFile,         1}, // @pymeth CopyFile|Copy a file.
	{"DebugBreak",			PyDebugBreak,       1}, // @pymeth DebugBreak|Breaks into the C debugger.
	{"DeleteFile",			PyDeleteFile,       1}, // @pymeth DeleteFile|Deletes the specified file.
	{"DragQueryFile",		PyDragQueryFile,    1}, // @pymeth DragQueryFile|Retrieve the file names for dropped files.
	{"DragFinish",			PyDragFinish,       1}, // @pymeth DragFinish|Free memory associated with dropped files.
	{"DuplicateHandle",     PyDuplicateHandle,  1}, // @pymeth DuplicateHandle|Duplicates a handle.
	{"EndUpdateResource",   PyEndUpdateResource, 1 }, // @pymeth EndUpdateResource|Ends a resource update cycle of a PE file.
	{"EnumResourceNames",   PyEnumResourceNames, 1 }, // @pymeth EnumResourceNames|Enumerates all the resources of the specified type from the nominated file.
	{"ExpandEnvironmentStrings",PyExpandEnvironmentStrings, 1}, // @pymeth ExpandEnvironmentStrings|Expands environment-variable strings and replaces them with their defined values. 
	{"ExitWindows",         PyExitWindows,      1}, // @pymeth ExitWindows|Logs off the current user
	{"ExitWindowsEx",       PyExitWindowsEx,      1}, // @pymeth ExitWindowsEx|either logs off the current user, shuts down the system, or shuts down and restarts the system.
	{"FindFiles",			PyFindFiles,        1}, // @pymeth FindFiles|Find files matching a file spec.
	{"FindFirstChangeNotification", PyFindFirstChangeNotification, 1}, // @pymeth FindFirstChangeNotification|Creates a change notification handle and sets up initial change notification filter conditions.
	{"FindNextChangeNotification", PyFindNextChangeNotification, 1}, // @pymeth FindNextChangeNotification|Requests that the operating system signal a change notification handle the next time it detects an appropriate change.
	{"FindCloseChangeNotification", PyFindCloseChangeNotification, 1}, // @pymeth FindCloseChangeNotification|Closes the change notification handle.
	{"FindExecutable",		PyFindExecutable,   1}, // @pymeth FindExecutable|Find an executable associated with a document.
	{"FormatMessage",		PyFormatMessage,    1}, // @pymeth FormatMessage|Return an error message string.
	{"FormatMessageW",		PyFormatMessageW,    1}, // @pymeth FormatMessageW|Return an error message string (as a Unicode object).
	{"FreeLibrary",			PyFreeLibrary,1},       // @pymeth FreeLibrary|Decrements the reference count of the loaded dynamic-link library (DLL) module.
	{"GetAsyncKeyState",	PyGetAsyncKeyState,1}, // @pymeth GetAsyncKeyState|Retrieves the asynch state of a virtual key code.
	{"GetCommandLine",		PyGetCommandLine,   1}, // @pymeth GetCommandLine|Return the application's command line.
	{"GetComputerName",     PyGetComputerName,  1}, // @pymeth GetComputerName|Returns the local computer name
	{"GetUserName",         PyGetUserName,  1},     // @pymeth GetUserName|Returns the current user name.
	{"GetCursorPos",		PyGetCursorPos,   1},   // @pymeth GetCursorPos|Returns the position of the cursor, in screen co-ordinates.
	{"GetCurrentThread",    PyGetCurrentThread,   1}, // @pymeth GetCurrentThread|Returns a pseudohandle for the current thread.
	{"GetCurrentThreadId",  PyGetCurrentThreadId,   1}, // @pymeth GetCurrentThreadId|Returns the thread ID for the current thread.
	{"GetCurrentProcessId", PyGetCurrentProcessId,   1}, // @pymeth GetCurrentProcessId|Returns the thread ID for the current thread.
	{"GetCurrentProcess",   PyGetCurrentProcess,   1}, // @pymeth GetCurrentProcess|Returns a pseudohandle for the current process.
	{"GetConsoleTitle",		PyGetConsoleTitle,  1}, // @pymeth GetConsoleTitle|Return the application's console title.
	{"GetDiskFreeSpace",	PyGetDiskFreeSpace, 1}, // @pymeth GetDiskFreeSpace|Retrieves information about a disk.
	{"GetDomainName",		PyGetDomainName, 1}, 	// @pymeth GetDomainName|Returns the current domain name
	{"GetEnvironmentVariable", PyGetEnvironmentVariable, 1}, // @pymeth GetEnvironmentVariable|Retrieves the value of an environment variable.
	{"GetFileAttributes",   PyGetFileAttributes,1}, // @pymeth GetFileAttributes|Retrieves the attributes for the named file.
	{"GetFocus",            PyGetFocus,         1}, // @pymeth GetFocus|Retrieves the handle of the keyboard focus window associated with the thread that called the method. 
	{"GetFullPathName",     PyGetFullPathName,1},   // @pymeth GetFullPathName|Returns the full path of a (possibly relative) path
	{"GetKeyState",			PyGetKeyState,      1}, // @pymeth GetKeyState|Retrives the last known key state for a key.
	{"GetLastError",		PyGetLastError,     1}, // @pymeth GetLastError|Retrieves the last error code known by the system.
	{"GetLogicalDrives",	PyGetLogicalDrives,     1}, // @pymeth GetLogicalDrives|Returns a bitmask representing the currently available disk drives.
	{"GetLogicalDriveStrings",	PyGetLogicalDriveStrings,     1}, // @pymeth GetLogicalDriveStrings|Returns a list of strings for all the drives.
	{"GetModuleFileName",	PyGetModuleFileName,1}, // @pymeth GetModuleFileName|Retrieves the filename of the specified module.
	{"GetModuleHandle",     PyGetModuleHandle,1},   // @pymeth GetModuleHandle|Returns the handle of an already loaded DLL.
	{"GetProfileSection",	PyGetProfileSection,1}, // @pymeth GetProfileSection|Returns a list of entries in an INI file.
	{"GetProcAddress",      PyGetProcAddress,1},    // @pymeth GetProcAddress|Returns the address of the specified exported dynamic-link library (DLL) function.
	{"GetProfileVal",		PyGetProfileVal,    1}, // @pymeth GetProfileVal|Returns a value from an INI file.
	{"GetShortPathName",	PyGetShortPathName, 1}, // @pymeth GetShortPathName|Returns the 8.3 version of a pathname.
	{"GetStdHandle",	PyGetStdHandle,	1}, // @pymeth GetStdHandle|Returns a handle for the standard input, standard output, or standard error device
	{"GetSysColor",			PyGetSysColor,      1}, // @pymeth GetSysColor|Returns the system colors.
	{"GetSystemDefaultLangID",PyGetSystemDefaultLangID,1}, // @pymeth GetSystemDefaultLangID|Retrieves the system default language identifier. 
	{"GetSystemDefaultLCID",PyGetSystemDefaultLCID,1}, // @pymeth GetSystemDefaultLCID|Retrieves the system default locale identifier. 
	{"GetSystemDirectory",	PyGetSystemDirectory,1}, // @pymeth GetSystemDirectory|Returns the Windows system directory.
	{"GetSystemInfo",		PyGetSystemInfo, 1},    // @pymeth GetSystemInfo|Retrieves information about the current system.
	{"GetSystemMetrics",	PyGetSystemMetrics, 1}, // @pymeth GetSystemMetrics|Returns the specified system metrics.
	{"GetSystemTime",		PyGetSystemTime,	1},	// @pymeth GetSystemTime|Returns the current system time.
	{"GetTempFileName",		PyGetTempFileName,  1}, // @pymeth GetTempFileName|Creates a temporary file.
	{"GetTempPath",			PyGetTempPath,      1}, // @pymeth GetTempPath|Returns the path designated as holding temporary files.
	{"GetTickCount",	    PyGetTickCount,      1}, // @pymeth GetTickCount|Returns the milliseconds since windows started.
	{"GetTimeZoneInformation",	PyGetTimeZoneInformation,1}, // @pymeth GetTimeZoneInformation|Returns the system time-zone information.
	{"GetVersion",			PyGetVersion,       1}, // @pymeth GetVersion|Returns Windows version information.
	{"GetVersionEx",		PyGetVersionEx,       1}, // @pymeth GetVersionEx|Returns Windows version information as a tuple.
	{"GetVolumeInformation",PyGetVolumeInformation,1}, // @pymeth GetVolumeInformation|Returns information about a volume and file system attached to the system.
	{"GetWindowsDirectory", PyGetWindowsDirectory,1}, // @pymeth GetWindowsDirectory|Returns the windows directory.
	{"GetWindowLong",       PyGetWindowLong,1}, // @pymeth GetWindowLong|Retrieves a long value at the specified offset into the extra window memory of the given window.
	{"GetUserDefaultLangID",PyGetUserDefaultLangID,1}, // @pymeth GetUserDefaultLangID|Retrieves the user default language identifier. 
	{"GetUserDefaultLCID",  PyGetUserDefaultLCID,1}, // @pymeth GetUserDefaultLCID|Retrieves the user default locale identifier.
	{"InitiateSystemShutdown",  PyInitiateSystemShutdown,1}, // @pymeth InitiateSystemShutdown|Initiates a shutdown and optional restart of the specified computer. 
	{"LoadCursor",          PyLoadCursor, 1}, // @pymeth LoadCursor|Loads a cursor.
	{"LoadLibrary",	        PyLoadLibrary,1}, // @pymeth LoadLibrary|Loads the specified DLL, and returns the handle.
	{"LoadLibraryEx",	    PyLoadLibraryEx,1}, // @pymeth LoadLibraryEx|Loads the specified DLL, and returns the handle.
	{"LoadResource",	    PyLoadResource,1}, // @pymeth LoadResource|Finds and loads a resource from a PE file.
	{"LoadString",	        PyLoadString,1}, // @pymeth LoadString|Loads a string from a resource file.
	{"MessageBeep",         PyMessageBeep,1}, // @pymeth MessageBeep|Plays a predefined waveform sound.
	{"MessageBoxEx",        PyMessageBox, 1},
	{"MessageBox",          PyMessageBox, 1}, // @pymeth MessageBox|Display a message box.
	{"MoveFile",			PyMoveFile,			1}, // @pymeth MoveFile|Moves or renames a file.
	{"MoveFileEx",			PyMoveFileEx,		1}, // @pymeth MoveFileEx|Moves or renames a file.
	{"OpenProcess",         PyOpenProcess, 1}, // @pymeth OpenProcess|Retrieves a handle to an existing process.
	{"OutputDebugString",	PyOutputDebugString, 1 }, // @pymeth OutputDebugString|Writes output to the Windows debugger.
	{"PostMessage",         PyPostMessage, 1}, // @pymeth PostMessage|Post a message to a window.
	{"PostQuitMessage",     PyPostQuitMessage, 1}, // @pymeth PostQuitMessage|Posts a quit message.
	{"PostThreadMessage",   PyPostThreadMessage, 1}, // @pymeth PostThreadMessage|Post a message to a thread.
	{"RegCloseKey",			PyRegCloseKey, 1}, // @pymeth RegCloseKey|Closes a registry key.
	{"RegConnectRegistry",	PyRegConnectRegistry, 1}, // @pymeth RegConnectRegistry|Establishes a connection to a predefined registry handle on another computer.
	{"RegCreateKey",        PyRegCreateKey, 1}, // @pymeth RegCreateKey|Creates the specified key, or opens the key if it already exists.
	{"RegDeleteKey",        PyRegDeleteKey, 1}, // @pymeth RegDeleteKey|Deletes the specified key.
	{"RegDeleteValue",      PyRegDeleteValue, 1}, // @pymeth RegDeleteValue|Removes a named value from the specified registry key.
	{"RegEnumKey",          PyRegEnumKey, 1}, // @pymeth RegEnumKey|Enumerates subkeys of the specified open registry key.
	{"RegEnumValue",        PyRegEnumValue, 1}, // @pymeth RegEnumValue|Enumerates values of the specified open registry key.
	{"RegFlushKey",	        PyRegFlushKey, 1}, // @pymeth RegFlushKey|Writes all the attributes of the specified key to the registry.
	{"RegLoadKey",          PyRegLoadKey, 1}, // @pymeth RegLoadKey|Creates a subkey under HKEY_USER or HKEY_LOCAL_MACHINE and stores registration information from a specified file into that subkey.
	{"RegOpenKey",          PyRegOpenKey, 1}, // @pymeth RegOpenKey|Alias for <om win32api.RegOpenKeyEx>
	{"RegOpenKeyEx",        PyRegOpenKey, 1}, // @pymeth RegOpenKeyEx|Opens the specified key.
	{"RegQueryValue",       PyRegQueryValue, 1}, // @pymeth RegQueryValue|Retrieves the value associated with the unnamed value for a specified key in the registry.
	{"RegQueryValueEx",	PyRegQueryValueEx, 1}, // @pymeth RegQueryValueEx|Retrieves the type and data for a specified value name associated with an open registry key. 
	{"RegQueryInfoKey",	PyRegQueryInfoKey, 1}, // @pymeth RegQueryInfoKey|Returns information about the specified key.
	{"RegSaveKey",          PyRegSaveKey, 1}, // @pymeth RegSaveKey|Saves the specified key, and all its subkeys to the specified file.
	{"RegSetValue",         PyRegSetValue, 1}, // @pymeth RegSetValue|Associates a value with a specified key.  Currently, only strings are supported.
	{"RegSetValueEx",       PyRegSetValueEx, 1}, // @pymeth RegSetValueEx|Stores data in the value field of an open registry key.
	{"RegisterWindowMessage",PyRegisterWindowMessage, 1}, // @pymeth RegisterWindowMessage|Given a string, return a system wide unique message ID.
	{"SearchPath",          PySearchPath, 1}, // @pymeth SearchPath|Searches a path for a file.
	{"SendMessage",         PySendMessage, 1}, // @pymeth SendMessage|Send a message to a window.
	{"SetConsoleTitle",     PySetConsoleTitle, 1}, // @pymeth SetConsoleTitle|Sets the title for the current console.
	{"SetCursorPos",		PySetCursorPos,1}, // @pymeth SetCursorPos|The SetCursorPos function moves the cursor to the specified screen coordinates.
	{"SetErrorMode",        PySetErrorMode, 1}, // @pymeth SetErrorMode|Controls whether the system will handle the specified types of serious errors, or whether the process will handle them.
	{"SetFileAttributes",   PySetFileAttributes,1}, // @pymeth SetFileAttributes|Sets the named file's attributes.
	{"SetSystemTime",		PySetSystemTime,	1},	// @pymeth SetSystemTime|Sets the system time.	
	{"SetClassLong",       PySetClassLong,1}, // @pymeth SetClassLong|Replaces the specified 32-bit (long) value at the specified offset into the extra class memory for the window.
	{"SetClassWord",       PySetClassWord,1}, // @pymeth SetClassWord|Replaces the specified 32-bit (long) value at the specified offset into the extra class memory for the window.
	{"SetClassWord",       PySetWindowWord,1}, // @pymeth SetWindowWord|
	{"SetCursor",           PySetCursor,1}, // @pymeth SetCursor|Set the cursor to the HCURSOR object.
	{"SetStdHandle",	PySetStdHandle,	1}, // @pymeth SetStdHandle|Sets a handle for the standard input, standard output, or standard error device
	{"SetWindowLong",       PySetWindowLong,1}, // @pymeth SetWindowLong|Places a long value at the specified offset into the extra window memory of the given window.
	{"ShellExecute",		PyShellExecute,		1}, // @pymeth ShellExecute|Executes an application.
	{"ShowCursor",			PyShowCursor,		1}, // @pymeth ShowCursor|The ShowCursor method displays or hides the cursor. 
	{"Sleep",				PySleep,			1}, 
	{"SleepEx",				PySleep,			1}, // @pymeth Sleep|Suspends current application execution
	{"TerminateProcess",	PyTerminateProcess,	1}, // @pymeth TerminateProcess|Terminates a process.
	{"Unicode",             PyWin_NewUnicode,         1},	// @pymeth Unicode|Creates a new <o PyUnicode> object
	{"UpdateResource",     PyUpdateResource, 1 },  // @pymeth UpdateResource|Updates a resource in a PE file.
	{"WinExec",             PyWinExec,  		1}, // @pymeth WinExec|Execute a program.
	{"WinHelp",             PyWinHelp,  		1}, // @pymeth WinHelp|Invokes the Windows Help engine.
	{"WriteProfileSection",	PyWriteProfileSection,  1}, // @pymeth WriteProfileSection|Writes a complete section to an INI file or registry.
	{"WriteProfileVal",		PyWriteProfileVal,  1}, // @pymeth WriteProfileVal|Write a value to a Windows INI file.
	{"HIBYTE",              PyHIBYTE,           1}, // @pymeth HIBYTE|An interface to the win32api HIBYTE macro.
	{"LOBYTE",              PyLOBYTE,           1}, // @pymeth LOBYTE|An interface to the win32api LOBYTE macro.
	{"HIWORD",              PyHIWORD,           1}, // @pymeth HIWORD|An interface to the win32api HIWORD macro.
	{"LOWORD",              PyLOWORD,           1}, // @pymeth LOWORD|An interface to the win32api LOWORD macro.
	{"RGB",                 PyRGB,              1}, // @pymeth RGB|An interface to the win32api RGB macro.
	{"MAKELANGID",          PyMAKELANGID,       1}, // @pymeth MAKELANGID|Creates a language identifier from a primary language identifier and a sublanguage identifier.
	{NULL,			NULL}
};

extern "C" __declspec(dllexport) void
initwin32api(void)
{
  PyObject *dict, *module;
  PyWinGlobals_Ensure();
  module = Py_InitModule("win32api", win32api_functions);
  dict = PyModule_GetDict(module);
  Py_INCREF(PyWinExc_ApiError);
  PyDict_SetItemString(dict, "error", PyWinExc_ApiError);
  PyDict_SetItemString(dict,"STD_INPUT_HANDLE",
		       PyInt_FromLong(STD_INPUT_HANDLE));
  PyDict_SetItemString(dict,"STD_OUTPUT_HANDLE",
		       PyInt_FromLong(STD_OUTPUT_HANDLE));
  PyDict_SetItemString(dict,"STD_ERROR_HANDLE",
		       PyInt_FromLong(STD_ERROR_HANDLE));
}
