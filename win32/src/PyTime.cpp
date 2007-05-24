//
// PyTime.cpp -- date/time type for Python
//
// @doc
#include "PyWinTypes.h"
#include "PyWinObjects.h"
#ifndef MS_WINCE
#include "time.h"
#endif
#include "tchar.h"

#ifndef NO_PYWINTYPES_TIME

#ifdef MS_WINCE
#include <oleauto.h> // Time conversion functions on CE.
// The Python helpers.
BOOL PyCE_UnixTimeToFileTime(time_t t, LPFILETIME pft);
BOOL PyCE_UnixTimeToSystemTime(time_t t, LPSYSTEMTIME pst);
BOOL PyCE_FileTimeToUnixTime(FILETIME *pft, time_t *pt);
BOOL PyCE_SystemTimeToUnixTime(SYSTEMTIME *pst, time_t *pt);
void PyCE_TimeStructToSystemTime(struct tm *ptm, SYSTEMTIME *pst);
#endif

#if _MSC_VER < 1100
// MSVC < 5.0 headers dont have these
WINOLEAUTAPI_(INT) SystemTimeToVariantTime(LPSYSTEMTIME lpSystemTime, DOUBLE* pvtime);
WINOLEAUTAPI_(INT) VariantTimeToSystemTime(DOUBLE vtime, LPSYSTEMTIME lpSystemTime);
#endif

static WORD SequenceIndexAsWORD(PyObject *seq, int index)
{
	PyObject *t = PySequence_GetItem(seq, index);
	int ret = t ? PyInt_AsLong(t) : -1;
	Py_XDECREF(t);
	return (WORD)ret;
}

PyObject *PyWin_NewTime(PyObject *timeOb)
{
	PyObject *result = NULL;
/*****	  Commented out temporarily
	if ( PyFloat_Check(timeOb) )
	{
		result = new PyTime(PyFloat_AS_DOUBLE((PyFloatObject *)timeOb));
	}
	else *****/
	PyObject *cleanupOb = NULL; // must be xdefref'd.
	// Support objects with a "timetuple" method.
	PyObject *method = PyObject_GetAttrString(timeOb, "timetuple");
	if (method==NULL)
		PyErr_Clear();
	else {
		timeOb = PyEval_CallObject(method, NULL);
		Py_DECREF(method);
		if (!timeOb)
			return NULL;
		cleanupOb = timeOb; // new reference that must be nuked.
		// now we should fall into the sequence check!
	}
	if ( PyNumber_Check(timeOb) )
	{
		// XXX - should possibly check for long_long, as sizeof(time_t) > sizeof(long)
		// on x64
		long t = PyInt_AsLong(timeOb);
		if ( t == -1 )
		{
			if ( !PyErr_Occurred() )
				PyErr_BadArgument();
		} else
			result = new PyTime((time_t)t);
	}
	else if ( PySequence_Check(timeOb) )
	{
		PyErr_Clear(); // ensure stale errors don't trip us.
		if (PySequence_Length(timeOb) < 6)
		{
			Py_XDECREF(cleanupOb);
			return PyErr_Format(PyExc_ValueError, "time tuple must have at least 6 elements");
		}
		SYSTEMTIME	st = {
			SequenceIndexAsWORD(timeOb, 0),
			SequenceIndexAsWORD(timeOb, 1),
			0,
			SequenceIndexAsWORD(timeOb, 2),
			SequenceIndexAsWORD(timeOb, 3),
			SequenceIndexAsWORD(timeOb, 4),
			SequenceIndexAsWORD(timeOb, 5),
			0
		};
		// A Python time tuple has 9 entries.  We allow a 10th to specify ms
		if (PySequence_Length(timeOb) > 9)
			st.wMilliseconds = SequenceIndexAsWORD(timeOb, 9);
		if ( !PyErr_Occurred() )
			result = new PyTime(st);
	}
	else
		// result stays NULL.
		PyErr_Format(PyExc_TypeError, "Objects of type '%s' can not be used as a time object",
		             timeOb->ob_type->tp_name);
	Py_XDECREF(cleanupOb);
	return result;
}

