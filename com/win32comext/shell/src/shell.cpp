// shell.cpp :
// $Id$

// Interfaces that support the Explorer Shell interfaces.

/***
Note that this source file contains embedded documentation.
This documentation consists of marked up text inside the
C comments, and is prefixed with an '@' symbol.  The source
files are processed by a tool called "autoduck" which
generates Windows .hlp files.
@doc
***/

#include "shell_pch.h"
#include "EmptyVC.h"
#include "PyIShellLink.h"
#include "PyIContextMenu.h"
#include "PyIExtractIcon.h"
#include "PyIShellExtInit.h"
#include "PyIShellFolder.h"
#include "PyIEmptyVolumeCache.h"
#include "PyIEmptyVolumeCacheCallBack.h"
#include "PyIEnumIDList.h"
#include "PyICopyHook.h"
#include "PyIOleWindow.h"
#include "PyIShellView.h"
#include "PyIShellIcon.h"
#include "PyIShellIconOverlay.h"
#include "PyIShellIconOverlayManager.h"
#include "PyIShellIconOverlayIdentifier.h"
#include "PyIShellBrowser.h"
#include "PyIBrowserFrameOptions.h"
#include "PyIPersist.h"
#include "PyIPersistFolder.h"
#include "PyIColumnProvider.h"
#include "PyIDropTargetHelper.h"
#include "PyIAsyncOperation.h"
#include "PyIQueryAssociations.h"
#include "PyIDockingWindow.h"
#include "PyIDeskBand.h"
#include "PyIShellLinkDataList.h"
#include "PyIUniformResourceLocator.h"
#include "PyIActiveDesktop.h"
#include "PyIExtractImage.h"

#include "PythonCOMRegister.h" // For simpler registration of IIDs etc.

// We should not be using this!
#define OleSetOleError PyCom_BuildPyException

static HMODULE shell32 = NULL;
static HMODULE shfolder = NULL;
static HMODULE shlwapi = NULL;

typedef BOOL (WINAPI * PFNSHGetSpecialFolderPath)(HWND, LPWSTR,  int, BOOL );
static PFNSHGetSpecialFolderPath pfnSHGetSpecialFolderPath = NULL;

typedef HRESULT (WINAPI * PFNSHGetFolderLocation)(HWND, int, HANDLE, DWORD, LPITEMIDLIST *);
static PFNSHGetFolderLocation pfnSHGetFolderLocation = NULL;

typedef HRESULT (WINAPI * PFNSHEmptyRecycleBin)(HWND, LPSTR, DWORD );
static PFNSHEmptyRecycleBin pfnSHEmptyRecycleBin = NULL;

typedef void (WINAPI * PFNSHGetSettings)(LPSHELLFLAGSTATE, DWORD);
static PFNSHGetSettings pfnSHGetSettings = NULL;

typedef HRESULT (WINAPI * PFNSHGetFolderPath)(HWND, int, HANDLE, DWORD, LPWSTR);
static PFNSHGetFolderPath pfnSHGetFolderPath = NULL;

typedef HRESULT (WINAPI *PFNSHSetFolderPath)(int, HANDLE, DWORD, LPCWSTR);
static PFNSHSetFolderPath pfnSHSetFolderPath = NULL;

typedef HRESULT (WINAPI *PFNSHQueryRecycleBin)(LPCWSTR, LPSHQUERYRBINFO);
static PFNSHQueryRecycleBin pfnSHQueryRecycleBin = NULL;

typedef HRESULT (WINAPI *PFNSHGetViewStatePropertyBag)(LPCITEMIDLIST, LPCWSTR, DWORD, REFIID, void **);
static PFNSHGetViewStatePropertyBag pfnSHGetViewStatePropertyBag = NULL;

typedef HRESULT (WINAPI *PFNSHILCreateFromPath)(LPCWSTR, LPITEMIDLIST *, DWORD *);
static PFNSHILCreateFromPath pfnSHILCreateFromPath = NULL;

typedef HRESULT (WINAPI * PFNAssocCreate)(CLSID, REFIID, LPVOID);
static PFNAssocCreate pfnAssocCreate = NULL;

typedef LRESULT (WINAPI *PFNSHShellFolderView_Message)(HWND, UINT, LPARAM);
static PFNSHShellFolderView_Message pfnSHShellFolderView_Message = NULL;

void PyShell_FreeMem(void *p)
{
	if (p!=NULL)
		CoTaskMemFree(p);
}

void *PyShell_AllocMem(ULONG cb)
{
	return CoTaskMemAlloc(cb);
}

// Some magic hackery macros :-)
#define _ILSkip(pidl, cb)       ((LPITEMIDLIST)(((BYTE*)(pidl))+cb))
#define _ILNext(pidl)           _ILSkip(pidl, (pidl)->mkid.cb)
UINT PyShell_ILGetSize(LPCITEMIDLIST pidl)
{
    UINT cbTotal = 0;
    if (pidl)
    {
		cbTotal += sizeof(pidl->mkid.cb);	// "Null" (ie, 0 .cb) terminator
		while (pidl->mkid.cb)
		{
		    cbTotal += pidl->mkid.cb;
		    pidl = _ILNext(pidl);
		}
    }
    return cbTotal;
}

