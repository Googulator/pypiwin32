"""Test pywin32's error semantics"""
import sys
import unittest
import win32api

import pytest
import pythoncom
import pywintypes
import winerror


class TestBase(unittest.TestCase):
    def _testExceptionIndex(self, exc, index, expected):
        # check the exception itself can be indexed if not py3k
        if sys.version_info < (3,):
            assert exc[index] == expected
        # and that exception.args can is the same.
        assert exc.args[index] == expected


class TestAPISimple(TestBase):
    def _getInvalidHandleException(self):
        try:
            win32api.CloseHandle(1)
        except win32api.error as exc:
            return exc
        self.fail("Didn't get invalid-handle exception.")

    def testSimple(self):
        with pytest.raises(pywintypes.error):
            win32api.CloseHandle(1)

    def testErrnoIndex(self):
        exc = self._getInvalidHandleException()
        self._testExceptionIndex(exc, 0, winerror.ERROR_INVALID_HANDLE)

    def testFuncIndex(self):
        exc = self._getInvalidHandleException()
        self._testExceptionIndex(exc, 1, "CloseHandle")

    def testMessageIndex(self):
        exc = self._getInvalidHandleException()
        expected = win32api.FormatMessage(
            winerror.ERROR_INVALID_HANDLE).rstrip()
        self._testExceptionIndex(exc, 2, expected)

    def testUnpack(self):
        try:
            win32api.CloseHandle(1)
            self.fail("expected exception!")
        except win32api.error as exc:
            assert exc.winerror == winerror.ERROR_INVALID_HANDLE
            assert exc.funcname == "CloseHandle"
            expected_msg = win32api.FormatMessage(
                winerror.ERROR_INVALID_HANDLE).rstrip()
            assert exc.strerror == expected_msg

    def testAsStr(self):
        exc = self._getInvalidHandleException()
        err_msg = win32api.FormatMessage(
            winerror.ERROR_INVALID_HANDLE).rstrip()
        # early on the result actually *was* a tuple - it must always look like
        # one
        err_tuple = (winerror.ERROR_INVALID_HANDLE, 'CloseHandle', err_msg)
        assert str(exc) == str(err_tuple)

    def testAsTuple(self):
        exc = self._getInvalidHandleException()
        err_msg = win32api.FormatMessage(
            winerror.ERROR_INVALID_HANDLE).rstrip()
        # early on the result actually *was* a tuple - it must be able to be
        # one
        err_tuple = (winerror.ERROR_INVALID_HANDLE, 'CloseHandle', err_msg)
        if sys.version_info < (3,):
            assert tuple(exc) == err_tuple
        else:
            assert exc.args == err_tuple

    def testClassName(self):
        exc = self._getInvalidHandleException()
        # The error class has always been named 'error'.  That's not ideal :(
        assert exc.__class__.__name__ == "error"

    def testIdentity(self):
        exc = self._getInvalidHandleException()
        assert exc.__class__ is pywintypes.error

    def testBaseClass(self):
        assert pywintypes.error.__bases__ == (Exception,)

    def testAttributes(self):
        exc = self._getInvalidHandleException()
        err_msg = win32api.FormatMessage(
            winerror.ERROR_INVALID_HANDLE).rstrip()
        assert exc.winerror == winerror.ERROR_INVALID_HANDLE
        assert exc.strerror == err_msg
        assert exc.funcname == 'CloseHandle'

    # some tests for 'insane' args.
    def testStrangeArgsNone(self):
        try:
            raise pywintypes.error()
            self.fail("Expected exception")
        except pywintypes.error as exc:
            assert exc.args == ()
            assert exc.winerror == None
            assert exc.funcname == None
            assert exc.strerror == None

    def testStrangeArgsNotEnough(self):
        try:
            raise pywintypes.error("foo")
            self.fail("Expected exception")
        except pywintypes.error as exc:
            assert exc.args[0] == "foo"
            # 'winerror' always args[0]
            assert exc.winerror == "foo"
            assert exc.funcname == None
            assert exc.strerror == None

    def testStrangeArgsTooMany(self):
        try:
            raise pywintypes.error("foo", "bar", "you", "never", "kn", 0)
            self.fail("Expected exception")
        except pywintypes.error as exc:
            assert exc.args[0] == "foo"
            assert exc.args[-1] == 0
            assert exc.winerror == "foo"
            assert exc.funcname == "bar"
            assert exc.strerror == "you"


