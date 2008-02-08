// This file implements the IShellItemArray Interface and Gateway for Python.
// Generated by makegw.py

#include "shell_pch.h"
#include "PyIShellItemArray.h"

// @doc - This file contains autoduck documentation
// ---------------------------------------------------
//
// Interface Implementation

PyIShellItemArray::PyIShellItemArray(IUnknown *pdisp):
	PyIUnknown(pdisp)
{
	ob_type = &type;
}

PyIShellItemArray::~PyIShellItemArray()
{
}

/* static */ IShellItemArray *PyIShellItemArray::GetI(PyObject *self)
{
	return (IShellItemArray *)PyIUnknown::GetI(self);
}

// @pymethod |PyIShellItemArray|BindToHandler|Description of BindToHandler.
PyObject *PyIShellItemArray::BindToHandler(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	// @pyparm <o PyIBindCtx>|pbc||Description for pbc
	// @pyparm <o PyIID>|rbhid||Description for rbhid
	// @pyparm <o PyIID>|riid||Description for riid
	PyObject *obpbc;
	PyObject *obrbhid;
	PyObject *obriid;
	IBindCtx *pbc;
	IID rbhid;
	IID riid;
	void *pvOut;
	if ( !PyArg_ParseTuple(args, "OOO:BindToHandler", &obpbc, &obrbhid, &obriid) )
		return NULL;
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyCom_InterfaceFromPyInstanceOrObject(obpbc, IID_IBindCtx, (void **)&pbc, TRUE /* bNoneOK */))
		 bPythonIsHappy = FALSE;
	if (!PyWinObject_AsIID(obrbhid, &rbhid)) bPythonIsHappy = FALSE;
	if (!PyWinObject_AsIID(obriid, &riid)) bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) return NULL;
	HRESULT hr;
	PY_INTERFACE_PRECALL;
	hr = pISIA->BindToHandler( pbc, rbhid, riid, &pvOut );
	if (pbc) pbc->Release();

	PY_INTERFACE_POSTCALL;

	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyCom_PyObjectFromIUnknown((IUnknown *)pvOut, riid, FALSE);
}

// @pymethod |PyIShellItemArray|GetPropertyStore|Description of GetPropertyStore.
PyObject *PyIShellItemArray::GetPropertyStore(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	GETPROPERTYSTOREFLAGS flags;
	// @pyparm int|flags||Description for flags
	// @pyparm <o PyIID>|riid||Description for riid
	PyObject *obriid;
	IID riid;
	void *pv;
	if ( !PyArg_ParseTuple(args, "kO:GetPropertyStore", &flags, &obriid) )
		return NULL;
	if (!PyWinObject_AsIID(obriid, &riid))
		return NULL;
	HRESULT hr;
	PY_INTERFACE_PRECALL;
	hr = pISIA->GetPropertyStore( flags, riid, &pv );
	PY_INTERFACE_POSTCALL;
	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyCom_PyObjectFromIUnknown((IUnknown *)pv, riid, FALSE);
}

// @pymethod |PyIShellItemArray|GetPropertyDescriptionList|Description of GetPropertyDescriptionList.
PyObject *PyIShellItemArray::GetPropertyDescriptionList(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	PROPERTYKEY keyType;
	PyObject *obkeyType;
	// @pyparm <o PyREFPROPERTYKEY>|keyType||Description for keyType
	// @pyparm <o PyIID>|riid||Description for riid
	PyObject *obriid;
	IID riid;
	void *pv;
	if ( !PyArg_ParseTuple(args, "OO:GetPropertyDescriptionList", &obkeyType, &obriid) )
		return NULL;
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyObject_AsSHCOLUMNID(obkeyType, &keyType)) bPythonIsHappy = FALSE;
	if (bPythonIsHappy && !PyWinObject_AsIID(obriid, &riid)) bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) return NULL;
	HRESULT hr;
	PY_INTERFACE_PRECALL;
	hr = pISIA->GetPropertyDescriptionList( keyType, riid, &pv );
	PY_INTERFACE_POSTCALL;
	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyCom_PyObjectFromIUnknown((IUnknown *)pv, riid, FALSE);
}

// @pymethod |PyIShellItemArray|GetAttributes|Description of GetAttributes.
PyObject *PyIShellItemArray::GetAttributes(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	SIATTRIBFLAGS dwAttribFlags;
	// @pyparm int|dwAttribFlags||Description for dwAttribFlags
	SFGAOF sfgaoMask;
	// @pyparm int|sfgaoMask||Description for sfgaoMask
	if ( !PyArg_ParseTuple(args, "kk:GetAttributes", &dwAttribFlags, &sfgaoMask) )
		return NULL;
	SFGAOF result;
	HRESULT hr;
	PY_INTERFACE_PRECALL;
	hr = pISIA->GetAttributes( dwAttribFlags, sfgaoMask, &result );
	PY_INTERFACE_POSTCALL;

	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyLong_FromUnsignedLong(result);
}

