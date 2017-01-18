# Test module for win32timezone

import doctest
import unittest

import pytest
import win32timezone


class Win32TimeZoneTest(unittest.TestCase):
    @pytest.mark.xfail
    def testWin32TZ(self):
        failed, total = doctest.testmod(win32timezone, verbose=False)
        assert not failed


if __name__ == '__main__':
    unittest.main()
