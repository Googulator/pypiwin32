"""genpy.py - The worker for makepy.  See makepy.py for more details

This code was moved simply to speed Python in normal circumstances.  As the makepy.py
is normally run from the command line, it reparses the code each time.  Now makepy
is nothing more than the command line handler and public interface.

The makepy command line etc handling is also getting large enough in its own right!
"""

import os
import sys
import string
import time
import win32com

import pythoncom
import build

error = "makepy.error"
makepy_version = "0.3.0" # Written to generated file.

# This map is used purely for the users benefit -it shows the
# raw, underlying type of Alias/Enums, etc.  The COM implementation
# does not use this map at runtime - all Alias/Enum have already
# been translated.
mapVTToTypeString = {
	pythoncom.VT_I2: 'types.IntType',
	pythoncom.VT_I4: 'types.IntType',
	pythoncom.VT_R4: 'types.FloatType',
	pythoncom.VT_R8: 'types.FloatType',
	pythoncom.VT_BSTR: 'types.StringType',
	pythoncom.VT_BOOL: 'types.IntType',
	pythoncom.VT_VARIANT: 'types.TypeType',
	pythoncom.VT_I1: 'types.IntType',
	pythoncom.VT_UI1: 'types.IntType',
	pythoncom.VT_UI2: 'types.IntType',
	pythoncom.VT_UI4: 'types.IntType',
	pythoncom.VT_I8: 'types.LongType',
	pythoncom.VT_UI8: 'types.LongType',
	pythoncom.VT_INT: 'types.IntType',
	pythoncom.VT_DATE: 'pythoncom.PyTimeType',
	pythoncom.VT_UINT: 'types.IntType',
}

# Given a propget function's arg desc, return the default paramaters for all
# params bar the first.  Eg, then Python does a:
# object.Property = "foo"
# Python can only pass the "foo" value.  If the property has
# multiple args, and the rest have default values, this allows
# Python to correctly pass those defaults.
def MakeDefaultArgsForPropertyPut(argsDesc):
	ret = []
	for desc in argsDesc[1:]:
		default = build.MakeDefaultArgRepr(desc)
		if default is None:
			break
		ret.append(default)
	return tuple(ret)
							

def MakeMapLineEntry(dispid, wFlags, retType, argTypes, user, resultCLSID):
	# Strip the default value
	argTypes = tuple(map(lambda what: what[:2], argTypes))
	return '(%s, %d, %s, %s, "%s", %s)' % \
		(dispid, wFlags, retType[:2], argTypes, user, resultCLSID)

def MakeEventMethodName(eventName):
	if eventName[:2]=="On":
		return eventName
	else:
		return "On"+eventName

def WriteSinkEventMap(obj):
	print '\t_dispid_to_func_ = {'
	for name, entry in obj.propMapGet.items() + obj.propMapPut.items() + obj.mapFuncs.items():
		fdesc = entry.desc
		id = fdesc[0]
		print '\t\t%9d : "%s",' % (entry.desc[0], MakeEventMethodName(entry.names[0]))
	print '\t\t}'
	

# MI is used to join my writable helpers, and the OLE
# classes.
class WritableItem:
	def __cmp__(self, other):
		"Compare for sorting"	
		ret = cmp(self.order, other.order)
		if ret==0 and self.doc: ret = cmp(self.doc[0], other.doc[0])
		return ret
	def __repr__(self):
		return "OleItem: doc=%s, order=%d" % (`self.doc`, self.order)


class RecordItem(build.OleItem, WritableItem):
  order = 9
  typename = "RECORD"

  def __init__(self, typeInfo, typeAttr, doc=None, bForUser=1):
    build.OleItem.__init__(self, doc)

  def WriteClass(self, generator):
    pass

class InterfaceItem(build.OleItem, WritableItem):
  order = 4
  typename = "INTERFACE"

  def __init__(self, typeInfo, typeAttr, doc=None, bForUser=1):
    build.OleItem.__init__(self, doc)

  def WriteClass(self, generator):
    pass