PyObject *PyObject_FromPIDL(LPCITEMIDLIST pidl, BOOL bFreeSystemPIDL)
{
	if (pidl==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	LPCITEMIDLIST pidl_free = pidl;
	PyObject *ret = PyList_New(0);
	if (!ret)
		return NULL;
	__try {
		while (pidl->mkid.cb) {
			// cb includes sizeof(cb) itself - so string len is cb-sizeof(cb)
			if (pidl->mkid.cb <= sizeof(pidl->mkid.cb)) {
				Py_DECREF(ret);
				ret = NULL;
				PyErr_SetString(PyExc_ValueError, "This string has an invalid sub-item (too short)");
				break;
			}
			// The length may be too large to read (and causing an
			// exception deep inside Python doesn't always leave
			// things in a good state!  Its also inconvenient to
			// always pass the size of the object - so explicitly
			// check we can read the memory.
			UINT cbdata = pidl->mkid.cb-sizeof(pidl->mkid.cb);
			if (IsBadReadPtr(pidl->mkid.abID, cbdata)) {
				Py_DECREF(ret);
				ret = NULL;
				PyErr_SetString(PyExc_ValueError, "This string has an invalid sub-item (too long)");
				break;
			}
			PyObject *sub = PyString_FromStringAndSize((char *)pidl->mkid.abID, cbdata);
			if (sub) {
				PyList_Append(ret, sub);
				Py_DECREF(sub);
			}
			pidl = _ILNext(pidl);
		}
	}
#if defined(__MINGW32__) || defined(MAINWIN)
		catch(...)
#else
		__except( EXCEPTION_EXECUTE_HANDLER )
#endif
	{
		Py_DECREF(ret);
		PyErr_SetString(PyExc_ValueError, "This string is an invalid PIDL (win32 exception unpacking)");
		ret = NULL;
	}
	if (bFreeSystemPIDL)
		PyShell_FreeMem( (void *)pidl_free);
	return ret;
}

// @object PyIDL|A Python representation of an IDL.  Implemented as list of Python strings.
BOOL PyObject_AsPIDL(PyObject *ob, LPITEMIDLIST *ppidl, BOOL bNoneOK /*= FALSE*/, UINT *pcb /* = NULL */)
{
	if (ob==Py_None) {
		if (!bNoneOK) {
			PyErr_SetString(PyExc_TypeError, "None is not a valid ITEMIDLIST in this context");
			return FALSE;
		}
		*ppidl = NULL;
		return TRUE;
	}
	if (!PySequence_Check(ob) || PyString_Check(ob)) {
		PyErr_Format(PyExc_TypeError, "Only sequences (but not strings) are valid ITEMIDLIST objects (got %s).", ob->ob_type->tp_name);
		return FALSE;
	}
	UINT num_items = (unsigned)PySequence_Length(ob);
	// first pass over the sequence to determine number of bytes.
	UINT cbTotal = sizeof((*ppidl)->mkid.cb);	// Null terminator
	UINT i;
	for (i=0;i<num_items;i++) {
		PyObject *sub = PySequence_GetItem(ob, i);
		if (!sub)
			return FALSE;
		if (!PyString_Check(sub)) {
			PyErr_Format(PyExc_TypeError, "ITEMIDLIST sub-items must be strings (got %s)", sub->ob_type->tp_name);
			Py_DECREF(sub);
			return FALSE;
		}
		cbTotal += sizeof((*ppidl)->mkid.cb) + PyString_GET_SIZE(sub);
		Py_DECREF(sub);
	}
	// Now again, filling our buffer.
	void *buf = PyShell_AllocMem( cbTotal );
	if (!buf) {
		PyErr_NoMemory();
		return FALSE;
	}
	LPITEMIDLIST pidl = (LPITEMIDLIST)buf;
	for (i=0;i<num_items;i++) {
		PyObject *sub = PySequence_GetItem(ob, i);
		if (!sub)
			return FALSE;
		if (!PyString_Check(sub)) {
			PyErr_Format(PyExc_TypeError, "ITEMIDLIST sub-items must be strings (got %s)", sub->ob_type->tp_name);
			Py_DECREF(sub);
			return FALSE;
		}
		pidl->mkid.cb = PyString_GET_SIZE(sub) + sizeof(pidl->mkid.cb);
		memcpy(pidl->mkid.abID, PyString_AS_STRING(sub), PyString_GET_SIZE(sub));
		Py_DECREF(sub);
		pidl = _ILNext(pidl);
	}
	pidl->mkid.cb = 0;
	*ppidl = (LPITEMIDLIST)buf;
	if (pcb) *pcb = cbTotal;
	return TRUE;
}

void PyObject_FreePIDL( LPCITEMIDLIST pidl )
{
	PyShell_FreeMem( (void *)pidl);
}

BOOL PyObject_AsPIDLArray(PyObject *obSeq, UINT *pcidl, LPCITEMIDLIST **ret)
{
	// string is a seq - handle that
	*pcidl = 0;
	*ret = NULL;
	if (PyString_Check(obSeq) || !PySequence_Check(obSeq)) {
		PyErr_SetString(PyExc_TypeError, "Must be an array of IDLs");
		return FALSE;
	}
	int n = PySequence_Length(obSeq);
	LPCITEMIDLIST *ppidl = (LPCITEMIDLIST *)malloc(n * sizeof(ITEMIDLIST *));
	if (!ppidl) {
		PyErr_NoMemory();
		return FALSE;
	}
	for (int i=0;i<n;i++) {
		PyObject *ob = PySequence_GetItem(obSeq, i);
		if (!ob || !PyObject_AsPIDL(ob, (ITEMIDLIST **)&ppidl[i], FALSE )) {
			Py_XDECREF(ob);
			PyObject_FreePIDLArray(n, ppidl);
			return FALSE;
		}
		Py_DECREF(ob);
	}
	*pcidl = n;
	*ret = ppidl;
	return TRUE;
}

void PyObject_FreePIDLArray(UINT cidl, LPCITEMIDLIST *pidl)
{
	for (UINT i=0;i<cidl;i++)
		if (pidl[i])
			PyObject_FreePIDL(pidl[i]);
	free(pidl);
}

PyObject *PyObject_FromPIDLArray(UINT cidl, LPCITEMIDLIST *pidl)
{
	if (pidl==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *ob = PyList_New(cidl);
	if (!ob) return NULL;
	for (UINT i=0;i<cidl;i++) {
		PyObject *n = PyObject_FromPIDL(pidl[i], FALSE);
		if (!n) {
			Py_DECREF(ob);
			return NULL;
		}
		PyList_SET_ITEM(ob, i, n); // consumes ref to 'n'
	}
	return ob;
}

// See MSDN http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_basics/shell_basics_programming/transferring/clipboard.asp
// (or search MSDN for "CFSTR_SHELLIDLIST"
#define GetPIDLFolder(pida) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[0])
#define GetPIDLItem(pida, i) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[i+1])
PyObject *PyObject_FromCIDA(CIDA *pida)
{
	unsigned int i;
	PyObject *ret = NULL;
	PyObject *obItems = NULL;
	if (pida==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *obFolder = PyObject_FromPIDL(GetPIDLFolder(pida), FALSE);
	if (obFolder==NULL)
		goto done;
	// cidl == Number of PIDLs that are being transferred, *not* counting the parent folder
	obItems = PyList_New(pida->cidl);
	for (i=0;i<pida->cidl;i++) {
		PyObject *obChild = PyObject_FromPIDL(GetPIDLItem(pida, i), FALSE);
		if (obChild==NULL)
			goto done;
		PyList_SET_ITEM(obItems, i, obChild);
	}
	assert(obFolder && obItems);
	ret = Py_BuildValue("OO", obFolder, obItems);
done:
	Py_XDECREF(obItems);
	Py_XDECREF(obFolder);
	return ret;
}

struct PyCIDAHelper {
	ITEMIDLIST *pidl;
	UINT pidl_size;
};

PyObject *PyObject_AsCIDA(PyObject *ob)
{
	PyObject *obParent, *obKids;
	PyObject *ret = NULL;
	ITEMIDLIST *pidlParent = NULL;
	UINT cbParent;
	PyCIDAHelper *pKids = NULL;
	int nKids = 0;
	int i;
	if (!PyArg_ParseTuple(ob, "OO:CIDA", &obParent, &obKids))
		return NULL;
	if (!PyObject_AsPIDL(obParent, &pidlParent, FALSE, &cbParent))
		goto done;
	if (!PySequence_Check(obKids)) {
		PyErr_Format(PyExc_ValueError,
					 "Kids must be a sequence if PIDLs (not %s)",
					 obKids->ob_type->tp_name);
		goto done;
	}
	nKids = PySequence_Length(obKids);
	if (nKids==-1)
		goto done;
	pKids = (PyCIDAHelper *)malloc(sizeof(PyCIDAHelper) * nKids);
	if (pKids==NULL) {
		PyErr_NoMemory();
		goto done;
	}
	memset(pKids, 0, sizeof(PyCIDAHelper) * nKids);
	for (i=0;i<nKids;i++) {
		BOOL ok;
		PyObject *obKid = PySequence_GetItem(obKids, i);
		if (!obKids)
			goto done;
		ok = PyObject_AsPIDL(obKid, &pKids[i].pidl, FALSE, &pKids[i].pidl_size);
		Py_DECREF(obKid);
		if (!ok)
			goto done;
	}
	/* Calculate size of final buffer. */
	{ /* temp scope for new locals */
	UINT nbytes, pidl_offset;
	LPBYTE pidl_buf;
	CIDA *pcida;
	// count, plus array of offsets.
	nbytes = pidl_offset = sizeof(UINT) + (sizeof(UINT) * (nKids+1));
	// The parent.
	nbytes += cbParent;
	// and each kid.
	for (i=0;i<nKids;i++)
		nbytes += pKids[i].pidl_size;
	ret = PyString_FromStringAndSize(NULL, nbytes);
	pcida = (CIDA *)PyString_AS_STRING(ret);
	pcida->cidl = nKids; // not counting parent.
	pidl_buf = ((LPBYTE)pcida) + pidl_offset;
	pcida->aoffset[0] = pidl_offset;
	memcpy(pidl_buf, pidlParent, cbParent);
	pidl_buf += cbParent;
	pidl_offset += cbParent;
	for (i=0;i<nKids;i++) {
		pcida->aoffset[i+1] = pidl_offset;
		memcpy(pidl_buf, pKids[i].pidl, pKids[i].pidl_size);
		pidl_offset += pKids[i].pidl_size;
		pidl_buf += pKids[i].pidl_size;
	}
	assert(pidl_buf == ((LPBYTE)pcida) + nbytes);
	} // end temp scope
done:
	if (pidlParent) PyObject_FreePIDL(pidlParent);
	if (pKids) {
		for (i=0;i<nKids;i++) {
			if (pKids[i].pidl)
				PyObject_FreePIDL(pKids[i].pidl);
		}
		free(pKids);
	}
	return ret;
}

BOOL PyObject_AsTBBUTTONs( PyObject *ob, TBBUTTON **ppButtons, UINT *pnButtons )
{
	*ppButtons = NULL;
	if (ob==Py_None) {
		*pnButtons = 0;
		return TRUE;
	}
	if (!PySequence_Check(ob)) {
		PyErr_Format(PyExc_TypeError, "TBBUTTONs must be a sequence (got %s)", ob->ob_type->tp_name);
		return FALSE;
	}
	UINT seqsize = PySequence_Size(ob);
	UINT i;
	if (seqsize == -1)
		return FALSE;
	*ppButtons = (TBBUTTON *)malloc(seqsize * sizeof(TBBUTTON));
	if (!*ppButtons) {
		PyErr_NoMemory();
		return FALSE;
	}
	memset(*ppButtons, 0, seqsize * sizeof(TBBUTTON));
	for (i=0;i<seqsize;i++) {
		TBBUTTON *pThis = (*ppButtons)+i;
		PyObject *sub = PySequence_GetItem(ob, i);
		if (!sub) goto failed;
		int ok = PyArg_ParseTuple(sub, "iiBB|li",
							 &pThis->iBitmap,
							 &pThis->idCommand,
							 &pThis->fsState,
							 &pThis->fsStyle,
							 &pThis->dwData,
							 &pThis->iString);
		Py_DECREF(sub);
		if (!ok) goto failed;
	}
	*pnButtons = seqsize;
	return TRUE;
failed:
	if (*ppButtons)
		free(*ppButtons);
	*ppButtons = NULL;
	return FALSE;
}

void PyObject_FreeTBBUTTONs(TBBUTTON *p)
{
	if (p)
		free(p);
}

PyObject *PyWinObject_FromRESOURCESTRING(LPCSTR str)
{
	if (HIWORD(str)==0)
		return PyInt_FromLong(LOWORD(str));
	return PyString_FromString(str);
}

// @object PyCMINVOKECOMMANDINFO|A tuple of parameters to be converted to a CMINVOKECOMMANDINFO struct
// @tupleitem 0|int|Mask|Combination of shellcon.CMIC_MASK_* constants, can be 0
// @tupleitem 1|<o PyHANDLE>|hwnd|Window that owns the shortcut menu
// @tupleitem 2|int or str|Verb|Action to be carried out, specified as a string command or integer menu item id
// @tupleitem 3|str|Parameters|Extra parameters to be passed to the command line for the action, can be None
// @tupleitem 4|str|Directory|Working directory, can be None
// @tupleitem 5|int|Show|Combination of win32con.SW_* constants for any windows that may be created
// @tupleitem 6|int|HotKey|Hot key for any application that may be started
// @tupleitem 7|<o PyHANDLE>|Icon|Handle to icon to use for application, can be None
BOOL PyObject_AsCMINVOKECOMMANDINFO(PyObject *ob, CMINVOKECOMMANDINFO *pci)
{
	PyObject *obVerb, *obhwnd, *obhIcon;
	ZeroMemory(pci, sizeof(CMINVOKECOMMANDINFO));
	pci->cbSize=sizeof(CMINVOKECOMMANDINFO);
	if (!PyArg_ParseTuple(ob, "iOOzziiO:CMINVOKECOMMANDINFO tuple", &pci->fMask, &obhwnd, 
	                                 &obVerb, &pci->lpParameters, &pci->lpDirectory, 
	                                 &pci->nShow, &pci->dwHotKey, &obhIcon))
		return FALSE;
	if (!PyWinObject_AsHANDLE(obhwnd, (HANDLE *)&pci->hwnd, FALSE))
		return NULL;
	if (!PyWinObject_AsHANDLE(obhIcon, (HANDLE *)&pci->hIcon, TRUE))
		return NULL;
	if (PyString_Check(obVerb)) {
		pci->lpVerb = PyString_AsString(obVerb);
	} else if (PyInt_Check(obVerb)) {
		pci->lpVerb = MAKEINTRESOURCEA(PyInt_AsLong(obVerb));
	} else {
		PyErr_Format(PyExc_TypeError, "verb must be an int or string");
		return FALSE;
	}
	return TRUE;
}
void PyObject_FreeCMINVOKECOMMANDINFO( CMINVOKECOMMANDINFO *pci )
{
}

static PyObject *PyString_FromMaybeNullString(const char *sz)
{
	if (sz)
		return PyString_FromString(sz);
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *PyObject_FromCMINVOKECOMMANDINFO(const CMINVOKECOMMANDINFO *pci)
{
	if (!pci) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *obVerb = PyWinObject_FromRESOURCESTRING(pci->lpVerb);
	if (!obVerb) return NULL;
	PyObject *obParams = PyString_FromMaybeNullString(pci->lpParameters);
	if (!obParams) {
		Py_DECREF(obVerb);
		return NULL;
	}
	PyObject *obDir = PyString_FromMaybeNullString(pci->lpDirectory);
	if (!obDir) {
		Py_DECREF(obVerb);
		Py_DECREF(obParams);
		return NULL;
	}
	return Py_BuildValue("iNNNNiiN", pci->fMask, PyWinLong_FromHANDLE(pci->hwnd), 
	                                 obVerb, obParams, obDir, 
	                                 pci->nShow, pci->dwHotKey, PyWinLong_FromHANDLE(pci->hIcon));
}

BOOL PyObject_AsSTRRET( PyObject *ob, STRRET &out )
{
	if (PyInt_Check(ob)) {
		out.uType = STRRET_OFFSET;
		out.uOffset = PyInt_AsLong(ob);
		return TRUE;
	}
	if (PyString_Check(ob)) {
		out.uType = STRRET_CSTR;
		strncpy(out.cStr, PyString_AsString(ob), MAX_PATH);
		return TRUE;
	}
	PyErr_Format(PyExc_TypeError, "Can't convert objects of type '%s' to STRRET", ob->ob_type->tp_name);
	return FALSE;
}

void PyObject_FreeSTRRET(STRRET &s)
{
	if (s.uType==STRRET_WSTR) {
		PyShell_FreeMem(s.pOleStr);
		s.pOleStr = NULL;
	}
}

PyObject *PyObject_FromSTRRET(STRRET *ps, ITEMIDLIST *pidl, BOOL bFree)
{
	if (ps==NULL) {
		if (bFree)
			PyObject_FreeSTRRET(*ps);
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *ret;
	switch (ps->uType) {
		case STRRET_CSTR:
			ret = PyString_FromString(ps->cStr);
			break;
		case STRRET_OFFSET:
			ret = PyString_FromString(((char *)pidl)+ps->uOffset);
			break;
		case STRRET_WSTR:
			ret = PyWinObject_FromWCHAR(ps->pOleStr);
			break;
		default:
			PyErr_SetString(PyExc_RuntimeError, "unknown uType");
			ret = NULL;
			break;
	}
	if (bFree)
		PyObject_FreeSTRRET(*ps);
	return ret;
}

BOOL PyObject_AsMSG( PyObject *obpmsg, MSG *msg )
{
	PyObject *obhwnd;
	return PyArg_ParseTuple(obpmsg, "Oiiii(ii)", &obhwnd,&msg->message,&msg->wParam,&msg->lParam,&msg->time,&msg->pt.x,&msg->pt.y)
		&& PyWinObject_AsHANDLE(obhwnd, (HANDLE *)&msg->hwnd, FALSE);
}

PyObject *PyObject_FromMSG(const MSG *msg)
{
	if (!msg) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return Py_BuildValue("Niiii(ii)", PyWinLong_FromHANDLE(msg->hwnd),msg->message,msg->wParam,msg->lParam,msg->time,msg->pt.x,msg->pt.y);
}

BOOL PyObject_AsFOLDERSETTINGS( PyObject *ob, FOLDERSETTINGS *pf)
{
	return PyArg_ParseTuple(ob, "ii", &pf->ViewMode, &pf->fFlags);
}
PyObject *PyObject_FromFOLDERSETTINGS( const FOLDERSETTINGS *pf)
{
	if (!pf) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return Py_BuildValue("ii", pf->ViewMode, pf->fFlags);
}

BOOL PyObject_AsRECT( PyObject *ob, RECT *r)
{
	return PyArg_ParseTuple(ob, "iiii", &r->left, &r->top, &r->right, &r->bottom);
}
PyObject *PyObject_FromRECT(const RECT *r)
{
	if (!r) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return Py_BuildValue("iiii", r->left, r->top, r->right, r->bottom);
}

static BOOL _CopyToWCHAR(PyObject *ob, WCHAR *buf, unsigned buf_size)
{
	WCHAR *sz;
	if (!PyWinObject_AsWCHAR(ob, &sz, FALSE))
		return FALSE;
	wcsncpy(buf, sz, buf_size);
	buf[buf_size-1] = L'\0';
	PyWinObject_FreeWCHAR(sz);
	return TRUE;
}
#define COPY_TO_WCHAR(ob, buf) _CopyToWCHAR((ob), (buf), sizeof((buf))/sizeof((buf)[0]))

BOOL PyObject_AsSHCOLUMNID(PyObject *ob, SHCOLUMNID *p)
{
	PyObject *obGUID;
	if (!PyArg_ParseTuple(ob, "Oi:SHCOLUMNID tuple",
	     &obGUID, &p->pid))
		return FALSE;
	return PyWinObject_AsIID(obGUID, &p->fmtid);
}

PyObject *PyObject_FromSHCOLUMNID(LPCSHCOLUMNID p)
{
	if (!p) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *obIID = PyWinObject_FromIID(p->fmtid);
	if (!obIID)
		return NULL;
	return Py_BuildValue("Ni", obIID, p->pid);
}

BOOL PyObject_AsSHCOLUMNINIT(PyObject *ob, SHCOLUMNINIT *p)
{
	PyObject *obName;
	if (!PyArg_ParseTuple(ob, "iiO:SHCOLUMNINIT tuple",
	     &p->dwFlags, &p->dwReserved, &obName))
		return FALSE;
	return COPY_TO_WCHAR(obName, p->wszFolder);
}

PyObject *PyObject_FromSHCOLUMNINIT(LPCSHCOLUMNINIT p)
{
	if (!p) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *obName = PyWinObject_FromWCHAR(p->wszFolder);
	if (!obName)
		return NULL;
	return Py_BuildValue("iiN", p->dwFlags, p->dwReserved, obName);
}

BOOL PyObject_AsSHCOLUMNINFO(PyObject *ob, SHCOLUMNINFO *p)
{
	PyObject *obID, *obTitle, *obDescription;
	if (!PyArg_ParseTuple(ob, "OiiiiOO:SHCOLUMNINFO tuple",
	     &obID, &p->vt, &p->fmt, &p->cChars, &p->csFlags,
	     &obTitle, &obDescription))
		return FALSE;
	if (!PyObject_AsSHCOLUMNID(obID, &p->scid))
		return FALSE;
	if (!COPY_TO_WCHAR(obTitle, p->wszTitle))
		return FALSE;
	if (!COPY_TO_WCHAR(obDescription, p->wszDescription))
		return FALSE;
	return TRUE;
}
PyObject *PyObject_FromSHCOLUMNINFO(LPCSHCOLUMNINFO p)
{
	if (!p) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *rc = NULL, *obID = NULL;
	PyObject *obDescription = NULL, *obTitle = NULL;
	obID = PyObject_FromSHCOLUMNID(&p->scid);
	if (!obID) goto done;
	obTitle = PyWinObject_FromWCHAR(p->wszTitle);
	if (!obTitle) goto done;
	obDescription = PyWinObject_FromWCHAR(p->wszDescription);
	if (!obDescription) goto done;
	rc = Py_BuildValue("OiiiiOO", obID, p->vt, p->fmt, p->cChars, 
	                   p->csFlags, obTitle, obDescription);
done:
	Py_XDECREF(obID);
	Py_XDECREF(obDescription);
	Py_XDECREF(obTitle);
	return rc;
}

BOOL PyObject_AsSHCOLUMNDATA(PyObject *ob, SHCOLUMNDATA *p)
{
	PyObject *obExt, *obFile;
	if (!PyArg_ParseTuple(ob, "iiiOO:SHCOLUMNDATA tuple",
	     &p->dwFlags, &p->dwFileAttributes, &p->dwReserved,
		 &obExt, &obFile))
		return FALSE;
	if (!PyWinObject_AsWCHAR(obExt, &p->pwszExt, FALSE))
		return FALSE;
	return COPY_TO_WCHAR(obFile, p->wszFile);
}

void PyObject_FreeSHCOLUMNDATA(SHCOLUMNDATA *p)
{
	PyWinObject_FreeWCHAR(p->pwszExt);
}

PyObject *PyObject_FromSHCOLUMNDATA(LPCSHCOLUMNDATA p)
{
	if (!p) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *obFile = PyWinObject_FromWCHAR(p->wszFile);
	if (!obFile) return NULL;
	PyObject *obExt = PyWinObject_FromWCHAR(p->pwszExt);
	if (!obExt) {
		Py_DECREF(obFile);
		return NULL;
	}
	return Py_BuildValue("iiiNN", p->dwFlags, p->dwFileAttributes, p->dwReserved,
	                     obExt, obFile);
}

// @object SHFILEINFO|A tuple representing a SHFILEINFO structure
// Represented as a tuple of (hIcon, iIcon, dwAttributes, displayName, typeName)
PyObject *PyObject_FromSHFILEINFO(SHFILEINFO *p)
{
	if (!p) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *obDisplayName = PyWinObject_FromTCHAR(p->szDisplayName);
	PyObject *obTypeName = PyWinObject_FromTCHAR(p->szTypeName);
	return Py_BuildValue("iiiNN", p->hIcon, p->iIcon, p->dwAttributes, 
	                              obDisplayName, obTypeName);
}

// @object PyLPOLEMENUGROUPWIDTHS|Tuple containing 6 ints indicating nbr of options in each menu group
BOOL PyObject_AsOLEMENUGROUPWIDTHS( PyObject *oblpMenuWidths, OLEMENUGROUPWIDTHS *pWidths)
{
	return PyArg_ParseTuple(oblpMenuWidths, "iiiiii",
							&pWidths->width[0], &pWidths->width[1],
							&pWidths->width[2], &pWidths->width[3],
							&pWidths->width[4], &pWidths->width[5]) != NULL;
}

PyObject *PyObject_FromOLEMENUGROUPWIDTHS(OLEMENUGROUPWIDTHS *pWidths)
{
	if (!pWidths) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return Py_BuildValue("(iiiiii)",
						 pWidths->width[0], pWidths->width[1],
						 pWidths->width[2], pWidths->width[3],
						 pWidths->width[4], pWidths->width[5]);
}

static BOOL MakeDoubleTerminatedStringList(PyObject *ob, TCHAR **ret)
{
	if (ob==Py_None) {
		*ret = NULL;
		return TRUE;
	}
	DWORD len;
	TCHAR *sz;
	if (!PyWinObject_AsTCHAR(ob, &sz, FALSE, &len))
		return FALSE;
	*ret = (TCHAR *)malloc( sizeof(TCHAR) * (len+2) );
	if (!*ret){
		PyWinObject_FreeTCHAR(sz);
		PyErr_NoMemory();
		return FALSE;
	}
	memcpy(*ret, sz, sizeof(TCHAR) * (len+1));
	(*ret)[len+1] = '\0'; // second term.
	PyWinObject_FreeTCHAR(sz);
	return TRUE;
}

void PyObject_FreeSHFILEOPSTRUCT(SHFILEOPSTRUCT *p)
{
	if (p->pFrom)
		free((void *)p->pFrom);
	if (p->pTo)
		free((void *)p->pTo);
	if (p->lpszProgressTitle)
		PyWinObject_FreeTCHAR((TCHAR *)p->lpszProgressTitle);
	if ((p->fFlags&FOF_WANTMAPPINGHANDLE) && (p->hNameMappings!=NULL))
		SHFreeNameMappings(p->hNameMappings);
}

// @object SHFILEOPSTRUCT|A tuple representing a Win32 shell SHFILEOPSTRUCT structure, used with <om shell.SHFileOperation>
// @comm From and To can contain multiple file names concatenated with a single null between them, eg
// "c:\\file1.txt\0c:\\file2.txt".  A double null terminator will be appended automatically.
// If To specifies multiple file names, flags must contain FOF_MULTIDESTFILES
BOOL PyObject_AsSHFILEOPSTRUCT(PyObject *ob, SHFILEOPSTRUCT *p)
{
	PyObject *obFrom, *obTo, *obNameMappings = Py_None, *obProgressTitle = Py_None, *obhwnd;
	memset(p, 0, sizeof(*p));
	if (!PyArg_ParseTuple(ob, "OiOO|iOO",
						  &obhwnd, // @tupleitem 0|int|hwnd|Handle of window in which to display status messages
						  &p->wFunc, // @tupleitem 1|int|wFunc|One of the shellcon.FO_* values
						  &obFrom, // @tupleitem 2|str/unicode|From|String containing source file name(s) separated by nulls
						  &obTo, // @tupleitem 3|str/unicode|To|String containing destination file name(s) separated by nulls, can be None
						  &p->fFlags, // @tupleitem 4|int|flags|Combination of shellcon.FOF_* flags. Default=0
						  &obNameMappings, // @tupleitem 5|None|NameMappings|Maps input file names to their new names.  This is actually output, and must be None if passed as input. Default=None
						  &obProgressTitle)) // @tupleitem 6|string|ProgressTitle|Title for progress dialog (flags must contain FOF_SIMPLEPROGRESS). Default=None
		return FALSE;
	if (!PyWinObject_AsHANDLE(obhwnd, (HANDLE *)&p->hwnd, TRUE))
		return NULL;
	if (obNameMappings != Py_None) {
		PyErr_SetString(PyExc_TypeError, "The NameMappings value must be None");
		return FALSE;
	}
	if (!MakeDoubleTerminatedStringList(obFrom, (LPTSTR *)&p->pFrom))
		goto error;

	if (!MakeDoubleTerminatedStringList(obTo, (LPTSTR *)&p->pTo))
		goto error;

	if (!PyWinObject_AsTCHAR(obProgressTitle, (LPTSTR *)&p->lpszProgressTitle, TRUE))
		return FALSE;
	return TRUE;
error:
	PyObject_FreeSHFILEOPSTRUCT(p);
	return FALSE;
}

#if (PY_VERSION_HEX >= 0x02030000) // PyGILState only in 2.3+

// Callback for BrowseForFolder
struct PyCallback {
	PyObject *fn;
	PyObject *data;
};


static int CALLBACK PyBrowseCallbackProc(
    HWND hwnd, 
    UINT uMsg, 
    LPARAM lParam, 
    LPARAM lpData
    )
{
	int rc = 0;
	PyObject *result = NULL;
	PyObject *args = NULL;
	PyGILState_STATE state = PyGILState_Ensure();
	PyCallback *pc = (PyCallback *)lpData;
	if (!pc) {
		PySys_WriteStderr("SHBrowseForFolder callback with no data!\n");
		goto done;
	}
	assert(!PyErr_Occurred());
	args = Py_BuildValue("NilO", PyWinLong_FromHANDLE(hwnd), uMsg, lParam, pc->data);
	if (!args) goto done;
	result = PyEval_CallObject(pc->fn, args);
	// API says must return 0, but there might be a good reason.
	if (result && PyInt_Check(result))
		rc = PyInt_AsLong(result);
done:
	if (PyErr_Occurred()) {
		PySys_WriteStderr("SHBrowseForFolder callback failed!\n");
		PyErr_Print();
	}
	Py_XDECREF(args);
	Py_XDECREF(result);
	PyGILState_Release(state);
	return rc;
}

#endif // PY_VERSION_HEX
//////////////////////////////////////////////////////////////
//
// The methods
//

// @pymethod (<o PyIDL>, string displayName, iImage)|shell|SHBrowseForFolder|Displays a dialog box that enables the user to select a shell folder.
static PyObject *PySHBrowseForFolder( PyObject *self, PyObject *args)
{
	BROWSEINFO bi;
	memset(&bi, 0, sizeof(BROWSEINFO));
	PyObject *rc = NULL;
	PyObject *obhwndOwner=Py_None;
	PyObject *obPIDL = Py_None;
	PyObject *obTitle = Py_None;
	PyObject *obcb = Py_None;
	PyObject *obcbparam = Py_None;
	TCHAR retPath[MAX_PATH];
	bi.pszDisplayName = retPath;
	LPITEMIDLIST pl = NULL;
#if (PY_VERSION_HEX >= 0x02030000) // PyGILState only in 2.3+
	PyCallback pycb;
#endif

	if(!PyArg_ParseTuple(args, "|OOOlOO:SHBrowseForFolder",
			&obhwndOwner, // @pyparm <o PyHANDLE>|hwndOwner|None|Parent window for the dialog box, can be None
			&obPIDL,		// @pyparm <o PyIDL>|pidlRoot|None|PIDL identifying the place to start browsing. Desktop is used if not specified
			&obTitle,		// @pyparm <o Unicode>/string|title|None|Title to be displayed with the directory tree
			&bi.ulFlags,	// @pyparm int|flags|0|Combination of shellcon.BIF_* flags
			&obcb,  // @pyparm object|callback|None|A callable object to be used as the callback, or None
			&obcbparam))   // @pyparm object|callback_data|None|An object passed to the callback function
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwndOwner, (HANDLE *)&bi.hwndOwner, TRUE))
		return NULL;
	if (obcb != Py_None) {
#if (PY_VERSION_HEX >= 0x02030000) // PyGILState only in 2.3+
		if (!PyCallable_Check(obcb)) {
			PyErr_SetString(PyExc_TypeError, "Callback item must None or a callable object");
			goto done;
		}
		pycb.fn = obcb;
		pycb.data = obcbparam;
		bi.lParam = (LPARAM)&pycb;
		bi.lpfn = PyBrowseCallbackProc;
#else // PY_VERSION_HEX
		PyErr_SetString(PyExc_NotImplementedError,
						"Callbacks can only be specified in Python 2.3+");
		return NULL;
#endif // PY_VERSION_HEX
		
	} // else bi.lParam/lpfn remains 0
	if (!PyObject_AsPIDL(obPIDL, (LPITEMIDLIST *)&bi.pidlRoot, TRUE))
		goto done;

	if (!PyWinObject_AsTCHAR(obTitle, (TCHAR **)&bi.lpszTitle, TRUE))
		goto done;

	{ // new scope to avoid warnings about the goto
	PY_INTERFACE_PRECALL;
	pl = SHBrowseForFolder(&bi);
	PY_INTERFACE_POSTCALL;
	}

	// @rdesc The result is ALWAYS a tuple of 3 items.  If the user cancels the
	// dialog, all items are None.  If the dialog is closed normally, the result is
	// a tuple of (PIDL, DisplayName, iImageList)
	if (pl){
		PyObject *obPidl = PyObject_FromPIDL(pl, TRUE);
		PyObject *obDisplayName = PyWinObject_FromTCHAR(bi.pszDisplayName);
		rc = Py_BuildValue("OOi", obPidl, obDisplayName, bi.iImage);
		Py_XDECREF(obPidl);
		Py_XDECREF(obDisplayName);
	}
	else {
		rc = Py_BuildValue("OOO", Py_None, Py_None, Py_None);
	}
done:
	if (bi.pidlRoot) PyObject_FreePIDL(bi.pidlRoot);
	if (bi.lpszTitle) PyWinObject_FreeTCHAR((TCHAR *)bi.lpszTitle);
	return rc;
	// @comm If you provide a callback function, it should take 4 args:
	// hwnd, msg, lp, data.  Data will be whatever you passed as callback_data,
	// and the rest are integers.  See the Microsoft documentation for
	// SHBrowseForFolder, or the browse_for_folder.py shell sample for more
	// information.
}

// @pymethod string|shell|SHGetPathFromIDList|Converts an IDLIST to a path.
static PyObject *PySHGetPathFromIDList(PyObject *self, PyObject *args)
{
	CHAR buffer[MAX_PATH];
	PyObject *rc;
	LPITEMIDLIST pidl;
	PyObject *obPidl;

	if (!PyArg_ParseTuple(args, "O:SHGetPathFromIDList", &obPidl))
		// @pyparm <o PyIDL>|idl||The ITEMIDLIST
		return NULL;
	if (!PyObject_AsPIDL(obPidl, &pidl))
		return NULL;

	PY_INTERFACE_PRECALL;
	BOOL ok = SHGetPathFromIDListA(pidl, buffer);
	PY_INTERFACE_POSTCALL;
	if (!ok) {
		OleSetOleError(E_FAIL);
		rc = NULL;
	} else
		rc = PyString_FromString(buffer);
	PyObject_FreePIDL(pidl);
	return rc;
}

// @pymethod <o PyUnicode>|shell|SHGetPathFromIDListW|Converts an IDLIST to a path.
static PyObject *PySHGetPathFromIDListW(PyObject *self, PyObject *args)
{
	WCHAR buffer[MAX_PATH];
	PyObject *rc;
	LPITEMIDLIST pidl;
	PyObject *obPidl;

	if (!PyArg_ParseTuple(args, "O:SHGetPathFromIDListW", &obPidl))
		// @pyparm <o PyIDL>|idl||The ITEMIDLIST
		return NULL;
	if (!PyObject_AsPIDL(obPidl, &pidl))
		return NULL;

	PY_INTERFACE_PRECALL;
	BOOL ok = SHGetPathFromIDListW(pidl, buffer);
	PY_INTERFACE_POSTCALL;
	if (!ok) {
		OleSetOleError(E_FAIL);
		rc = NULL;
	} else
		rc = PyWinObject_FromWCHAR(buffer);
	PyObject_FreePIDL(pidl);
	return rc;
}

// @pymethod <o PyUnicode>|shell|SHGetSpecialFolderPath|Retrieves the path of a special folder. 
static PyObject *PySHGetSpecialFolderPath(PyObject *self, PyObject *args)
{
	HWND hwndOwner;
	PyObject *obhwndOwner;
	int nFolder;
	BOOL bCreate = FALSE;
	if(!PyArg_ParseTuple(args, "Oi|i:SHGetSpecialFolderPath",
			&obhwndOwner, // @pyparm <o PyHANDLE>|hwndOwner||Parent window, can be None (or 0)
			&nFolder, // @pyparm int|nFolder||One of the CSIDL_* constants specifying the path.
			&bCreate)) // @pyparm int|bCreate|0|Should the path be created.
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwndOwner, (HANDLE *)&hwndOwner, TRUE))
		return NULL;
	// @comm This method is only available in shell version 4.71.  If the 
	// function is not available, a COM Exception with HRESULT=E_NOTIMPL 
	// will be raised.  If the function fails, a COM Exception with 
	// HRESULT=E_FAIL will be raised.
	if (pfnSHGetSpecialFolderPath==NULL)
		return OleSetOleError(E_NOTIMPL);

	WCHAR buf[MAX_PATH+1];
	PY_INTERFACE_PRECALL;
	BOOL ok = (*pfnSHGetSpecialFolderPath)(hwndOwner, buf, nFolder, bCreate);
	PY_INTERFACE_POSTCALL;
	if (!ok)
		return OleSetOleError(E_FAIL);
	return PyWinObject_FromWCHAR(buf);
}

// @pymethod <o PyIDL>|shell|SHGetSpecialFolderLocation|Retrieves the PIDL of a special folder.
static PyObject *PySHGetSpecialFolderLocation(PyObject *self, PyObject *args)
{
	HWND hwndOwner;
	PyObject *obhwndOwner;
	int nFolder;
	if(!PyArg_ParseTuple(args, "Oi:SHGetSpecialFolderLocation",
			&obhwndOwner, // @pyparm <o PyHANDLE>|hwndOwner||Parent window, can be None (or 0)
			&nFolder)) // @pyparm int|nFolder||One of the CSIDL_* constants specifying the path.
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwndOwner, (HANDLE *)&hwndOwner, TRUE))
		return NULL;
	LPITEMIDLIST pidl;
	PY_INTERFACE_PRECALL;
	HRESULT hr = SHGetSpecialFolderLocation(hwndOwner, nFolder, &pidl);
	PY_INTERFACE_POSTCALL;
	if (FAILED(hr))
		return OleSetOleError(hr);
	PyObject *rc = PyObject_FromPIDL(pidl, TRUE);
	return rc;
}