// @pymethod <o PyTime>|pywintypes|Time|Creates a new time object.
PyObject *PyWinMethod_NewTime(PyObject *self, PyObject *args)
{
	PyObject *timeOb;
	// @pyparm object|timeRepr||An integer/float/tuple time representation.
	// @comm Note that the parameter can be any object that supports
	// int(object) - for example , another PyTime object.
	// <nl>The integer should be as defined by the Python time module.
	// See the description of the <o PyTime> object for more information.
	if ( !PyArg_ParseTuple(args, "O", &timeOb) )
		return NULL;

	return PyWin_NewTime(timeOb);
}

PyObject *PyWinObject_FromSYSTEMTIME(const SYSTEMTIME &t)
{
	return new PyTime(t);
}
PyObject *PyWinObject_FromFILETIME(const FILETIME &t)
{
	return new PyTime(t);
}
PyObject *PyWinObject_FromDATE(DATE t)
{
	return new PyTime(t);
}
PyObject *PyWinTimeObject_FromLong(long t)
{
	return new PyTime((time_t)t);
}

// Converts a TimeStamp, which is in 100 nanosecond units like a FILETIME
// See comments in pywintypes.h re LARGE_INTEGER vs TimeStamp
PyObject *PyWinObject_FromTimeStamp(const LARGE_INTEGER &ts)
{
	FILETIME ft;
	ft.dwHighDateTime=ts.HighPart;
	ft.dwLowDateTime=ts.LowPart;
	return new PyTime(ft);
}

BOOL PyWinObject_AsDATE(PyObject *ob, DATE *pDate)
{
	PyObject *newref = NULL;
	BOOL rc;
	if (!PyTime_Check(ob)) {
		if (!(ob = PyWin_NewTime(ob)))
			return FALSE;
		newref = ob;
	}
	rc = ((PyTime *)ob)->GetTime(pDate);
	Py_XDECREF(newref);
	return rc;
}

BOOL PyWinObject_AsFILETIME(PyObject *ob,	FILETIME *pDate)
{
	PyObject *newref = NULL;
	BOOL rc;
	if (!PyTime_Check(ob)) {
		if (!(ob = PyWin_NewTime(ob)))
			return FALSE;
		newref = ob;
	}
	rc = ((PyTime *)ob)->GetTime(pDate);
	Py_XDECREF(newref);
	return rc;
}
BOOL PyWinObject_AsSYSTEMTIME(PyObject *ob, SYSTEMTIME *pDate)
{
	PyObject *newref = NULL;
	BOOL rc;
	if (!PyTime_Check(ob)) {
		if (!(ob = PyWin_NewTime(ob)))
			return FALSE;
		newref = ob;
	}
	rc = ((PyTime *)ob)->GetTime(pDate);
	Py_XDECREF(newref);
	return rc;
}


#define SECS_PER_DAY	(24.0 * 60.0 * 60.0)

#ifndef MS_WINCE

/* the following code is taken from Python 2.3 Modules/datetimemodule.c
 * it is used for calculating day of the year for PyTime::Format
 */