# Given an enum, write all aliases for it.
# (no longer necessary for new style code, but still used for old code.
def WriteAliasesForItem(item, aliasItems):
  for alias in aliasItems.values():
    if item.doc and alias.aliasDoc and (alias.aliasDoc[0]==item.doc[0]):
      alias.WriteAliasItem(aliasItems)
      
class AliasItem(build.OleItem, WritableItem):
  order = 2
  typename = "ALIAS"

  def __init__(self, typeinfo, attr, doc=None, bForUser = 1):
    build.OleItem.__init__(self, doc)

    ai = attr[14]
    self.attr = attr
    if type(ai) == type(()) and \
      type(ai[1])==type(0): # XXX - This is a hack - why tuples?  Need to resolve?
      href = ai[1]
      alinfo = typeinfo.GetRefTypeInfo(href)
      self.aliasDoc = alinfo.GetDocumentation(-1)
      self.aliasAttr = alinfo.GetTypeAttr()
    else:
      self.aliasDoc = None
      self.aliasAttr = None

  def WriteAliasItem(self, aliasDict):
    # we could have been written as part of an alias dependency
    if self.bWritten:
      return

    if self.aliasDoc:
      depName = self.aliasDoc[0]
      if aliasDict.has_key(depName):
        aliasDict[depName].WriteAliasItem(aliasDict)
      print self.doc[0] + " = " + depName
    else:
      ai = self.attr[14]
      if type(ai) == type(0):
        try:
          typeStr = mapVTToTypeString[ai]
          print "# %s=%s" % (self.doc[0], typeStr)
        except KeyError:
          print self.doc[0] + " = None # Can't convert alias info " + str(ai)
    print
    self.bWritten = 1

class EnumerationItem(build.OleItem, WritableItem):
  order = 1
  typename = "ENUMERATION"

  def __init__(self, typeinfo, attr, doc=None, bForUser=1):
    build.OleItem.__init__(self, doc)

    self.clsid = attr[0]
    self.mapVars = {}
    typeFlags = attr[11]
    self.hidden = typeFlags & pythoncom.TYPEFLAG_FHIDDEN or \
                  typeFlags & pythoncom.TYPEFLAG_FRESTRICTED

    for j in range(attr[7]):
      vdesc = typeinfo.GetVarDesc(j)
      name = typeinfo.GetNames(vdesc[0])[0]
      self.mapVars[name] = build.MapEntry(vdesc)

  def WriteEnumerationHeaders(self, aliasItems):
    enumName = self.doc[0]
    print "%s=constants # Compatibility with previous versions." % (enumName)
    WriteAliasesForItem(self, aliasItems)
    
  def WriteEnumerationItems(self):
    enumName = self.doc[0]
    # Write in name alpha order
    names = self.mapVars.keys()
    names.sort()
    for name in names:
      entry = self.mapVars[name]
      vdesc = entry.desc
      if vdesc[4] == pythoncom.VAR_CONST:
        if type(vdesc[1])==type(0):
          if vdesc[1]==0x80000000: # special case
            use = "0x80000000"
          else:
            use = hex(vdesc[1])
        else:
          use = repr(str(vdesc[1]))
        print "\t%-30s=%-10s # from enum %s" % (build.MakePublicAttributeName(name), use, enumName)

