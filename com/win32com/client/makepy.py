# Originally written by Curt Hagenlocher, and various bits
# and pieces by Mark Hammond (and now Greg Stein has had
# a go too :-)

# Note that the main worker code has been moved to genpy.py
# As this is normally run from the command line, it reparses the code each time.  
# Now this is nothing more than the command line handler and public interface.

# XXX - TO DO
# XXX - Greg and Mark have some ideas for a revamp - just no
#       time - if you want to help, contact us for details.
#       Main idea is to drop the classes exported and move to a more
#       traditional data driven model.

"""Generate a .py file from an OLE TypeLibrary file.


 This module is concerned only with the actual writing of
 a .py file.  It draws on the @build@ module, which builds 
 the knowledge of a COM interface.
 
"""
usageHelp = """ \
Usage:

  makepy.py [-h] [-x0|1] [-u] [-o filename] [-d] [typelib, ...]
  
  typelib -- A TLB, DLL, OCX, Description, or possibly something else.
  -h    -- Do not generate hidden methods.
  -u    -- Python 1.5 and earlier: Do not convert all Unicode objects to strings.
           Python 1.6 and later: Do convert all Unicode objects to strings.
  -o outputFile -- Generate to named file - dont generate to standard directory.
  -i [typelib] -- Show info for specified typelib, or select the typelib if not specified.
  -v    -- Verbose output
  -q    -- Quiet output
  -d    -- Generate the base code now and classes code on demand
  
Examples:
  makepy.py
    Present a list of type libraries.
    
  makepy.py "Microsoft Excel 8.0 Object Library"
    Generate support for the typelibrary with the specified description
    (in this case, MS Excel object model)

"""

import genpy, string, sys, os, types, pythoncom
import selecttlb
import gencache
from win32com.client import NeedUnicodeConversions

bForDemandDefault = 0 # Default value of bForDemand - toggle this to change the world - see also gencache.py

error = "makepy.error"

def usage():
	sys.stderr.write (usageHelp)
	sys.exit(2)

def ShowInfo(spec):
	if not spec:
		tlbSpec = selecttlb.SelectTlb(excludeFlags=selecttlb.FLAG_HIDDEN)
		if tlbSpec is None:
			return
		try:
			tlb = pythoncom.LoadRegTypeLib(tlbSpec.clsid, tlbSpec.major, tlbSpec.minor, tlbSpec.lcid)
		except pythoncom.com_error: # May be badly registered.
			sys.stderr.write("Warning - could not load registered typelib '%s'\n" % (tlbSpec.clsid))
			tlb = None
		
		infos = [(tlb, tlbSpec)]
	else:
		infos = GetTypeLibsForSpec(spec)
	for (tlb, tlbSpec) in infos:
		desc = tlbSpec.desc
		if desc is None:
			if tlb is None:
				desc = "<Could not load typelib %s>" % (tlbSpec.dll)
			else:
				desc = tlb.GetDocumentation(-1)[0]
		print desc
		print " %s, lcid=%s, major=%s, minor=%s" % (tlbSpec.clsid, tlbSpec.lcid, tlbSpec.major, tlbSpec.minor)
		print " >>> # Use these commands in Python code to auto generate .py support"
		print " >>> from win32com.client import gencache"
		print " >>> gencache.EnsureModule('%s', %s, %s, %s)" % (tlbSpec.clsid, tlbSpec.lcid, tlbSpec.major, tlbSpec.minor)

class SimpleProgress(genpy.GeneratorProgress):
	"""A simple progress class prints its output to stderr
	"""
	def __init__(self, verboseLevel):
		self.verboseLevel = verboseLevel
	def Close(self):
		pass
	def Finished(self):
		if self.verboseLevel>1:
			sys.stderr.write("Generation complete..\n")
	def SetDescription(self, desc, maxticks = None):
		if self.verboseLevel:
			sys.stderr.write(desc + "\n")
	def Tick(self, desc = None):
		pass

	def VerboseProgress(self, desc, verboseLevel = 2):
		if self.verboseLevel >= verboseLevel:
			sys.stderr.write(desc + "\n")

	def LogBeginGenerate(self, filename):
		self.VerboseProgress("Generating to %s" % filename, 1)
	
	def LogWarning(self, desc):
		self.VerboseProgress("WARNING: " + desc, 1)