// @pymethod int, <o SHFILEINFO>|shell|SHGetFileInfo|Retrieves information about an object in the file system, such as a file, a folder, a directory, or a drive root.
static PyObject *PySHGetFileInfo(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	PyObject *obName;
	TCHAR *name = NULL;
	LPITEMIDLIST pidl = NULL;
	TCHAR *pidl_or_name;
	int attr, flags, info_attrs=0;
	BOOL ok;
	if (!PyArg_ParseTuple(args, "Oii|i", 
			&obName, // @pyparm string/<o PyIDL>|name||The path and file name. Both absolute 
					 // and relative paths are valid.
					 // <nl>If the uFlags parameter includes the SHGFI_PIDL flag, this parameter 
					 // must be a valid <o PyIDL> object that uniquely identifies the file within 
					 // the shell's namespace. The PIDL must be a fully qualified PIDL. 
					 // Relative PIDLs are not allowed.
					 // <nl>If the uFlags parameter includes the SHGFI_USEFILEATTRIBUTES flag, this parameter does not have to be a valid file name. 
					 // The function will proceed as if the file exists with the specified name 
					 // and with the file attributes passed in the dwFileAttributes parameter. 
					 // This allows you to obtain information about a file type by passing 
					 // just the extension for pszPath and passing FILE_ATTRIBUTE_NORMAL 
					 // in dwFileAttributes.
					 // <nl>This string can use either short (the 8.3 form) or long file names.
			&attr, // @pyparm int|dwFileAttributes||Combination of one or more file attribute flags (FILE_ATTRIBUTE_ values). If uFlags does not include the SHGFI_USEFILEATTRIBUTES flag, this parameter is ignored.
			&flags, // @pyparm int|uFlags||Flags that specify the file information to retrieve.  See MSDN for details
			&info_attrs)) // @pyparm int|infoAttrs|0|Flags copied to the SHFILEINFO.dwAttributes member - useful when flags contains SHGFI_ATTR_SPECIFIED
		return NULL;
	if (flags & SHGFI_PIDL) {
		ok = PyObject_AsPIDL(obName, &pidl, FALSE);
		pidl_or_name = (TCHAR *)pidl;
	} else {
		ok = PyWinObject_AsTCHAR(obName, &name, FALSE);
		pidl_or_name = name;
	}
	if (!ok)
		return NULL;
	SHFILEINFO info;
	memset(&info, 0, sizeof(info));
	info.dwAttributes = info_attrs;
	PY_INTERFACE_PRECALL;
	DWORD_PTR dw = SHGetFileInfo(name, attr, &info, sizeof(info), flags);
	PY_INTERFACE_POSTCALL;
	ret = Py_BuildValue("NN", PyLong_FromUnsignedLongLong(dw), PyObject_FromSHFILEINFO(&info));
	if (name) PyWinObject_FreeTCHAR(name);
	if (pidl) PyObject_FreePIDL(pidl);
	return ret;
}