class DispatchItem(build.DispatchItem, WritableItem):
	order = 3

	def __init__(self, typeinfo, attr, doc=None):
		build.DispatchItem.__init__(self, typeinfo, attr, doc)

		self.bIsSink = 0

	def WriteClass(self, generator):
		if self.bIsSink:
			self.WriteEventSinkClassHeader(generator)
			self.WriteCallbackClassBody(generator)
		else:
			self.WriteClassHeader(generator)
			self.WriteClassBody(generator)

	def WriteClassHeader(self, generator):
		generator.checkWriteDispatchBaseClass()
		doc = self.doc
		className = build.MakePublicAttributeName(doc[0])
		print 'class ' + className + '(DispatchBaseClass):'
		if doc[1]: print '\t"""' + doc[1] + '"""'
		try:
			progId = pythoncom.ProgIDFromCLSID(self.clsid)
			print "\t# This class is creatable by the name '%s'" % (progId)
		except pythoncom.com_error:
			pass
		clsidStr = str(self.clsid)
		print "\tCLSID = pythoncom.MakeIID('" + clsidStr + "')"
		print
		self.bWritten = 1

	def WriteEventSinkClassHeader(self, generator):
		doc = self.doc
		className = build.MakePublicAttributeName(doc[0])
		print 'class ' + className + ':'
		if doc[1]: print '\t\"' + doc[1] + '\"'
		try:
			progId = pythoncom.ProgIDFromCLSID(self.clsid)
			print "\t# This class is creatable by the name '%s'" % (progId)
		except pythoncom.com_error:
			pass
		clsidStr = str(self.clsid)
		print '\tCLSID = CLSID_Sink = pythoncom.MakeIID(\'' + clsidStr + '\')'
		print '\t_public_methods_ = [] # For COM Server support'
		WriteSinkEventMap(self)
		print
		print '\tdef __init__(self, oobj = None):'
		print '\t\timport win32com.server.util'
		print '\t\tcpc=oobj._oleobj_.QueryInterface(pythoncom.IID_IConnectionPointContainer)'
		print '\t\tcp=cpc.FindConnectionPoint(self.CLSID_Sink)'
		print '\t\tcookie=cp.Advise(win32com.server.util.wrap(self))'
		print '\t\tself._olecp,self._olecp_cookie = cp,cookie'
		print '\tdef __del__(self):'
		print '\t\tself.close()'
		print '\tdef close(self):'
		print '\t\tif not self._olecp:'
		print '\t\t\tcp,cookie,self._olecp,self._olecp_cookie = self._olecp,self._olecp_cookie,None,None'
		print '\t\tcp.Unadvise(cookie)'
		print '\tdef _query_interface_(self, iid):'
		print '\t\timport win32com.server.util'
		print '\t\tif iid==self.CLSID: return win32com.server.util.wrap(self)'
		print
		self.bWritten = 1

	def WriteCallbackClassBody(self, generator):
		print "\t# Handlers for the control"
		print "\t# If you create handlers, they should have the following prototypes:"
		for name, entry in self.propMapGet.items() + self.propMapPut.items() + self.mapFuncs.items():
			fdesc = entry.desc
			id = fdesc[0]
			methName = MakeEventMethodName(entry.names[0])
			print '#\tdef ' + methName + '(self' + build.BuildCallList(fdesc, entry.names, "defaultNamedOptArg", "defaultNamedNotOptArg","defaultUnnamedArg") + '):'
			if entry.doc and entry.doc[1]: print '#\t\t"' + entry.doc[1] + '"'
		print
		self.bWritten = 1

	def WriteClassBody(self, generator):
		doc = self.doc
		# Write in alpha order.
		names = self.mapFuncs.keys()
		names.sort()
		specialItems = {"count":None, "item":None,"value":None,"_newenum":None} # If found, will end up with (entry, invoke_tupe)
		itemCount = None
		for name in names:
			entry=self.mapFuncs[name]
			if entry.desc[0]==pythoncom.DISPID_VALUE:
				lkey = "value"
			elif entry.desc[0]==pythoncom.DISPID_NEWENUM:
				specialItems["_newenum"] = (entry, entry.desc[4], None)
				continue # Dont build this one now!
			else:
				lkey = string.lower(name)
			if specialItems.has_key(lkey) and specialItems[lkey] is None: # remember if a special one.
				specialItems[lkey] = (entry, entry.desc[4], None)
			if generator.bBuildHidden or not entry.hidden:
				if entry.GetResultName():
					print '\t# Result is of type ' + entry.GetResultName()
				if entry.wasProperty:
					print '\t# The method %s is actually a property, but must be used as a method to correctly pass the arguments' % name
				ret = self.MakeFuncMethod(entry,build.MakePublicAttributeName(name))
				for line in ret:
					print line
		print "\t_prop_map_get_ = {"
		names = self.propMap.keys(); names.sort()
		for key in names:
			entry = self.propMap[key]
			resultName = entry.GetResultName()
			if resultName:
				print "\t\t# Property '%s' is an object of type '%s'" % (key, resultName)
			lkey = string.lower(key)
			details = entry.desc
			resultDesc = details[2]
			argDesc = ()
			mapEntry = MakeMapLineEntry(details[0], pythoncom.DISPATCH_PROPERTYGET, resultDesc, argDesc, key, entry.GetResultCLSIDStr())
			
			if entry.desc[0]==pythoncom.DISPID_VALUE:
				lkey = "value"
			elif entry.desc[0]==pythoncom.DISPID_NEWENUM:
				# XXX - should DISPATCH_METHOD in the next line use the invtype?
				specialItems["_newenum"] = (entry, pythoncom.DISPATCH_METHOD, mapEntry)
				continue # Dont build this one now!
			else:
				lkey = string.lower(key)
			if specialItems.has_key(lkey) and specialItems[lkey] is None: # remember if a special one.
				specialItems[lkey] = (entry, pythoncom.DISPATCH_PROPERTYGET, mapEntry)

			print '\t\t"%s": %s,' % (key, mapEntry)

		names = self.propMapGet.keys(); names.sort()
		for key in names:
			entry = self.propMapGet[key]
			if entry.GetResultName():
				print "\t\t# Method '%s' returns object of type '%s'" % (key, entry.GetResultName())
			details = entry.desc
			lkey = string.lower(key)
			argDesc = details[2]
			resultDesc = details[8]
			mapEntry = MakeMapLineEntry(details[0], pythoncom.DISPATCH_PROPERTYGET, resultDesc, argDesc, key, entry.GetResultCLSIDStr())
			if entry.desc[0]==pythoncom.DISPID_VALUE:
				lkey = "value"
			elif entry.desc[0]==pythoncom.DISPID_NEWENUM:
				specialItems["_newenum"] = (entry, pythoncom.DISPATCH_METHOD, mapEntry)
				continue # Dont build this one now!
			else:
				lkey = string.lower(key)
			if specialItems.has_key(lkey) and specialItems[lkey] is None: # remember if a special one.
				specialItems[lkey]=(entry, pythoncom.DISPATCH_PROPERTYGET, mapEntry)
			print '\t\t"%s": %s,' % (key, mapEntry)

		print "\t}"

		print "\t_prop_map_put_ = {"
		# These are "Invoke" args
		names = self.propMap.keys(); names.sort()
		for key in names:
			entry = self.propMap[key]
			lkey=string.lower(key)
			details = entry.desc
