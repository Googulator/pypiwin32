# An Python interface to the Scintilla control.
#
# Exposes Python classes that allow you to use Scintilla as
# a "standard" MFC edit control (eg, control.GetTextLength(), control.GetSel()
# plus many Scintilla specific features (eg control.SCIAddStyledText())

from pywin.mfc import window
import win32con
import win32ui
import win32api
import array
import struct
import string
import os
from scintillacon import *

# Load Scintilla.dll to get access to the control.
# We expect to find this in the same directory as win32ui.pyd
try:
	dllid = win32api.LoadLibrary(os.path.join(os.path.split(win32ui.__file__)[0], "Scintilla.DLL"))
except win32api.error: # Not there - lets see if Windows can find it by searching?
	dllid = win32api.LoadLibrary("Scintilla.DLL")

EM_GETTEXTRANGE = 1099
EM_EXLINEFROMCHAR = 1078
EM_FINDTEXTEX = 1103
EM_GETSELTEXT = 1086
EM_EXSETSEL = win32con.WM_USER + 55

class ScintillaControlInterface:
	def SCIAddText(self, text):
		sma = array.array('c', text)
		(a,l) = sma.buffer_info()
		self.SendScintilla(SCI_ADDTEXT, l, a)
	def SCIAddStyledText(self, text, style = None):
		# If style is None, text is assumed to be a "native" Scintilla buffer.
		# If style is specified, text is a normal string, and the style is
		# assumed to apply to the entire string.
		if style is not None:
			text = map(lambda char, style=style: char+chr(style), text)
			text = string.join(text, '')
		sma = array.array("c", text)
		(a,l) = sma.buffer_info()
		self.SendScintilla(SCI_ADDSTYLEDTEXT, l, a)
	def SCIInsertText(self, text, pos=-1):
		sma = array.array('c', text+"\0")
		(a,l) = sma.buffer_info()
		self.SendScintilla(SCI_INSERTTEXT, pos, a)
	def SCISetSavePoint(self):
		self.SendScintilla(SCI_SETSAVEPOINT)
	def SCISetUndoCollection(self, collectFlag):
		self.SendScintilla(SCI_SETUNDOCOLLECTION, collectFlag)
	def SCIAppendUndoStartAction(self):
		self.SendScintilla(SCI_APPENDUNDOSTARTACTION)

	def SCIGetCurrentPos(self):
		return self.SendScintilla(SCI_GETCURRENTPOS)
	def SCIGetCharAt(self, pos):
		# Must ensure char is unsigned!
		return chr(self.SendScintilla(SCI_GETCHARAT, pos) & 0xFF)
	def SCIGotoLine(self, line):
		self.SendScintilla(SCI_GOTOLINE, line)
	def SCIBraceMatch(self, pos, maxReStyle):
		return self.SendScintilla(SCI_BRACEMATCH, pos, maxReStyle)
	def SCIBraceHighlight(self, pos, posOpposite):
		return self.SendScintilla(SCI_BRACEHIGHLIGHT, pos, posOpposite)
	def SCIBraceBadHighlight(self, pos):
		return self.SendScintilla(SCI_BRACEBADLIGHT, pos)

	####################################
	# Styling
	def SCIGetEndStyled(self):
		return self.SendScintilla(SCI_GETENDSTYLED)
	def SCIStyleSetFore(self, num, v):
		return self.SendScintilla(SCI_STYLESETFORE, num, v)
	def SCIStyleSetFont(self, num, name):
		buff = array.array('c', name + "\0")
		addressBuffer = buff.buffer_info()[0]
		self.SendScintilla(SCI_STYLESETFONT, num, addressBuffer)
	def SCIStyleSetBold(self, num, bBold):
		self.SendScintilla(SCI_STYLESETBOLD, num, bBold)
	def SCIStyleSetItalic(self, num, bItalic):
		self.SendScintilla(SCI_STYLESETITALIC, num, bItalic)
	def SCIStyleSetSize(self, num, size):
		self.SendScintilla(SCI_STYLESETSIZE, num, size)
	def SCIGetViewWS(self):
		return self.SendScintilla(SCI_GETVIEWWS)
	def SCISetViewWS(self, val):
		self.SendScintilla(SCI_SETVIEWWS, not (val==0))
		self.InvalidateRect()
	def SCIGetViewEOL(self):
		return self.SendScintilla(SCI_GETVIEWEOL)
	def SCISetViewEOL(self, val):
		self.SendScintilla(SCI_SETVIEWEOL, not(val==0))
		self.InvalidateRect()
	def SCISetTabWidth(self, width):
		self.SendScintilla(SCI_SETTABWIDTH, width, 0)
	def SCIStartStyling(self, pos, mask):
		self.SendScintilla(SCI_STARTSTYLING, pos, mask)
	def SCISetStyling(self, pos, attr):
		self.SendScintilla(SCI_SETSTYLING, pos, attr)
	def SCISetStylingEx(self, ray): # ray is an array.
		address, length = ray.buffer_info()
		self.SendScintilla(SCI_SETSTYLINGEX, length, address)
	def SCIGetStyleAt(self, pos):
		return self.SendScintilla(SCI_GETSTYLEAT, pos)
	def SCISetMarginWidth(self, width):
		self.SendScintilla(SCI_SETMARGINWIDTH, width)
	# Markers
	def SCIMarkerDefine(self, markerNum, markerType):
		self.SendScintilla(SCI_MARKERDEFINE, markerNum, markerType)
	def SCIMarkerSetFore(self, markerNum, fore):
		self.SendScintilla(SCI_MARKERSETFORE, markerNum, fore)
	def SCIMarkerSetBack(self, markerNum, back):
		self.SendScintilla(SCI_MARKERSETBACK, markerNum, back)
	def SCIMarkerAdd(self, lineNo, markerNum):
		self.SendScintilla(SCI_MARKERADD, lineNo, markerNum)
	def SCIMarkerDelete(self, lineNo, markerNum):
		self.SendScintilla(SCI_MARKERDELETE, lineNo, markerNum)
	def SCIMarkerDeleteAll(self, markerNum=-1):
		self.SendScintilla(SCI_MARKERDELETEALL, markerNum)
	def SCIMarkerGet(self, lineNo):
		return self.SendScintilla(SCI_MARKERGET, lineNo)
	def SCIMarkerNext(self, lineNo, markerNum):
		return self.SendScintilla(SCI_MARKERNEXT, lineNo, markerNum)
	def SCICancel(self):
		self.SendScintilla(SCI_CANCEL)
	# AutoComplete
	def SCIAutoCShow(self, text):
		if type(text) in [type([]), type(())]:
			text = string.join(text)
		buff = array.array('c', text + "\0")
		addressBuffer = buff.buffer_info()[0]
		return self.SendScintilla(SCI_AUTOCSHOW, 0, addressBuffer)
	def SCIAutoCCancel(self):
		self.SendScintilla(SCI_AUTOCCANCEL)
	def SCIAutoCActive(self):
		return self.SendScintilla(SCI_AUTOCACTIVE)
	def SCIAutoCComplete(self):
		return self.SendScintilla(SCI_AUTOCCOMPLETE)
	def SCIAutoCStops(self, stops):
		buff = array.array('c', stops + "\0")
		addressBuffer = buff.buffer_info()[0]
		self.SendScintilla(SCI_AUTOCSTOPS, 0, addressBuffer)
	# Call tips
	def SCICallTipShow(self, text, pos=-1):
		if pos==-1: pos = self.GetSel()[0]
		buff = array.array('c', text + "\0")
		addressBuffer = buff.buffer_info()[0]
		self.SendScintilla(SCI_CALLTIPSHOW, pos, addressBuffer)
	def SCICallTipCancel(self):
		self.SendScintilla(SCI_CALLTIPCANCEL)
	def SCICallTipActive(self):
		return self.SendScintilla(SCI_CALLTIPACTIVE)
	def SCICallTipPosStart(self):
		return self.SendScintilla(SCI_CALLTIPPOSSTART)
	def SCINewline(self):
		self.SendScintilla(SCI_NEWLINE)

