import string, sys
from win32com.axscript.server.error import Exception
from win32com.axscript import axscript
from win32com.axscript.server import axsite
import pythoncom
from win32com.server import util, connect
import win32com.server.policy

class MySite(axsite.AXSite):

  def OnScriptError(self, error):
    exc = error.GetExceptionInfo()
    context, line, char = error.GetSourcePosition()
    print " >Exception:", exc[1]
    try:
      st = error.GetSourceLineText()
    except pythoncom.com_error:
      st = None
    if st is None: st = ""
    text = st + "\n" + (" " * (char-1)) + "^" + "\n" + exc[2]
    for line in string.split(text,"\n"):
      print "  >" + line

class MyCollection(util.Collection):
	def _NewEnum(self):
		print "Making new Enumerator"
		return util.Collection._NewEnum(self)

class Test:
  _public_methods_ = [ 'echo' ]
  _public_attrs_ = ['collection', 'verbose']
  def __init__(self):
    self.verbose = 0
    self.collection = util.wrap( MyCollection( [1,'Two',3] ))
    self.last = ""
#    self._connect_server_ = TestConnectServer(self)

  def echo(self, *args):
    self.last = string.join(map(str, args))
    if self.verbose:
      for arg in args:
        print arg,
      print
#    self._connect_server_.Broadcast(last)


#### Connections currently wont work, as there is no way for the engine to
#### know what events we support.  We need typeinfo support.

IID_ITestEvents = pythoncom.MakeIID("{8EB72F90-0D44-11d1-9C4B-00AA00125A98}")

class TestConnectServer(connect.ConnectableServer):
	_connect_interfaces_ = [IID_ITestEvents]
	# The single public method that the client can call on us
	# (ie, as a normal COM server, this exposes just this single method.
	def __init__(self, object):
		self.object = object
		
	def Broadcast(self,arg):
		# Simply broadcast a notification.
		self._BroadcastNotify(self.NotifyDoneIt, (arg,))

	def NotifyDoneIt(self, interface, arg):
		interface.Invoke(1000, 0, pythoncom.DISPATCH_METHOD, 1, arg)

VBScript = """\
prop = "Property Value"

sub hello(arg1)
   test.echo arg1
end sub
  
sub testcollection
   test.verbose = 1
   for each item in test.collection
     test.echo "Collection item is", item
   next
end sub
"""
PyScript = """\
print "PyScript is being parsed..."
prop = "Property Value"
def hello(arg1):
   test.echo(arg1)
   pass
   
def testcollection():
   test.verbose = 1
#   test.collection[1] = "New one"
   for item in test.collection:
     test.echo("Collection item is", item)
   pass
"""

ErrScript = """\
bad code for everyone!
"""

state_map = {
	axscript.SCRIPTSTATE_UNINITIALIZED: "SCRIPTSTATE_UNINITIALIZED",
	axscript.SCRIPTSTATE_INITIALIZED: "SCRIPTSTATE_INITIALIZED",
	axscript.SCRIPTSTATE_STARTED: "SCRIPTSTATE_STARTED",
	axscript.SCRIPTSTATE_CONNECTED: "SCRIPTSTATE_CONNECTED",
	axscript.SCRIPTSTATE_DISCONNECTED: "SCRIPTSTATE_DISCONNECTED",
	axscript.SCRIPTSTATE_CLOSED: "SCRIPTSTATE_CLOSED",
}

def _CheckEngineState(engine, name, state):
  got = engine.engine.eScript.GetScriptState()
  if got != state:
    got_name = state_map.get(got, str(got))
    state_name = state_map.get(state, str(state))
    raise RuntimeError, "Warning - engine %s has state %s, but expected %s" % (name, got_name, state_name)

def TestEngine(engineName, code, bShouldWork = 1):
  echoer = Test()
  model = {
    'test' : util.wrap(echoer),
    }

  site = MySite(model)
  engine = site._AddEngine(engineName)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_INITIALIZED)
  engine.AddCode(code)
  engine.Start()
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_STARTED)

  if not bShouldWork: return
  # Now call into the scripts IDispatch
  from win32com.client.dynamic import Dispatch
  ob = Dispatch(engine.GetScriptDispatch())
  try:
    ob.hello("Goober")
  except pythoncom.com_error, exc:
    print "***** Calling 'hello' failed", exc
    return
  if echoer.last != "Goober":
    print "***** Function call didnt set value correctly", `echoer.last`
    
  if str(ob.prop) != "Property Value":
    print "***** Property Value not correct - ", `ob.prop`

  ob.testcollection()

  # Now make sure my engines can evaluate stuff.
  result = engine.eParse.ParseScriptText("1+1", None, None, None, 0, 0, axscript.SCRIPTTEXT_ISEXPRESSION)
  if result != 2:
    print "Engine could not evaluate '1+1' - said the result was", result

  # re-initialize to make sure it transitions back to initialized again.
  engine.SetScriptState(axscript.SCRIPTSTATE_INITIALIZED)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_INITIALIZED)
  engine.Start()
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_STARTED)
  
  # Transition back to initialized, then through connected too.
  engine.SetScriptState(axscript.SCRIPTSTATE_INITIALIZED)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_INITIALIZED)
  engine.SetScriptState(axscript.SCRIPTSTATE_CONNECTED)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_CONNECTED)
  engine.SetScriptState(axscript.SCRIPTSTATE_INITIALIZED)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_INITIALIZED)

  engine.SetScriptState(axscript.SCRIPTSTATE_CONNECTED)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_CONNECTED)
  engine.SetScriptState(axscript.SCRIPTSTATE_DISCONNECTED)
  _CheckEngineState(site, engineName, axscript.SCRIPTSTATE_DISCONNECTED)
  engine.Close()
  #_CheckEngineState(site, engineName, axscript.SCRIPTSTATE_CLOSED)
  

def dotestall():
  TestEngine("VBScript", VBScript)
  TestEngine("Python", PyScript)
  print "Testing Exceptions"
  try:
    TestEngine("Python", ErrScript, 0)
  except pythoncom.com_error:
    pass
    
  try:
    TestEngine("VBScript", ErrScript, 0)
  except pythoncom.com_error:
    pass

def testall():
  dotestall()
  pythoncom.CoUninitialize()
  print "AXScript Host worked correctly - %d/%d COM objects left alive." % (pythoncom._GetInterfaceCount(), pythoncom._GetGatewayCount())

if __name__ == '__main__':
	testall()