#			if specialItems.has_key(lkey):
#				specialItems[lkey] = (entry, details[4], 0)
			# If default arg is None, write an empty tuple
			defArgDesc = build.MakeDefaultArgRepr(details[2])
			if defArgDesc is None:
				defArgDesc = ""
			else:
				defArgDesc = defArgDesc + ","
			print '\t\t"%s" : ((%s, LCID, %d, 0),(%s)),' % (key, details[0], pythoncom.DISPATCH_PROPERTYPUT, defArgDesc)

		names = self.propMapPut.keys(); names.sort()
		for key in names:
			entry = self.propMapPut[key]
			details = entry.desc
			defArgDesc = MakeDefaultArgsForPropertyPut(details[2])
			print '\t\t"%s": ((%s, LCID, %d, 0),%s),' % (key, details[0], details[4], defArgDesc)
		print "\t}"
		
		if specialItems["value"]:
			entry, invoketype, propArgs = specialItems["value"]
			if propArgs is None:
				typename = "method"
				ret = self.MakeFuncMethod(entry,'__call__')
			else:
				typename = "property"
				ret = [ "\tdef __call__(self):\n\t\treturn apply(self._ApplyTypes_, %s )" % propArgs]
			print "\t# Default %s for this class is '%s'" % (typename, entry.names[0])
			for line in ret:
				print line
			print "\t# str(ob) and int(ob) will use __call__"
			print "\tdef __str__(self, *args):"
			print "\t\treturn str(apply( self.__call__, args))"
			print "\tdef __int__(self, *args):"
			print "\t\treturn int(apply( self.__call__, args))"
			

		if specialItems["_newenum"]:
			enumEntry, invoketype, propArgs = specialItems["_newenum"]
			# If we have a default entry, assume enumerator is of same type