class GUIProgress(SimpleProgress):
	def __init__(self, verboseLevel):
		# Import some modules we need to we can trap failure now.
		import win32ui, pywin
		SimpleProgress.__init__(self, verboseLevel)
		self.dialog = None
		
	def Close(self):
		if self.dialog is not None:
			self.dialog.Close()
			self.dialog = None

	def Starting(self, tlb_desc):
		SimpleProgress.Starting(self, tlb_desc)
		if self.dialog is None:
			from pywin.dialogs import status
			self.dialog=status.ThreadedStatusProgressDialog(tlb_desc)
		else:
			self.dialog.SetTitle(tlb_desc)
		
	def SetDescription(self, desc, maxticks = None):
		self.dialog.SetText(desc)
		if maxticks:
			self.dialog.SetMaxTicks(maxticks)

	def Tick(self, desc = None):
		self.dialog.Tick()
		if desc is not None:
			self.dialog.SetText(desc)

def GetTypeLibsForSpec(arg):
	"""Given an argument on the command line (either a file name or a library description)
	return a list of actual typelibs to use.
	"""
	typelibs = []
	try:
		try:
			tlb = pythoncom.LoadTypeLib(arg)
			spec = selecttlb.TypelibSpec(None, 0,0,0)
			spec.FromTypelib(tlb, arg)
			typelibs.append((tlb, spec))
		except pythoncom.com_error:
			# See if it is a description
			tlbs = selecttlb.FindTlbsWithDescription(arg)
			if len(tlbs)==0:
				print "Could not locate a type library matching '%s'" % (arg)
			for spec in tlbs:
				# Version numbers not always reliable if enumerated from registry.
				# (as some libs use hex, other's dont.  Both examples from MS, of course.)
				if spec.dll is None:
					tlb = pythoncom.LoadRegTypeLib(spec.clsid, spec.major, spec.minor, spec.lcid)
				else:
					tlb = pythoncom.LoadTypeLib(spec.dll)
				
				# We have a typelib, but it may not be exactly what we specified
				# (due to automatic version matching of COM).  So we query what we really have!
				attr = tlb.GetLibAttr()
				spec.major = attr[3]
				spec.minor = attr[4]
				spec.lcid = attr[1]
				typelibs.append((tlb, spec))
		return typelibs
	except pythoncom.com_error:
		t,v,tb=sys.exc_info()
		sys.stderr.write ("Unable to load type library from '%s' - %s\n" % (arg, v))
		tb = None # Storing tb in a local is a cycle!
		sys.exit(1)

def GenerateFromTypeLibSpec(typelibInfo, file = None, verboseLevel = None, progressInstance = None, bUnicodeToString=NeedUnicodeConversions, bQuiet = None, bGUIProgress = None, bForDemand = bForDemandDefault, bBuildHidden = 1):
	if bQuiet is not None or bGUIProgress is not None:
		print "Please dont use the bQuiet or bGUIProgress params"
		print "use the 'verboseLevel', and 'progressClass' params"
	if verboseLevel is None:
		verboseLevel = 0 # By default, we use a gui, and no verbose level!

	if bForDemand and file is not None:
		raise RuntimeError, "You can only perform a demand-build when the output goes to the gen_py directory"
	if type(typelibInfo)==type(()):
		# Tuple
		typelibCLSID, lcid, major, minor  = typelibInfo
		tlb = pythoncom.LoadRegTypeLib(typelibCLSID, major, minor, lcid)
		spec = selecttlb.TypelibSpec(typelibCLSID, lcid, major, minor)
		spec.FromTypelib(tlb, str(typelibCLSID))
		typelibs = [(tlb, spec)]
	elif type(typelibInfo)==types.InstanceType:
		if typelibInfo.dll is None:
			# Version numbers not always reliable if enumerated from registry.
			tlb = pythoncom.LoadRegTypeLib(typelibInfo.clsid, typelibInfo.major, typelibInfo.minor, typelibInfo.lcid)
		else:
			tlb = pythoncom.LoadTypeLib(typelibInfo.dll)
		typelibs = [(tlb, typelibInfo)]
	elif hasattr(typelibInfo, "GetLibAttr"):
		# A real typelib object!
		tla = typelibInfo.GetLibAttr()
		guid = tla[0]
		lcid = tla[1]
		major = tla[3]
		minor = tla[4]
		spec = selecttlb.TypelibSpec(guid, lcid, major, minor)
		typelibs = [(typelibInfo, spec)]
	else:
		typelibs = GetTypeLibsForSpec(typelibInfo)

	if progressInstance is None:
		try:
			if not bForDemand: # Only go for GUI progress if not doing a demand-import
				# Win9x console programs don't seem to like our GUI!
				# (Actually, NT/2k wouldnt object to having them threaded - later!)
				import win32api, win32con
				if win32api.GetVersionEx()[3]==win32con.VER_PLATFORM_WIN32_NT:
					bMakeGuiProgress = 1
				else:
					try:
						win32api.GetConsoleTitle()
						# Have a console - can't handle GUI
						bMakeGuiProgress = 0
					except win32api.error:
						# no console - we can have a GUI
						bMakeGuiProgress = 1
				if bMakeGuiProgress:
					progressInstance = GUIProgress(verboseLevel)
		except ImportError: # No Pythonwin GUI around.
			pass
	if progressInstance is None:
		progressInstance = SimpleProgress(verboseLevel)
	progress = progressInstance

	bToGenDir = (file is None)

	for typelib, info in typelibs:
		if file is None:
			this_name = gencache.GetGeneratedFileName(info.clsid, info.lcid, info.major, info.minor)
			full_name = os.path.join(gencache.GetGeneratePath(), this_name)
			if bForDemand:
				try: os.unlink(full_name + ".py")
				except os.error: pass
				try: os.unlink(full_name + ".pyc")
				except os.error: pass
				try: os.unlink(full_name + ".pyo")
				except os.error: pass
				if not os.path.isdir(full_name):
					os.mkdir(full_name)
				outputName = os.path.join(full_name, "__init__.py")
			else:
				outputName = full_name + ".py"
			fileUse = open(outputName, "wt")
			progress.LogBeginGenerate(outputName)
		else:
			fileUse = file

		gen = genpy.Generator(typelib, info.dll, progress, bUnicodeToString=bUnicodeToString, bBuildHidden=bBuildHidden)

		gen.generate(fileUse, bForDemand)
		
		if file is None:
			fileUse.close()
		
		if bToGenDir:
			progress.SetDescription("Importing module")
			gencache.AddModuleToCache(info.clsid, info.lcid, info.major, info.minor)

	progress.Close()