// @pymethod string/<o PyUnicode>|shell|SHGetFolderPath|Retrieves the path of a folder. 
static PyObject *PySHGetFolderPath(PyObject *self, PyObject *args)
{
	HWND hwndOwner;
	PyObject *obhwndOwner;
	int nFolder;
	long flags;
	PyObject *obHandle;
	BOOL bCreate = FALSE;
	if(!PyArg_ParseTuple(args, "OiOl:SHGetFolderPath",
			&obhwndOwner,  // @pyparm <o PyHANDLE>|hwndOwner||Parent window, can be None (or 0)
			&nFolder, // @pyparm int|nFolder||One of the CSIDL_* constants specifying the path.
			&obHandle, // @pyparm <o PyHANDLE>|handle||An access token that can be used to represent a particular user, or None
			&flags)) // @pyparm int|flags||Controls which path is returned.  May be SHGFP_TYPE_CURRENT or SHGFP_TYPE_DEFAULT
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwndOwner, (HANDLE *)&hwndOwner, TRUE))
		return NULL;
	HANDLE handle;
	if (!PyWinObject_AsHANDLE(obHandle, &handle, TRUE))
		return NULL;

	// @comm This method is only available with later versions of shell32.dll, or if you have shfolder.dll installed on earlier systems
	if (pfnSHGetFolderPath==NULL)
		return OleSetOleError(E_NOTIMPL);

	WCHAR buf[MAX_PATH+1];
	PY_INTERFACE_PRECALL;
	HRESULT hr = (*pfnSHGetFolderPath)(hwndOwner, nFolder, handle, flags, buf);
	PY_INTERFACE_POSTCALL;

	if (FAILED(hr))
		return OleSetOleError(hr);
	return PyWinObject_FromWCHAR(buf);
}

// @pymethod |shell|SHSetFolderPath|Sets the location of one of the special folders
// @comm This function is only available on Windows 2000 or later
static PyObject *PySHSetFolderPath(PyObject *self, PyObject *args)
{	
	int csidl;
	HANDLE hToken;
	DWORD Flags=0;	// Flags is reserved
	PyObject *obToken=Py_None, *obPath;
	WCHAR *Path;

	if (pfnSHSetFolderPath==NULL)
		return OleSetOleError(E_NOTIMPL);
	if(!PyArg_ParseTuple(args, "lO|O:SHSetFolderPath",
		&csidl,		// @pyparm int|csidl||One of the shellcon.CSIDL_* values
		&obPath,	// @pyparm str/unicode|Path||The full path to be set
		&obToken))	// @pyparm <o PyHANDLE>|hToken|None|Handle to an access token, can be None 
		return NULL;
	if (!PyWinObject_AsHANDLE(obToken, &hToken, TRUE))
		return NULL;
	if (!PyWinObject_AsWCHAR(obPath, &Path, FALSE))
		return NULL;

	PY_INTERFACE_PRECALL;
	HRESULT hr = (*pfnSHSetFolderPath)(csidl, hToken, Flags, Path);
	PY_INTERFACE_POSTCALL;

	PyWinObject_FreeWCHAR(Path);
	if (FAILED(hr))
		return OleSetOleError(hr);
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod <o PyIDL>|shell|SHGetFolderLocation|Retrieves the PIDL of a folder.
static PyObject *PySHGetFolderLocation(PyObject *self, PyObject *args)
{
	HWND hwndOwner;
	HANDLE hToken=NULL;
	int nFolder;
	DWORD flags = 0;
	PyObject *obToken=Py_None, *obhwndOwner;

	if(!PyArg_ParseTuple(args, "Oi|Ol:SHGetFolderLocation",
			&obhwndOwner, // @pyparm int|hwndOwner||Window in which to display any neccessary dialogs
			&nFolder, // @pyparm int|nFolder||One of the CSIDL_* constants specifying the path.
			&obToken, // @pyparm <o PyHANDLE>|hToken|None|An access token that can be used to represent a particular user, or None
			&flags)) // @pyparm int|reserved|0|Must be 0
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwndOwner, (HANDLE *)&hwndOwner, TRUE))
		return NULL;
	if (!PyWinObject_AsHANDLE(obToken, &hToken, TRUE))
		return NULL;
	LPITEMIDLIST pidl;

	// @comm This method is only available with version 5 or later of the shell controls
	if (pfnSHGetFolderLocation==NULL)
		return OleSetOleError(E_NOTIMPL);
	PY_INTERFACE_PRECALL;
	HRESULT hr = (*pfnSHGetFolderLocation)(hwndOwner, nFolder, hToken, flags, &pidl);
	PY_INTERFACE_POSTCALL;

	if (FAILED(hr))
		return OleSetOleError(hr);
	PyObject *rc = PyObject_FromPIDL(pidl, TRUE);
	return rc;
}

// @pymethod |shell|SHAddToRecentDocs|Adds a document to the shell's list of recently used documents or clears all documents from the list. The user gains access to the list through the Start menu of the Windows taskbar.
static PyObject *PySHAddToRecentDocs(PyObject *self, PyObject *args)
{
	int flags;
	void *whatever;
	if(!PyArg_ParseTuple(args, "iz:SHAddToRecentDocs",
			&flags, // @pyparm int|flags||Flag that indicates the meaning of the whatever parameter
			&whatever)) // @pyparm string|whatever||A path or <o PyIDL>
		return NULL;

	PY_INTERFACE_PRECALL;
	SHAddToRecentDocs(flags, whatever);
	PY_INTERFACE_POSTCALL;
	Py_INCREF(Py_None);
	return Py_None;
}


// @pymethod |shell|SHEmptyRecycleBin|Empties the recycle bin on the specified drive.
static PyObject *PySHEmptyRecycleBin(PyObject *self, PyObject *args)
{
	HWND hwnd;
	PyObject *obhwnd;
	char *path;
	DWORD flags;
	if(!PyArg_ParseTuple(args, "Ozl:SHEmptyRecycleBin",
			&obhwnd, // @pyparm <o PyHANDLE>|hwnd||Handle to parent window, can be None
			&path, // @pyparm string|path||A NULL-terminated string that contains the path of the root drive on which the recycle bin is located. This parameter can contain the address of a string formatted with the drive, folder, and subfolder names (c:\windows\system . . .). It can also contain an empty string or NULL. If this value is an empty string or NULL, all recycle bins on all drives will be emptied.
			&flags)) // @pyparm int|flags||One of the SHERB_* values.
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwnd, (HANDLE *)&hwnd, TRUE))
		return NULL;
	// @comm This method is only available in shell version 4.71.  If the function is not available, a COM Exception with HRESULT=E_NOTIMPL will be raised.
	if (pfnSHEmptyRecycleBin==NULL)
		return OleSetOleError(E_NOTIMPL);

	PY_INTERFACE_PRECALL;
	HRESULT hr = (*pfnSHEmptyRecycleBin)(hwnd, path, flags);
	PY_INTERFACE_POSTCALL;
	if (FAILED(hr))
		return OleSetOleError(hr);
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod long,long|shell|SHQueryRecycleBin|Retrieves the total size and number of items in the Recycle Bin for a specified drive
static PyObject *PySHQueryRecycleBin(PyObject *self, PyObject *args)
{
	PyObject *obRootPath=Py_None;
	HRESULT hr;
	WCHAR *RootPath=NULL;
	SHQUERYRBINFO info;
	if (!PyArg_ParseTuple(args, "|O:SHQueryRecycleBin",
		&obRootPath))	// @pyparm <o PyUnicode>|RootPath|None|A path containing the drive whose recycle bin will be queried, or None for all drives
		return NULL;
	if (!PyWinObject_AsWCHAR(obRootPath, &RootPath, TRUE))
		return NULL;
	if (pfnSHQueryRecycleBin==NULL)
		return OleSetOleError(E_NOTIMPL);
	info.cbSize=sizeof(SHQUERYRBINFO);
	PY_INTERFACE_PRECALL;
	hr=(*pfnSHQueryRecycleBin)(RootPath, &info);
	PY_INTERFACE_POSTCALL;
	PyWinObject_FreeWCHAR(RootPath);
	if (FAILED(hr))
		return OleSetOleError(hr);
	return Py_BuildValue("LL", info.i64Size, info.i64NumItems);
}

static PyObject *PyWinObject_FromSHNAMEMAPPINGS(LPVOID hNameMappings)
{
	if (hNameMappings==NULL)
		return PyTuple_New(0);
	PyObject *ret, *ret_item;
	// according to the SDK, SHFILEOPSTRUCT.hNameMappings should be interpreted thusly:
	struct SHNAMEMAPPINGS{
		UINT nbr_of_mappings;
		LPSHNAMEMAPPINGW pmappings;  // on WinNT and up, the unicode version will always be returned
		};
	SHNAMEMAPPINGS *mappings=(SHNAMEMAPPINGS *)hNameMappings;
	ret=PyTuple_New(mappings->nbr_of_mappings);
	if (!ret)
		return NULL;
	for (UINT mapping_index=0; mapping_index<mappings->nbr_of_mappings; mapping_index++){
		ret_item=Py_BuildValue("NN",
			PyWinObject_FromWCHAR(mappings->pmappings[mapping_index].pszOldPath, mappings->pmappings[mapping_index].cchOldPath),
			PyWinObject_FromWCHAR(mappings->pmappings[mapping_index].pszNewPath, mappings->pmappings[mapping_index].cchNewPath));
		if (ret_item==NULL){
			Py_DECREF(ret);
			return NULL;
			}
		PyTuple_SET_ITEM(ret, mapping_index, ret_item);
		}
	return ret;
}

// @pymethod int, int|shell|SHFileOperation|Copies, moves, renames, or deletes a file system object.
// @rdesc The result is a tuple containing int result of the function itself, and the result of the
// fAnyOperationsAborted member after the operation.  If Flags contains FOF_WANTMAPPINGHANDLE,
// returned tuple will have a 3rd member containing a sequence of 2-tuples with the old and new file names
// of renamed files.  This will only have any content if FOF_RENAMEONCOLLISION was specified, and some
// filename conflicts actually occurred.
static PyObject *PySHFileOperation(PyObject *self, PyObject *args)
{
	PyObject *ob, *ret;
	if (!PyArg_ParseTuple(args, "O:SHFileOperation",
						  &ob)) // @pyparm <o SHFILEOPSTRUCT>|operation||Defines the operation to perform.
		return NULL;
	SHFILEOPSTRUCT op;
	if (!PyObject_AsSHFILEOPSTRUCT(ob, &op))
		return NULL;
	PY_INTERFACE_PRECALL;
	int rc = SHFileOperation(&op);
	PY_INTERFACE_POSTCALL;

	if (op.fFlags&FOF_WANTMAPPINGHANDLE)
		ret=Py_BuildValue("iOO&", rc, op.fAnyOperationsAborted ? Py_True : Py_False, PyWinObject_FromSHNAMEMAPPINGS, op.hNameMappings);
	else
		ret=Py_BuildValue("iO", rc, op.fAnyOperationsAborted ? Py_True : Py_False);
	PyObject_FreeSHFILEOPSTRUCT(&op);
	return ret;

}