class CScintillaEditInterface(ScintillaControlInterface):
	def close(self):
		self.colorizer = None
	def Clear(self):
		self.SendScintilla(win32con.WM_CLEAR)
	def Clear(self):
		self.SendScintilla(win32con.WM_CLEAR)
	def FindText(self, flags, range, findText):
		buff = array.array('c', findText + "\0")
		addressBuffer = buff.buffer_info()[0]
		ft = struct.pack('llLll', range[0], range[1], addressBuffer, 0, 0)
		ftBuff = array.array('c', ft)
		addressFtBuff = ftBuff.buffer_info()[0]
		rc = self.SendScintilla(EM_FINDTEXTEX, flags, addressFtBuff)
		ftUnpacked = struct.unpack('llLll', ftBuff.tostring())
		return rc, (ftUnpacked[3], ftUnpacked[4])

	def GetSel(self):
		currentPos = self.SendScintilla(SCI_GETCURRENTPOS)
		anchorPos = self.SendScintilla(SCI_GETANCHOR)
		if currentPos < anchorPos:
			return (currentPos, anchorPos)
		else:
			return (anchorPos, currentPos)
		return currentPos;

	def GetSelText(self):
		start, end = self.GetSel()
		txtBuf = array.array('c', " " * ((end-start)+1))
		addressTxtBuf = txtBuf.buffer_info()[0]
		self.SendScintilla(EM_GETSELTEXT, 0, addressTxtBuf)
		return txtBuf.tostring()[:-1]

	def SetSel(self, start=0, end=None):
		if type(start)==type(()):
			assert end is None, "If you pass a point in the first param, the second must be None"
			start, end = start
		elif end is None: 
			end = start
		if start < 0: start = self.GetTextLength()
		if end < 0: end = self.GetTextLength()
		assert start <= self.GetTextLength(), "The start postion is invalid"
		assert end <= self.GetTextLength(), "The end postion is invalid"
		cr = struct.pack('ll', start, end)
		crBuff = array.array('c', cr)
		addressCrBuff = crBuff.buffer_info()[0]
		rc = self.SendScintilla(EM_EXSETSEL, 0, addressCrBuff)

	def GetLineCount(self):
		return self.SendScintilla(win32con.EM_GETLINECOUNT)

	def LineFromChar(self, charPos=-1):
		if charPos==-1: charPos = self.GetSel()[0]
		assert charPos >= 0 and charPos <= self.GetTextLength(), "The charPos postion is invalid"
		#return self.SendScintilla(EM_EXLINEFROMCHAR, charPos)
		# EM_EXLINEFROMCHAR puts charPos in lParam, not wParam
		return self.SendScintilla(EM_EXLINEFROMCHAR, 0, charPos)
		
	def LineIndex(self, line):
		return self.SendScintilla(win32con.EM_LINEINDEX, line)
		
	def GetCurLineNumber(self):
		return self.LineFromChar(self.SCIGetCurrentPos())
		
	def GetTextLength(self):
		return self.SendScintilla(win32con.WM_GETTEXTLENGTH)

	def GetTextRange(self, start = 0, end = -1):
		if end == -1: end = self.SendScintilla(win32con.WM_GETTEXTLENGTH)
		assert end>=start, "Negative index requested (%d/%d)" % (start, end)
		assert start >= 0 and start <= self.GetTextLength(), "The start postion is invalid"
		assert end >= 0 and end <= self.GetTextLength(), "The end postion is invalid"
		initer = "=" * (end - start + 1)
		buff = array.array('c', initer)
		addressBuffer = buff.buffer_info()[0]
		tr = struct.pack('llL', start, end, addressBuffer)
		trBuff = array.array('c', tr)
		addressTrBuff = trBuff.buffer_info()[0]
		numChars = self.SendScintilla(EM_GETTEXTRANGE, 0, addressTrBuff)
		return buff.tostring()[:numChars]
		
	def ReplaceSel(self, str):
		buff = array.array('c', str + "\0")
		self.SendScintilla(win32con.EM_REPLACESEL, 0, buff.buffer_info()[0]);
		buff = None
	
	def GetLine(self, line=-1):
		if line == -1: line = self.GetCurLineNumber()
		start = self.LineIndex(line)
		end = self.LineIndex(line+1)
		return self.GetTextRange(start, end)

	def SetReadOnly(self, flag = 1):
		return self.SendScintilla(win32con.EM_SETREADONLY, flag)
		
	def LineScroll(self, lines, cols=0):
		return self.SendScintilla(win32con.EM_LINESCROLL, cols, lines)

	def GetFirstVisibleLine(self):
		return self.SendScintilla(win32con.EM_GETFIRSTVISIBLELINE)

	def SetWordWrap(self, mode):
		if mode <> win32ui.CRichEditView_WrapNone:
			raise ValueError, "We dont support word-wrap (I dont think :-)"