// @pymethod |PyIShellItemArray|GetCount|Description of GetCount.
PyObject *PyIShellItemArray::GetCount(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	if ( !PyArg_ParseTuple(args, ":GetCount") )
		return NULL;
	HRESULT hr;
	DWORD dwNumItems;
	PY_INTERFACE_PRECALL;
	hr = pISIA->GetCount( &dwNumItems );
	PY_INTERFACE_POSTCALL;

	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyInt_FromLong(dwNumItems);
}

// @pymethod |PyIShellItemArray|GetItemAt|Description of GetItemAt.
PyObject *PyIShellItemArray::GetItemAt(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	// @pyparm int|dwIndex||Description for dwIndex
	DWORD dwIndex;
	IShellItem *psi;
	if ( !PyArg_ParseTuple(args, "l:GetItemAt", &dwIndex) )
		return NULL;
	HRESULT hr;
	PY_INTERFACE_PRECALL;
	hr = pISIA->GetItemAt( dwIndex, &psi );
	PY_INTERFACE_POSTCALL;
	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyCom_PyObjectFromIUnknown(psi, IID_IShellItem, FALSE);
}

// @pymethod |PyIShellItemArray|EnumItems|Description of EnumItems.
PyObject *PyIShellItemArray::EnumItems(PyObject *self, PyObject *args)
{
	IShellItemArray *pISIA = GetI(self);
	if ( pISIA == NULL )
		return NULL;
	IEnumShellItems *penumShellItems;
	if ( !PyArg_ParseTuple(args, ":EnumItems") )
		return NULL;
	HRESULT hr;
	PY_INTERFACE_PRECALL;
	hr = pISIA->EnumItems( &penumShellItems );

	PY_INTERFACE_POSTCALL;

	if ( FAILED(hr) )
		return PyCom_BuildPyException(hr, pISIA, IID_IShellItemArray );
	return PyCom_PyObjectFromIUnknown(penumShellItems, IID_IEnumShellItems, FALSE);
}

// @object PyIShellItemArray|Description of the interface
static struct PyMethodDef PyIShellItemArray_methods[] =
{
	{ "BindToHandler", PyIShellItemArray::BindToHandler, 1 }, // @pymeth BindToHandler|Description of BindToHandler
	{ "GetPropertyStore", PyIShellItemArray::GetPropertyStore, 1 }, // @pymeth GetPropertyStore|Description of GetPropertyStore
	{ "GetPropertyDescriptionList", PyIShellItemArray::GetPropertyDescriptionList, 1 }, // @pymeth GetPropertyDescriptionList|Description of GetPropertyDescriptionList
	{ "GetAttributes", PyIShellItemArray::GetAttributes, 1 }, // @pymeth GetAttributes|Description of GetAttributes
	{ "GetCount", PyIShellItemArray::GetCount, 1 }, // @pymeth GetCount|Description of GetCount
	{ "GetItemAt", PyIShellItemArray::GetItemAt, 1 }, // @pymeth GetItemAt|Description of GetItemAt
	{ "EnumItems", PyIShellItemArray::EnumItems, 1 }, // @pymeth EnumItems|Description of EnumItems
	{ NULL }
};

PyComTypeObject PyIShellItemArray::type("PyIShellItemArray",
		&PyIUnknown::type,
		sizeof(PyIShellItemArray),
		PyIShellItemArray_methods,
		GET_PYCOM_CTOR(PyIShellItemArray));
// ---------------------------------------------------
//
// Gateway Implementation
STDMETHODIMP PyGShellItemArray::BindToHandler(
		/* [unique][in] */ IBindCtx * pbc,
		/* [in] */ REFGUID rbhid,
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void ** ppvOut)
{
	PY_GATEWAY_METHOD;
	PyObject *obpbc;
	PyObject *obrbhid;
	PyObject *obriid;
	obpbc = PyCom_PyObjectFromIUnknown(pbc, IID_IBindCtx, TRUE);
	obrbhid = PyWinObject_FromIID(rbhid);
	obriid = PyWinObject_FromIID(riid);
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("BindToHandler", &result, "OOO", obpbc, obrbhid, obriid);
	Py_XDECREF(obpbc);
	Py_XDECREF(obrbhid);
	Py_XDECREF(obriid);
	if (FAILED(hr)) return hr;
	// Process the Python results, and convert back to the real params
	PyObject *obppvOut;
	if (!PyArg_Parse(result, "O" , &obppvOut))
		return MAKE_PYCOM_GATEWAY_FAILURE_CODE("BindToHandler");
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyCom_InterfaceFromPyInstanceOrObject(obppvOut, riid, (void **)ppvOut, TRUE /* bNoneOK */))
		 bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) hr = MAKE_PYCOM_GATEWAY_FAILURE_CODE("BindToHandler");
	Py_DECREF(result);
	return hr;
}

