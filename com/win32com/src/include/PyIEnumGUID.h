/////////////////////////////////////////////////////////////////////////////
// class PyIEnumGUID
#ifndef NO_PYCOM_IENUMGUID
class PyIEnumGUID : public PyIEnum
{
public:
	MAKE_PYCOM_CTOR(PyIEnumGUID);
	static PyComEnumTypeObject type;
	static IEnumGUID *GetI(PyObject *self);

	static PyObject *Next(PyObject *self, PyObject *args);
	static PyObject *Skip(PyObject *self, PyObject *args);
	static PyObject *Reset(PyObject *self, PyObject *args);
	static PyObject *Clone(PyObject *self, PyObject *args);

protected:
	PyIEnumGUID(IUnknown *);
	~PyIEnumGUID();
};
#endif // NO_PYCOM_IENUMGUID

/////////////////////////////////////////////////////////////////////////////
// class PyIEnumCATEGORYINFO
#ifndef NO_PYCOM_IENUMCATEGORYINFO
class PyIEnumCATEGORYINFO : public PyIEnum
{
public:
	MAKE_PYCOM_CTOR(PyIEnumCATEGORYINFO);
	static PyComEnumTypeObject type;
	static IEnumCATEGORYINFO *GetI(PyObject *self);

	static PyObject *Next(PyObject *self, PyObject *args);
	static PyObject *Skip(PyObject *self, PyObject *args);
	static PyObject *Reset(PyObject *self, PyObject *args);
	static PyObject *Clone(PyObject *self, PyObject *args);

protected:
	PyIEnumCATEGORYINFO(IUnknown *);
	~PyIEnumCATEGORYINFO();
};
#endif // NO_PYCOM_IENUMCATEGORYINFO

