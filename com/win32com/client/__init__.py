# This module exists to create the "best" dispatch object for a given
# object.  If "makepy" support for a given object is detected, it is
# used, otherwise a dynamic dispatch object.

# Note that if the unknown dispatch object then returns a known
# dispatch object, the known class will be used.  This contrasts
# with dynamic.Dispatch behaviour, where dynamic objects are always used.
import dynamic, CLSIDToClass, pythoncom
import pywintypes

def __WrapDispatch(dispatch, userName = None, resultCLSID = None, typeinfo = None, \
                  UnicodeToString = 1, clsctx = pythoncom.CLSCTX_SERVER):
  """
    Helper function to return a makepy generated class for a CLSID if it exists,
    otherwise cope by using CDispatch.
  """
  if resultCLSID is None:
    try:
      typeinfo = dispatch.GetTypeInfo()
      resultCLSID = str(typeinfo.GetTypeAttr()[0])
    except pythoncom.com_error:
      pass
  if resultCLSID is not None:
    try:
      return CLSIDToClass.GetClass(resultCLSID)(dispatch)
    except KeyError: # We dont know this CLSID yet
      # Attempt to load generated module support
      # This may load the module, and make it available
      try:
        import gencache
        if gencache.GetModuleForCLSID(resultCLSID) is not None:
          try:
            return CLSIDToClass.GetClass(resultCLSID)(dispatch)
          except KeyError: # still dont know it?
            pass
      except ImportError:
        # no gencache avail - thats OK!
        pass

  # Return a "dynamic" object - best we can do!
  return dynamic.Dispatch(dispatch, userName, CDispatch, typeinfo, UnicodeToString=UnicodeToString,clsctx=clsctx)


def GetObject(Pathname = None, Class = None, clsctx = None):
  """
    Mimic VB's GetObject() function.

    ob = GetObject(Class = "ProgID") or GetObject(Class = clsid) will
    connect to an already running instance of the COM object.
    
    ob = GetObject(r"c:\blah\blah\foo.xls") (aka the COM moniker syntax)
    will return a ready to use Python wrapping of the required COM object.

    Note: You must specifiy one or the other of these arguments. I know
    this isn't pretty, but it is what VB does. Blech. If you don't
    I'll throw ValueError at you. :)
    
    This will most likely throw pythoncom.com_error if anything fails.
  """
  resultCLSID = None
  
  if clsctx is None:
    clsctx = pythoncom.CLSCTX_ALL
    
  if (Pathname is None and Class is None) or \
     (Pathname is not None and Class is not None):
    raise ValueError, "You must specify a value for Pathname or Class, but not both."

  if Class is not None:
    return GetActiveObject(Class, clsctx)
  else:
    return Moniker(Pathname, clsctx)    

def GetActiveObject(Class, clsctx = pythoncom.CLSCTX_ALL):
  """
    Python friendly version of GetObject's ProgID/CLSID functionality.
  """  
  resultCLSID = pywintypes.IID(Class)
  dispatch = pythoncom.GetActiveObject(resultCLSID)
  dispatch = dispatch.QueryInterface(pythoncom.IID_IDispatch)
  return __WrapDispatch(dispatch, Class, resultCLSID = resultCLSID, clsctx = pythoncom.CLSCTX_ALL)

def Moniker(Pathname, clsctx = pythoncom.CLSCTX_ALL):
  """
    Python friendly version of GetObject's moniker functionality.
  """
  moniker, i, bindCtx = pythoncom.MkParseDisplayName(Pathname)
  dispatch = moniker.BindToObject(bindCtx, None, pythoncom.IID_IDispatch)
  return __WrapDispatch(dispatch, Pathname, clsctx = clsctx)
  
def Dispatch(dispatch, userName = None, resultCLSID = None, typeinfo = None, UnicodeToString=1, clsctx = pythoncom.CLSCTX_SERVER):
  """Creates a Dispatch based COM object.
  """
  dispatch, userName = dynamic._GetGoodDispatchAndUserName(dispatch,userName,clsctx)
  return __WrapDispatch(dispatch, userName, resultCLSID, typeinfo, UnicodeToString, clsctx)

def DispatchEx(clsid, machine=None, userName = None, resultCLSID = None, typeinfo = None, UnicodeToString=1, clsctx = None):
  """Creates a Dispatch based COM object on a specific machine.
  """
  # If InProc is registered, DCOM will use it regardless of the machine name 
  # (and regardless of the DCOM config for the object.)  So unless the user
  # specifies otherwise, we exclude inproc apps when a remote machine is used.
  if clsctx is None:
    clsctx = pythoncom.CLSCTX_SERVER
    if machine is not None: clsctx = clsctx & ~pythoncom.CLSCTX_INPROC
  if machine is None:
    serverInfo = None
  else:
    serverInfo = (machine,)          
  if userName is None: userName = clsid
  dispatch = pythoncom.CoCreateInstanceEx(clsid, None, clsctx, serverInfo, (pythoncom.IID_IDispatch,))[0]
  return Dispatch(dispatch, userName, resultCLSID, typeinfo, UnicodeToString=UnicodeToString, clsctx=clsctx)