// @pymethod <o PyIShellFolder>|shell|SHGetDesktopFolder|Retrieves the <o PyIShellFolder> interface for the desktop folder, which is the root of the shell's namespace. 
static PyObject *PySHGetDesktopFolder(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":SHGetDesktopFolder"))
		return NULL;
	IShellFolder *psf;
	PY_INTERFACE_PRECALL;
	HRESULT hr = SHGetDesktopFolder(&psf);
	PY_INTERFACE_POSTCALL;
	if (FAILED(hr))
		return OleSetOleError(hr);
	return PyCom_PyObjectFromIUnknown(psf, IID_IShellFolder, FALSE);
}

// @pymethod |shell|SHUpdateImage|Notifies the shell that an image in the system image list has changed.
static PyObject *PySHUpdateImage(PyObject *self, PyObject *args)
{
	PyObject *obHash;
	UINT flags;
	int index, imageIndex;
	if(!PyArg_ParseTuple(args, "Oiii:SHUpdateImage", 
			&obHash,		// @pyparm string|HashItem||Full path of file containing the icon as returned by <om PyIExtractIcon.GetIconLocation>
			&index,			// @pyparm int|Index||Index of the icon in the above file
			&flags,			// @pyparm int|Flags||GIL_NOTFILENAME or GIL_SIMULATEDOC
			&imageIndex))	// @pyparm int|ImageIndex||Index of the icon in the system image list
		return NULL;

	TCHAR *szHash;
	if (!PyWinObject_AsTCHAR(obHash, &szHash))
		return NULL;
	PY_INTERFACE_PRECALL;
	SHUpdateImage(szHash, index, flags, imageIndex);
	PY_INTERFACE_POSTCALL;
	PyWinObject_FreeTCHAR(szHash);
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod |shell|SHChangeNotify|Notifies the system of an event that an application has performed. An application should use this function if it performs an action that may affect the shell. 
static PyObject *PySHChangeNotify(PyObject *self, PyObject *args)
{
	LONG wEventID;
	UINT flags, datatype;
	PyObject *ob1, *ob2 = Py_None, *ret=NULL;
	if(!PyArg_ParseTuple(args, "iiO|O:SHChangeNotify", 
			&wEventID,	// @pyparm int|EventId||Combination of shellcon.SHCNE_* constants
			&flags,		// @pyparm int|Flags||Combination of shellcon.SHCNF_* constants that specify type of last 2 parameters
						// Only one of the type flags may be specified, combined with one of the SHCNF_FLUSH* flags
			&ob1,		// @pyparm object|Item1||Type is dependent on the event to be signalled
			&ob2))		// @pyparm object|Item2||Type is dependent on the event to be signalled
		return NULL;
	// SHCFN_IDLIST is 0, so shift away upper bits containing *FLUSH* flags for comparison
	datatype=(flags<<20)>>20;
	void *p1=NULL, *p2=NULL;
	BOOL bsuccess=TRUE;
	switch (datatype){
		case SHCNF_IDLIST:
			bsuccess=PyObject_AsPIDL(ob1, (ITEMIDLIST **)&p1, FALSE) 
				  && PyObject_AsPIDL(ob2, (ITEMIDLIST **)&p2, TRUE);
			break;
		case SHCNF_DWORD:
			p1 = (void *)PyInt_AsLong(ob1);
			if (p1==(VOID *)-1 && PyErr_Occurred())
				bsuccess=FALSE;
			else if (ob2!=Py_None){
				p2 = (void *)PyInt_AsLong(ob2);
				if (p2==(void *)-1 && PyErr_Occurred())
					bsuccess=FALSE;
				}
			break;
		case SHCNF_PATHA:
		case SHCNF_PRINTERA:
			p1 = (void *)PyString_AsString(ob1);
			if (p1==NULL)
				bsuccess=FALSE;
			else if (ob2!=Py_None){
				p2 = (void *)PyString_AsString(ob2);
				if (p2==NULL)
					bsuccess=FALSE;
				}
			break;
		case SHCNF_PATHW:
		case SHCNF_PRINTERW:
			bsuccess=PyWinObject_AsWCHAR(ob1, (WCHAR **)&p1, FALSE) 
				  && PyWinObject_AsWCHAR(ob2, (WCHAR **)&p2, TRUE);
			break;
		default:
			PyErr_Format(PyExc_ValueError, "Type %d is not supported", datatype);
			bsuccess=FALSE;
		}

	if (bsuccess){
		PY_INTERFACE_PRECALL;
		SHChangeNotify(wEventID, flags, p1, p2);
		PY_INTERFACE_POSTCALL;
		Py_INCREF(Py_None);
		ret=Py_None;
		}

	switch (datatype){
		case SHCNF_IDLIST:
			PyObject_FreePIDL((ITEMIDLIST *)p1);
			PyObject_FreePIDL((ITEMIDLIST *)p2);
			break;
		case SHCNF_PATHW:
		case SHCNF_PRINTERW:
			PyWinObject_FreeWCHAR((WCHAR *)p1);
			PyWinObject_FreeWCHAR((WCHAR *)p2);
			break;
		default:
			break;
		}
	return ret;
}

// @pymethod int|shell|SHChangeNotifyRegister|Registers a window that receives notifications from the file system or shell.
static PyObject *PySHChangeNotifyRegister(PyObject *self, PyObject *args)
{
	typedef ULONG (WINAPI * PFNSHChangeNotifyRegister)(HWND hwnd,
									int fSources,
									LONG fEvents,
									UINT wMsg,
									int cEntries,
									SHChangeNotifyEntry *pfsne);

	HMODULE hmod = GetModuleHandle(TEXT("shell32.dll"));
	PFNSHChangeNotifyRegister pfnSHChangeNotifyRegister = NULL;
	// This isn't always exported by name - but by ordinal 2!!
	if (hmod) pfnSHChangeNotifyRegister=(PFNSHChangeNotifyRegister)GetProcAddress(hmod, MAKEINTRESOURCEA(2));
	if (pfnSHChangeNotifyRegister==NULL)
		return OleSetOleError(E_NOTIMPL);
	// The SDK says of the array of entries:
	// "This array should always be set to one when calling SHChangeNotifyRegister or
	//  SHChangeNotifyDeregister will not work properly."
	// Therefore, we support just one item in the array - and don't require it
	// to be an array!
	HWND hwnd;
	int sources;
    LONG events;
    UINT msg;
	PyObject *obPIDL, *obhwnd;
	SHChangeNotifyEntry entry;
	if(!PyArg_ParseTuple(args, "Oiii(Oi):SHChangeNotifyRegister",
			&obhwnd, // @pyparm <o PyHANDLE>|hwnd||Handle to the window that receives the change or notification messages.
			&sources, // @pyparm int|sources||One or more values that indicate the type of events for which to receive notifications.
			&events, // @pyparm int|events||Change notification events for which to receive notification.
			&msg,  // @pyparm int|msg||Message to be posted to the window procedure.
			&obPIDL,
			&entry.fRecursive))
		return NULL;
	if (!PyWinObject_AsHANDLE(obhwnd, (HANDLE *)&hwnd, FALSE))
		return NULL;
	if (!PyObject_AsPIDL(obPIDL, (ITEMIDLIST **)&entry.pidl, TRUE))
		return NULL;
	ULONG rc;
	PY_INTERFACE_PRECALL;
	rc = (*pfnSHChangeNotifyRegister)(hwnd, sources, events, msg, 1, &entry);
	PY_INTERFACE_POSTCALL;
	PyObject_FreePIDL(entry.pidl);
	return PyInt_FromLong(rc);
}

// @pymethod |shell|SHChangeNotifyDeregister|Unregisters the client's window process from receiving notification events
static PyObject *PySHChangeNotifyDeregister(PyObject *self, PyObject *args)
{
	typedef BOOL (WINAPI * PFNSHChangeNotifyDeregister)(LONG uid);
	HMODULE hmod = GetModuleHandle(TEXT("shell32.dll"));
	PFNSHChangeNotifyDeregister pfnSHChangeNotifyDeregister = NULL;
	// This isn't always exported by name - but by ordinal 4!!
	if (hmod) pfnSHChangeNotifyDeregister=(PFNSHChangeNotifyDeregister)GetProcAddress(hmod, MAKEINTRESOURCEA(4));
	if (pfnSHChangeNotifyDeregister==NULL)
		return OleSetOleError(E_NOTIMPL);
    LONG id;
	if(!PyArg_ParseTuple(args, "i:SHChangeNotifyDeregister",
			&id)) // @pyparm int|id||The registration identifier (ID) returned by <om shell.SHChangeNotifyRegister>.
		return NULL;

	BOOL rc;
	PY_INTERFACE_PRECALL;
	rc = (*pfnSHChangeNotifyDeregister)(id);
	PY_INTERFACE_POSTCALL;
	if (!rc)
		return OleSetOleError(E_FAIL);
	Py_INCREF(Py_None);
	return Py_None;
}

// @pymethod int/string|shell|DragQueryFile|Retrieves the names (or count) of dropped files
static PyObject *PyDragQueryFile(PyObject *self, PyObject *args)
{
	HDROP hglobal;
	PyObject *obhglobal;
	UINT index;
	if(!PyArg_ParseTuple(args, "Oi:DragQueryFile", 
			&obhglobal, // @pyparm <o PyHANDLE>|hglobal||The HGLOBAL object - generally obtained via the 'data_handle' property of a <o PySTGMEDIUM> object.
			&index)) // @pyparm int|index||The index to retrieve.  If -1, the result if an integer representing the valid index values.
		return NULL;
	if (!PyWinObject_AsHANDLE(obhglobal, (HANDLE *)&hglobal, FALSE))
		return NULL;
	if (index==0xFFFFFFFF) {
		return PyInt_FromLong(DragQueryFile(hglobal, index, NULL, 0));
	}
	// get the buffer size
	UINT nchars = DragQueryFile(hglobal, index, NULL, 0)+2;
	TCHAR *sz = (TCHAR *)malloc(nchars * sizeof(TCHAR));
	if (sz==NULL)
		return PyErr_NoMemory();
	nchars = ::DragQueryFile(hglobal, index, sz, nchars);
	PyObject *ret = PyString_FromStringAndSize(sz, nchars);
	free(sz);
	return ret;
}

// @pymethod int/<o PyUnicode>|shell|DragQueryFileW|Retrieves the names (or count) of dropped files
static PyObject *PyDragQueryFileW(PyObject *self, PyObject *args)
{
	HDROP hglobal;
	PyObject *obhglobal;
	UINT index;
	if(!PyArg_ParseTuple(args, "Oi:DragQueryFileW", 
			&obhglobal, // @pyparm <o PyHANDLE>|hglobal||The HGLOBAL object - generally obtained via the 'data_handle' property of a <o PySTGMEDIUM> object.
			&index)) // @pyparm int|index||The index to retrieve.  If -1, the result if an integer representing the valid index values.
		return NULL;
	if (!PyWinObject_AsHANDLE(obhglobal, (HANDLE *)&hglobal, FALSE))
		return NULL;
	if (index==0xFFFFFFFF) {
		return PyInt_FromLong(DragQueryFileW(hglobal, index, NULL, 0));
	}
	// get the buffer size
	UINT nchars = DragQueryFileW(hglobal, index, NULL, 0)+2;
	WCHAR *sz = (WCHAR *)malloc(nchars * sizeof(WCHAR));
	if (sz==NULL)
		return PyErr_NoMemory();
	nchars = ::DragQueryFileW(hglobal, index, sz, nchars);
	PyObject *ret = PyWinObject_FromWCHAR(sz, nchars);
	free(sz);
	return ret;
}

// @pymethod int, (int,int)|shell|DragQueryPoint|Retrieves the position of the mouse pointer at the time a file was dropped during a drag-and-drop operation.
// @rdesc The first item of the return tuple is True if the drop occurred in the client area of the window, or False if the drop did not occur in the client area of the window.
// @comm The window for which coordinates are returned is the window that received the WM_DROPFILES message
static PyObject *PyDragQueryPoint(PyObject *self, PyObject *args)
{
	HDROP hglobal;
	PyObject *obhglobal;
	if(!PyArg_ParseTuple(args, "O:DragQueryFile", 
			&obhglobal)) // @pyparm <o PyHANDLE>|hglobal||The HGLOBAL object - generally obtained the 'data_handle' property of a <o PySTGMEDIUM>
		return NULL;
	if (!PyWinObject_AsHANDLE(obhglobal, (HANDLE *)&hglobal, FALSE))
		return NULL;
	POINT pt;
	BOOL result = ::DragQueryPoint(hglobal, &pt);
	return Py_BuildValue("O(ii)", result ? Py_True : Py_False, pt.x, pt.y);
}

// @pymethod <o PyIUnknown>|shell|SHGetInstanceExplorer|Allows components that run in a Web browser (Iexplore.exe) or a nondefault Windows Explorer (Explorer.exe) process to hold a reference to the process. The components can use the reference to prevent the process from closing prematurely.
static PyObject *PySHGetInstanceExplorer(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":SHGetInstanceExplorer"))
		return NULL;
	IUnknown *pUnk = NULL;
	HRESULT hr = SHGetInstanceExplorer(&pUnk);
	if (FAILED(hr))
		return OleSetOleError(hr);
	return PyCom_PyObjectFromIUnknown(pUnk, IID_IUnknown, FALSE);
	// @comm SHGetInstanceExplorer succeeds only if it is called from within 
	// an Explorer.exe or Iexplorer.exe process. It is typically used by 
	// components that run in the context of the Web browser (Iexplore.exe). 
	// However, it is also useful when Explorer.exe has been configured to 
	// run all folders in a second process. SHGetInstanceExplorer fails if 
	// the component is running in the default Explorer.exe process. There 
	// is no need to hold a reference to this process, as it is shut down 
	// only when the user logs out.
}