STDMETHODIMP PyGShellItemArray::GetPropertyStore(
		/* [in] */ GETPROPERTYSTOREFLAGS flags,
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void ** ppv)
{
	PY_GATEWAY_METHOD;
	PyObject *obriid;
	obriid = PyWinObject_FromIID(riid);
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("GetPropertyStore", &result, "kO", flags, obriid);
	Py_XDECREF(obriid);
	if (FAILED(hr)) return hr;
	// Process the Python results, and convert back to the real params
	PyObject *obppv;
	if (!PyArg_Parse(result, "O" , &obppv))
		return MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetPropertyStore");
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyCom_InterfaceFromPyInstanceOrObject(obppv, riid, (void **)ppv, TRUE /* bNoneOK */))
		 bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) hr = MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetPropertyStore");
	Py_DECREF(result);
	return hr;
}

STDMETHODIMP PyGShellItemArray::GetPropertyDescriptionList(
		/* [in] */ REFPROPERTYKEY keyType,
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void ** ppv)
{
	PY_GATEWAY_METHOD;
	PyObject *obkeyType = PyObject_FromSHCOLUMNID(&keyType);
	if (obkeyType==NULL) return MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetPropertyDescriptionList");
	PyObject *obriid;
	obriid = PyWinObject_FromIID(riid);
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("GetPropertyDescriptionList", &result, "OO", obkeyType, obriid);
	Py_DECREF(obkeyType);
	Py_XDECREF(obriid);
	if (FAILED(hr)) return hr;
	// Process the Python results, and convert back to the real params
	PyObject *obppv;
	if (!PyArg_Parse(result, "O" , &obppv))
		return MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetPropertyDescriptionList");
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyCom_InterfaceFromPyInstanceOrObject(obppv, riid, (void **)ppv, TRUE /* bNoneOK */))
		 bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) hr = MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetPropertyDescriptionList");
	Py_DECREF(result);
	return hr;
}

STDMETHODIMP PyGShellItemArray::GetAttributes(
		/* [in] */ SIATTRIBFLAGS dwAttribFlags,
		/* [in] */ SFGAOF sfgaoMask,
		/* [out] */ SFGAOF * psfgaoAttribs)
{
	PY_GATEWAY_METHOD;
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("GetAttributes", &result, "kk", dwAttribFlags, sfgaoMask);
	if (FAILED(hr)) return hr;
	*psfgaoAttribs = PyLong_AsUnsignedLongMask(result);
	hr = PyCom_SetAndLogCOMErrorFromPyException("GetAttributes", IID_IShellItemArray);
	Py_DECREF(result);
	return hr;

}

STDMETHODIMP PyGShellItemArray::GetCount(
		/* [out] */ DWORD * pdwNumItems)
{
	PY_GATEWAY_METHOD;
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("GetCount", &result);
	if (FAILED(hr)) return hr;
	// Process the Python results, and convert back to the real params
	*pdwNumItems = PyLong_AsUnsignedLong(result);
	hr = PyCom_SetAndLogCOMErrorFromPyException("GetCount", IID_IShellItemArray);
	Py_DECREF(result);
	return hr;
}

STDMETHODIMP PyGShellItemArray::GetItemAt(
		/* [in] */ DWORD dwIndex,
		/* [out] */ IShellItem ** ppsi)
{
	PY_GATEWAY_METHOD;
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("GetItemAt", &result, "l", dwIndex);
	if (FAILED(hr)) return hr;
	// Process the Python results, and convert back to the real params
	PyObject *obppsi;
	if (!PyArg_Parse(result, "O" , &obppsi))
		return MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetItemAt");
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyCom_InterfaceFromPyInstanceOrObject(obppsi, IID_IShellItem, (void **)ppsi, TRUE /* bNoneOK */))
		 bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) hr = MAKE_PYCOM_GATEWAY_FAILURE_CODE("GetItemAt");
	Py_DECREF(result);
	return hr;
}

STDMETHODIMP PyGShellItemArray::EnumItems(
		/* [out] */ IEnumShellItems ** ppenumShellItems)
{
	PY_GATEWAY_METHOD;
	PyObject *result;
	HRESULT hr=InvokeViaPolicy("EnumItems", &result);
	if (FAILED(hr)) return hr;
	// Process the Python results, and convert back to the real params
	PyObject *obppenumShellItems;
	if (!PyArg_Parse(result, "O" , &obppenumShellItems))
		return MAKE_PYCOM_GATEWAY_FAILURE_CODE("EnumItems");
	BOOL bPythonIsHappy = TRUE;
	if (bPythonIsHappy && !PyCom_InterfaceFromPyInstanceOrObject(obppenumShellItems, IID_IEnumShellItems, (void **)ppenumShellItems, TRUE /* bNoneOK */))
		 bPythonIsHappy = FALSE;
	if (!bPythonIsHappy) hr = MAKE_PYCOM_GATEWAY_FAILURE_CODE("EnumItems");
	Py_DECREF(result);
	return hr;
}