class CDispatch(dynamic.CDispatch):
  """
    The dynamic class used as a last resort.
    The purpose of this overriding of dynamic.CDispatch is to perpetuate the policy
    of using the makepy generated wrapper Python class instead of dynamic.CDispatch
    if/when possible.
  """
  def _wrap_dispatch_(self, ob, userName = None, returnCLSID = None, UnicodeToString = 1):
    return Dispatch(ob, userName, returnCLSID,None,UnicodeToString)

class Constants:
  """A container for generated COM constants.
  """
  def __init__(self):
    self.__dicts__ = [] # A list of dictionaries
  def __getattr__(self, a):
    for d in self.__dicts__:
      if d.has_key(a):
        return d[a]
    raise AttributeError, a

# And create an instance.
constants = Constants()

# A helpers for DispatchWithEvents - this becomes __setattr__ for the
# temporary class.
def _event_setattr_(self, attr, val):
  try:
    # Does the COM object have an attribute of this name?
    self.__class__.__bases__[0].__setattr__(self, attr, val)
  except AttributeError:
    # Otherwise just stash it away in the instance.
    self.__dict__[attr] = val

# An instance of this "proxy" is created to break the COM circular references
# that exist (ie, when we connect to the COM events, COM keeps a reference
# to the object.  Thus, the Event connection must be manually broken before
# our object can die.  This solves the problem by manually breaking the connection
# to the real object as the proxy dies.
class EventsProxy:
  def __init__(self, ob):
    self.__dict__['_obj_'] = ob
  def __del__(self):
    self._obj_.close()
  def __getattr__(self, attr):
    return getattr(self._obj_, attr)
  def __setattr__(self, attr, val):
    return setattr(self._obj_, attr, val)

def DispatchWithEvents(clsid, user_event_class):
  """Create a COM object that can fire events to a user defined class.
  clsid -- The ProgID or CLSID of the object to create.
  user_event_class -- A Python class object that responds to the events.

  This requires makepy support for the COM object being created.  If
  this support does not exist it will be automatically generated by
  this function.  If the object does not support makepy, a TypeError
  exception will be raised.

  The result is a class instance that both represents the COM object
  and handles events from the COM object.

  It is important to note that the returned instance is not a direct
  instance of the user_event_class, but an instance of a temporary
  class object that derives from three classes:
  * The makepy generated class for the COM object
  * The makepy generated class for the COM events
  * The user_event_class as passed to this function.

  If this is not suitable, see the getevents function for an alternative
  technique of handling events.

  Example:

  >>> class IEEvents:
  ...    def OnVisible(self, visible):
  ...       print "Visible changed:", visible
  ...
  >>> ie = DispatchWithEvents("InternetExplorer.Application", IEEvents)
  >>> ie.Visible = 1
  Visible changed: 1
  >>> 
  """
  # First, get the object.
  disp = Dispatch(clsid)
  if not disp.__dict__.get("CLSID"): # Eeek - no makepy support - try and build it.
    try:
      ti = disp._oleobj_.GetTypeInfo()
      disp_clsid = ti.GetTypeAttr()[0]
      tlb, index = ti.GetContainingTypeLib()
      tla = tlb.GetLibAttr()
      mod = gencache.EnsureModule(tla[0], tla[1], tla[3], tla[4])
      # Get the class from the module.
      disp_class = mod.CLSIDToClassMap[str(disp_clsid)]
    except pythoncom.com_error:
      raise TypeError, "This COM object can not automate the makepy process - please run makepy manually for this object"
  else:
    disp_class = disp.__class__
  # Create a new class that derives from 3 classes - the dispatch class, the event sink class and the user class.
  import new
  events_class = getevents(clsid)
  result_class = new.classobj("COMEventClass", (disp_class, events_class, user_event_class), {"__setattr__" : _event_setattr_})
  instance = result_class(disp._oleobj_) # This only calls the first base class __init__.
  events_class.__init__(instance, instance)
  return EventsProxy(instance)

def getevents(clsid):
    """Determine the default outgoing interface for a class, given
    either a clsid or progid. It returns a class - you can
    conveniently derive your own handler from this class and implement
    the appropriate methods.

    This method relies on the classes produced by makepy. You must use
    either makepy or the gencache module to ensure that the
    appropriate support classes have been generated for the com server
    that you will be handling events from.

    Beware of creating circular references: this will happen if your
    handler has a reference to an object that has a reference back to
    the event source. Call the 'close' method to break the chain.
    
    Example:

    >>>win32com.client.gencache.EnsureModule('{EAB22AC0-30C1-11CF-A7EB-0000C05BAE0B}',0,1,1)
    <module 'win32com.gen_py.....
    >>>
    >>> class InternetExplorerEvents(win32com.client.getevents("InternetExplorer.Application.1")):
    ...    def OnVisible(self, Visible):
    ...        print "Visibility changed: ", Visible
    ...
    >>>
    >>> ie=win32com.client.Dispatch("InternetExplorer.Application.1")
    >>> events=InternetExplorerEvents(ie) 
    >>> ie.Visible=1
    Visibility changed:  1
    >>>
    """

    # find clsid given progid or clsid
    clsid=str(pywintypes.IID(clsid))
    # return default outgoing interface for that class
    return CLSIDToClass.GetClass(clsid).default_source
