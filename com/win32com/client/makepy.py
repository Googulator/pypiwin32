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

  makepy.py [-h] [-x0|1] [-u] [typelib, ...]
  
  typelib -- A TLB, DLL, OCX, Description, or possibly something else.
  -h    -- Generate hidden methods.
  -u    -- Do not convert all Unicode objects to strings.
  -o outputFile -- Generate to named file - dont generate to standard directory.
  -i [typelib] -- Show info for specified typelib, or select the typelib if not specified.
  -v    -- Verbose output
  -q    -- Quiet output
  
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
			self.dialog=status.StatusProgressDialog(tlb_desc)
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
			typelibs.append(tlb, spec)
		except pythoncom.com_error:
			# See if it is a description
			tlbs = selecttlb.FindTlbsWithDescription(arg)
			if len(tlbs)==0:
				print "Could not locate a type library matching '%s'" % (arg)
			for spec in tlbs:
				tlb = pythoncom.LoadRegTypeLib(spec.clsid, spec.major, spec.minor, spec.lcid)
				# We have a typelib, but it may not be exactly what we specified
				# (due to automatic version matching of COM).  So we query what we really have!
				attr = tlb.GetLibAttr()
				spec.major = attr[3]
				spec.minor = attr[4]
				spec.lcid = attr[1]
				typelibs.append(tlb, spec)
		return typelibs
	except pythoncom.com_error:
		t,v,tb=sys.exc_info()
		sys.stderr.write ("Unable to load type library from '%s' - %s\n" % (arg, v))
		sys.exit(1)


def GenerateFromTypeLibSpec(typelibInfo, file = None, verboseLevel = None, progressInstance = None, bUnicodeToString=1, bQuiet = None, bGUIProgress = None ):
	if bQuiet is not None or bGUIProgress is not None:
		print "Please dont use the bQuiet or bGUIProgress params"
		print "use the 'verboseLevel', and 'progressClass' params"
	if verboseLevel is None:
		verboseLevel = 0 # By default, we use a gui, and no verbose level!
		
	if type(typelibInfo)==type(()):
		# Tuple
		typelibCLSID, lcid, major, minor  = typelibInfo
		tlb = pythoncom.LoadRegTypeLib(typelibCLSID, major, minor, lcid)
		spec = selecttlb.TypelibSpec(typelibCLSID, lcid, major, minor)
		spec.FromTypelib(tlb, str(typelibCLSID))
		typelibs = [(tlb, spec)]
	elif type(typelibInfo)==types.InstanceType:
		tlb = pythoncom.LoadRegTypeLib(typelibInfo.clsid, typelibInfo.major, typelibInfo.minor, typelibInfo.lcid)
		typelibs = [(tlb, typelibInfo)]
	else:
		typelibs = GetTypeLibsForSpec(typelibInfo)

	if progressInstance is None:
		progressInstance = GUIProgress(verboseLevel)
		
	progress = progressInstance

	bToGenDir = (file is None)

	for typelib, info in typelibs:
		if file is None:
			outputName = gencache.GetGeneratedFileName(info.clsid, info.lcid, info.major, info.minor)
			outputName = os.path.join(gencache.GetGeneratePath(), outputName) + ".py"
			progress.LogBeginGenerate(outputName)
			fileUse = open(outputName, "wt")
		else:
			fileUse = file

		gen = genpy.Generator(typelib, info.dll, progress, bUnicodeToString=bUnicodeToString)
		gen.generate(fileUse)
		
		if file is None:
			fileUse.close()
		
		if bToGenDir:
			progress.SetDescription("Importing module")
			gencache.AddModuleToCache(info.clsid, info.lcid, info.major, info.minor)

	progress.Close()

def main():
	import getopt
	hiddenSpec = 0
	bUnicodeToString = 1
	outputName = None
	verboseLevel = 1
	doit = 1
	try:
		opts, args = getopt.getopt(sys.argv[1:], 'vo:huiq')
		for o,v in opts:
			if o=='-h':
				hiddenSpec = 1
			elif o=='-u':
				bUnicodeToString = 0
			elif o=='-o':
				outputName = v
			elif o=='-v':
				verboseLevel = verboseLevel + 1
			elif o=='-q':
				verboseLevel = verboseLevel - 1
			elif o=='-i':
				ShowInfo(v)
				doit = 0

	except (getopt.error, error), msg:
		sys.stderr.write (msg + "\n")
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
		GenerateFromTypeLibSpec(arg, f, verboseLevel = verboseLevel)

	if f:	
		f.close()


if __name__=='__main__':
	rc = main()
	if rc:
		sys.exit(rc)
	sys.exit(0)