#			if defEntry:
#				resultCLSID = defEntry.GetResultCLSIDStr()
#			else:
#				resultCLSID = "None"
			resultCLSID = enumEntry.GetResultCLSIDStr()
			print '\tdef _NewEnum(self):'
			print '\t\t"Create an enumerator from this object"'
			print '\t\treturn win32com.client.util.WrapEnum(self._oleobj_.InvokeTypes(%d,LCID,%d,(13, 10),()),%s)' % (pythoncom.DISPID_NEWENUM, enumEntry.desc[4], resultCLSID)
			print '\tdef __getitem__(self, index):'
			print '\t\t"Allow this class to be accessed as a collection"'
			print "\t\tif not self.__dict__.has_key('_enum_'):"
			print "\t\t\timport win32com.client.util"
			print "\t\t\tself.__dict__['_enum_'] = self._NewEnum()"
			print "\t\treturn self._enum_.__getitem__(index)"
		else: # Not an Enumerator, but may be an "Item/Count" based collection
			if specialItems["item"]:
				entry, invoketype, propArgs = specialItems["item"]
				print '\t#This class has Item property/method which may take args - allow indexed access'
				print '\tdef __getitem__(self, item):'
				print '\t\treturn self._get_good_object_(apply(self._oleobj_.Invoke, (0x%x, LCID, %d, 1, item)), "Item")' % (entry.desc[0], invoketype)
		if specialItems["count"]:
			entry, invoketype, propArgs = specialItems["count"]
			if propArgs is None:
				typename = "method"
				ret = self.MakeFuncMethod(entry,'__len__')
			else:
				typename = "property"
				ret = [ "\tdef __len__(self):\n\t\treturn apply(self._ApplyTypes_, %s )" % propArgs]
			print "\t#This class has Count() %s - allow len(ob) to provide this" % (typename)
			for line in ret:
				print line
			# Also include a __nonzero__
			print "\t#This class has a __len__ - this is needed so 'if object:' always returns TRUE."
			print "\tdef __nonzero__(self):"
			print "\t\treturn 1"
	
		print
		self.bWritten = 1


class CoClassItem(build.OleItem, WritableItem):
  order = 5
  typename = "COCLASS"

  def __init__(self, typeinfo, attr, doc=None, sources = [], interfaces = [], bForUser=1):
    build.OleItem.__init__(self, doc)
    self.clsid = attr[0]
    self.sources = sources
    self.interfaces = interfaces

  def WriteClass(self, generator):
    generator.checkWriteCoClassBaseClass()
    doc = self.doc
    className = build.MakePublicAttributeName(doc[0])
    try:
      progId = pythoncom.ProgIDFromCLSID(self.clsid)
      print "# This CoClass is known by the name '%s'" % (progId)
    except pythoncom.com_error:
      pass
    print 'class %s(CoClassBaseClass): # A CoClass' % (className)
    if doc and doc[1]: print '\t# ' + doc[1]
    clsidStr = str(self.clsid)
    print '\tCLSID = pythoncom.MakeIID("%s")' % (clsidStr)
    print '\tcoclass_sources = ['
    defItem = None
    for source, flag, dual in self.sources:
      if flag & pythoncom.IMPLTYPEFLAG_FDEFAULT:
        defItem = source
      print '\t\t%s,' % (build.MakePublicAttributeName(source.doc[0]))
    print '\t]'
    if defItem:
      print '\tdefault_source = %s' % (build.MakePublicAttributeName(defItem.doc[0]))
    print '\tcoclass_interfaces = ['
    defItem = None
    for interface, flag, dual in self.interfaces:
      if flag & pythoncom.IMPLTYPEFLAG_FDEFAULT: # and dual:
        defItem = interface
      print '\t\t%s,' % (build.MakePublicAttributeName(interface.doc[0]))
    print '\t]'
    if defItem:
      print '\tdefault_interface = %s' % (build.MakePublicAttributeName(defItem.doc[0]))
    self.bWritten = 1
    print