class CScintillaColorEditInterface(CScintillaEditInterface):
	################################
	# Plug-in colorizer support
	def _GetColorizer(self):
		if not hasattr(self, "colorizer"):
			self.colorizer = self._MakeColorizer()
		return self.colorizer
	def _MakeColorizer(self):
		# Give parent a chance to hook.
		if hasattr(self.GetParent(), "_MakeColorizer"):
			return self.GetParent()._MakeColorizer()
		import formatter
		return formatter.PythonSourceFormatter(self)

	def Reformat(self, bReload=1):
		c = self._GetColorizer()
		if c is not None: c.Reformat(bReload)

	# The Parent window will normally hook
	def HookStyleNotify(self, parent = None):
		if self._GetColorizer() is not None: # No need if we have no color!
			if parent is None: parent = self.GetParentFrame()
			parent.HookNotify(self.OnStyleNeeded, SCN_STYLENEEDED)

	def OnStyleNeeded(self, std, extra):
		bytes = win32ui.GetBytes( extra, struct.calcsize("iii") )
		endPosPaint, ch, modifiers = struct.unpack("iii", bytes)
		endStyledChar = self.SendScintilla(SCI_GETENDSTYLED)
		lineEndStyled = self.LineFromChar(endStyledChar)
		endStyled = self.LineIndex(lineEndStyled)
		#print "enPosPaint %d endStyledChar %d lineEndStyled %d endStyled %d" % (endPosPaint, endStyledChar, lineEndStyled, endStyled)
		self._GetColorizer().Colorize(endStyled, endPosPaint)

class CScintillaEdit(window.Wnd, CScintillaColorEditInterface):
	def __init__(self, wnd=None):
		if wnd is None:
			wnd = win32ui.CreateWnd()
		window.Wnd.__init__(self, wnd)
	def SendScintilla(self, msg, w=0, l=0):
		return self.SendMessage(msg, w, l)
	def CreateWindow(self, style, rect, parent, id):
		self._obj_.CreateWindow(
				"Scintilla",
				"Scintilla",
				style,
				rect,
				parent,
				id,
				None)

