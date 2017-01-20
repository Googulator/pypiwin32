# Originally contributed by Stefan Schukat as part of this arbitrary-sized
# arrays patch.
import pytest
from win32com.client import gencache
from win32com.test import util

ZeroD = 0
OneDEmpty = []
OneD = [1, 2, 3]
TwoD = [
    [1, 2, 3],
    [1, 2, 3],
    [1, 2, 3]
]

TwoD1 = [
    [
        [1, 2, 3, 5],
        [1, 2, 3],
        [1, 2, 3]
    ],
    [
        [1, 2, 3],
        [1, 2, 3],
        [1, 2, 3]
    ]
]

OneD1 = [
    [
        [1, 2, 3],
        [1, 2, 3],
        [1, 2, 3]
    ],
    [
        [1, 2, 3],
        [1, 2, 3]
    ]
]

OneD2 = [
    [1, 2, 3],
    [1, 2, 3, 4, 5],
    [
        [1, 2, 3, 4, 5],
        [1, 2, 3, 4, 5],
        [1, 2, 3, 4, 5]
    ]
]

ThreeD = [
    [
        [1, 2, 3],
        [1, 2, 3],
        [1, 2, 3]
    ],
    [
        [1, 2, 3],
        [1, 2, 3],
        [1, 2, 3]
    ]
]

FourD = [
    [
        [[1, 2, 3], [1, 2, 3], [1, 2, 3]],
        [[1, 2, 3], [1, 2, 3], [1, 2, 3]],
        [[1, 2, 3], [1, 2, 3], [1, 2, 3]]
    ],
    [
        [[1, 2, 3], [1, 2, 3], [1, 2, 3]],
        [[1, 2, 3], [1, 2, 3], [1, 2, 3]],
        [[1, 2, 3], [1, 2, 3], [1, 2, 3]]
    ]
]

LargeD = [
             [[list(range(10))] * 10],
         ] * 512


def _normalize_array(a):
    if not isinstance(a, type(())):
        return a
    ret = []
    for i in a:
        ret.append(_normalize_array(i))
    return ret


class ArrayTest(util.TestCase):
    def setUp(self):
        self.arr = gencache.EnsureDispatch("PyCOMTest.ArrayTest")

    def tearDown(self):
        self.arr = None

    def _doTest(self, array):
        self.arr.Array = array
        assert _normalize_array(self.arr.Array) == array

    @pytest.mark.xfail
    def testZeroD(self):
        self._doTest(ZeroD)

    @pytest.mark.xfail
    def testOneDEmpty(self):
        self._doTest(OneDEmpty)

    @pytest.mark.xfail
    def testOneD(self):
        self._doTest(OneD)

    @pytest.mark.xfail
    def testTwoD(self):
        self._doTest(TwoD)

    @pytest.mark.xfail
    def testThreeD(self):
        self._doTest(ThreeD)

    @pytest.mark.xfail
    def testFourD(self):
        self._doTest(FourD)

    @pytest.mark.xfail
    def testTwoD1(self):
        self._doTest(TwoD1)

    @pytest.mark.xfail
    def testOneD1(self):
        self._doTest(OneD1)

    @pytest.mark.xfail
    def testOneD2(self):
        self._doTest(OneD2)

    @pytest.mark.xfail
    def testLargeD(self):
        self._doTest(LargeD)


if __name__ == "__main__":
    try:
        util.testmain()
    except SystemExit as rc:
        if not rc:
            raise