def GenerateChildFromTypeLibSpec(child, typelibInfo, verboseLevel = None, progressInstance = None, bUnicodeToString=NeedUnicodeConversions):
	if verboseLevel is None:
		verboseLevel = 0 # By default, we use no gui, and no verbose level for the children.
	if type(typelibInfo)==type(()):
		typelibCLSID, lcid, major, minor  = typelibInfo
		tlb = pythoncom.LoadRegTypeLib(typelibCLSID, major, minor, lcid)
	else:
		tlb = typelibInfo
		tla = typelibInfo.GetLibAttr()
		typelibCLSID = tla[0]
		lcid = tla[1]
		major = tla[3]
		minor = tla[4]
	spec = selecttlb.TypelibSpec(typelibCLSID, lcid, major, minor)
	spec.FromTypelib(tlb, str(typelibCLSID))
	typelibs = [(tlb, spec)]

	if progressInstance is None:
		progressInstance = SimpleProgress(verboseLevel)
	progress = progressInstance

	for typelib, info in typelibs:
		dir_name = gencache.GetGeneratedFileName(info.clsid, info.lcid, info.major, info.minor)
		dir_path_name = os.path.join(gencache.GetGeneratePath(), dir_name)
		progress.LogBeginGenerate(dir_path_name)

		gen = genpy.Generator(typelib, info.dll, progress, bUnicodeToString=bUnicodeToString)
		gen.generate_child(child, dir_path_name)
		progress.SetDescription("Importing module")
		__import__("win32com.gen_py." + dir_name + "." + child)
	progress.Close()

def main():
	import getopt
	hiddenSpec = 1
	bUnicodeToString = NeedUnicodeConversions
	outputName = None
	verboseLevel = 1
	doit = 1
	bForDemand = bForDemandDefault
	try:
		opts, args = getopt.getopt(sys.argv[1:], 'vo:huiqd')
		for o,v in opts:
			if o=='-h':
				hiddenSpec = 0
			elif o=='-u':
				bUnicodeToString = not NeedUnicodeConversions
			elif o=='-o':
				outputName = v
			elif o=='-v':
				verboseLevel = verboseLevel + 1
			elif o=='-q':
				verboseLevel = verboseLevel - 1
			elif o=='-i':
				if len(args)==0:
					ShowInfo(None)
				else:
					for arg in args:
						ShowInfo(arg)
				doit = 0
			elif o=='-d':
				bForDemand = not bForDemand

	except (getopt.error, error), msg:
		sys.stderr.write (str(msg) + "\n")
		usage()

	if bForDemand and outputName is not None:
		sys.stderr.write("Can not use -d and -o together\n")
		usage()

	if not doit:
		return 0		
	if len(args)==0:
		rc = selecttlb.SelectTlb()
		if rc is None:
			sys.exit(1)
		args = [ rc ]

	if outputName is not None:
		f = open(outputName, "w")
	else:
		f = None

	for arg in args:
		GenerateFromTypeLibSpec(arg, f, verboseLevel = verboseLevel, bForDemand = bForDemand, bBuildHidden = hiddenSpec)

	if f:	
		f.close()


if __name__=='__main__':
	rc = main()
	if rc:
		sys.exit(rc)
	sys.exit(0)