class TestCOMSimple(TestBase):
    def _getException(self):
        try:
            pythoncom.StgOpenStorage("foo", None, 0)
        except pythoncom.com_error as exc:
            return exc
        self.fail("Didn't get storage exception.")

    def testIs(self):
        assert pythoncom.com_error is pywintypes.com_error

    def testSimple(self):
        with pytest.raises(
                pythoncom.com_error):
            pythoncom.StgOpenStorage("foo",
            None,
            0)

    def testErrnoIndex(self):
        exc = self._getException()
        self._testExceptionIndex(exc, 0, winerror.STG_E_INVALIDFLAG)

    def testMessageIndex(self):
        exc = self._getException()
        expected = win32api.FormatMessage(winerror.STG_E_INVALIDFLAG).rstrip()
        self._testExceptionIndex(exc, 1, expected)

    def testAsStr(self):
        exc = self._getException()
        err_msg = win32api.FormatMessage(winerror.STG_E_INVALIDFLAG).rstrip()
        # early on the result actually *was* a tuple - it must always look like
        # one
        err_tuple = (winerror.STG_E_INVALIDFLAG, err_msg, None, None)
        assert str(exc) == str(err_tuple)

    def testAsTuple(self):
        exc = self._getException()
        err_msg = win32api.FormatMessage(winerror.STG_E_INVALIDFLAG).rstrip()
        # early on the result actually *was* a tuple - it must be able to be
        # one
        err_tuple = (winerror.STG_E_INVALIDFLAG, err_msg, None, None)
        if sys.version_info < (3,):
            assert tuple(exc) == err_tuple
        else:
            assert exc.args == err_tuple

    def testClassName(self):
        exc = self._getException()
        assert exc.__class__.__name__ == "com_error"

    def testIdentity(self):
        exc = self._getException()
        assert exc.__class__ is pywintypes.com_error

    def testBaseClass(self):
        exc = self._getException()
        assert pywintypes.com_error.__bases__ == (Exception,)

    def testAttributes(self):
        exc = self._getException()
        err_msg = win32api.FormatMessage(winerror.STG_E_INVALIDFLAG).rstrip()
        assert exc.hresult == winerror.STG_E_INVALIDFLAG
        assert exc.strerror == err_msg
        assert exc.argerror == None
        assert exc.excepinfo == None

    def testStrangeArgsNone(self):
        try:
            raise pywintypes.com_error()
            self.fail("Expected exception")
        except pywintypes.com_error as exc:
            assert exc.args == ()
            assert exc.hresult == None
            assert exc.strerror == None
            assert exc.argerror == None
            assert exc.excepinfo == None

    def testStrangeArgsNotEnough(self):
        try:
            raise pywintypes.com_error("foo")
            self.fail("Expected exception")
        except pywintypes.com_error as exc:
            assert exc.args[0] == "foo"
            assert exc.hresult == "foo"
            assert exc.strerror == None
            assert exc.excepinfo == None
            assert exc.argerror == None

    def testStrangeArgsTooMany(self):
        try:
            raise pywintypes.com_error("foo", "bar", "you", "never", "kn", 0)
            self.fail("Expected exception")
        except pywintypes.com_error as exc:
            assert exc.args[0] == "foo"
            assert exc.args[-1] == 0
            assert exc.hresult == "foo"
            assert exc.strerror == "bar"
            assert exc.excepinfo == "you"
            assert exc.argerror == "never"


if __name__ == '__main__':
    unittest.main()
