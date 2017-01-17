import datetime
import operator
import sys
import time
import unittest

import pytest
import pywintypes
from pywin32_testutil import str2bytes, ob2memory


class TestCase(unittest.TestCase):
    def testPyTimeFormat(self):
        struct_current = time.localtime()
        pytime_current = pywintypes.Time(struct_current)
        # try and test all the standard parts of the format
        # Note we used to include '%Z' testing, but that was pretty useless as
        # it always returned the local timezone.
        format_strings = "%a %A %b %B %c %d %H %I %j %m %M %p %S %U %w %W %x %X %y %Y"
        for fmt in format_strings.split():
            v1 = pytime_current.Format(fmt)
            v2 = time.strftime(fmt, struct_current)
            assert v1 == v2, "format %s failed - %r != %r" % \
                             (fmt, v1, v2)

    @pytest.mark.xfail
    def testPyTimePrint(self):
        # This used to crash with an invalid, or too early time.
        # We don't really want to check that it does cause a ValueError
        # (as hopefully this wont be true forever).  So either working, or
        # ValueError is OK.
        try:
            t = pywintypes.Time(-2)
            t.Format()
        except ValueError:
            return

    @pytest.mark.xfail
    def testTimeInDict(self):
        d = {}
        d['t1'] = pywintypes.Time(1)
        assert d['t1'] == pywintypes.Time(1)

    @pytest.mark.xfail
    def testPyTimeCompare(self):
        t1 = pywintypes.Time(100)
        t1_2 = pywintypes.Time(100)
        t2 = pywintypes.Time(101)

        assert t1 == t1_2
        assert t1 <= t1_2
        assert t1_2 >= t1

        assert t1 != t2
        assert t1 < t2
        assert t2 > t1

    @pytest.mark.xfail
    def testPyTimeCompareOther(self):
        t1 = pywintypes.Time(100)
        t2 = None
        assert t1 != t2

    def testTimeTuple(self):
        now = datetime.datetime.now()  # has usec...
        # timetuple() lost usec - pt must be <=...
        pt = pywintypes.Time(now.timetuple())
        # *sob* - only if we have a datetime object can we compare like this.
        if isinstance(pt, datetime.datetime):
            assert pt <= now

    @pytest.mark.xfail
    def testTimeTuplems(self):
        now = datetime.datetime.now()  # has usec...
        tt = now.timetuple() + (now.microsecond // 1000,)
        pt = pywintypes.Time(tt)
        # we can't compare if using the old type, as it loses all sub-second
        # res.
        if isinstance(pt, datetime.datetime):
            assert now == pt

    def testPyTimeFromTime(self):
        t1 = pywintypes.Time(time.time())
        assert pywintypes.Time(t1) is t1

    def testGUID(self):
        s = "{00020400-0000-0000-C000-000000000046}"
        iid = pywintypes.IID(s)
        iid2 = pywintypes.IID(ob2memory(iid), True)
        assert iid == iid2
        with pytest.raises(
                ValueError):
            pywintypes.IID(str2bytes('00'),
            True)  # too short
        with pytest.raises(TypeError):
            pywintypes.IID(0, True)  # no buffer

    def testGUIDRichCmp(self):
        s = "{00020400-0000-0000-C000-000000000046}"
        iid = pywintypes.IID(s)
        assert not (s is None)
        assert not (None == s)
        assert s is not None
        assert None != s
        if sys.version_info > (3, 0):
            with pytest.raises(TypeError):
                operator.gt(None, s)
            with pytest.raises(TypeError):
                operator.gt(s, None)
            with pytest.raises(TypeError):
                operator.lt(None, s)
            with pytest.raises(TypeError):
                operator.lt(s, None)

    def testGUIDInDict(self):
        s = "{00020400-0000-0000-C000-000000000046}"
        iid = pywintypes.IID(s)
        d = dict(item=iid)
        assert d['item'] == iid


if __name__ == '__main__':
    unittest.main()