// @pymethod string|shell|PIDLAsString|Given a PIDL object, return the raw PIDL bytes as a string
static PyObject *PyPIDLAsString(PyObject *self, PyObject *args)
{
	PyObject *obPIDL;
	// @pyparm <o PyIDL>|pidl||The PIDL object (ie, a list of strings)
	if (!PyArg_ParseTuple(args, "O:PIDLAsString", &obPIDL))
		return NULL;
	UINT cb;
	ITEMIDLIST *ppidls;
	if (!PyObject_AsPIDL(obPIDL, &ppidls, FALSE, &cb))
		return NULL;
	PyObject *ret = PyString_FromStringAndSize((char *)ppidls, cb);
	PyShell_FreeMem(ppidls);
	return ret;
}

// @pymethod <o PyIDL>|shell|StringAsPIDL|Given a PIDL as a raw string, return a PIDL object (ie, a list of strings)
static PyObject *PyStringAsPIDL(PyObject *self, PyObject *args)
{
	char *szPIDL;
	Py_ssize_t pidllen;
	// @pyparm string|pidl||The PIDL as a raw string.
	if (!PyArg_ParseTuple(args, "s#:StringAsPIDL", &szPIDL, &pidllen))
		return NULL;
	return PyObject_FromPIDL((LPCITEMIDLIST)szPIDL, FALSE);
}	

// @pymethod <o PyIDL>|shell|AddressAsPIDL|Given the address of a PIDL in memory, return a PIDL object (ie, a list of strings)
static PyObject *PyAddressAsPIDL(PyObject *self, PyObject *args)
{
	LPCITEMIDLIST lpidl;
	PyObject *obpidl;
	// @pyparm int|address||The address of the PIDL
	if (!PyArg_ParseTuple(args, "O:AddressAsPIDL", &obpidl))
		return NULL;
	if (!PyWinLong_AsVoidPtr(obpidl, (void **)&lpidl))
		return NULL;
	return PyObject_FromPIDL(lpidl, FALSE);
}

// @pymethod <o PyIDL>, list|shell|StringAsCIDA|Given a CIDA as a raw string, return the folder PIDL and list of children
static PyObject *PyStringAsCIDA(PyObject *self, PyObject *args)
{
	char *szCIDA;
	Py_ssize_t pidllen;
	// @pyparm string|pidl||The PIDL as a raw string.
	if (!PyArg_ParseTuple(args, "s#:StringAsCIDA", &szCIDA, &pidllen))
		return NULL;
	return PyObject_FromCIDA((CIDA *)szCIDA);
	// @rdesc The result is the PIDL of the folder, and a list of child PIDLs.
}

// @pymethod string|shell|CIDAAsString|Given a (pidl, child_pidls) object, return a CIDA as a string
static PyObject *PyCIDAAsString(PyObject *self, PyObject *args)
{
	PyObject *obCIDA;
	// @pyparm string|pidl||The PIDL as a raw string.
	if (!PyArg_ParseTuple(args, "O:CIDAAsString", &obCIDA))
		return NULL;
	return PyObject_AsCIDA(obCIDA);
	// @rdesc The result is a string with the CIDA bytes.
}

#define CHECK_SET_VAL(mask1, mask2, x) \
	if (mask1 & mask2 ) PyDict_SetItemString(ret, #x, state.x ? Py_True : Py_False);

// @pymethod dict|shell|SHGetSettings|Retrieves the current shell option settings.
static PyObject *PySHGetSettings(PyObject *self, PyObject *args)
{
	DWORD mask = -1;
	// @pyparm int|mask|-1|The values being requested - one of the shellcon.SSF_* constants
	if (!PyArg_ParseTuple(args, "|l:SHGetSettings", &mask))
		return NULL;

	// @comm This method is only available in shell version 4.71.  If the 
	// function is not available, a COM Exception with HRESULT=E_NOTIMPL 
	// will be raised.
	if (pfnSHGetSettings==NULL)
		return OleSetOleError(E_NOTIMPL);

	SHELLFLAGSTATE state;
	PY_INTERFACE_PRECALL;
	(*pfnSHGetSettings)(&state, mask);
	PY_INTERFACE_POSTCALL;

	PyObject *ret = PyDict_New();
	CHECK_SET_VAL(SSF_DESKTOPHTML, mask, fDesktopHTML);
	
	CHECK_SET_VAL(SSF_DONTPRETTYPATH, mask, fDontPrettyPath);
	CHECK_SET_VAL(SSF_DOUBLECLICKINWEBVIEW, mask, fDoubleClickInWebView);
	CHECK_SET_VAL(SSF_HIDEICONS, mask, fHideIcons);
	CHECK_SET_VAL(SSF_MAPNETDRVBUTTON, mask, fMapNetDrvBtn);
	CHECK_SET_VAL(SSF_NOCONFIRMRECYCLE, mask, fNoConfirmRecycle);
	CHECK_SET_VAL(SSF_SHOWALLOBJECTS, mask, fShowAllObjects);
	CHECK_SET_VAL(SSF_SHOWATTRIBCOL, mask, fShowAttribCol);
	CHECK_SET_VAL(SSF_SHOWCOMPCOLOR, mask, fShowCompColor);
	CHECK_SET_VAL(SSF_SHOWEXTENSIONS, mask, fShowExtensions);
	CHECK_SET_VAL(SSF_SHOWINFOTIP, mask, fShowInfoTip);
	CHECK_SET_VAL(SSF_SHOWSYSFILES, mask, fShowSysFiles);
	CHECK_SET_VAL(SSF_WIN95CLASSIC, mask, fWin95Classic);
	// If requesting any of the top 3 bits, return them as well
	if (mask & 0xE000) {
		PyObject *val = PyInt_FromLong(state.fRestFlags);
		if (val) {
			PyDict_SetItemString(ret, "fRestFlags", val);
			Py_DECREF(val);
		}
	}
	return ret;
	// @rdesc The result is a dictionary, the contents of which depend on
	// the mask param.  Key names are the same as the SHELLFLAGSTATE
	// structure members - 'fShowExtensions', 'fNoConfirmRecycle', etc
}

// @pymethod string|shell|FILEGROUPDESCRIPTORAsString|Creates a FILEGROUPDESCRIPTOR from a sequence of mapping objects, each with FILEDESCRIPTOR attributes
static PyObject *PyFILEGROUPDESCRIPTORAsString(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	FILEGROUPDESCRIPTORA *fgd = NULL;
	FILEGROUPDESCRIPTORW *fgdw = NULL;
	PyObject *ob;
	int i;
	// @pyparm [FILEDESCRIPTOR, ...]|descriptors||A sequence of FILEDESCRIPTOR objects.
	// Each filedescriptor object must be a mapping/dictionary, with the following
	// optional attributes: dwFlags, clsid, sizel, pointl, dwFileAttributes,
	// ftCreationTime, ftLastAccessTime, ftLastWriteTime, nFileSize
	// If these attributes do not exist, or are None, they will be ignored - hence
	// you only need specify attributes you care about.
	// <nl>In general, you can omit dwFlags - it will be set correctly based
	// on what other attributes exist.
	// @pyparm bool|make_unicode|False|If true, a FILEDESCRIPTORW object is created
	int make_unicode = FALSE;
	if (!PyArg_ParseTuple(args, "O|i", &ob, &make_unicode))
		return NULL;
	if (!PySequence_Check(ob))
		return PyErr_Format(PyExc_TypeError, "must be a sequence of mapping objects (got '%s')",
							ob->ob_type->tp_name);
	int num = PySequence_Length(ob);
	if (num==-1)
		return NULL;
	int cb;
	if (make_unicode)
		cb = sizeof(FILEGROUPDESCRIPTORW) + sizeof(FILEDESCRIPTORW)*(num-1);
	else
		cb = sizeof(FILEGROUPDESCRIPTORA) + sizeof(FILEDESCRIPTORA)*(num-1);
	PyObject *ret_string = PyString_FromStringAndSize(NULL, cb);
	if (!ret_string) goto done;
	fgd = (FILEGROUPDESCRIPTORA *)PyString_AS_STRING(ret_string);
	fgdw = (FILEGROUPDESCRIPTORW *)PyString_AS_STRING(ret_string);
	memset(fgd, 0, cb);
	fgd->cItems = num;
	for (i=0;i<num;i++) {
		BOOL ok = TRUE;
		// These structures are the same until the very end, where the
		// unicode/ascii buffer is - so we can use either pointer
		// BUT - their start pointer *does* depend on Unicode - so
		// handle that.
		FILEDESCRIPTORA *fd;
		FILEDESCRIPTORW *fdw;
		if (make_unicode) {
			fdw = fgdw->fgd+i;
			fd = (FILEDESCRIPTORA *)fdw;
		} else {
			fd = fgd->fgd+i;
			fdw = (FILEDESCRIPTORW *)fd;
		}
		PyObject *attr;
		PyObject *sub = PySequence_GetItem(ob, i);
		if (!sub) goto done;
		if (!PyMapping_Check(sub)) {
			PyErr_Format(PyExc_TypeError, "sub-items must be mapping objects (got '%s')",
						 sub->ob_type->tp_name);
			goto loop_failed;
		}
		
		attr = PyMapping_GetItemString(sub, "dwFlags");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags = PyInt_AsLong(attr);
			ok = !PyErr_Occurred();
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;
		
		attr = PyMapping_GetItemString(sub, "clsid");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_CLSID;
			ok = PyWinObject_AsIID(attr, &fd->clsid);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "sizel");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_SIZEPOINT;
			ok = PyArg_ParseTuple(attr, "ii", &fd->sizel.cx, &fd->sizel.cy);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "pointl");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_SIZEPOINT;
			ok = PyArg_ParseTuple(attr, "ii", &fd->pointl.x, &fd->pointl.y);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "dwFileAttributes");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_ATTRIBUTES;
			fd->dwFileAttributes = PyInt_AsLong(attr);
			ok = !PyErr_Occurred();
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "ftCreationTime");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_CREATETIME;
			ok = PyWinObject_AsFILETIME(attr, &fd->ftCreationTime);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "ftLastAccessTime");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_ACCESSTIME;
			ok = PyWinObject_AsFILETIME(attr, &fd->ftLastAccessTime);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "ftLastWriteTime");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_WRITESTIME;
			ok = PyWinObject_AsFILETIME(attr, &fd->ftLastWriteTime);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "nFileSize");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			fd->dwFlags |= FD_FILESIZE;
			ok = PyLong_AsTwoI32(attr, (int *)&fd->nFileSizeHigh, (unsigned *)&fd->nFileSizeLow);
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		attr = PyMapping_GetItemString(sub, "cFileName");
		if (attr==NULL) PyErr_Clear();
		if (attr && attr != Py_None) {
			// no filename flags??
			if (make_unicode) {
				WCHAR *t;
				ok = PyWinObject_AsWCHAR(attr, &t);
				if (ok) {
					wcsncpy(fdw->cFileName, t, sizeof(fdw->cFileName)/sizeof(WCHAR));
					PyWinObject_FreeWCHAR(t);
				}
			} else {
				char *t;
				ok = PyWinObject_AsString(attr, &t);
				if (ok) {
					strncpy(fd->cFileName, t, sizeof(fd->cFileName)/sizeof(char));
					PyWinObject_FreeString(t);
				}
			}
		}
		Py_XDECREF(attr);
		if (!ok) goto loop_failed;

		Py_DECREF(sub);
		continue; // don't fall through to loop error!
loop_failed:
		Py_DECREF(sub);
		goto done;
	}
	// all worked - setup result.
	ret = ret_string;
	ret_string = NULL;
done:
	Py_XDECREF(ret_string);
	return ret;
}

// @pymethod [dict, ...]|shell|StringAsFILEGROUPDESCRIPTOR|Decodes a FILEGROUPDESCRIPTOR packed in a string
static PyObject *PyStringAsFILEGROUPDESCRIPTOR(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	FILEGROUPDESCRIPTORA *fgd = NULL;
	FILEGROUPDESCRIPTORW *fgdw = NULL;
	void *buf;
	int i, num;
	Py_ssize_t cb, size_a, size_w;
	BOOL ok = FALSE;
	// @pyparm buffer|buf||A string packed as either FILEGROUPDESCRIPTORW or FILEGROUPDESCRIPTORW
	// @pyparm bool|make_unicode|-1|Should this be treated as a FILEDESCRIPTORW?  If -1
	// the size of the buffer will be used to make that determination.  Thus, if
	// the buffer is not the exact size of a correct FILEDESCRIPTORW or FILEDESCRIPTORA,
	// you will need to specify this parameter.
	int make_unicode = -1;
	if (!PyArg_ParseTuple(args, "z#|i", &buf, &cb, &make_unicode))
		return NULL;
	if (!buf) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	if (cb < sizeof(UINT))
		return PyErr_Format(PyExc_ValueError, "The buffer is way too small!");
	fgd = (FILEGROUPDESCRIPTORA *)buf;
	fgdw = (FILEGROUPDESCRIPTORW *)buf;
	num = fgd->cItems;
	size_a = sizeof(FILEGROUPDESCRIPTORA) + sizeof(FILEDESCRIPTORA)*(num-1);
	size_w = sizeof(FILEGROUPDESCRIPTORW) + sizeof(FILEDESCRIPTORW)*(num-1);
	if (make_unicode==-1) {
		// allow an extra byte that may be added!
		if (cb==size_a || cb-1==size_a)
			make_unicode = 0;
		else if (cb==size_w || cb-1==size_w)
			make_unicode = 1;
	}
	if (make_unicode==-1)
		return PyErr_Format(PyExc_ValueError,
							"You must specify a value for make_unicode - got a "
							"buffer of %d bytes, but an ascii one would be %d "
							"bytes, and unicode %d bytes", cb, size_a, size_w);
	if ((make_unicode && cb < size_w) || (!make_unicode && cb < size_a)) {
		return PyErr_Format(PyExc_ValueError,
							"The buffer is too small - require %d bytes, but "
							"only %d supplied", make_unicode ? size_w : size_a,
							cb);
	}
	ret = PyList_New(num);
	if (!ret) return NULL;
	for (i=0;i<num;i++) {
		FILEDESCRIPTORA *fd;
		FILEDESCRIPTORW *fdw;
		if (make_unicode) {
			fdw = fgdw->fgd+i;
			fd = (FILEDESCRIPTORA *)fdw;
		} else {
			fd = fgd->fgd+i;
			fdw = (FILEDESCRIPTORW *)fd;
		}
		PyObject *sub = PyDict_New();
		if (!sub) goto done;
		PyObject *val;
		// These structures are the same until the very end, where the
		// unicode/ascii buffer is - so we can use either pointer
		val = PyInt_FromLong(fd->dwFlags);
		if (val) PyDict_SetItemString(sub, "dwFlags", val);
		Py_XDECREF(val);
		
		if (fd->dwFlags & FD_CLSID) {
			val = PyWinObject_FromIID(fd->clsid);
			if (val) PyDict_SetItemString(sub, "clsid", val);
			Py_XDECREF(val);
		}

		if (fd->dwFlags & FD_SIZEPOINT) {
			val = Py_BuildValue("ii", fd->sizel.cx, fd->sizel.cy);
			if (val) PyDict_SetItemString(sub, "sizel", val);
			Py_XDECREF(val);
			val = Py_BuildValue("ii", fd->pointl.x, fd->pointl.y);
			if (val) PyDict_SetItemString(sub, "pointl", val);
			Py_XDECREF(val);
		}

		if (fd->dwFlags & FD_ATTRIBUTES) {
			val = PyInt_FromLong(fd->dwFileAttributes);
			if (val) PyDict_SetItemString(sub, "dwFileAttributes", val);
			Py_XDECREF(val);
		}

		if (fd->dwFlags & FD_CREATETIME) {
			val = PyWinObject_FromFILETIME(fd->ftCreationTime);
			if (val) PyDict_SetItemString(sub, "ftCreationTime", val);
			Py_XDECREF(val);
		}

		if (fd->dwFlags & FD_ACCESSTIME) {
			val = PyWinObject_FromFILETIME(fd->ftLastAccessTime);
			if (val) PyDict_SetItemString(sub, "ftLastAccessTime", val);
			Py_XDECREF(val);
		}

		if (fd->dwFlags & FD_WRITESTIME) {
			val = PyWinObject_FromFILETIME(fd->ftLastWriteTime);
			if (val) PyDict_SetItemString(sub, "ftLastWriteTime", val);
			Py_XDECREF(val);
		}

		if (fd->dwFlags & FD_FILESIZE) {
			val = PyLong_FromTwoInts(fd->nFileSizeHigh, fd->nFileSizeLow);
			if (val) PyDict_SetItemString(sub, "nFileSize", val);
			Py_XDECREF(val);
		}
		// no filename flags??
		if (make_unicode)
			val = PyWinObject_FromWCHAR(fdw->cFileName);
		else
			val = PyString_FromString(fd->cFileName);
		if (val) PyDict_SetItemString(sub, "cFileName", val);
		Py_XDECREF(val);
		PyList_SET_ITEM(ret, i, sub); // 'sub' reference consumed.
	}
	ok = TRUE;
done:
	if (!ok) {
		Py_DECREF(ret);
		ret = NULL;
	}
	return ret;
}