class GeneratorProgress:
	def __init__(self):
		pass
	def Starting(self, tlb_desc):
		"""Called when the process starts.
		"""
		self.tlb_desc = tlb_desc
	def Finished(self):
		"""Called when the process is complete.
		"""
	def SetDescription(self, desc, maxticks = None):
		"""We are entering a major step.  If maxticks, then this
		is how many ticks we expect to make until finished
		"""
	def Tick(self, desc = None):
		"""Minor progress step.  Can provide new description if necessary
		"""
	def VerboseProgress(self, desc):
		"""Verbose/Debugging output.
		"""
	def LogWarning(self, desc):
		"""If a warning is generated
		"""
	def LogBeginGenerate(self, filename):
		pass
	def Close(self):
		pass
			
class Generator:
  def __init__(self, typelib, sourceFilename, progressObject, bBuildHidden=0, bUnicodeToString=0):
    self.bHaveWrittenDispatchBaseClass = 0
    self.bHaveWrittenCoClassBaseClass = 0

    self.typelib = typelib
    self.sourceFilename = sourceFilename
    self.bBuildHidden = bBuildHidden
    self.bUnicodeToString = bUnicodeToString
    self.progress = progressObject
    # These 2 are later additions and most of the code still 'print's...
    self.file = None

  def BuildOleItemsFromType(self):
    oleItems = {}
    enumItems = {}
    aliasItems = {}
    for i in xrange(self.typelib.GetTypeInfoCount()):
      info = self.typelib.GetTypeInfo(i)
      infotype = self.typelib.GetTypeInfoType(i)
      doc = self.typelib.GetDocumentation(i)
      attr = info.GetTypeAttr()
      itemClass = None
      if infotype == pythoncom.TKIND_ENUM or infotype == pythoncom.TKIND_MODULE:
        newItem = EnumerationItem(info, attr, doc)
        enumItems[newItem.doc[0]] = newItem
      elif infotype in [pythoncom.TKIND_DISPATCH, pythoncom.TKIND_INTERFACE]:
        if infotype==pythoncom.TKIND_INTERFACE:
          # For now we believe this flag.  It appears we could also
          # look down the cImplTypes for IDispatch, but this is simpler.
          if not attr[11] & pythoncom.TYPEFLAG_FDISPATCHABLE:
            ### we can't build these
            self.progress.VerboseProgress("Skipped interface: %s" % doc[0])
            continue
          else:
            self.progress.VerboseProgress("Dispatchable interface: %s" % doc[0])
        else: # A TKIND_DISPATCH
          if attr[11] & pythoncom.TYPEFLAG_FDUAL:
            self.progress.VerboseProgress("Dual interface: %s" % doc[0])
          else:
            self.progress.VerboseProgress("Dispatch interface: %s" % doc[0])
        # Get to work.
        if oleItems.has_key(attr[0]):
          continue # already built by CoClass processing.
        newItem = DispatchItem(info, attr, doc)
        oleItems[newItem.clsid] = newItem
      elif infotype == pythoncom.TKIND_RECORD or infotype == pythoncom.TKIND_UNION:
        newItem = RecordItem(info, attr, doc)
        ### where to put the newItem ??
      elif infotype == pythoncom.TKIND_ALIAS:
        newItem = AliasItem(info, attr, doc)
        aliasItems[newItem.doc[0]] = newItem
      elif infotype == pythoncom.TKIND_COCLASS:
        # try to find the source and dispinterfaces for the coclass
        # We no longer generate specific OCX support for the CoClass, as there
        # may be multiple Dispatch and multiple source interfaces.  We cant
        # predict much in this scenario, so we move the responsibility to
        # the Python programmer.
        # (It also keeps win32ui(ole) out of the core generated import dependencies.
        sources = []
        interfaces = []
        for j in range(attr[8]):
          flags = info.GetImplTypeFlags(j)
          refType = info.GetRefTypeInfo(info.GetRefTypeOfImplType(j))
          refAttr = refType.GetTypeAttr()
          dual = refAttr[11] & pythoncom.TYPEFLAG_FDUAL
          # Ensure the references interfaces have been generated.
          if oleItems.has_key(refAttr[0]):
            dispItem = oleItems[refAttr[0]]
          else:
            dispItem = DispatchItem(refType, refAttr, refType.GetDocumentation(-1))
            oleItems[dispItem.clsid] = dispItem
          if flags & pythoncom.IMPLTYPEFLAG_FSOURCE:
            dispItem.bIsSink = 1
            sources.append(dispItem, flags, dual)
          else:
            interfaces.append(dispItem, flags, dual)

        newItem = CoClassItem(info, attr, doc, sources, interfaces)
        oleItems[newItem.clsid] = newItem
      else:
        self.progress.LogWarning("Unknown TKIND found: %d" % infotype)
  
    return oleItems, enumItems, aliasItems
  
  def generate(self, file):
    self.file = file
    oldOut = sys.stdout
    sys.stdout = file
    try:
      self.do_generate()
    finally:
      sys.stdout = oldOut
      self.file = None
      self.progress.Finished()

  def do_generate(self):
    moduleDoc = self.typelib.GetDocumentation(-1)
    docDesc = ""
    if moduleDoc[1]:
      docDesc = moduleDoc[1]
    self.progress.Starting(docDesc)
    self.progress.SetDescription("Building definitions from type library...")
    oleItems, enumItems, aliasItems = self.BuildOleItemsFromType()
    la = self.typelib.GetLibAttr()

    print '# Created by makepy.py version %s from %s' % (makepy_version,os.path.split(self.sourceFilename)[1])
    print '# On date: %s' % time.ctime(time.time())
