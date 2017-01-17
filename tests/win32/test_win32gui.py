# tests for win32gui
import array
import operator
import sys
import unittest
import win32gui

import pytest
import pywin32_testutil


# theoretically should be in pywin32_testutil, but this is the only place
# that currently needs such a function...


def ob2bytes(ob):
    if sys.version_info < (3, 0):
        return str(buffer(ob))
    # py3k.
    return bytes(ob)


class TestPyGetString(unittest.TestCase):
    def test_get_string(self):
        # test invalid addresses cause a ValueError rather than crash!
        with pytest.raises(ValueError):
            win32gui.PyGetString(0)
        with pytest.raises(ValueError):
            win32gui.PyGetString(1)
        with pytest.raises(ValueError):
            win32gui.PyGetString(1, 1)


class TestPyGetMemory(unittest.TestCase):
    def test_ob(self):
        # Check the PyGetMemory result and a bytes string can be compared
        test_data = pywin32_testutil.str2bytes("\0\1\2\3\4\5\6")
        c = array.array("b", test_data)
        addr, buflen = c.buffer_info()
        got = win32gui.PyGetMemory(addr, buflen)
        assert len(got) == len(test_data)
        assert ob2bytes(got) == test_data

    @pytest.mark.xfail
    def test_memory_index(self):
        # Check we can index into the buffer object returned by PyGetMemory
        test_data = pywin32_testutil.str2bytes("\0\1\2\3\4\5\6")
        c = array.array("b", test_data)
        addr, buflen = c.buffer_info()
        got = win32gui.PyGetMemory(addr, buflen)
        assert got[0] == pywin32_testutil.str2bytes('\0')

    def test_memory_slice(self):
        # Check we can slice the buffer object returned by PyGetMemory
        test_data = pywin32_testutil.str2bytes("\0\1\2\3\4\5\6")
        c = array.array("b", test_data)
        addr, buflen = c.buffer_info()
        got = win32gui.PyGetMemory(addr, buflen)
        assert got[0:3] == pywin32_testutil.str2bytes('\0\1\2')

    @pytest.mark.xfail
    def test_real_view(self):
        # Do the PyGetMemory, then change the original memory, then ensure
        # the initial object we fetched sees the new value.
        test_data = pywin32_testutil.str2bytes("\0\1\2\3\4\5\6")
        c = array.array("b", test_data)
        addr, buflen = c.buffer_info()
        got = win32gui.PyGetMemory(addr, buflen)
        assert got[0] == pywin32_testutil.str2bytes('\0')
        new = pywin32_testutil.str2bytes('\1')
        c[0] = 1
        assert got[0] == new

    def test_memory_not_writable(self):
        # Check the buffer object fetched by PyGetMemory isn't writable.
        test_data = pywin32_testutil.str2bytes("\0\1\2\3\4\5\6")
        c = array.array("b", test_data)
        addr, buflen = c.buffer_info()
        got = win32gui.PyGetMemory(addr, buflen)
        new = pywin32_testutil.str2bytes('\1')
        with pytest.raises(TypeError):
            operator.setitem(got, 0, new)


if __name__ == '__main__':
    unittest.main()