// @pymethod dict|shell|ShellExecuteEx|Performs an operation on a file.
static PyObject *PyShellExecuteEx(PyObject *self, PyObject *args, PyObject *kw)
{
	PyObject *ret = NULL;
	BOOL ok;
	SHELLEXECUTEINFO *p = (SHELLEXECUTEINFO *)malloc(sizeof(*p));
	if (!p)
		return PyErr_NoMemory();

	memset(p, 0, sizeof(*p));
	p->cbSize = sizeof(*p);

	static char *kw_items[]= {
		"fMask","hwnd","lpVerb","lpFile", "lpParameters", "lpDirectory",
		"nShow", "lpIDList", "lpClass", "hkeyClass", "dwHotKey",
		"hIcon", "hMonitor", NULL,
	};
	PyObject *obhwnd=Py_None, *obVerb = NULL, *obFile = NULL, *obParams = NULL;
	PyObject *obDirectory = NULL, *obIDList = NULL, *obClass = NULL;
	PyObject *obhkeyClass = NULL, *obHotKey = NULL, *obhIcon = NULL;
	PyObject *obhMonitor = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|lOOOOOlOOOOOO", kw_items,
							// @pyparm int|fMask|0|The default mask for the
							// structure.  Other masks may be added based on
							// what paramaters are supplied.
		                             &p->fMask, 
									 &obhwnd, // @pyparm <o PyHANDLE>|hwnd|0|
									 &obVerb, // @pyparm string|lpVerb||
									 &obFile, // @pyparm string|lpFile||
									 &obParams, // @pyparm string|lpParams||
									 &obDirectory, // @pyparm string|lpDirectory||
									 &p->nShow, // @pyparm int|nShow|0|
									 &obIDList, // @pyparm <o PyIDL>|lpIDList||
									 &obClass, // @pyparm string|obClass||
									 &obhkeyClass, // @pyparm int|hkeyClass||
									 &obHotKey, // @pyparm int|dwHotKey||
									 &obhIcon, // @pyparm <o PyHANDLE>|hIcon||
									 &obhMonitor)) // @pyparm <o PyHANDLE>|hMonitor||
		goto done;
	if (!PyWinObject_AsHANDLE(obhwnd, (HANDLE *)&p->hwnd, TRUE))
		goto done;
	if (obVerb && !PyWinObject_AsString(obVerb, (char **)&p->lpVerb))
		goto done;
	if (obFile && !PyWinObject_AsString(obFile, (char **)&p->lpFile))
		goto done;
	if (obParams && !PyWinObject_AsString(obParams, (char **)&p->lpParameters))
		goto done;
	if (obDirectory && !PyWinObject_AsString(obDirectory, (char **)&p->lpDirectory))
		goto done;
	if (obIDList) {
		p->fMask |= SEE_MASK_IDLIST;
		if (!PyObject_AsPIDL(obIDList, (ITEMIDLIST **)&p->lpIDList))
			goto done;
	}
	if (obClass) {
		p->fMask |= SEE_MASK_CLASSNAME;
		if (!PyWinObject_AsString(obClass, (char **)&p->lpClass))
			goto done;
	}
	if (obhkeyClass) {
		p->fMask |= SEE_MASK_CLASSKEY;
		if (!PyWinObject_AsHKEY(obhkeyClass, &p->hkeyClass))
			goto done;
	}
	if (obHotKey) {
		p->fMask |= SEE_MASK_HOTKEY;
		p->dwHotKey = PyInt_AsLong(obHotKey);
		if (PyErr_Occurred())
			goto done;
	}
	if (obhIcon) {
// SEE_MASK_ICON is defined around 'if (NTDDI_VERSION < NTDDI_LONGHORN)' and commented as 'not used'
#ifndef SEE_MASK_ICON
		PyErr_SetString(PyExc_NotImplementedError, "SEE_MASK_ICON not declared on this platform");
		goto done;
#else
		p->fMask |= SEE_MASK_ICON;
		if (!PyWinObject_AsHANDLE(obhIcon, &p->hIcon))
			goto done;
#endif
	}
	if (obhMonitor) {
		p->fMask |= SEE_MASK_HMONITOR;
		if (!PyWinObject_AsHANDLE(obhMonitor, &p->hMonitor))
			goto done;
	}
	{ // new scope to avoid warnings about the goto
	PY_INTERFACE_PRECALL;
	ok = ShellExecuteEx(p);
	PY_INTERFACE_POSTCALL;
	}
	if (ok) {
		ret = Py_BuildValue("{s:l,s:N}",
							"hInstApp", p->hInstApp,
							"hProcess", PyWinObject_FromHANDLE(p->hProcess));
		// @rdesc The result is a dictionary based on documented result values
		// in the structure.  Currently this is "hInstApp" and "hProcess"
	} else
		PyWin_SetAPIError("ShellExecuteEx");
	
done:
	PyWinObject_FreeString((char *)p->lpVerb);
	PyWinObject_FreeString((char *)p->lpFile);
	PyWinObject_FreeString((char *)p->lpParameters);
	PyWinObject_FreeString((char *)p->lpDirectory);
	PyWinObject_FreeString((char *)p->lpClass);
	PyObject_FreePIDL((ITEMIDLIST *)p->lpIDList);
	return ret;
}

// @pymethod <o PyIQueryAssociations>|shell|AssocCreate|Creates a <o PyIQueryAssociations> object
static PyObject *PyAssocCreate(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":AssocCreate"))
		return NULL;
	if (pfnAssocCreate==NULL)
		return OleSetOleError(E_NOTIMPL);

	HRESULT hr;
	IQueryAssociations *pRet = NULL;
	PY_INTERFACE_PRECALL;
	hr = (*pfnAssocCreate)(CLSID_QueryAssociations, IID_IQueryAssociations, (void **)&pRet);
	PY_INTERFACE_POSTCALL;

	if (FAILED(hr))
		return PyCom_BuildPyException(hr);
	return PyCom_PyObjectFromIUnknown(pRet, IID_IQueryAssociations, FALSE);
}

// @pymethod <o PyIPropertyBag>|shell|SHGetViewStatePropertyBag|Retrieves an interface for the view state of a folder
// @comm This function will also return IPropertyBag2, but we don't have a python implementation of this interface yet
static PyObject *PySHGetViewStatePropertyBag(PyObject *self, PyObject *args)
{
	LPITEMIDLIST pidl=NULL;
	WCHAR *bagname=NULL;
	DWORD flags;
	IID riid;
	void *output;
	PyObject *obpidl, *obbagname, *obriid, *ret=NULL;
	HRESULT hr;

	if (pfnSHGetViewStatePropertyBag==NULL)
		return OleSetOleError(E_NOTIMPL);
	if (!PyArg_ParseTuple(args, "OOkO",
		&obpidl,		// @pyparm <o PyIDL>|pidl||An item id list that identifies the folder
		&obbagname,		// @pyparm <o PyUnicode>|BagName||Name of the property bag to retrieve
		&flags,			// @pyparm int|Flags||Combination of SHGVSPB_* flags
		&obriid))		// @pyparm <o PyIID>|riid||The interface to return, usually IID_IPropertyBag
		return NULL;
	if (PyWinObject_AsIID(obriid, &riid) &&
		PyWinObject_AsWCHAR(obbagname, &bagname, FALSE) &&
		PyObject_AsPIDL(obpidl, &pidl, FALSE, NULL)){
		PY_INTERFACE_PRECALL;
		hr=(*pfnSHGetViewStatePropertyBag)(pidl, bagname, flags, riid, &output);
		PY_INTERFACE_POSTCALL;
		if (FAILED(hr))
			PyCom_BuildPyException(hr);
		else
			ret=PyCom_PyObjectFromIUnknown((IUnknown *)output, riid, FALSE);
		}

	if (bagname!=NULL)
		PyWinObject_FreeWCHAR(bagname);
	if (pidl!=NULL)
		PyObject_FreePIDL(pidl);
	return ret;
}

// @pymethod (<o PyIDL>, int)|shell|SHILCreateFromPath|Retrieves the PIDL and attributes for a path
// @rdesc Returns the PIDL for the given path and any requested attributes
static PyObject *PySHILCreateFromPath(PyObject *self, PyObject *args)
{
	WCHAR *path=NULL;
	LPITEMIDLIST pidl=NULL;
	DWORD flags;
	PyObject *obpath, *obpidl;
	HRESULT hr;

	if (pfnSHILCreateFromPath==NULL)
		return OleSetOleError(E_NOTIMPL);
	if (!PyArg_ParseTuple(args, "Ok:SHILCreateFromPath", 
		&obpath,		// @pyparm <o PyUnicode>|Path||The path whose PIDL will be returned
		&flags))		// @pyparm int|Flags||A combination of SFGAO_* constants as used with GetAttributesOf
		return NULL;
	if (!PyWinObject_AsWCHAR(obpath, &path, FALSE))
		return NULL;
	PY_INTERFACE_PRECALL;
	hr=SHILCreateFromPath(path, &pidl, &flags);
	PY_INTERFACE_POSTCALL;
	PyWinObject_FreeWCHAR(path);
	obpidl=PyObject_FromPIDL(pidl, TRUE);
	if (obpidl==NULL)
		return NULL;
	return Py_BuildValue("Nk", obpidl, flags);
}