#    print '#\n# Command line used:', string.join(sys.argv[1:]," ")

    self.progress.SetDescription("Generating...", len(oleItems.values())+len(enumItems.values()))

    try:
      print '"' + moduleDoc[1] + '"'
    except:
      pass
    print 'makepy_version =', `makepy_version`
    print
    print 'import win32com.client.CLSIDToClass, pythoncom'
    print 'from types import TupleType'
    if self.bUnicodeToString:
    	print 'from pywintypes import UnicodeType'
    print
    print '# The following 3 lines may need tweaking for the particular server'
    print '# Candidates are pythoncom.Missing and pythoncom.Empty'
    print 'defaultNamedOptArg=pythoncom.Missing'
    print 'defaultNamedNotOptArg=pythoncom.Missing'
    print 'defaultUnnamedArg=pythoncom.Missing'
    print
    print 'CLSID = pythoncom.MakeIID(\'' + str(la[0]) + '\')'
    print 'MajorVersion = ' + str(la[3])
    print 'MinorVersion = ' + str(la[4])
    print 'LibraryFlags = ' + str(la[5])
    print 'LCID = ' + hex(la[1])
    print

    # Generate the constants and their support.
    if enumItems:
	    print "class constants:"
	    list = enumItems.values()
	    list.sort()
	    for oleitem in list:
	        oleitem.WriteEnumerationItems()
	    print
	    print    
	    for oleitem in list:
	      self.progress.Tick()
	      oleitem.WriteEnumerationHeaders(aliasItems)

    list = oleItems.values()
    list.sort()
    for oleitem in list:
      self.progress.Tick()
      oleitem.WriteClass(self)