static int _days_before_month[] = {
        0, /* unused; this vector uses 1-based indexing */
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/* year -> 1 if leap year, else 0. */
static int is_leap(int year)
{
        /* Cast year to unsigned.  The result is the same either way, but
         * C can generate faster code for unsigned mod than for signed
         * mod (especially for % 4 -- a good compiler should just grab
         * the last 2 bits when the LHS is unsigned).
         */
        const unsigned int ayear = (unsigned int)year;
        return ayear % 4 == 0 && (ayear % 100 != 0 || ayear % 400 == 0);
}

/* year, month -> number of days in year preceeding first day of month */
static int
days_before_month(int year, int month)
{
        int days;

        assert(month >= 1);
        assert(month <= 12);
        days = _days_before_month[month];
        if (month > 2 && is_leap(year))
                ++days;
        return days;
}

// @pymethod <o PyUnicode>|PyTime|Format|Formats the time value.

PyObject *PyTime::Format(PyObject *self, PyObject *args)
{
	PyObject *obFormat = NULL;
	// @pyparm string|format|%c|The format.  See the comments section for a description of the supported flags.
	if (!PyArg_ParseTuple(args, "|O:Format", &obFormat))
		return NULL;
	TCHAR *fmt = TEXT("%c");
	BOOL bFreeString = FALSE;
	if (obFormat) {
		bFreeString = TRUE;
		if (!PyWinObject_AsTCHAR(obFormat, &fmt))
			return NULL;
	}
	TCHAR szBuffer[256];
	PyTime *pTime = (PyTime *)self;

	SYSTEMTIME	st;
	if ( !VariantTimeToSystemTime(pTime->m_time, &st) )
	{
		PyErr_SetString(PyExc_ValueError, "illegal internal value");
		return NULL;
	}

	struct tm tm = { 0 };
	tm.tm_sec = st.wSecond;
	tm.tm_min = st.wMinute;
	tm.tm_hour = st.wHour;
	tm.tm_mday = st.wDay;
	tm.tm_mon = st.wMonth - 1;
	tm.tm_year = st.wYear - 1900;

	/* Ask windows for the current is_dst flag */
	TIME_ZONE_INFORMATION tzinfo;
	switch (GetTimeZoneInformation(&tzinfo)) {
		case TIME_ZONE_ID_STANDARD:
			tm.tm_isdst = 0;
			break;
		case TIME_ZONE_ID_DAYLIGHT:
			tm.tm_isdst = 1;
			break;
		default:
			tm.tm_isdst = -1;
			break;
	}
	/* tm_wday: day of week (0-6) sunday=0 : weekday(y, m, d) */
	/* tm_yday: day of year (0-365) january 1=0: days_before_month(y, m) + d */
	tm.tm_wday = st.wDayOfWeek;
	tm.tm_yday = days_before_month(st.wYear, st.wMonth) + st.wDay - 1;

	if (!_tcsftime(szBuffer, 256/*_countof()*/, fmt, &tm))
		szBuffer[0] = '\0'; // Better error?
	PyObject *rc = PyWinObject_FromTCHAR(szBuffer);
	if (bFreeString) PyWinObject_FreeTCHAR(fmt);
	return rc;
	// @comm The following format characters are supported.
	// @flagh Character|Description
	// @flag %a|Abbreviated weekday name
	// @flag %A|Full weekday name
	// @flag %b|Abbreviated month name
	// @flag %B|Full month name
	// @flag %c|Date and time representation appropriate for locale
	// @flag %d|Day of month as decimal number (01 - 31)
	// @flag %H|Hour in 24-hour format (00 - 23)
	// @flag %I|Hour in 12-hour format (01 - 12)
	// @flag %j|Day of year as decimal number (001 - 366)
	// @flag %m|Month as decimal number (01 - 12)
	// @flag %M|Minute as decimal number (00 - 59)
	// @flag %p|Current locale's A.M./P.M. indicator for 12-hour clock
	// @flag %S|Second as decimal number (00 - 59)
	// @flag %U|Week of year as decimal number, with Sunday as first day of week (00 - 51)
	// @flag %w|Weekday as decimal number (0 - 6; Sunday is 0)
	// @flag %W|Week of year as decimal number, with Monday as first day of week (00 - 51)
	// @flag %x|Date representation for current locale
	// @flag %X|Time representation for current locale
	// @flag %y|Year without century, as decimal number (00 - 99)
	// @flag %Y|Year with century, as decimal number
	// @flag %z, %Z|Time-zone name or abbreviation; no characters if time zone is unknown
	// @flag %%|Percent sign
	// @comm As in the printf function, the # flag may prefix any formatting code. In that case, the meaning of the format code is changed as follows.
	// @flagh Format Code|Meaning
	// @flag %#a, %#A, %#b, %#B, %#p, %#X, %#z, %#Z, %#%|# flag is ignored.
	// @flag %#c|Long date and time representation, appropriate for current locale. For example: "Tuesday, March 14, 1995, 12:41:29".
	// @flag %#x|Long date representation, appropriate to current locale. For example: "Tuesday, March 14, 1995".
	// @flag %#d, %#H, %#I, %#j, %#m, %#M, %#S, %#U, %#w, %#W, %#y, %#Y|Remove leading zeros (if any).
}

// @object PyTime|A Python object, representing an instant in time.
// @comm A PyTime object is used primarily when exchanging date/time information
// with COM objects or other win32 functions.
// <nl>Using int(timeObject) will return an integer compatible with
// the standard Python time module.
// @ex First import the time module|import time
// @ex To return a simple string|time.ctime(int(timeObject))
// @ex To return a string formatted as the long date in control panel|time.strftime("%#c", time.localtime(int(timeObject)))
// @xref <om pywintypes.Time>

#endif /* MS_WINCE */
static struct PyMethodDef PyTime_methods[] = {
#ifndef MS_WINCE
	{"Format",     PyTime::Format, 1}, 	// @pymeth Format|Formats the time value
#endif
	{NULL}
};

static PyNumberMethods PyTime_NumberMethods =
{
	PyTime::binaryFailureFunc,	/* nb_add */
	PyTime::binaryFailureFunc,	/* nb_subtract */
	PyTime::binaryFailureFunc,	/* nb_multiply */
	PyTime::binaryFailureFunc,	/* nb_divide */
	PyTime::binaryFailureFunc,	/* nb_remainder */
	PyTime::binaryFailureFunc,	/* nb_divmod */
	PyTime::ternaryFailureFunc,	/* nb_power */
	PyTime::unaryFailureFunc,	/* nb_negative */
	PyTime::unaryFailureFunc,	/* nb_positive */
	PyTime::unaryFailureFunc,	/* nb_absolute */
	PyTime::nonzeroFunc,		/* nb_nonzero */
	PyTime::unaryFailureFunc,	/* nb_invert */
	PyTime::binaryFailureFunc,	/* nb_lshift */
	PyTime::binaryFailureFunc,	/* nb_rshift */
	PyTime::binaryFailureFunc,	/* nb_and */
	PyTime::binaryFailureFunc,	/* nb_xor */
	PyTime::binaryFailureFunc,	/* nb_or */
	0,							/* nb_coerce (allowed to be zero) */
	PyTime::intFunc,			/* nb_int */
	PyTime::unaryFailureFunc,	/* nb_long */
	PyTime::floatFunc,			/* nb_float */
	PyTime::unaryFailureFunc,	/* nb_oct */
	PyTime::unaryFailureFunc,	/* nb_hex */
};
// @pymeth __int__|Used when an integer representation of the time object is required.
// @pymeth __float__|Used when a floating point representation of the time object is required.


PYWINTYPES_EXPORT PyTypeObject PyTimeType =
{
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"time",
	sizeof(PyTime),
	0,
	PyTime::deallocFunc,		/* tp_dealloc */
	NULL,		/* tp_print */
	PyTime::getattrFunc,	/* tp_getattr */
	0,						/* tp_setattr */
	// @pymeth __cmp__|Used when time objects are compared.
	PyTime::compareFunc,	/* tp_compare */
	// @pymeth __repr__|Used for repr(ob)
	PyTime::reprFunc, 	/* tp_repr */
	&PyTime_NumberMethods,	/* tp_as_number */
	0,						/* tp_as_sequence */
	0,						/* tp_as_mapping */
	// @pymeth __hash__|Used when the hash value of an time object is required
	PyTime::hashFunc,		/* tp_hash */
	0,						/* tp_call */
	// @pymeth __str__|Used for str(ob)
	PyTime::strFunc,		/* tp_str */
};

PyTime::PyTime(DATE t)
{
	ob_type = &PyTimeType;
	_Py_NewReference(this);
	m_time = t;
}

PyTime::PyTime(time_t t)
{
	ob_type = &PyTimeType;
	_Py_NewReference(this);

#ifdef MS_WINCE
	/* WinCE makes life harder than it should be! */
	FILETIME ftLocal, ftUTC;
	PyCE_UnixTimeToFileTime( (time_t)t, &ftUTC);
	FileTimeToLocalFileTime(&ftUTC, &ftLocal);

	time_t temp_t;

	PyCE_FileTimeToUnixTime(&ftLocal, &temp_t);
	m_time = (double)temp_t;

#else
	/* "Normal" Win32 handling */
	m_time = 0;
	struct tm *ptm = localtime(&t);
	if (ptm != NULL) { // otherwise an invalid integer

		SYSTEMTIME st = {
			ptm->tm_year + 1900,
			ptm->tm_mon + 1,
			ptm->tm_wday,
			ptm->tm_mday,
			ptm->tm_hour,
			ptm->tm_min,
			ptm->tm_sec,
			0
		};
		(void)SystemTimeToVariantTime(&st, &m_time);
	}
#endif /* MS_WINCE */
}

PyTime::PyTime(const SYSTEMTIME &t)
{
	ob_type = &PyTimeType;
	_Py_NewReference(this);
	m_time = 0;
	// Not declared as const systemtime - 
	// better not take the chance!
	SYSTEMTIME nt = t;
	(void)SystemTimeToVariantTime(&nt, &m_time);
}

PyTime::PyTime(const FILETIME &t)
{
	ob_type = &PyTimeType;
	_Py_NewReference(this);
	SYSTEMTIME st;
	m_time = 0;
	FileTimeToSystemTime(&t, &st);
	(void)SystemTimeToVariantTime(&st, &m_time);
}

BOOL PyTime::GetTime(DATE *pDate)
{
	*pDate = m_time;
	return TRUE;
}

BOOL PyTime::GetTime(FILETIME *pDate)
{
	SYSTEMTIME time;
	if (!VariantTimeToSystemTime(m_time, &time)) {
		PyWin_SetAPIError("VariantTimeToSystemTime");
		return FALSE;
	}
	if (!SystemTimeToFileTime(&time, pDate)) {
		PyWin_SetAPIError("SystemTimeToFileTime");
		return FALSE;
	}
	return TRUE;
}

BOOL PyTime::GetTime(SYSTEMTIME *pDate)
{
	if (!VariantTimeToSystemTime(m_time, pDate)) {
		PyWin_SetAPIError("VariantTimeToSystemTime");
		return FALSE;
	}
	return TRUE;
}

PyObject *PyTime::str()
{
	// Just re-use 'Format()'.
	PyObject *args = PyTuple_New(0);
	PyObject *ret = Format(this, args);
	Py_XDECREF(args);
	return ret;
}

int PyTime::compare(PyObject *ob)
{
	DATE time2 = ((PyTime *)ob)->m_time;

	if ( m_time < time2 )
		return -1;
	if ( m_time > time2 )
		return 1;
	return 0;
}

long PyTime::hash(void)
{
	/* arbitrarily use seconds as the hash value */
	return (long)(m_time * SECS_PER_DAY);
}

long PyTime::asLong(void)
{
	SYSTEMTIME	st;

	if ( !VariantTimeToSystemTime(m_time, &st) )
	{
		PyErr_SetString(PyExc_ValueError, "illegal internal value");
		return -1;
	}

	struct tm tm = { 0 };
	tm.tm_sec = st.wSecond;
	tm.tm_min = st.wMinute;
	tm.tm_hour = st.wHour;
	tm.tm_mday = st.wDay;
	tm.tm_mon = st.wMonth - 1;
	tm.tm_year = st.wYear - 1900;
	tm.tm_isdst = -1;	/* have the library figure it out */

#ifdef MS_WINCE
	/* Windows CE hacks! */
	FILETIME ft;
	long t;
	PyCE_TimeStructToSystemTime(&tm, &st);	
	SystemTimeToFileTime(&st, &ft);
	PyCE_FileTimeToUnixTime(&ft, &t);

	return t;
#else
	/* Normal win32 code */
	long result = (long)mktime(&tm);
	if ( result == -1 )
	{
		PyErr_SetString(PyExc_ValueError, "illegal time value");
		return -1;
	}
	return result;
#endif
}

PyObject *PyTime::repr()
{
	TCHAR dateBuf[128];
	TCHAR timeBuf[128];
	SYSTEMTIME st;

	if ( !VariantTimeToSystemTime(m_time, &st) ) 
		_tcscpy(timeBuf, _T("!illegal internal value!"));
	else
	{
		if (0==GetDateFormat(
				LOCALE_USER_DEFAULT,
				DATE_SHORTDATE,
				&st,
				NULL,
				dateBuf,
				sizeof(dateBuf)/sizeof(TCHAR)))
			wsprintf(dateBuf,_T("!GetDateFormat failed (%ld)!"), GetLastError());
		if (0==GetTimeFormat(
				LOCALE_USER_DEFAULT,
				0,
				&st,
				NULL,
				timeBuf,
				sizeof(timeBuf)))
			wsprintf(timeBuf,_T("!GetTimeFormat failed (%ld)!"), GetLastError());
	}

	TCHAR resBuf[160];
	wsprintf(resBuf, _T("<PyTime:%s %s>"), dateBuf, timeBuf);
	return PyString_FromTCHAR(resBuf);
}

PyObject *PyTime::getattr(char *name)
{
	PyObject *res;

	res = Py_FindMethod(PyTime_methods, this, name);
	if (res != NULL)
		return res;
	PyErr_Clear();
	SYSTEMTIME st;
	if (!VariantTimeToSystemTime(m_time, &st)) {
		PyWin_SetAPIError("VariantTimeToSystemTime");
		return NULL;
	}
	// @prop int|year|
	if ( !strcmp(name, "year") )
	{
		return PyInt_FromLong(st.wYear);
	}
	// @prop int|month|
	else if ( !strcmp(name, "month") )
	{
		return PyInt_FromLong(st.wMonth);
	}
	// @prop int|weekday|
	else if ( !strcmp(name, "weekday") )
	{
		return PyInt_FromLong(st.wDayOfWeek);
	}
	// @prop int|day|
	else if ( !strcmp(name, "day") )
	{
		return PyInt_FromLong(st.wDay);
	}
	// @prop int|hour|
	else if ( !strcmp(name, "hour") )
	{
		return PyInt_FromLong(st.wHour);
	}
	// @prop int|minute|
	else if ( !strcmp(name, "minute") )
	{
		return PyInt_FromLong(st.wMinute);
	}
	// @prop int|second|
	else if ( !strcmp(name, "second") )
	{
		return PyInt_FromLong(st.wSecond);
	}
	// @prop int|msec|
	else if ( !strcmp(name, "msec") )
	{
		return PyInt_FromLong(st.wMilliseconds);
	}
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

/*static*/ PyObject *PyTime::unaryFailureFunc(PyObject *ob)
{
	PyErr_SetString(PyExc_TypeError, "bad operand type");
	return NULL;
}
/*static*/ PyObject *PyTime::binaryFailureFunc(PyObject *ob1, PyObject *ob2)
{
	PyErr_SetString(PyExc_TypeError, "bad operand type");
	return NULL;
}
/*static*/ PyObject *PyTime::ternaryFailureFunc(PyObject *ob1, PyObject *ob2, PyObject *ob3)
{
	PyErr_SetString(PyExc_TypeError, "bad operand type");
	return NULL;
}

/*static*/ void PyTime::deallocFunc(PyObject *ob)
{
	delete (PyTime *)ob;
}

// @pymethod string|PyTime|__str__|Used when a (8-bit) string representation of the time object is required.
 PyObject * PyTime::strFunc(PyObject *ob)
{
	return ((PyTime *)ob)->str();
}

// @pymethod int|PyTime|__cmp__|Used when time objects are compared.
int PyTime::compareFunc(PyObject *ob1, PyObject *ob2)
{
	return ((PyTime *)ob1)->compare(ob2);
}

// @pymethod int|PyTime|__hash__|Used when the hash value of an time object is required
long PyTime::hashFunc(PyObject *ob)
{
	return ((PyTime *)ob)->hash();
}


// @pymethod |PyTime|__nonzero__|Used for detecting true/false.
/*static*/ int PyTime::nonzeroFunc(PyObject *ob)
{
	/* always non-zero */
	return 1;
}

// @pymethod |PyTime|__int__|Used when an integer representation of the time object is required.
 PyObject * PyTime::intFunc(PyObject *ob)
{
	long result = ((PyTime *)ob)->asLong();
	if ( result == -1 )
		return NULL;
	return PyInt_FromLong(result);
	// @rdesc The integer result can be used with the time module.
	// Please see the main description for the <o PyTime> object.
}

// @pymethod |PyTime|__float__|Used when a floating point representation of the time object is required.
 PyObject * PyTime::floatFunc(PyObject *ob)
{
	double result;
	((PyTime *)ob)->GetTime(&result);
	return PyFloat_FromDouble(result);
}

// @pymethod |PyTime|__repr__|
PyObject *PyTime::reprFunc(PyObject *ob)
{
	return ((PyTime *)ob)->repr();
}

// @pymethod |PyTime|__getattr__|Used to get an attribute of the object.
/*static*/ PyObject *PyTime::getattrFunc(PyObject *ob, char *attr)
{
	return ((PyTime *)ob)->getattr(attr);
}

#else // NO_PYWINTYPES_TIME
// We dont have a decent time implementation, but
// we need _some_ implementation of these functions!

double PyCE_SystemTimeToCTime(SYSTEMTIME* pstTime)
{
	SYSTEMTIME stBase;
	FILETIME   ftTime;
	FILETIME   ftBase; 
	__int64    iTime;
	__int64    iBase;

	SystemTimeToFileTime(pstTime, &ftTime);

	stBase.wYear         = 1970;
	stBase.wMonth        = 1;
	stBase.wDayOfWeek    = 1;
	stBase.wDay          = 1;
	stBase.wHour         = 0;
	stBase.wMinute       = 0;
	stBase.wSecond       = 0;
	stBase.wMilliseconds = 0;
	SystemTimeToFileTime(&stBase, &ftBase);

	iTime=ftTime.dwHighDateTime;	
	iTime=iTime << 32;				
	iTime |= ftTime.dwLowDateTime;

	iBase=ftBase.dwHighDateTime;
	iBase=iBase << 32;
	iBase |= ftBase.dwLowDateTime;

	return (double)((iTime - iBase) / 10000000L);
}

BOOL PyCE_UnixTimeToFileTime(time_t t, LPFILETIME pft)
{
	// Note that LONGLONG is a 64-bit value
	LONGLONG ll;
	ll = ((__int64)t * 10000000) + 116444736000000000;
//	ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD)ll;
	pft->dwHighDateTime = (DWORD)(ll >> 32);
	return TRUE;
}

// We expose some time functions, but just return
// standard Python floats.  We need a better solution!

PYWINTYPES_EXPORT PyObject *PyWinObject_FromSYSTEMTIME(const SYSTEMTIME &st)
{
	return PyFloat_FromDouble(PyCE_SystemTimeToCTime((SYSTEMTIME *)&st));
}

PYWINTYPES_EXPORT PyObject *PyWinObject_FromFILETIME(const FILETIME &t)
{
	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&t, &st))
		return PyInt_FromLong(-1);
	return PyFloat_FromDouble(PyCE_SystemTimeToCTime(&st));
}
//PYWINTYPES_EXPORT BOOL PyWinObject_AsDATE(PyObject *ob, DATE *pDate)
//{
//}
PYWINTYPES_EXPORT BOOL PyWinObject_AsFILETIME(PyObject *ob,	FILETIME *ft)
{
	PyObject *intOb = PyNumber_Int(ob);
	if (intOb==NULL) return FALSE;
	time_t t = (time_t)PyInt_AsLong(intOb);
	BOOL rc = PyCE_UnixTimeToFileTime(t, ft);
	Py_DECREF(intOb);
	return rc;
}
PYWINTYPES_EXPORT BOOL PyWinObject_AsSYSTEMTIME(PyObject *ob, SYSTEMTIME *st)
{
	FILETIME ft;
	if (!PyWinObject_AsFILETIME(ob, &ft))
		return FALSE;
	if (!FileTimeToSystemTime(&ft, st)) {
		PyErr_SetString(PyExc_TypeError, "The value is out of range for a SYSTEMTIME");
		return FALSE;
	}
	return TRUE;
}


#endif // NO_PYWINTYPES_TIME