/* List of module functions */
// @module shell|A module, encapsulating the ActiveX Control interfaces
static struct PyMethodDef shell_methods[]=
{
    { "AssocCreate",    PyAssocCreate, 1 }, // @pymeth AssocCreate|Creates a <o PyIQueryAssociations> object
    { "DragQueryFile",    PyDragQueryFile, 1 }, // @pymeth DragQueryFile|Retrieves the file names of dropped files that have resulted from a successful drag-and-drop operation.
    { "DragQueryFileW",   PyDragQueryFileW, 1 }, // @pymeth DragQueryFileW|Retrieves the file names of dropped files that have resulted from a successful drag-and-drop operation.
	{ "DragQueryPoint",   PyDragQueryPoint, 1}, // @pymeth DragQueryPoint|Retrieves the position of the mouse pointer at the time a file was dropped during a drag-and-drop operation.
    { "SHGetPathFromIDList",    PySHGetPathFromIDList, 1 }, // @pymeth SHGetPathFromIDList|Converts an <o PyIDL> to a path.
    { "SHGetPathFromIDListW",   PySHGetPathFromIDListW, 1 }, // @pymeth SHGetPathFromIDListW|Converts an <o PyIDL> to a unicode path.
    { "SHBrowseForFolder",    PySHBrowseForFolder, 1 }, // @pymeth SHBrowseForFolder|Displays a dialog box that enables the user to select a shell folder.
	{ "SHGetFileInfo",        PySHGetFileInfo, 1}, // @pymeth SHGetFileInfo|Retrieves information about an object in the file system, such as a file, a folder, a directory, or a drive root.
    { "SHGetFolderPath", PySHGetFolderPath, 1 }, // @pymeth SHGetFolderPath|Retrieves the path of a folder.
    { "SHSetFolderPath", PySHSetFolderPath, 1 }, // @pymeth SHSetFolderPath|Sets the location of a special folder
	{ "SHGetFolderLocation", PySHGetFolderLocation, 1 }, // @pymeth SHGetFolderLocation|Retrieves the <o PyIDL> of a folder.
    { "SHGetSpecialFolderPath", PySHGetSpecialFolderPath, 1 }, // @pymeth SHGetSpecialFolderPath|Retrieves the path of a special folder.
    { "SHGetSpecialFolderLocation", PySHGetSpecialFolderLocation, 1 }, // @pymeth SHGetSpecialFolderLocation|Retrieves the <o PyIDL> of a special folder.
    { "SHAddToRecentDocs", PySHAddToRecentDocs, 1 }, // @pymeth SHAddToRecentDocs|Adds a document to the shell's list of recently used documents or clears all documents from the list. The user gains access to the list through the Start menu of the Windows taskbar.
    { "SHChangeNotify", PySHChangeNotify, 1 }, // @pymeth SHChangeNotify|Notifies the system of an event that an application has performed. An application should use this function if it performs an action that may affect the shell.
    { "SHEmptyRecycleBin", PySHEmptyRecycleBin, 1 }, // @pymeth SHEmptyRecycleBin|Empties the recycle bin on the specified drive.
	{ "SHQueryRecycleBin", PySHQueryRecycleBin, 1}, // @pymeth SHQueryRecycleBin|Returns the number of items and total size of recycle bin
	{ "SHGetDesktopFolder", PySHGetDesktopFolder, 1}, // @pymeth SHGetDesktopFolder|Retrieves the <o PyIShellFolder> interface for the desktop folder, which is the root of the shell's namespace. 
    { "SHUpdateImage", PySHUpdateImage, 1}, // @pymeth SHUpdateImage|Notifies the shell that an image in the system image list has changed.
    { "SHChangeNotify", PySHChangeNotify, 1}, // @pymeth SHChangeNotify|Notifies the system of an event that an application has performed.
	{ "SHChangeNotifyRegister", PySHChangeNotifyRegister, 1}, // @pymeth SHChangeNotifyRegister|Registers a window that receives notifications from the file system or shell.
	{ "SHChangeNotifyDeregister", PySHChangeNotifyDeregister, 1}, // @pymeth SHChangeNotifyDeregister|Unregisters the client's window process from receiving notification events
	{ "SHGetInstanceExplorer", PySHGetInstanceExplorer, 1}, // @pymeth SHGetInstanceExplorer|Allows components that run in a Web browser (Iexplore.exe) or a nondefault Windows� Explorer (Explorer.exe) process to hold a reference to the process. The components can use the reference to prevent the process from closing prematurely.
	{ "SHFileOperation", PySHFileOperation, 1}, // @pymeth SHFileOperation|Copies, moves, renames, or deletes a file system object.
	{ "StringAsCIDA", PyStringAsCIDA, 1}, // @pymeth StringAsCIDA|Given a CIDA as a raw string, return pidl_folder, [pidl_children, ...]
	{ "CIDAAsString", PyCIDAAsString, 1}, // @pymeth CIDAAsString|Given a (pidl, child_pidls) object, return a CIDA as a string
	{ "StringAsPIDL", PyStringAsPIDL, 1}, // @pymeth StringAsPIDL|Given a PIDL as a raw string, return a PIDL object (ie, a list of strings)
	{ "AddressAsPIDL", PyAddressAsPIDL, 1}, // @pymeth AddressAsPIDL|Given the address of a PIDL, return a PIDL object (ie, a list of strings)
	{ "PIDLAsString", PyPIDLAsString, 1}, // @pymeth PIDLAsString|Given a PIDL object, return the raw PIDL bytes as a string
	{ "SHGetSettings", PySHGetSettings, 1}, // @pymeth SHGetSettings|Retrieves the current shell option settings.
	{ "FILEGROUPDESCRIPTORAsString", PyFILEGROUPDESCRIPTORAsString, 1}, // @pymeth FILEGROUPDESCRIPTORAsString|Creates a FILEGROUPDESCRIPTOR from a sequence of mapping objects, each with FILEDESCRIPTOR attributes
	{ "StringAsFILEGROUPDESCRIPTOR", PyStringAsFILEGROUPDESCRIPTOR, 1}, // @pymeth StringAsFILEGROUPDESCRIPTOR|Decodes a FILEGROUPDESCRIPTOR packed in a string
	{ "ShellExecuteEx", (PyCFunction)PyShellExecuteEx, METH_VARARGS|METH_KEYWORDS}, // @pymeth ShellExecuteEx|Performs an action on a file.
	{ "SHGetViewStatePropertyBag", PySHGetViewStatePropertyBag, METH_VARARGS}, // @pymeth SHGetViewStatePropertyBag|Retrieves an interface for a folder's view state
	{ "SHILCreateFromPath", PySHILCreateFromPath, METH_VARARGS}, // @pymeth SHILCreateFromPath|Returns the PIDL and attributes of a path
	{ NULL, NULL },
};


static const PyCom_InterfaceSupportInfo g_interfaceSupportData[] =
{
	PYCOM_INTERFACE_CLIENT_ONLY       (ShellLink),
	PYCOM_INTERFACE_IID_ONLY		  (ShellLinkA),
	PYCOM_INTERFACE_CLSID_ONLY		  (ShellLink),
	PYCOM_INTERFACE_IID_ONLY		  (ShellLinkW),
	PYCOM_INTERFACE_FULL(AsyncOperation),
	PYCOM_INTERFACE_FULL(ContextMenu),
	PYCOM_INTERFACE_FULL(ExtractIcon),
	PYCOM_INTERFACE_CLIENT_ONLY		(ExtractImage),
	PYCOM_INTERFACE_IID_ONLY		  (ExtractIconW),
	PYCOM_INTERFACE_FULL(ShellExtInit),
	PYCOM_INTERFACE_FULL(ShellFolder),
	PYCOM_INTERFACE_FULL(ShellIcon),
	PYCOM_INTERFACE_FULL(ShellIconOverlay),
	PYCOM_INTERFACE_FULL(ShellIconOverlayIdentifier),
	PYCOM_INTERFACE_FULL(ShellIconOverlayManager),
	PYCOM_INTERFACE_FULL(ShellView),
	PYCOM_INTERFACE_FULL(ShellBrowser),
	PYCOM_INTERFACE_FULL(EnumIDList),
	PYCOM_INTERFACE_FULL(BrowserFrameOptions),
	PYCOM_INTERFACE_FULL(PersistFolder),
	PYCOM_INTERFACE_FULL(ColumnProvider),
	PYCOM_INTERFACE_FULL(DropTargetHelper),
	PYCOM_INTERFACE_CLIENT_ONLY(EmptyVolumeCacheCallBack),
	PYCOM_INTERFACE_CLIENT_ONLY(QueryAssociations),
	PYCOM_INTERFACE_SERVER_ONLY(DeskBand),
	PYCOM_INTERFACE_SERVER_ONLY(DockingWindow),
	PYCOM_INTERFACE_SERVER_ONLY(EmptyVolumeCache),
	PYCOM_INTERFACE_SERVER_ONLY(EmptyVolumeCache2),
	// IID_ICopyHook doesn't exist - hack it up
	{ &IID_IShellCopyHook, "IShellCopyHook", "IID_IShellCopyHook", &PyICopyHook::type, GET_PYGATEWAY_CTOR(PyGCopyHook) },
	{ &IID_IShellCopyHook, "ICopyHook", "IID_ICopyHook", NULL, NULL  },
	PYCOM_INTERFACE_CLIENT_ONLY(ShellLinkDataList),
	PYCOM_INTERFACE_CLIENT_ONLY(UniformResourceLocator),
	PYCOM_INTERFACE_CLIENT_ONLY (ActiveDesktop),
	PYCOM_INTERFACE_CLIENT_ONLY (ActiveDesktopP),
	PYCOM_INTERFACE_CLIENT_ONLY (ADesktopP2),
};

static int AddConstant(PyObject *dict, const char *key, long value)
{
	PyObject *oval = PyInt_FromLong(value);
	if (!oval)
	{
		return 1;
	}
	int rc = PyDict_SetItemString(dict, (char*)key, oval);
	Py_DECREF(oval);
	return rc;
}
static int AddIID(PyObject *dict, const char *key, REFGUID guid)
{
	PyObject *obiid = PyWinObject_FromIID(guid);
	if (!obiid) return 1;
	int rc = PyDict_SetItemString(dict, (char*)key, obiid);
	Py_DECREF(obiid);
	return rc;
}

#define ADD_CONSTANT(tok) AddConstant(dict, #tok, tok)
#define ADD_IID(tok) AddIID(dict, #tok, tok)

/* Module initialisation */
extern "C" __declspec(dllexport) void initshell()
{
	char *modName = "shell";
	PyObject *oModule;
	// Create the module and add the functions
	oModule = Py_InitModule(modName, shell_methods);
	if (!oModule) /* Eeek - some serious error! */
		return;
	PyObject *dict = PyModule_GetDict(oModule);
	if (!dict) return; /* Another serious error!*/

	PyDict_SetItemString(dict, "error", PyWinExc_COMError);
	// Register all of our interfaces, gateways and IIDs.
	PyCom_RegisterExtensionSupport(dict, g_interfaceSupportData, sizeof(g_interfaceSupportData)/sizeof(PyCom_InterfaceSupportInfo));

	// load dll's and function pointers ahead of time
	shell32=GetModuleHandle(TEXT("shell32.dll"));
	if (shell32==NULL)
		shell32 = LoadLibrary(TEXT("shell32.dll"));
	if (shell32!=NULL){
		pfnSHGetFolderLocation=(PFNSHGetFolderLocation)GetProcAddress(shell32, "SHGetFolderLocation");
		pfnSHGetSpecialFolderPath = (PFNSHGetSpecialFolderPath)GetProcAddress(shell32, "SHGetSpecialFolderPathW");
		pfnSHEmptyRecycleBin = (PFNSHEmptyRecycleBin)GetProcAddress(shell32, "SHEmptyRecycleBinA");
		pfnSHQueryRecycleBin = (PFNSHQueryRecycleBin)GetProcAddress(shell32, "SHQueryRecycleBinW");
		pfnSHGetSettings = (PFNSHGetSettings)GetProcAddress(shell32, "SHGetSettings");
		pfnSHGetFolderPath=(PFNSHGetFolderPath)GetProcAddress(shell32, "SHGetFolderPathW");
		// For some reason, SHSetFolderPath is only exported by ordinal SHSetFolderPathA is 231
		pfnSHSetFolderPath=(PFNSHSetFolderPath)GetProcAddress(shell32, (LPCSTR)232);
		pfnSHILCreateFromPath=(PFNSHILCreateFromPath)GetProcAddress(shell32, "SHILCreateFromPath");
		pfnSHShellFolderView_Message=(PFNSHShellFolderView_Message)GetProcAddress(shell32, "SHShellFolderView_Message");
	}
	// SHGetFolderPath comes from shfolder.dll on older systems
	if (pfnSHGetFolderPath==NULL){
		shfolder = GetModuleHandle(TEXT("shfolder.dll"));
		if (shfolder==NULL)
			shfolder = LoadLibrary(TEXT("shfolder.dll"));
		if (shfolder!=NULL)
			pfnSHGetFolderPath=(PFNSHGetFolderPath)GetProcAddress(shfolder, "SHGetFolderPathW");
	}

	shlwapi=GetModuleHandle(TEXT("shlwapi.dll"));
	if (shlwapi==NULL)
		shlwapi=LoadLibrary(TEXT("shlwapi.dll"));
	if (shlwapi!=NULL){
		pfnSHGetViewStatePropertyBag=(PFNSHGetViewStatePropertyBag)GetProcAddress(shlwapi, "SHGetViewStatePropertyBag");
		pfnAssocCreate=(PFNAssocCreate)GetProcAddress(shlwapi, "AssocCreate");
	}

	ADD_CONSTANT(SLR_NO_UI);
	ADD_CONSTANT(SLR_NOLINKINFO);
	ADD_CONSTANT(SLR_INVOKE_MSI);
	ADD_CONSTANT(SLR_ANY_MATCH);
	ADD_CONSTANT(SLR_UPDATE);
	ADD_CONSTANT(SLR_NOUPDATE);
	ADD_CONSTANT(SLR_NOSEARCH);
	ADD_CONSTANT(SLR_NOTRACK);
	ADD_CONSTANT(SLGP_SHORTPATH);
	ADD_CONSTANT(SLGP_UNCPRIORITY);
	ADD_CONSTANT(SLGP_RAWPATH);
	ADD_CONSTANT(HOTKEYF_ALT);
	ADD_CONSTANT(HOTKEYF_CONTROL);
	ADD_CONSTANT(HOTKEYF_EXT);
	ADD_CONSTANT(HOTKEYF_SHIFT);
	ADD_IID(CLSID_ShellLink);
	ADD_IID(CLSID_ShellDesktop);
	ADD_IID(CLSID_NetworkPlaces);
	ADD_IID(CLSID_NetworkDomain);
	ADD_IID(CLSID_NetworkServer);
	ADD_IID(CLSID_NetworkShare);
	ADD_IID(CLSID_MyComputer);
	ADD_IID(CLSID_Internet);
	ADD_IID(CLSID_ShellFSFolder);
	ADD_IID(CLSID_RecycleBin);
	ADD_IID(CLSID_ControlPanel);
	ADD_IID(CLSID_Printers);
	ADD_IID(CLSID_MyDocuments);
	ADD_IID(CLSID_DragDropHelper);
	
	ADD_IID(FMTID_Intshcut);
	ADD_IID(FMTID_InternetSite);

	ADD_IID(CGID_Explorer);
	ADD_IID(CGID_ShellDocView);
	ADD_IID(CLSID_InternetShortcut);
	ADD_IID(CLSID_ActiveDesktop);

#if (_WIN32_IE >= 0x0400)
	ADD_IID(CGID_ShellServiceObject);
	ADD_IID(CGID_ExplorerBarDoc);
	ADD_IID(SID_SShellDesktop);
	ADD_IID(SID_SUrlHistory);
	ADD_IID(SID_SInternetExplorer);
	ADD_IID(SID_SWebBrowserApp);
	ADD_IID(SID_SProgressUI);
	ADD_IID(SID_LinkSite);
	ADD_IID(SID_ShellFolderViewCB);
	ADD_IID(SID_SShellBrowser);
#else
#	pragma message("Please update your SDK headers - IE5 features missing!")
#endif

#if (_WIN32_IE >= 0x0500)
	ADD_IID(FMTID_ShellDetails);
	ADD_IID(FMTID_Storage);
	ADD_IID(FMTID_ImageProperties);
	ADD_IID(FMTID_Displaced);
	ADD_IID(FMTID_Briefcase);
	ADD_IID(FMTID_Misc);
	ADD_IID(FMTID_WebView);
	ADD_IID(FMTID_AudioSummaryInformation);
	ADD_IID(FMTID_Volume);
	ADD_IID(FMTID_Query);
	ADD_IID(FMTID_SummaryInformation);
	ADD_IID(FMTID_MediaFileSummaryInformation);
	ADD_IID(FMTID_ImageSummaryInformation);

#else
#	pragma message("Please update your SDK headers - IE5 features missing!")
#endif
}