#    list = aliasItems.values()
#    if list:
#      print "# The following Aliases have been generated for informational purposes only"
#      for oleitem in list:
#        oleitem.WriteAliasCode(aliasItems)

    # Write out _all_ my generated CLSID's in the map
    print 'CLSIDToClassMap = {'
    for item in oleItems.values():
      if item.bWritten:
        print "\t'%s' : %s," % (str(item.clsid), build.MakePublicAttributeName(item.doc[0]))
    print '}'
    print
    print 'win32com.client.CLSIDToClass.RegisterCLSIDsFromDict( CLSIDToClassMap )'
    if enumItems:
      print 'win32com.client.constants.__dicts__.append(constants.__dict__)'
    print


  def checkWriteDispatchBaseClass(self):
    if not self.bHaveWrittenDispatchBaseClass:
      print "_PyIDispatchType = pythoncom.TypeIIDs[pythoncom.IID_IDispatch]"
      print
      print "class DispatchBaseClass:"
      print '\tdef __init__(self, oobj=None):'
      print '\t\tif oobj is None:'
      print '\t\t\toobj = pythoncom.new(self.CLSID)'
      print '\t\telif type(self) == type(oobj): # An instance'
      print '\t\t\toobj = oobj._oleobj_.QueryInterface(self.CLSID, pythoncom.IID_IDispatch) # Must be a valid COM instance'
      print '\t\tself.__dict__["_oleobj_"] = oobj # so we dont call __setattr__'
      # Provide a prettier name than the CLSID
      print '\tdef __repr__(self):'
      print '\t\treturn "<win32com.gen_py.%s.%s>" % (__doc__, self.__class__.__name__)'
      print

      print '\tdef _ApplyTypes_(self, dispid, wFlags, retType, argTypes, user, resultCLSID, *args):'
      print '\t\treturn self._get_good_object_(apply(self._oleobj_.InvokeTypes, (dispid, LCID, wFlags, retType, argTypes) + args), user, resultCLSID)'
      print

      # Create . operators for the class.
      print '\tdef __getattr__(self, attr):'
      print '\t\ttry:'
      print '\t\t\targs=self._prop_map_get_[attr]'
      print '\t\texcept KeyError:'
      print '\t\t\traise AttributeError, attr'
      print '\t\treturn apply(self._ApplyTypes_, args)'
      print
      print '\tdef __setattr__(self, attr, value):'
      print '\t\tif self.__dict__.has_key(attr): self.__dict__[attr] = value; return'
      print '\t\ttry:'
      print '\t\t\targs, defArgs=self._prop_map_put_[attr]'
      print '\t\texcept KeyError:'
      print '\t\t\traise AttributeError, attr'
      print '\t\tapply(self._oleobj_.Invoke, args + (value,) + defArgs)'
      
      print "\tdef _get_good_single_object_(self, obj, obUserName=None, resultCLSID=None):"
      print "\t\tif _PyIDispatchType==type(obj):"
      print "\t\t\treturn win32com.client.Dispatch(obj, obUserName, resultCLSID, UnicodeToString=%d)" % (self.bUnicodeToString)
      if self.bUnicodeToString:
        print "\t\telif UnicodeType==type(obj):"
        print "\t\t\treturn str(obj)"
      print "\t\treturn obj"
      print "\tdef _get_good_object_(self, obj, obUserName=None, resultCLSID=None):"
      print "\t\tif obj is None:"
      print "\t\t\treturn None"
      print "\t\telif type(obj)==TupleType:"
      print "\t\t\treturn tuple(map(lambda o, s=self, oun=obUserName, rc=resultCLSID: s._get_good_single_object_(o, oun, rc),  obj))"
      print "\t\telse:"
      print "\t\t\treturn self._get_good_single_object_(obj, obUserName, resultCLSID)"
      print
      self.bHaveWrittenDispatchBaseClass = 1

  def checkWriteCoClassBaseClass(self):
    if not self.bHaveWrittenCoClassBaseClass:
      print "class CoClassBaseClass:"
      print '\tdef __init__(self, oobj=None):'
      print '\t\tif oobj is None: oobj = pythoncom.new(self.CLSID)'
#      print '\t\tself.__dict__["_oleobj_"] = oobj'
      print '\t\tself.__dict__["_dispobj_"] = self.default_interface(oobj)'
      # Provide a prettier name than the CLSID
      print '\tdef __repr__(self):'
      print '\t\treturn "<win32com.gen_py.%s.%s>" % (__doc__, self.__class__.__name__)'
      print
      print '\tdef __getattr__(self, attr):'
      print '\t\td=self.__dict__["_dispobj_"]'
      print '\t\tif d is not None: return getattr(d, attr)'
      print '\t\traise AttributeError, attr'
      print '\tdef __setattr__(self, attr, value):'
      print '\t\tif self.__dict__.has_key(attr): self.__dict__[attr] = value; return'
      print '\t\ttry:'
      print '\t\t\td=self.__dict__["_dispobj_"]'
      print '\t\t\tif d is not None:'
      print '\t\t\t\td.__setattr__(attr, value)'
      print '\t\t\t\treturn'
      print '\t\texcept AttributeError:'
      print '\t\t\tpass'
      print '\t\tself.__dict__[attr] = value'
      print
      self.bHaveWrittenCoClassBaseClass = 1

if __name__=='__main__':
  print "This is a worker module.  Please use makepy to generate Python files."
