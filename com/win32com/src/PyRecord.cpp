#include "stdafx.h"
#include "PythonCOM.h"

// @doc

// A refugee from pywintypes and should die
PyObject *PyString_FromUnicode( const OLECHAR *str )
{
	if (str==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *uo = PyWinObject_FromOLECHAR(str);
	if (uo==NULL) return NULL;
	PyObject *ret = PyUnicode_EncodeMBCS(PyUnicode_AS_UNICODE(uo), PyUnicode_GET_SIZE(uo), NULL);
	Py_DECREF(uo);
	return ret;
}

#ifdef LINK_AGAINST_RECORDINFO
// Helpers to avoid linking directly to these newer functions
static const IID g_IID_IRecordInfo = IID_IRecordInfo;
HRESULT PySafeArrayGetRecordInfo( SAFEARRAY *  psa, IRecordInfo **  prinfo )
{
	return SafeArrayGetRecordInfo(psa, prinfo);
}
HRESULT PyGetRecordInfoFromGuids( REFGUID g, ULONG major, ULONG minor, LCID lcid, REFGUID gti, IRecordInfo **ppr)
{
	return GetRecordInfoFromGuids( g, major, minor, lcid, gti, ppr);
}
#else

// IID_IRecordInfo = {0000002F-0000-0000-C000-000000000046}
EXTERN_C const GUID g_IID_IRecordInfo \
                = { 0x2f, 0, 0, { 0xC0,0,0,0,0,0,0,0x46 } };

HRESULT PySafeArrayGetRecordInfo( SAFEARRAY *  psa, IRecordInfo **  prinfo )
{
	static HRESULT (STDAPICALLTYPE *pfnSAGRI)(SAFEARRAY *, IRecordInfo **) = NULL;
	if (pfnSAGRI==NULL) {
		HMODULE hmod = GetModuleHandle(_T("oleaut32.dll"));
		if (hmod==NULL)
			return E_NOTIMPL;
		pfnSAGRI = (HRESULT (STDAPICALLTYPE *)(SAFEARRAY *, IRecordInfo **))
			GetProcAddress(hmod, "SafeArrayGetRecordInfo");
		if (pfnSAGRI==NULL)
			return E_NOTIMPL;
	}
	return (*pfnSAGRI)(psa, prinfo);
}
HRESULT PyGetRecordInfoFromGuids( REFGUID g, ULONG major, ULONG minor, LCID lcid, REFGUID gti, IRecordInfo **ppr)
{
	static HRESULT (STDAPICALLTYPE *pfnGRIFG)(REFGUID, ULONG, ULONG, LCID, REFGUID, IRecordInfo **) = NULL;
	if (pfnGRIFG==NULL) {
		HMODULE hmod = GetModuleHandle(_T("oleaut32.dll"));
		if (hmod==NULL)
			return E_NOTIMPL;
		pfnGRIFG = (HRESULT (STDAPICALLTYPE *)(REFGUID, ULONG, ULONG, LCID, REFGUID, IRecordInfo **))
			GetProcAddress(hmod, "GetRecordInfoFromGuids");
		if (pfnGRIFG==NULL)
			return E_NOTIMPL;
	}
	return (*pfnGRIFG)(g, major, minor, lcid, gti, ppr);
}
#endif // LINK_AGAINST_RECORDINFO

// The owner of the record buffer - many records may point here!
class PyRecordBuffer
{
public:
	PyRecordBuffer(int size)
	{
		data = PyMem_Malloc(size);
		if (data==NULL)
			PyErr_NoMemory();
		ref = 0;
	}
	~PyRecordBuffer()
	{
		if (data) PyMem_Free(
#ifdef ANY // Python lost ANY in 2.2, but fails to build with "void *" in 1.5 :(
				(ANY *)
#else
				(void *)
#endif
					data);

	}
	void AddRef() {
		ref++;
	}
	void Release() {
		if (--ref==0) {
			delete this;
			return;
		}
	}
	void *data;
	long ref;
};

// @object PyRecord|An object that represents a COM User Defined Type.
// @comm Once created or obtained from other methods, you can simply
// get and set attributes.
class PyRecord : public PyObject
{
public:
	PyRecord(IRecordInfo *ri, PVOID data, PyRecordBuffer *owner);
	~PyRecord();

	static void tp_dealloc(PyObject *ob);
	static PyObject *tp_getattr(PyObject *self, char *name);
	static int tp_setattr(PyObject *self, char *name, PyObject *v);
	static PyObject *tp_repr(PyObject *self);
	static int tp_compare(PyObject *self, PyObject *other);

	static PyTypeObject Type;
	IRecordInfo *pri;
	void *pdata;
	PyRecordBuffer *owner;
};

BOOL PyRecord_Check(PyObject *ob) {return ((ob)->ob_type == &PyRecord::Type);}

BOOL PyObject_AsVARIANTRecordInfo(PyObject *ob, VARIANT *pv)
{
	if (!PyRecord_Check(ob)) {
		PyErr_SetString(PyExc_TypeError, "Only com_record objects can be used as records");
		return NULL;
	}
	PyRecord *pyrec = (PyRecord *)ob;
	HRESULT hr = pyrec->pri->RecordCreateCopy(pyrec->pdata, &V_RECORD(pv));
	if (FAILED(hr)) {
		PyCom_BuildPyException(hr, pyrec->pri, g_IID_IRecordInfo);
		return FALSE;
	}
	V_RECORDINFO(pv) = pyrec->pri;
	pyrec->pri->AddRef();
	return TRUE;
}

PyObject *PyObject_FromSAFEARRAYRecordInfo(SAFEARRAY *psa)
{
	PyObject *ret = NULL, *ret_tuple = NULL;
	IRecordInfo *info = NULL;
	BYTE *source_data = NULL, *this_dest_data = NULL;
	long lbound, ubound, nelems, i;
	ULONG cb_elem;
	PyRecordBuffer *owner = NULL;
	HRESULT hr = PySafeArrayGetRecordInfo(psa, &info);
	if (FAILED(hr)) goto exit;
	hr = SafeArrayAccessData(psa, (void **)&source_data);
	if (FAILED(hr)) goto exit;
	// Allocate a new chunk of memory
	hr = SafeArrayGetUBound(psa, 1, &ubound);
	if (FAILED(hr)) goto exit;
	hr = SafeArrayGetLBound(psa, 1, &lbound);
	if (FAILED(hr)) goto exit;
	nelems = ubound-lbound;
	hr = info->GetSize(&cb_elem);
	if (FAILED(hr)) goto exit;
	owner = new PyRecordBuffer(nelems * cb_elem);
	if (PyErr_Occurred()) goto exit;
	owner->AddRef(); // unref'd at end - for successful failure cleanup
	ret_tuple = PyTuple_New(nelems);
	if (ret_tuple==NULL) goto exit;
	this_dest_data = (BYTE *)owner->data;
	for (i=0;i<nelems;i++) {
		hr = info->RecordInit(this_dest_data);
		if (FAILED(hr)) goto exit;

		hr = info->RecordCopy(source_data, this_dest_data);
		if (FAILED(hr)) goto exit;
		PyTuple_SET_ITEM(ret_tuple, i, new PyRecord(info, this_dest_data, owner));
		this_dest_data += cb_elem;
		source_data += cb_elem;
	}
	ret = ret_tuple;
	Py_INCREF(ret); // for decref on cleanup.
exit:
	if (FAILED(hr)) {
		if (info)
			PyCom_BuildPyException(hr, info, g_IID_IRecordInfo);
		else
			PyCom_BuildPyException(hr);
		Py_XDECREF(ret);
		ret = NULL;
	}
	if (owner != NULL) owner->Release();
	Py_XDECREF(ret_tuple);
	if (info) info->Release();
	if (source_data!=NULL) SafeArrayUnaccessData(psa);
	return ret;
}
// Creates a new Record by TAKING A COPY of the passed record.
PyObject *PyObject_FromRecordInfo(IRecordInfo *ri, void *data, ULONG cbData)
{
	if ((data != NULL && cbData==0) || (data==NULL && cbData != 0))
		return PyErr_Format(PyExc_RuntimeError, "Both or neither data and size must be given");
	ULONG cb;
	HRESULT hr = ri->GetSize(&cb);
	if (FAILED(hr))
		return PyCom_BuildPyException(hr, ri, g_IID_IRecordInfo);
	if (cbData != 0 && cbData != cb)
		return PyErr_Format(PyExc_ValueError, "Expecting a string of %d bytes (got %d)", cb, cbData);
	PyRecordBuffer *owner = new PyRecordBuffer(cb);
	if (PyErr_Occurred()) { // must be mem error!
		delete owner;
		return NULL;
	}
	hr = ri->RecordInit(owner->data);
	if (FAILED(hr)) {
		delete owner;
		return PyCom_BuildPyException(hr, ri, g_IID_IRecordInfo);
	}
	hr = data==NULL ? 0 : ri->RecordCopy(data, owner->data);
	if (FAILED(hr)) {
		delete owner;
		return PyCom_BuildPyException(hr, ri, g_IID_IRecordInfo);
	}
	return new PyRecord(ri, owner->data, owner);
}

// @pymethod <o PyRecord>|pythoncom|GetRecordFromGuids|Creates a new record object from the given GUIDs
PyObject *pythoncom_GetRecordFromGuids(PyObject *self, PyObject *args)
{
	void *data = NULL;
	PyObject *obGuid, *obInfoGuid, *obdata=Py_None;
	int major, minor, lcid;
	int cb = 0;
	if (!PyArg_ParseTuple(args, "OiiiO|O:GetRecordFromGuids", 
		&obGuid, // @pyparm <o PyIID>|iid||The GUID of the type library
		&major, // @pyparm int|verMajor||The major version number of the type lib.
		&minor, // @pyparm int|verMinor||The minor version number of the type lib.
		&lcid, // @pyparm int|lcid||The LCID of the type lib.
		&obInfoGuid, // @pyparm <o PyIID>|infoIID||The GUID of the record info in the library
		&obdata)) // @pyparm string or buffer|data|None|The raw data to initialize the record with.
		return NULL;
	if (!PyWinObject_AsReadBuffer(obdata, &data, &cb, TRUE))
		return NULL;
	GUID guid, infoGuid;
	if (!PyWinObject_AsIID(obGuid, &guid))
		return NULL;
	if (!PyWinObject_AsIID(obInfoGuid, &infoGuid))
		return NULL;
	IRecordInfo *i = NULL;
	HRESULT hr = PyGetRecordInfoFromGuids(guid, major, minor, lcid, infoGuid, &i);
	if (FAILED(hr))
		return PyCom_BuildPyException(hr);
	PyObject *ret = PyObject_FromRecordInfo(i, data, cb);
	i->Release();
	return ret;
}

// @pymethod <o PyRecord>|pythoncom|GetRecordFromTypeInfo|Creates a new record object from a <o PyITypeInfo> interface
// @comm This function will fail if the specified type info does not have a guid defined
PyObject *pythoncom_GetRecordFromTypeInfo(PyObject *self, PyObject *args)
{
	PyObject *obtypeinfo, *ret;
	ITypeInfo *pITI=NULL;
	IRecordInfo *pIRI=NULL;
	HRESULT hr;
	if (!PyArg_ParseTuple(args, "O:GetRecordFromTypeInfo", 
		&obtypeinfo)) // @pyparm <o PyITypeInfo>|TypeInfo||The type information to be converted into a PyRecord object
		return NULL;
	if (!PyCom_InterfaceFromPyInstanceOrObject(obtypeinfo, IID_ITypeInfo, (void **)&pITI, FALSE))
		return NULL;

	hr=GetRecordInfoFromTypeInfo(pITI, &pIRI);
	if (FAILED(hr))
		ret=PyCom_BuildPyException(hr);
	else
		ret = PyObject_FromRecordInfo(pIRI, NULL, 0);
	pITI->Release();
	if (pIRI!=NULL)
		pIRI->Release();
	return ret;
}

PyRecord::PyRecord(IRecordInfo *ri, PVOID data, PyRecordBuffer *owner)
{
	ob_type = &PyRecord::Type;
	_Py_NewReference(this);
	ri->AddRef();
	pri = ri;
	pdata = data;
	this->owner = owner;
	owner->AddRef();
};

PyRecord::~PyRecord()
{
	owner->Release();
	pri->Release();
}


PyTypeObject PyRecord::Type =
{
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"com_record",
	sizeof(PyRecord),
	0,
	PyRecord::tp_dealloc,		/* tp_dealloc */
	0,		/* tp_print */
	PyRecord::tp_getattr,				/* tp_getattr */
	PyRecord::tp_setattr,				/* tp_setattr */
	PyRecord::tp_compare,				/* tp_compare */
	&PyRecord::tp_repr,				/* tp_repr */
	0,						/* tp_as_number */
	0,						/* tp_as_sequence */
	0,						/* tp_as_mapping */
	0,
	0,						/* tp_call */
	0,		/* tp_str */
};

static PyObject *PyRecord_reduce(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	PyRecord *pyrec = (PyRecord *)self;
	PyObject *obModule = NULL, *obModDict = NULL, *obFunc = NULL;
	ITypeInfo *pti = NULL;
	TYPEATTR *pta = NULL;
	ULONG cb;
	HRESULT hr;
	GUID structguid;
	if (!PyArg_ParseTuple(args, ":reduce"))
		return NULL;
	hr = pyrec->pri->GetTypeInfo(&pti);
	if (FAILED(hr)||pti==NULL) {
		PyCom_BuildPyException(hr);
		goto done;
	}
	hr = pti->GetTypeAttr(&pta);
	if (FAILED(hr)||pta==NULL) {
		PyCom_BuildPyException(hr);
		goto done;
	}
	hr = pyrec->pri->GetGuid(&structguid);
	if (FAILED(hr)) {
		PyCom_BuildPyException(hr);
		goto done;
	}
	hr = pyrec->pri->GetSize(&cb);
	if (FAILED(hr)) {
		PyCom_BuildPyException(hr);
		goto done;
	}
	obModule = PyImport_ImportModule("pythoncom");
	if (obModule)
		obModDict = PyModule_GetDict(obModule); // no ref added!
	if (obModDict)
		obFunc = PyDict_GetItemString(obModDict, "GetRecordFromGuids"); // no ref added!
	if (!obFunc) {
		PyErr_Clear();
		PyErr_SetString(PyExc_RuntimeError, "pythoncom.GetRecordFromGuids() can't be located!");
		goto done;
	}
	ret = Py_BuildValue("O(NHHiNN)",
						obFunc,
						PyWinObject_FromIID(pta->guid),
						pta->wMajorVerNum,
						pta->wMinorVerNum,
						pta->lcid,
						PyWinObject_FromIID(structguid),
						PyString_FromStringAndSize((char *)pyrec->pdata, cb));

done:
	if (pta&& pti)
		pti->ReleaseTypeAttr(pta);
	if (pti) pti->Release();
	Py_XDECREF(obModule);
	// obModDict and obFunc have no new reference.
	return ret;
}

// The object itself.
// Any method names should be "__blah__", as they override
// structure names!
static struct PyMethodDef PyRecord_methods[] = {
	{"__reduce__",      PyRecord_reduce, 1}, // This allows the copy module to work!
	{NULL}
};

static BSTR *_GetFieldNames(IRecordInfo *pri, ULONG *pnum)
{
	ULONG num_names;
	HRESULT hr = pri->GetFieldNames(&num_names, NULL);
	if (FAILED(hr)) {
		PyCom_BuildPyException(hr, pri, g_IID_IRecordInfo);
		return NULL;
	}
	BSTR *strings = new BSTR [num_names];
	if (strings==NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	for (ULONG i = 0; i < num_names; i++)
		strings[i] = NULL;

	hr = pri->GetFieldNames(&num_names, strings);
	if (FAILED(hr)) {
		PyCom_BuildPyException(hr, pri, g_IID_IRecordInfo);
		return NULL;
	}
	*pnum = num_names;
	return strings;
}
static void _FreeFieldNames(BSTR *strings, ULONG num_names)
{
	for (ULONG i = 0; i < num_names; i++)
		SysFreeString(strings[i]);
	delete[] strings;
}

PyObject *PyRecord::tp_repr(PyObject *self)
{
	ULONG i;
	PyRecord *pyrec = (PyRecord *)self;

	ULONG num_names;
	PyObject *s = PyString_FromString("com_struct{");

	BSTR *strings = _GetFieldNames(pyrec->pri, &num_names);
	if (strings==NULL) return NULL;

	PyObject *comma = PyString_FromString(", ");
	PyObject *equals = PyString_FromString(" = ");
	for (i = 0; i < num_names && s != NULL; i++) {
		char *name;
		if (PyWin_WCHAR_AsString(strings[i], (DWORD)-1, &name)) {
			if (i > 0)
				PyString_Concat(&s, comma);
			PyString_ConcatAndDel(&s, PyString_FromString(name));
			PyString_Concat(&s, equals);
			PyObject *sub_object = PyRecord::tp_getattr(self, name);
			PyString_ConcatAndDel(&s, PyObject_Repr(sub_object));
			Py_XDECREF(sub_object);
			PyWinObject_FreeString(name);
		}
	}
	Py_XDECREF(comma);
	Py_XDECREF(equals);
	PyString_ConcatAndDel(&s, PyString_FromString("}"));
	_FreeFieldNames(strings, num_names);
	return s;
}

PyObject *PyRecord::tp_getattr(PyObject *self, char *name)
{
	PyObject *res;
	PyRecord *pyrec = (PyRecord *)self;

	if (strcmp(name, "__members__")==0) {
		ULONG cnames = 0;
		HRESULT hr = pyrec->pri->GetFieldNames(&cnames, NULL);
		if (FAILED(hr))
			return PyCom_BuildPyException(hr, pyrec->pri, g_IID_IRecordInfo);
		BSTR *strs = (BSTR *)malloc(sizeof(BSTR) * cnames);
		if (strs==NULL)
			return PyErr_NoMemory();
		hr = pyrec->pri->GetFieldNames(&cnames, strs);
		if (FAILED(hr)) {
			free(strs);
			return PyCom_BuildPyException(hr, pyrec->pri, g_IID_IRecordInfo);
		}
		PyObject *ret = PyList_New(cnames);
		for (ULONG i=0;i<cnames && ret != NULL;i++) {
			PyObject *item = PyString_FromUnicode(strs[i]);
			SysFreeString(strs[i]);
			if (item==NULL) {
				Py_DECREF(ret);
				ret = NULL;
			} else
				PyList_SET_ITEM(ret, i, item); // ref count swallowed.
		}
		free(strs);
		return ret;
	}
	res = Py_FindMethod(PyRecord_methods, self, name);
	if (res != NULL)
		return res;
	PyErr_Clear();
	WCHAR *wname;
	if (!PyWin_String_AsWCHAR(name, (DWORD)-1, &wname))
		return NULL;

	VARIANT vret;
	VariantInit(&vret);
	void *sub_data = NULL;

	PY_INTERFACE_PRECALL;
	HRESULT hr = pyrec->pri->GetFieldNoCopy(pyrec->pdata, wname, &vret, &sub_data);
	PY_INTERFACE_POSTCALL;

	if (FAILED(hr)) {
		if (hr == TYPE_E_FIELDNOTFOUND){
			PyErr_Format(PyExc_AttributeError,
				"This record has no field named '%s'", wname);
			PyWinObject_FreeString(wname);
			return NULL;
			}
		PyWinObject_FreeString(wname);
		return PyCom_BuildPyException(hr, pyrec->pri, g_IID_IRecordInfo);
	}

	PyWinObject_FreeString(wname);
	// Short-circuit sub-structs and arrays here, so we dont allocate a new chunk
	// of memory and copy it - we need sub-structs to persist.
	if (V_VT(&vret)==(VT_BYREF | VT_RECORD))
		return new PyRecord(V_RECORDINFO(&vret), V_RECORD(&vret), pyrec->owner);
	else if (V_VT(&vret)==(VT_BYREF | VT_ARRAY | VT_RECORD)) {
		SAFEARRAY *psa = *V_ARRAYREF(&vret);
		int d = SafeArrayGetDim(psa);
		if (sub_data==NULL)
			return PyErr_Format(PyExc_RuntimeError, "Did not get a buffer for the array!");
		if (SafeArrayGetDim(psa) != 1)
			return PyErr_Format(PyExc_TypeError, "Only support single dimensional arrays of records");
		IRecordInfo *sub = NULL;
		long ubound, lbound, nelems;
		int i;
		BYTE *this_data;
		PyObject *ret_tuple = NULL;
		ULONG element_size = 0;
		hr = SafeArrayGetUBound(psa, 1, &ubound);
		if (FAILED(hr)) goto array_end;
		hr = SafeArrayGetLBound(psa, 1, &lbound);
		if (FAILED(hr)) goto array_end;
		hr = PySafeArrayGetRecordInfo(psa, &sub);
		if (FAILED(hr)) goto array_end;
		hr = sub->GetSize(&element_size);
		if (FAILED(hr)) goto array_end;
		nelems = ubound-lbound;
		ret_tuple = PyTuple_New(nelems);
		if (ret_tuple==NULL) goto array_end;
		this_data = (BYTE *)sub_data;
		for (i=0;i<nelems;i++) {
			PyTuple_SET_ITEM(ret_tuple, i, new PyRecord(sub, this_data, pyrec->owner));
			this_data += element_size;
		}
array_end:
		if (sub)
			sub->Release();
		if (FAILED(hr)) 
			return PyCom_BuildPyException(hr, pyrec->pri, g_IID_IRecordInfo);
		return ret_tuple;
	}

	// The rest of the object as passed as "BYREF VT_INT, or BYREF VT_STRING"
	// This default conversion we use is a little slow (but it will do!)
	PyObject *ret = PyCom_PyObjectFromVariant(&vret);
//	VariantClear(&vret);
	return ret;
}

int PyRecord::tp_setattr(PyObject *self, char *name, PyObject *v)
{
	VARIANT val;
	PyRecord *pyrec = (PyRecord *)self;

	if (!PyCom_VariantFromPyObject(v, &val))
		return -1;

	WCHAR *wname;
	if (!PyWin_String_AsWCHAR(name, (DWORD)-1, &wname))
		return -1;

	PY_INTERFACE_PRECALL;
	HRESULT hr = pyrec->pri->PutField(INVOKE_PROPERTYPUT, pyrec->pdata, wname, &val);
	PY_INTERFACE_POSTCALL;
	PyWinObject_FreeString(wname);
	VariantClear(&val);
	if (FAILED(hr)) {
		PyCom_BuildPyException(hr, pyrec->pri, g_IID_IRecordInfo);
		return -1;
	}
	return 0;
}

int PyRecord::tp_compare(PyObject *self, PyObject *other)
{
	PyRecord *pyself = (PyRecord *)self;
	PyRecord *pyother = (PyRecord *)other;
	if (!pyself->pri->IsMatchingType(pyother->pri)) {
		// Not matching types - just compare the object addresses!
		// (doesnt matter what we use here, as long as it is consistent)
		return pyself->pdata < pyother->pdata ? -1 : 1;
	}
	// Need to do a recursive compare, as some elements may be pointers
	// (eg, strings, objects)
	ULONG num_names;
	BSTR *strings = _GetFieldNames(pyself->pri, &num_names);
	if (strings==NULL) return NULL;
	int ret;
	for (ULONG i=0;i<num_names;i++) {
		ret = 0;
		char *name;
		if (PyWin_WCHAR_AsString(strings[i], (DWORD)-1, &name)) {
			PyObject *self_sub = PyRecord::tp_getattr(self, name);
			if (self_sub) {
				PyObject *other_sub = PyRecord::tp_getattr(other, name);
				if (other_sub) {
					ret = PyObject_Compare(self_sub, other_sub);
					Py_DECREF(other_sub);
				}
				Py_DECREF(self_sub);
			}
			PyWinObject_FreeString(name);
		}
		else
			PyErr_Clear();
		if (ret != 0)
			break;
	}
	_FreeFieldNames(strings, num_names);
	return ret;
}

void PyRecord::tp_dealloc(PyObject *ob)
{
	delete (PyRecord *)ob;
}
