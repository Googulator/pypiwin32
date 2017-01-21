build_id = "220"  # may optionally include a ".{patchno}" suffix.
# Putting buildno at the top prevents automatic __doc__ assignment, and
# I *want* the build number at the top :)
__doc__ = """This is a distutils setup-script for the pywin32 extensions

To build the pywin32 extensions, simply execute:
  python setup2.py -q build
or
  python setup2.py -q install
to build and install into your current Python installation.

These extensions require a number of libraries to build, some of which may
require you to install special SDKs or toolkits.  This script will attempt
to build as many as it can, and at the end of the build will report any
extension modules that could not be built and why.

This has got complicated due to the various different versions of
Visual Studio used - some VS versions are not compatible with some SDK
versions.  Below are the Windows SDK versions required (and the URL - although
these are subject to being changed by MS at any time:)

Python 2.4->2.5:
  Microsoft Windows Software Development Kit Update for Windows Vista (version 6.0)
  http://www.microsoft.com/downloads/en/details.aspx?FamilyID=4377f86d-c913-4b5c-b87e-ef72e5b4e065

Python 2.6+:
  Microsoft Windows SDK for Windows 7 and .NET Framework 4 (version 7.1)
  http://www.microsoft.com/downloads/en/details.aspx?FamilyID=6b6c21d2-2006-4afa-9702-529fa782d63b

If you multiple SDK versions on a single machine, set the MSSDK environment
variable to point at the one you want to use.  Note that using the SDK for
a particular platform (eg, Windows 7) doesn't force you to use that OS as your
build environment.  If the links above don't work, use google to find them.

Building:
---------

To install the pywin32 extensions, execute:
  python setup2.py -q install

This will install the built extensions into your site-packages directory,
create an appropriate .pth file, and should leave everything ready to use.
There is no need to modify the registry.

To build or install debug (_d) versions of these extensions, ensure you have
built or installed a debug version of Python itself, then pass the "--debug"
flag to the build command - eg:
  python setup2.py -q build --debug
or to build and install a debug version:
  python setup2.py -q build --debug install

To build 64bit versions of this:

* py2.5 and earlier - sorry, I've given up in disgust.  Using VS2003 with
  the Vista SDK is just too painful to make work, and VS2005 is not used for
  any released versions of Python. See revision 1.69 of this file for the
  last version that attempted to support and document this process.

*  2.6 and later: On a 64bit OS, just build as you would on a 32bit platform.
   On a 32bit platform (ie, to cross-compile), you must use VS2008 to
   cross-compile Python itself. Note that by default, the 64bit tools are not
   installed with VS2008, so you may need to adjust your VS2008 setup. Then
   use:

      setup2.py build --plat-name=win-amd64

   see the distutils cross-compilation documentation for more details.
"""
# Originally by Thomas Heller, started in 2000 or so.
import os
import re
import string
import sys
from tempfile import gettempdir

is_py3k = sys.version_info > (3,)  # get this out of the way early on...
# We have special handling for _winreg so our setup3.py script can avoid
# using the 'imports' fixer and therefore start much faster...
import winreg

# The rest of our imports.
from setuptools import setup
from setuptools import Extension
from setuptools.command.build_ext import build_ext
from distutils.command.build import build
from distutils.command.install_data import install_data
from distutils.core import Command

bdist_msi = None  # Do not build any MSI scripts

from distutils import log

# some modules need a static CRT to avoid problems caused by them having a
# manifest.
static_crt_modules = ["winxpgui"]

from distutils.dep_util import newer_group
from distutils.sysconfig import get_config_vars
from distutils.filelist import FileList
import distutils.util
import distutils.file_util

# prevent the new in 3.5 suffix of "cpXX-win32" from being added.
# (adjusting both .cp35-win_amd64.pyd and .cp35-win32.pyd to .pyd)
try:
    get_config_vars()["EXT_SUFFIX"] = re.sub(
        "\\.cp\d\d-win((32)|(_amd64))", "", get_config_vars()["EXT_SUFFIX"])
except KeyError:
    pass  # no EXT_SUFFIX in this build.

build_id_patch = build_id
if not "." in build_id_patch:
    build_id_patch = build_id_patch + ".0"
pywin32_version = "%d.%d.%s" % (sys.version_info[0], sys.version_info[1],
                                build_id_patch)
print(("Building pywin32", pywin32_version))

try:
    this_file = __file__
except NameError:
    this_file = sys.argv[0]

this_file = os.path.abspath(this_file)
# We get upset if the cwd is not our source dir, but it is a PITA to
# insist people manually CD there first!
if os.path.dirname(this_file):
    os.chdir(os.path.dirname(this_file))


# Start address we assign base addresses from.  See comment re
# dll_base_address later in this file...
dll_base_address = 0x1e200000

class WinExt(Extension):
    # Base class for all win32 extensions, with some predefined
    # library and include dirs, and predefined windows libraries.
    # Additionally a method to parse .def files into lists of exported
    # symbols, and to read

    def __init__(self, name, sources,
                 include_dirs=[],
                 define_macros=None,
                 undef_macros=None,
                 library_dirs=[],
                 libraries="",
                 runtime_library_dirs=None,
                 extra_objects=None,
                 extra_compile_args=None,
                 extra_link_args=None,
                 export_symbols=None,
                 export_symbol_file=None,
                 pch_header=None,
                 windows_h_version=None,  # min version of windows.h needed.
                 extra_swig_commands=None,
                 is_regular_dll=False,  # regular Windows DLL?
                 # list of headers which may not be installed forcing us to
                 # skip this extension
                 optional_headers=[],
                 base_address=None,
                 depends=None,
                 platforms=None,  # none means 'all platforms'
                 unicode_mode=None,
                 # 'none'==default or specifically true/false.
                 implib_name=None,
                 delay_load_libraries="",
                 ):
        libary_dirs = library_dirs,
        include_dirs = ['com/win32com/src/include',
                        'win32/src'] + include_dirs
        libraries = libraries.split()
        self.delay_load_libraries = delay_load_libraries.split()
        libraries.extend(self.delay_load_libraries)

        if export_symbol_file:
            export_symbols = export_symbols or []
            export_symbols.extend(self.parse_def_file(export_symbol_file))

        # Some of our swigged files behave differently in distutils vs
        # MSVC based builds.  Always define DISTUTILS_BUILD so they can tell.
        define_macros = define_macros or []
        define_macros.append(("DISTUTILS_BUILD", None))
        define_macros.append(("_CRT_SECURE_NO_WARNINGS", None))
        self.pch_header = pch_header
        self.extra_swig_commands = extra_swig_commands or []
        self.windows_h_version = windows_h_version
        self.optional_headers = optional_headers
        self.is_regular_dll = is_regular_dll
        self.base_address = base_address
        self.platforms = platforms
        self.implib_name = implib_name
        Extension.__init__(self, name, sources,
                           include_dirs,
                           define_macros,
                           undef_macros,
                           library_dirs,
                           libraries,
                           runtime_library_dirs,
                           extra_objects,
                           extra_compile_args,
                           extra_link_args,
                           export_symbols)
        if not hasattr(self, 'swig_deps'):
            self.swig_deps = []
        self.extra_compile_args.extend(['/DUNICODE', '/D_UNICODE', '/DWINNT'])
        self.depends = depends or []  # stash it here, as py22 doesn't have it.
        self.unicode_mode = unicode_mode

        self.finalize_options()

    def parse_def_file(self, path):
        # Extract symbols to export from a def-file
        result = []
        for line in open(path).readlines():
            line = line.rstrip()
            if line and line[0] in string.whitespace:
                tokens = line.split()
                if not tokens[0][0] in string.ascii_letters:
                    continue
                result.append(','.join(tokens))
        return result

    def get_source_files(self, dsp):
        result = []
        if dsp is None:
            return result
        dsp_path = os.path.dirname(dsp)
        seen_swigs = []
        for line in open(dsp, "r"):
            fields = line.strip().split("=", 2)
            if fields[0] == "SOURCE":
                ext = os.path.splitext(fields[1])[1].lower()
                if ext in ['.cpp', '.c', '.i', '.rc', '.mc']:
                    pathname = os.path.normpath(
                        os.path.join(dsp_path, fields[1]))
                    result.append(pathname)
                    if ext == '.i':
                        seen_swigs.append(pathname)

        # ack - .dsp files may have references to the generated 'foomodule.cpp'
        # from 'foo.i' - but we now do things differently...
        for ss in seen_swigs:
            base, ext = os.path.splitext(ss)
            nuke = base + "module.cpp"
            try:
                result.remove(nuke)
            except ValueError:
                pass
        # Sort the sources so that (for example) the .mc file is processed first,
        # building this may create files included by other source files.
        build_order = ".i .mc .rc .cpp".split()
        decorated = sorted([(build_order.index(os.path.splitext(
            fname)[-1].lower()), fname) for fname in result])
        result = [item[1] for item in decorated]
        return result

    def finalize_options(self):
        if self.delay_load_libraries:
            self.libraries.append("delayimp")
            for delay_lib in self.delay_load_libraries:
                self.extra_link_args.append(
                    "/delayload:%s.dll" % delay_lib)

        # Try and find the MFC source code, so we can reach inside for
        # some of the ActiveX support we need.  We need to do this late, so
        # the environment is setup correctly.
        # Only used by the win32uiole extensions, but I can't be
        # bothered making a subclass just for this - so they all get it!
        found_mfc = False
        for incl in os.environ.get("INCLUDE", "").split(os.pathsep):
            # first is a "standard" MSVC install, second is the Vista SDK.
            for candidate in ("..\src\occimpl.h",
                              "..\..\src\mfc\occimpl.h"):
                check = os.path.join(incl, candidate)
                if os.path.isfile(check):
                    self.extra_compile_args.append(
                        '/DMFC_OCC_IMPL_H=\\"%s\\"' % candidate)
                    found_mfc = True
                    break
            if found_mfc:
                break

        if not hasattr(self, '_needs_stub'):
            self._needs_stub = False


class WinExt_pythonwin(WinExt):

    def __init__(self, name, **kw):
        if 'unicode_mode' not in kw:
            kw['unicode_mode'] = None
        kw.setdefault("extra_compile_args", []).extend(
            ['-D_AFXDLL', '-D_AFXEXT', '-D_MBCS'])

        WinExt.__init__(self, name, **kw)

    def get_pywin32_dir(self):
        return "pythonwin"


class WinExt_win32(WinExt):

    def __init__(self, name, **kw):
        WinExt.__init__(self, name, **kw)

    def get_pywin32_dir(self):
        return "win32"


class WinExt_ISAPI(WinExt):

    def get_pywin32_dir(self):
        return "isapi"


# Note this is used only for "win32com extensions", not pythoncom
# itself - thus, output is "win32comext"
class WinExt_win32com(WinExt):

    def __init__(self, name, **kw):
        kw["libraries"] = kw.get("libraries", "") + " oleaut32 ole32"

        # COM extensions require later windows headers.
        if not kw.get("windows_h_version"):
            kw["windows_h_version"] = 0x500
        WinExt.__init__(self, name, **kw)

    def get_pywin32_dir(self):
        return "win32comext/" + self.name


# Exchange extensions get special treatment:
# * Look for the Exchange SDK in the registry.
# * Output directory is different than the module's basename.
# * Require use of the Exchange 2000 SDK - this works for both VC6 and 7
class WinExt_win32com_mapi(WinExt_win32com):

    def __init__(self, name, **kw):
        # The Exchange 2000 SDK seems to install itself without updating
        # LIB or INCLUDE environment variables.  It does register the core
        # directory in the registry tho - look it up
        sdk_install_dir = None
        libs = kw.get("libraries", "")
        keyname = "SOFTWARE\Microsoft\Exchange\SDK"
        flags = winreg.KEY_READ
        try:
            flags |= winreg.KEY_WOW64_32KEY
        except AttributeError:
            # this version doesn't support 64 bits, so must already be using
            # 32bit key.
            pass
        for root in winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER:
            try:
                keyob = winreg.OpenKey(root, keyname, 0, flags)
                value, type_id = winreg.QueryValueEx(keyob, "INSTALLDIR")
                if type_id == winreg.REG_SZ:
                    sdk_install_dir = value
                    break
            except WindowsError:
                pass
        if sdk_install_dir is not None:
            d = os.path.join(sdk_install_dir, "SDK", "Include")
            if os.path.isdir(d):
                kw.setdefault("include_dirs", []).insert(0, d)
            d = os.path.join(sdk_install_dir, "SDK", "Lib")
            if os.path.isdir(d):
                kw.setdefault("library_dirs", []).insert(0, d)

        # The stand-alone exchange SDK has these libs
        if distutils.util.get_platform() == 'win-amd64':
            # Additional utility functions are only available for 32-bit
            # builds.
            pass
        else:
            libs += " version user32 advapi32 Ex2KSdk sadapi netapi32"
        kw["libraries"] = libs
        WinExt_win32com.__init__(self, name, **kw)

    def get_pywin32_dir(self):
        # 'win32com.mapi.exchange' and 'win32com.mapi.exchdapi' currently only
        # ones with this special requirement
        return "win32comext/mapi"


class WinExt_win32com_axdebug(WinExt_win32com):

    def __init__(self, name, **kw):
        # Later SDK versions again ship with activdbg.h, but if we attempt
        # to use our own copy of that file with that SDK, we fail to link.
        kw.setdefault('extra_compile_args', []).append("/DHAVE_SDK_ACTIVDBG")
        WinExt_win32com.__init__(self, name, **kw)


# A hacky extension class for pywintypesXX.dll and pythoncomXX.dll
class WinExt_system32(WinExt):

    def get_pywin32_dir(self):
        return "pywin32_system32"


class WinExt_Executable(WinExt):
    pass


class WinExt_win32_Executable(WinExt_Executable, WinExt_win32):
    pass


class WinExt_pythonwin_Executable(WinExt_Executable, WinExt_pythonwin):
    pass

################################################################
# Extensions to the distutils commands.

# Start with 2to3 related stuff for py3k.
do_2to3 = is_py3k


# 'build' command
class my_build(build):

    def run(self):
        build.run(self)
        # write a pywin32.version.txt.
        ver_fname = os.path.join(gettempdir(), "pywin32.version.txt")
        try:
            f = open(ver_fname, "w")
            f.write("%s\n" % build_id)
            f.close()
        except EnvironmentError as why:
            print(("Failed to open '%s': %s" % (ver_fname, why)))


class build_scintilla(Command):
    """
    Wrapper for distutil command `build`.
    """

    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def _build_scintilla(self):
        path = 'pythonwin\\Scintilla'
        makefile = 'makefile_pythonwin'
        makeargs = []

        if self.debug:
            makeargs.append("DEBUG=1")
        if not self.verbose:
            makeargs.append("/C")  # nmake: /C Suppress output messages
            makeargs.append("QUIET=1")
        # We build the DLL into our own temp directory, then copy it to the
        # real directory - this avoids the generated .lib/.exp
        build_temp = os.path.abspath(
            os.path.join(
                self.build_temp,
                "scintilla"))
        self.mkpath(build_temp)
        # Use short-names, as the scintilla makefiles barf with spaces.
        if " " in build_temp:
            # ack - can't use win32api!!!  This is the best I could come up
            # with:
            # C:\>for %I in ("C:\Program Files",) do @echo %~sI
            # C:\PROGRA~1
            cs = os.environ.get('comspec', 'cmd.exe')
            cmd = cs + ' /c for %I in ("' + build_temp + '",) do @echo %~sI'
            build_temp = os.popen(cmd).read().strip()
            assert os.path.isdir(build_temp), build_temp
        makeargs.append("SUB_DIR_O=%s" % build_temp)
        makeargs.append("SUB_DIR_BIN=%s" % build_temp)
        makeargs.append("DIR_PYTHON=%s" % sys.prefix)

        cwd = os.getcwd()
        os.chdir(path)
        try:
            cmd = ["nmake.exe", "/nologo", "/f", makefile] + makeargs
            self.spawn(cmd)
        finally:
            os.chdir(cwd)

        # The DLL goes in the Pythonwin directory.
        if self.debug:
            base_name = "scintilla_d.dll"
        else:
            base_name = "scintilla.dll"
        self.copy_file(
            os.path.join(self.build_temp, "scintilla", base_name),
            os.path.join(self.build_lib, "pythonwin"))

    def run(self):
        if getattr(self, 'dry_run', False):
            return
        self._build_scintilla()


class my_build_ext(build_ext):

    def finalize_options(self):
        build_ext.finalize_options(self)
        self.windows_h_version = None
        # The pywintypes library is created in the build_temp
        # directory, so we need to add this to library_dirs
        self.library_dirs.append(self.build_temp)
        self.mingw32 = (self.compiler == "mingw32")
        if self.mingw32:
            self.libraries.append("stdc++")

        self.excluded_extensions = []  # list of (ext, why)
        self.swig_cpp = True  # hrm - deprecated - should use swig_opts=-c++??
        if not hasattr(self, 'plat_name'):
            # Old Python version that doesn't support cross-compile
            self.plat_name = distutils.util.get_platform()

    def _link_executable(self, *args, **kwargs):
        del kwargs['export_symbols']
        del kwargs['build_temp']
        return self.compiler.link_executable(*args, **kwargs)

    def run(self):
        build_ext.run(self)
        # self.run_command('build_scintilla')

    def build_extensions(self):
        # First, sanity-check the 'extensions' list
        self.check_extensions_list(self.extensions)

        self.found_libraries = {}

        if not hasattr(self.compiler, 'initialized'):
            # 2.3 and earlier initialized at construction
            self.compiler.initialized = True
        else:
            if not self.compiler.initialized:
                self.compiler.initialize()

        link_shared_object = self.compiler.link_shared_object
        # Here we hack a "pywin32" directory (one of 'win32', 'win32com',
        # 'pythonwin' etc), as distutils doesn't seem to like the concept
        # of multiple top-level directories.
        assert self.package is None
        for ext in self.extensions:
            try:
                self.package = ext.get_pywin32_dir()
            except AttributeError:
                raise RuntimeError("Not a win32 package!")
            if issubclass(type(ext), WinExt_Executable):
                self.compiler.link_shared_object = self._link_executable
            self.build_extension(ext)
            if issubclass(type(ext), WinExt_Executable):
                self.compiler.link_shared_object = link_shared_object
            if not issubclass(type(ext), WinExt_Executable):
                extra = self.debug and "_d.lib" or ".lib"
                if ext.name in ("pywintypes", "pythoncom", "axscript"):
                    name1 = "%s%s" % (ext.name, extra)
                    name2 = "%s%s" % (ext.name, extra)
                elif ext.name in ("win32ui",):
                    name1 = name2 = ext.name + extra
                else:
                    name1 = name2 = None
                if name1 is not None:
                    src = os.path.join(self.build_temp, os.path.dirname(ext.sources[0]), name1)
                    dst = os.path.join(self.build_temp, name2)
                if os.path.abspath(src) != os.path.abspath(dst) and os.path.isfile(src) and not os.path.isfile(dst):
                    print('Copying file from: ', src)
                    print('Copying file to: ', dst)
                    self.copy_file(src, dst)

                    # self._copy_mfc()
        # self._build_scintilla()
        # Copy cpp lib files needed to create Python COM extensions
        # print('Listing build directory:')
        # self.list_files(os.path.dirname(self.build_temp))
        # The MFC DLLs.

    def _copy_mfc(self):
        try:
            target_dir = os.path.join(self.build_lib, "pythonwin")
            if sys.hexversion < 0x2060000:
                # hrm - there doesn't seem to be a 'redist' directory for this
                # compiler (even the installation CDs only seem to have the MFC
                # DLLs in the "win\system" directory - just grab it from
                # system32 (but we can't even use win32api for that!)
                src = os.path.join(
                    os.environ.get('SystemRoot'),
                    'System32',
                    'mfc71.dll')
                if not os.path.isfile(src):
                    raise RuntimeError("Can't find %r" % (src,))
                self.copy_file(src, target_dir)
            else:
                plat_dir_64 = "x64"
                # 2.6, 2.7, 3.0, 3.1 and 3.2 all use(d) vs2008 (compiler
                # version 1500)
                if sys.hexversion < 0x3030000:
                    product_key = r"SOFTWARE\Microsoft\VisualStudio\9.0\Setup\VC"
                    plat_dir_64 = "amd64"
                    mfc_dir = "Microsoft.VC90.MFC"
                    mfc_files = "mfc90.dll mfc90u.dll mfcm90.dll mfcm90u.dll Microsoft.VC90.MFC.manifest".split()
                # 3.3 and 3.4 use(d) vs2010 (compiler version 1600, crt=10)
                elif sys.hexversion < 0x3050000:
                    product_key = r"SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VC"
                    mfc_dir = "Microsoft.VC100.MFC"
                    mfc_files = ["mfc100u.dll", "mfcm100u.dll"]
                # 3.5 and later on vs2015 (compiler version 1900, crt=14)
                else:
                    product_key = r"SOFTWARE\Microsoft\VisualStudio\14.0\Setup\VC"
                    mfc_dir = "Microsoft.VC140.MFC"
                    mfc_files = ["mfc140u.dll", "mfcm140u.dll"]

                # On a 64bit host, the value we are looking for is actually in
                # SysWow64Node - but that is only available on xp and later.
                access = winreg.KEY_READ
                if sys.getwindowsversion()[0] >= 5:
                    access = access | 512  # KEY_WOW64_32KEY
                if self.plat_name == 'win-amd64':
                    plat_dir = plat_dir_64
                else:
                    plat_dir = "x86"
                # Find the redist directory.
                vckey = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, product_key,
                                       0, access)
                val, val_typ = winreg.QueryValueEx(vckey, "ProductDir")
                mfc_dir = os.path.join(val, "redist", plat_dir, mfc_dir)
                if not os.path.isdir(mfc_dir):
                    raise RuntimeError(
                        "Can't find the redist dir at %r" %
                        (mfc_dir))
                for f in mfc_files:
                    self.copy_file(os.path.join(mfc_dir, f), target_dir)
        except (EnvironmentError, RuntimeError) as exc:
            print(("Can't find an installed VC for the MFC DLLs:", exc))

    def get_ext_filename(self, name):
        # The pywintypes and pythoncom extensions have special names
        extra_dll = self.debug and "_d.dll" or ".dll"
        # *sob* - python fixed this bug in python 3.1 (bug 6403)
        # So in the fixed versions we only get the base name, and if the
        # output name is simply 'dir\name' we need to nothing.

        if name in ['perfmondata', 'PyISAPI_loader']:
            return name + extra_dll
        else:
            return build_ext.get_ext_filename(self, name)

    def get_export_symbols(self, ext):
        if issubclass(type(ext), WinExt_Executable):
            return []
        elif ext.is_regular_dll:
            return ext.export_symbols
        else:
            return build_ext.get_export_symbols(self, ext)

    def find_swig(self):
        if "SWIG" in os.environ:
            swig = os.environ["SWIG"]
        else:
            # We know where our swig is
            swig = os.path.abspath(r"swig\swig.exe")
        lib = os.path.join(os.path.dirname(swig), "swig_lib")
        os.environ["SWIG_LIB"] = lib
        return swig

    def swig_sources(self, sources, extension):
        new_sources = []
        swig_sources = []
        swig_targets = {}
        # XXX this drops generated C/C++ files into the source tree, which
        # is fine for developers who want to distribute the generated
        # source -- but there should be an option to put SWIG output in
        # the temp dir.
        # Adding py3k to the mix means we *really* need to move to generating
        # to the temp dir...
        target_ext = '.cpp'
        for source in sources:
            (base, sext) = os.path.splitext(source)
            if sext == ".i":  # SWIG interface file
                if os.path.split(base)[1] in swig_include_files:
                    continue
                swig_sources.append(source)
                # Patch up the filenames for various special cases...
                if os.path.basename(base) in swig_interface_parents:
                    swig_targets[source] = base + target_ext
                else:
                    new_target = '%s_swig%s' % (base, target_ext)
                    new_sources.append(new_target)
                    swig_targets[source] = new_target
            else:
                new_sources.append(source)

        if not swig_sources:
            return new_sources

        swig = self.find_swig()
        for source in swig_sources:
            swig_cmd = [swig, "-python", "-c++"]
            swig_cmd.append("-dnone", )  # we never use the .doc files.
            swig_cmd.extend(extension.extra_swig_commands)
            if not is_py3k:
                swig_cmd.append("-DSWIG_PY2K")
            if distutils.util.get_platform() == 'win-amd64':
                swig_cmd.append("-DSWIG_PY64BIT")
            else:
                swig_cmd.append("-DSWIG_PY32BIT")
            target = swig_targets[source]
            try:
                interface_parent = swig_interface_parents[
                    os.path.basename(os.path.splitext(source)[0])]
            except KeyError:
                # "normal" swig file - no special win32 issues.
                pass
            else:
                # Using win32 extensions to SWIG for generating COM classes.
                if interface_parent is not None:
                    # generating a class, not a module.
                    swig_cmd.append("-pythoncom")
                    if interface_parent:
                        # A class deriving from other than the default
                        swig_cmd.extend(
                            ["-com_interface_parent", interface_parent])

            # This 'newer' check helps python 2.2 builds, which otherwise
            # *always* regenerate the .cpp files, meaning every future
            # build for any platform sees these as dirty.
            # This could probably go once we generate .cpp into the temp dir.
            fqsource = os.path.abspath(source)
            fqtarget = os.path.abspath(target)
            rebuild = self.force or (
                ext and newer_group(
                    ext.swig_deps +
                    [fqsource],
                    fqtarget))
            log.debug("should swig %s->%s=%s", source, target, rebuild)
            if rebuild:
                swig_cmd.extend(["-o", fqtarget, fqsource])
                log.info("swigging %s to %s", source, target)
                out_dir = os.path.dirname(source)
                cwd = os.getcwd()
                os.chdir(out_dir)
                try:
                    self.spawn(swig_cmd)
                finally:
                    os.chdir(cwd)
            else:
                log.info("skipping swig of %s", source)

        return new_sources


################################################################

class my_install_data(install_data):
    """A custom install_data command, which will install it's files
    into the standard directories (normally lib/site-packages).
    """

    def finalize_options(self):
        if self.install_dir is None:
            installobj = self.distribution.get_command_obj('install')
            self.install_dir = installobj.install_lib
        print(('Installing data files to %s' % self.install_dir))
        install_data.finalize_options(self)

        # TODO: Find out why these files exist
        data_files = []
        for directory, files in self.data_files:
            files = list(files)
            for file in files.copy():
                file_path = os.path.join(self.install_dir, directory, file)
                print(file_path)
                if os.path.exists(file_path):
                    files.remove(file)
            data_files.append((directory, files))

            dir_path = os.path.join(self.install_dir, directory)
            if os.path.isfile(dir_path):
                os.remove(dir_path)

        self.data_files = data_files


################################################################

pywintypes = WinExt_system32('pywintypes',
                             sources=[
                                 "win32/src/PyACL.cpp",
                                 "win32/src/PyDEVMODE.cpp",
                                 "win32/src/PyHANDLE.cpp",
                                 "win32/src/PyIID.cpp",
                                 "win32/src/PyLARGE_INTEGER.cpp",
                                 "win32/src/PyOVERLAPPED.cpp",
                                 "win32/src/PySECURITY_ATTRIBUTES.cpp",
                                 "win32/src/PySECURITY_DESCRIPTOR.cpp",
                                 "win32/src/PySID.cpp",
                                 "win32/src/PyTime.cpp",
                                 "win32/src/PyUnicode.cpp",
                                 "win32/src/PyWAVEFORMATEX.cpp",
                                 "win32/src/PyWinTypesmodule.cpp",
                             ],
                             depends=[
                                 "win32/src/PyWinObjects.h",
                                 "win32/src/PyWinTypes.h",
                                 "win32/src/PySoundObjects.h",
                                 "win32/src/PySecurityObjects.h",
                             ],
                             extra_compile_args=['-DBUILD_PYWINTYPES'],
                             libraries="advapi32 user32 ole32 oleaut32",
                             pch_header="PyWinTypes.h",
                             )

win32_extensions = [pywintypes]

win32_extensions.append(
    WinExt_win32("perfmondata",
                 sources=[
                     "win32/src/PerfMon/PyPerfMsgs.mc",
                     "win32/src/PerfMon/perfmondata.cpp",
                 ],
                 libraries="advapi32",
                 unicode_mode=True,
                 export_symbol_file="win32/src/PerfMon/perfmondata.def",
                 is_regular_dll=1,
                 depends=[
                     "win32/src/PerfMon/perfutil.h",
                     "win32/src/PerfMon/PyPerfMonControl.h",
                 ],
                 ),
)

for info in (
        # (name, libraries, UNICODE, WINVER, sources)
        ("mmapfile", "", None, None, "win32/src/mmapfilemodule.cpp"),
        ("odbc", "odbc32 odbccp32", None, None, "win32/src/odbc.cpp"),
        ("perfmon", "", True, None, """
            win32/src/PerfMon/MappingManager.cpp
            win32/src/PerfMon/PerfCounterDefn.cpp
            win32/src/PerfMon/PerfObjectType.cpp
            win32/src/PerfMon/PyPerfMon.cpp
            """),
        ("timer", "user32", None, None, "win32/src/timermodule.cpp"),
        ("win2kras", "rasapi32", None, 0x0500, "win32/src/win2krasmodule.cpp"),
        ("win32cred", "AdvAPI32 credui", True,
         0x0501, 'win32/src/win32credmodule.cpp'),
        ("win32crypt", "Crypt32 Advapi32", True, 0x0500, """
            win32/src/win32crypt/win32cryptmodule.cpp
            win32/src/win32crypt/win32crypt_structs.cpp
            win32/src/win32crypt/PyCERTSTORE.cpp
            win32/src/win32crypt/PyCERT_CONTEXT.cpp
            win32/src/win32crypt/PyCRYPTHASH.cpp
            win32/src/win32crypt/PyCRYPTKEY.cpp
            win32/src/win32crypt/PyCRYPTMSG.cpp
            win32/src/win32crypt/PyCRYPTPROV.cpp
            win32/src/win32crypt/PyCTL_CONTEXT.cpp
            """),
        ("win32file", "", None, 0x0500, """
              win32/src/win32file.i
              win32/src/win32file_comm.cpp
              """),
        ("win32event", "user32", None, None, "win32/src/win32event.i"),
        ("win32clipboard", "gdi32 user32 shell32", None,
         None, "win32/src/win32clipboardmodule.cpp"),

        # win32gui handled below
        ("win32job", "user32", True, 0x0500, 'win32/src/win32job.i'),
        ("win32lz", "lz32", None, None, "win32/src/win32lzmodule.cpp"),
        ("win32net", "netapi32 advapi32", True, None, """
              win32/src/win32net/win32netfile.cpp    win32/src/win32net/win32netgroup.cpp
              win32/src/win32net/win32netmisc.cpp    win32/src/win32net/win32netmodule.cpp
              win32/src/win32net/win32netsession.cpp win32/src/win32net/win32netuse.cpp
              win32/src/win32net/win32netuser.cpp
              """),
        ("win32pdh", "", True, None, "win32/src/win32pdhmodule.cpp"),
        ("win32pipe", "", None, None, 'win32/src/win32pipe.i win32/src/win32popen.cpp'),
        ("win32print", "winspool user32 gdi32", None,
         0x0500, "win32/src/win32print/win32print.cpp"),
        ("win32process", "advapi32 user32", None,
         0x0500, "win32/src/win32process.i"),
        ("win32profile", "Userenv", True, None,
         'win32/src/win32profilemodule.cpp'),
        ("win32ras", "rasapi32 user32", None,
         0x0500, "win32/src/win32rasmodule.cpp"),
        ("win32security", "advapi32 user32 netapi32", True, 0x0500, """
            win32/src/win32security.i
            win32/src/win32security_sspi.cpp win32/src/win32security_ds.cpp
            """),
        ("win32service", "advapi32 oleaut32 user32", True, 0x0501, """
            win32/src/win32service_messages.mc
            win32/src/win32service.i
            """),
        ("win32trace", "advapi32", None, None, "win32/src/win32trace.cpp"),
        ("win32wnet", "netapi32 mpr", None, None, """
            win32/src/win32wnet/PyNCB.cpp
            win32/src/win32wnet/PyNetresource.cpp
            win32/src/win32wnet/win32wnet.cpp
            """),
        ("win32inet", "wininet", None, 0x500, """
            win32/src/win32inet.i
            win32/src/win32inet_winhttp.cpp
            """),
        ("win32console", "kernel32", True, 0x0501,
         "win32/src/win32consolemodule.cpp"),
        ("win32ts", "WtsApi32", True, 0x0501, "win32/src/win32tsmodule.cpp"),
        ("_win32sysloader", "", None, 0x0501, "win32/src/_win32sysloader.cpp"),
        ("win32transaction", "kernel32", True, 0x0501,
         "win32/src/win32transactionmodule.cpp"),

):

    name, lib_names, unicode_mode = info[:3]
    # unicode_mode == None means "not on py2.6, yes on py3", True means everywhere
    # False means nowhere.
    windows_h_ver = sources = None
    if len(info) > 3:
        windows_h_ver = info[3]
    if len(info) > 4:
        sources = info[4].split()
    extra_compile_args = []
    ext = WinExt_win32(name,
                       libraries=lib_names,
                       extra_compile_args=extra_compile_args,
                       windows_h_version=windows_h_ver,
                       sources=sources,
                       unicode_mode=unicode_mode)
    win32_extensions.append(ext)

# The few that need slightly special treatment
win32_extensions += [
    WinExt_win32("win32evtlog",
                 sources="""
                win32\\src\\win32evtlog_messages.mc win32\\src\\win32evtlog.i
                """.split(),
                 libraries="advapi32 oleaut32",
                 delay_load_libraries="wevtapi",
                 windows_h_version=0x0600
                 ),
    WinExt_win32("win32api",
                 sources="""
                win32/src/win32apimodule.cpp win32/src/win32api_display.cpp
                """.split(),
                 libraries="user32 advapi32 shell32 version",
                 delay_load_libraries="powrprof",
                 windows_h_version=0x0500,
                 ),
    WinExt_win32("win32gui",
                 sources="""
                win32/src/win32dynamicdialog.cpp
                win32/src/win32gui.i
               """.split(),
                 windows_h_version=0x0500,
                 libraries="gdi32 user32 comdlg32 comctl32 shell32",
                 define_macros=[("WIN32GUI", None)],
                 ),
    # winxpgui is built from win32gui.i, but sets up different #defines before
    # including windows.h.  It also has an XP style manifest.
    # WinExt_win32("winxpgui",
    #             sources="""
    #            win32/src/winxpgui.rc win32/src/win32dynamicdialog.cpp
    #            win32/src/win32gui.i
    #           """.split(),
    #             libraries="gdi32 user32 comdlg32 comctl32 shell32",
    #             windows_h_version=0x0500,
    #             define_macros=[("WIN32GUI", None), ("WINXPGUI", None)],
    #             extra_swig_commands=["-DWINXPGUI"],
    #             ),
    # winxptheme
    # WinExt_win32("_winxptheme",
    #             sources=["win32/src/_winxptheme.i"],
    #             libraries="gdi32 user32 comdlg32 comctl32 shell32 Uxtheme",
    #             windows_h_version=0x0500,
    #             ),
]
win32_extensions += [
    WinExt_win32('servicemanager',
                 sources=[
                     "win32/src/PythonServiceMessages.mc",
                     "win32/src/PythonService.cpp"],
                 extra_compile_args=['-DPYSERVICE_BUILD_DLL'],
                 libraries="user32 ole32 advapi32 shell32",
                 windows_h_version=0x500,
                 unicode_mode=True, ),
]

# win32help uses htmlhelp.lib which is built with MSVC7 and /GS.  This
# causes problems with references to the @__security_check_cookie magic.
# Use bufferoverflowu.lib if it exists.
win32help_libs = "htmlhelp user32 advapi32"
win32_extensions += [
    WinExt_win32('win32help',
                 sources=["win32/src/win32helpmodule.cpp"],
                 libraries=win32help_libs,
                 windows_h_version=0x500),
]

dirs = {
    'adsi': 'com/win32comext/adsi/src',
    'propsys': 'com/win32comext/propsys/src',
    'shell': 'com/win32comext/shell/src',
    'axcontrol': 'com/win32comext/axcontrol/src',
    'axdebug': 'com/win32comext/axdebug/src',
    'axscript': 'com/win32comext/axscript/src',
    'directsound': 'com/win32comext/directsound/src',
    'ifilter': 'com/win32comext/ifilter/src',
    'internet': 'com/win32comext/internet/src',
    'mapi': 'com/win32comext/mapi/src',
    'authorization': 'com/win32comext/authorization/src',
    'taskscheduler': 'com/win32comext/taskscheduler/src',
    'bits': 'com/win32comext/bits/src',
    'win32com': 'com/win32com/src',
}

# The COM modules.
pythoncom = WinExt_system32(
    'pythoncom',
    sources=(
        """
                        %(win32com)s/dllmain.cpp            %(win32com)s/ErrorUtils.cpp
                        %(win32com)s/MiscTypes.cpp          %(win32com)s/oleargs.cpp
                        %(win32com)s/PyComHelpers.cpp       %(win32com)s/PyFactory.cpp
                        %(win32com)s/PyGatewayBase.cpp      %(win32com)s/PyIBase.cpp
                        %(win32com)s/PyIClassFactory.cpp    %(win32com)s/PyIDispatch.cpp
                        %(win32com)s/PyIUnknown.cpp         %(win32com)s/PyRecord.cpp
                        %(win32com)s/extensions/PySTGMEDIUM.cpp %(win32com)s/PyStorage.cpp
                        %(win32com)s/PythonCOM.cpp          %(win32com)s/Register.cpp
                        %(win32com)s/stdafx.cpp             %(win32com)s/univgw.cpp
                        %(win32com)s/univgw_dataconv.cpp    %(win32com)s/extensions/PyFUNCDESC.cpp
                        %(win32com)s/extensions/PyGConnectionPoint.cpp      %(win32com)s/extensions/PyGConnectionPointContainer.cpp
                        %(win32com)s/extensions/PyGEnumVariant.cpp          %(win32com)s/extensions/PyGErrorLog.cpp
                        %(win32com)s/extensions/PyGPersist.cpp              %(win32com)s/extensions/PyGPersistPropertyBag.cpp
                        %(win32com)s/extensions/PyGPersistStorage.cpp       %(win32com)s/extensions/PyGPersistStream.cpp
                        %(win32com)s/extensions/PyGPersistStreamInit.cpp    %(win32com)s/extensions/PyGPropertyBag.cpp
                        %(win32com)s/extensions/PyGStream.cpp               %(win32com)s/extensions/PyIBindCtx.cpp
                        %(win32com)s/extensions/PyICatInformation.cpp       %(win32com)s/extensions/PyICatRegister.cpp
                        %(win32com)s/extensions/PyIConnectionPoint.cpp      %(win32com)s/extensions/PyIConnectionPointContainer.cpp
                        %(win32com)s/extensions/PyICreateTypeInfo.cpp       %(win32com)s/extensions/PyICreateTypeLib.cpp
                        %(win32com)s/extensions/PyICreateTypeLib2.cpp       %(win32com)s/extensions/PyIDataObject.cpp
                        %(win32com)s/extensions/PyIDropSource.cpp           %(win32com)s/extensions/PyIDropTarget.cpp
                        %(win32com)s/extensions/PyIEnumCATEGORYINFO.cpp     %(win32com)s/extensions/PyIEnumConnectionPoints.cpp
                        %(win32com)s/extensions/PyIEnumConnections.cpp      %(win32com)s/extensions/PyIEnumFORMATETC.cpp
                        %(win32com)s/extensions/PyIEnumGUID.cpp             %(win32com)s/extensions/PyIEnumSTATPROPSETSTG.cpp
                        %(win32com)s/extensions/PyIEnumSTATPROPSTG.cpp      %(win32com)s/extensions/PyIEnumSTATSTG.cpp
                        %(win32com)s/extensions/PyIEnumString.cpp           %(win32com)s/extensions/PyIEnumVARIANT.cpp
                        %(win32com)s/extensions/PyIErrorLog.cpp             %(win32com)s/extensions/PyIExternalConnection.cpp
                        %(win32com)s/extensions/PyIGlobalInterfaceTable.cpp %(win32com)s/extensions/PyILockBytes.cpp
                        %(win32com)s/extensions/PyIMoniker.cpp              %(win32com)s/extensions/PyIOleWindow.cpp
                        %(win32com)s/extensions/PyIPersist.cpp              %(win32com)s/extensions/PyIPersistFile.cpp
                        %(win32com)s/extensions/PyIPersistPropertyBag.cpp   %(win32com)s/extensions/PyIPersistStorage.cpp
                        %(win32com)s/extensions/PyIPersistStream.cpp        %(win32com)s/extensions/PyIPersistStreamInit.cpp
                        %(win32com)s/extensions/PyIPropertyBag.cpp          %(win32com)s/extensions/PyIPropertySetStorage.cpp
                        %(win32com)s/extensions/PyIPropertyStorage.cpp      %(win32com)s/extensions/PyIProvideClassInfo.cpp
                        %(win32com)s/extensions/PyIRunningObjectTable.cpp   %(win32com)s/extensions/PyIServiceProvider.cpp
                        %(win32com)s/extensions/PyIStorage.cpp              %(win32com)s/extensions/PyIStream.cpp
                        %(win32com)s/extensions/PyIType.cpp                 %(win32com)s/extensions/PyITypeObjects.cpp
                        %(win32com)s/extensions/PyTYPEATTR.cpp              %(win32com)s/extensions/PyVARDESC.cpp
                        %(win32com)s/extensions/PyICancelMethodCalls.cpp    %(win32com)s/extensions/PyIContext.cpp
                        %(win32com)s/extensions/PyIEnumContextProps.cpp     %(win32com)s/extensions/PyIClientSecurity.cpp
                        %(win32com)s/extensions/PyIServerSecurity.cpp
                        """ %
        dirs).split(),
    depends=(
        r"""
                    %(win32com)s/include\propbag.h          %(win32com)s/include\PyComTypeObjects.h
                    %(win32com)s/include\PyFactory.h        %(win32com)s/include\PyGConnectionPoint.h
                    %(win32com)s/include\PyGConnectionPointContainer.h
                    %(win32com)s/include\PyGPersistStorage.h %(win32com)s/include\PyIBindCtx.h
                    %(win32com)s/include\PyICatInformation.h %(win32com)s/include\PyICatRegister.h
                    %(win32com)s/include\PyIDataObject.h    %(win32com)s/include\PyIDropSource.h
                    %(win32com)s/include\PyIDropTarget.h    %(win32com)s/include\PyIEnumConnectionPoints.h
                    %(win32com)s/include\PyIEnumConnections.h %(win32com)s/include\PyIEnumFORMATETC.h
                    %(win32com)s/include\PyIEnumGUID.h      %(win32com)s/include\PyIEnumSTATPROPSETSTG.h
                    %(win32com)s/include\PyIEnumSTATSTG.h   %(win32com)s/include\PyIEnumString.h
                    %(win32com)s/include\PyIEnumVARIANT.h   %(win32com)s/include\PyIExternalConnection.h
                    %(win32com)s/include\PyIGlobalInterfaceTable.h %(win32com)s/include\PyILockBytes.h
                    %(win32com)s/include\PyIMoniker.h       %(win32com)s/include\PyIOleWindow.h
                    %(win32com)s/include\PyIPersist.h       %(win32com)s/include\PyIPersistFile.h
                    %(win32com)s/include\PyIPersistStorage.h %(win32com)s/include\PyIPersistStream.h
                    %(win32com)s/include\PyIPersistStreamInit.h %(win32com)s/include\PyIRunningObjectTable.h
                    %(win32com)s/include\PyIStorage.h       %(win32com)s/include\PyIStream.h
                    %(win32com)s/include\PythonCOM.h        %(win32com)s/include\PythonCOMRegister.h
                    %(win32com)s/include\PythonCOMServer.h  %(win32com)s/include\stdafx.h
                    %(win32com)s/include\univgw_dataconv.h
                    %(win32com)s/include/PyICancelMethodCalls.h    %(win32com)s/include/PyIContext.h
                    %(win32com)s/include/PyIEnumContextProps.h     %(win32com)s/include/PyIClientSecurity.h
                    %(win32com)s/include/PyIServerSecurity.h
                    """ %
        dirs).split(),
    libraries="oleaut32 ole32 user32 urlmon",
    export_symbol_file='com/win32com/src/PythonCOM.def',
    extra_compile_args=['-DBUILD_PYTHONCOM'],
    pch_header="stdafx.h",
    windows_h_version=0x500,
    base_address=dll_base_address,
)
dll_base_address += 0x80000  # pythoncom is large!
com_extensions = [pythoncom]
com_extensions += [
    WinExt_win32com('adsi', libraries="ACTIVEDS ADSIID user32 advapi32",
                    sources=("""
                        %(adsi)s/adsi.i                 %(adsi)s/adsi.cpp
                        %(adsi)s/PyIADsContainer.i      %(adsi)s/PyIADsContainer.cpp
                        %(adsi)s/PyIADsUser.i           %(adsi)s/PyIADsUser.cpp
                        %(adsi)s/PyIADsDeleteOps.i      %(adsi)s/PyIADsDeleteOps.cpp
                        %(adsi)s/PyIDirectoryObject.i   %(adsi)s/PyIDirectoryObject.cpp
                        %(adsi)s/PyIDirectorySearch.i   %(adsi)s/PyIDirectorySearch.cpp
                        %(adsi)s/PyIDsObjectPicker.i    %(adsi)s/PyIDsObjectPicker.cpp

                        %(adsi)s/adsilib.i
                        %(adsi)s/PyADSIUtil.cpp         %(adsi)s/PyDSOPObjects.cpp
                        %(adsi)s/PyIADs.cpp
                        """ % dirs).split()),
    WinExt_win32com('axcontrol', pch_header="axcontrol_pch.h",
                    sources=("""
                        %(axcontrol)s/AXControl.cpp
                        %(axcontrol)s/PyIOleControl.cpp          %(axcontrol)s/PyIOleControlSite.cpp
                        %(axcontrol)s/PyIOleInPlaceActiveObject.cpp
                        %(axcontrol)s/PyIOleInPlaceSiteEx.cpp    %(axcontrol)s/PyISpecifyPropertyPages.cpp
                        %(axcontrol)s/PyIOleInPlaceUIWindow.cpp  %(axcontrol)s/PyIOleInPlaceFrame.cpp
                        %(axcontrol)s/PyIObjectWithSite.cpp      %(axcontrol)s/PyIOleInPlaceObject.cpp
                        %(axcontrol)s/PyIOleInPlaceSiteWindowless.cpp  %(axcontrol)s/PyIViewObject.cpp
                        %(axcontrol)s/PyIOleClientSite.cpp       %(axcontrol)s/PyIOleInPlaceSite.cpp
                        %(axcontrol)s/PyIOleObject.cpp           %(axcontrol)s/PyIViewObject2.cpp
                        %(axcontrol)s/PyIOleCommandTarget.cpp
                        """ % dirs).split()),
    WinExt_win32com('axscript',
                    sources=("""
                        %(axscript)s/AXScript.cpp
                        %(axscript)s/GUIDS.cpp                   %(axscript)s/PyGActiveScript.cpp
                        %(axscript)s/PyGActiveScriptError.cpp    %(axscript)s/PyGActiveScriptParse.cpp
                        %(axscript)s/PyGActiveScriptSite.cpp     %(axscript)s/PyGObjectSafety.cpp
                        %(axscript)s/PyIActiveScript.cpp         %(axscript)s/PyIActiveScriptError.cpp
                        %(axscript)s/PyIActiveScriptParse.cpp    %(axscript)s/PyIActiveScriptParseProcedure.cpp
                        %(axscript)s/PyIActiveScriptSite.cpp     %(axscript)s/PyIMultiInfos.cpp
                        %(axscript)s/PyIObjectSafety.cpp         %(axscript)s/stdafx.cpp
                        """ % dirs).split(),
                    depends=("""
                             %(axscript)s/AXScript.h
                             %(axscript)s/guids.h                %(axscript)s/PyGActiveScriptError.h
                             %(axscript)s/PyIActiveScriptError.h %(axscript)s/PyIObjectSafety.h
                             %(axscript)s/PyIProvideMultipleClassInfo.h
                             %(axscript)s/stdafx.h
                             """ % dirs).split(),
                    extra_compile_args=['-DPY_BUILD_AXSCRIPT'],
                    implib_name="axscript",
                    pch_header="stdafx.h"
                    ),
    # ActiveDebugging is a mess.  See the comments in the docstring of this
    # module for details on getting it built.
    WinExt_win32com_axdebug('axdebug',
                            libraries="axscript",
                            pch_header="stdafx.h",
                            sources=("""
                    %(axdebug)s/AXDebug.cpp
                    %(axdebug)s/PyIActiveScriptDebug.cpp
                    %(axdebug)s/PyIActiveScriptErrorDebug.cpp
                    %(axdebug)s/PyIActiveScriptSiteDebug.cpp
                    %(axdebug)s/PyIApplicationDebugger.cpp
                    %(axdebug)s/PyIDebugApplication.cpp
                    %(axdebug)s/PyIDebugApplicationNode.cpp
                    %(axdebug)s/PyIDebugApplicationNodeEvents.cpp
                    %(axdebug)s/PyIDebugApplicationThread.cpp
                    %(axdebug)s/PyIDebugCodeContext.cpp
                    %(axdebug)s/PyIDebugDocument.cpp
                    %(axdebug)s/PyIDebugDocumentContext.cpp
                    %(axdebug)s/PyIDebugDocumentHelper.cpp
                    %(axdebug)s/PyIDebugDocumentHost.cpp
                    %(axdebug)s/PyIDebugDocumentInfo.cpp
                    %(axdebug)s/PyIDebugDocumentProvider.cpp
                    %(axdebug)s/PyIDebugDocumentText.cpp
                    %(axdebug)s/PyIDebugDocumentTextAuthor.cpp
                    %(axdebug)s/PyIDebugDocumentTextEvents.cpp
                    %(axdebug)s/PyIDebugDocumentTextExternalAuthor.cpp
                    %(axdebug)s/PyIDebugExpression.cpp
                    %(axdebug)s/PyIDebugExpressionCallBack.cpp
                    %(axdebug)s/PyIDebugExpressionContext.cpp
                    %(axdebug)s/PyIDebugProperties.cpp
                    %(axdebug)s/PyIDebugSessionProvider.cpp
                    %(axdebug)s/PyIDebugStackFrame.cpp
                    %(axdebug)s/PyIDebugStackFrameSniffer.cpp
                    %(axdebug)s/PyIDebugStackFrameSnifferEx.cpp
                    %(axdebug)s/PyIDebugSyncOperation.cpp
                    %(axdebug)s/PyIEnumDebugApplicationNodes.cpp
                    %(axdebug)s/PyIEnumDebugCodeContexts.cpp
                    %(axdebug)s/PyIEnumDebugExpressionContexts.cpp
                    %(axdebug)s/PyIEnumDebugPropertyInfo.cpp
                    %(axdebug)s/PyIEnumDebugStackFrames.cpp
                    %(axdebug)s/PyIEnumRemoteDebugApplications.cpp
                    %(axdebug)s/PyIEnumRemoteDebugApplicationThreads.cpp
                    %(axdebug)s/PyIMachineDebugManager.cpp
                    %(axdebug)s/PyIMachineDebugManagerEvents.cpp
                    %(axdebug)s/PyIProcessDebugManager.cpp
                    %(axdebug)s/PyIProvideExpressionContexts.cpp
                    %(axdebug)s/PyIRemoteDebugApplication.cpp
                    %(axdebug)s/PyIRemoteDebugApplicationEvents.cpp
                    %(axdebug)s/PyIRemoteDebugApplicationThread.cpp
                    %(axdebug)s/stdafx.cpp
                     """ % dirs).split(),
                            ),
    WinExt_win32com('internet', pch_header="internet_pch.h",
                    sources=("""
                        %(internet)s/internet.cpp                   %(internet)s/PyIDocHostUIHandler.cpp
                        %(internet)s/PyIHTMLOMWindowServices.cpp    %(internet)s/PyIInternetBindInfo.cpp
                        %(internet)s/PyIInternetPriority.cpp        %(internet)s/PyIInternetProtocol.cpp
                        %(internet)s/PyIInternetProtocolInfo.cpp    %(internet)s/PyIInternetProtocolRoot.cpp
                        %(internet)s/PyIInternetProtocolSink.cpp    %(internet)s/PyIInternetSecurityManager.cpp
                    """ % dirs).split(),
                    depends=["%(internet)s/internet_pch.h" % dirs]),

    # WinExt_win32com('mapi', libraries='mapi32', pch_header='PythonCOM.h',
    #                    sources=('''
    #                        %(mapi)s/mapi.i                 %(mapi)s/mapi.cpp
    #                        %(mapi)s/PyIABContainer.i       %(mapi)s/PyIABContainer.cpp
    #                        %(mapi)s/PyIAddrBook.i          %(mapi)s/PyIAddrBook.cpp
    #                        %(mapi)s/PyIAttach.i            %(mapi)s/PyIAttach.cpp
    #                        %(mapi)s/PyIDistList.i          %(mapi)s/PyIDistList.cpp
    #                        %(mapi)s/PyIMailUser.i          %(mapi)s/PyIMailUser.cpp
    #                        %(mapi)s/PyIMAPIContainer.i     %(mapi)s/PyIMAPIContainer.cpp
    #                        %(mapi)s/PyIMAPIFolder.i        %(mapi)s/PyIMAPIFolder.cpp
    #                        %(mapi)s/PyIMAPIProp.i          %(mapi)s/PyIMAPIProp.cpp
    #                        %(mapi)s/PyIMAPISession.i       %(mapi)s/PyIMAPISession.cpp
    #                        %(mapi)s/PyIMAPIStatus.i        %(mapi)s/PyIMAPIStatus.cpp
    #                        %(mapi)s/PyIMAPITable.i         %(mapi)s/PyIMAPITable.cpp
    #                        %(mapi)s/PyIMessage.i           %(mapi)s/PyIMessage.cpp
    #                        %(mapi)s/PyIMsgServiceAdmin.i   %(mapi)s/PyIMsgServiceAdmin.cpp
    #                        %(mapi)s/PyIMsgStore.i          %(mapi)s/PyIMsgStore.cpp
    #                        %(mapi)s/PyIProfAdmin.i         %(mapi)s/PyIProfAdmin.cpp
    #                        %(mapi)s/PyIProfSect.i          %(mapi)s/PyIProfSect.cpp
    #                        %(mapi)s/PyIConverterSession.i	%(mapi)s/PyIConverterSession.cpp
    #                        %(mapi)s/PyIMAPIAdviseSink.cpp
    #                        %(mapi)s/mapiutil.cpp
    #                        %(mapi)s/mapiguids.cpp
    #                        ''' % dirs).split()),
    #    WinExt_win32com_mapi('exchange', libraries='mapi32',
    #                         sources=('''
    #                                  %(mapi)s/exchange.i         %(mapi)s/exchange.cpp
    #                                  %(mapi)s/PyIExchangeManageStore.i %(mapi)s/PyIExchangeManageStore.cpp
    #                                  ''' % dirs).split()),
    #    WinExt_win32com_mapi('exchdapi', libraries='mapi32',
    #                         sources=('''
    #                                  %(mapi)s/exchdapi.i         %(mapi)s/exchdapi.cpp
    #                                  ''' % dirs).split()),

    #                         %(shell)s/PyIAsyncOperation.cpp

    WinExt_win32com('shell', libraries='shell32', pch_header="shell_pch.h",
                    windows_h_version=0x600,
                    sources=("""
                        %(shell)s/PyIActiveDesktop.cpp
                        %(shell)s/PyIApplicationDestinations.cpp
                        %(shell)s/PyIApplicationDocumentLists.cpp
                        %(shell)s/PyIBrowserFrameOptions.cpp
                        %(shell)s/PyICategorizer.cpp
                        %(shell)s/PyICategoryProvider.cpp
                        %(shell)s/PyIColumnProvider.cpp
                        %(shell)s/PyIContextMenu.cpp
                        %(shell)s/PyIContextMenu2.cpp
                        %(shell)s/PyIContextMenu3.cpp
                        %(shell)s/PyICopyHook.cpp
                        %(shell)s/PyICurrentItem.cpp
                        %(shell)s/PyICustomDestinationList.cpp
                        %(shell)s/PyIDefaultExtractIconInit.cpp
                        %(shell)s/PyIDeskBand.cpp
                        %(shell)s/PyIDisplayItem.cpp
                        %(shell)s/PyIDockingWindow.cpp
                        %(shell)s/PyIDropTargetHelper.cpp
                        %(shell)s/PyIEnumExplorerCommand.cpp
                        %(shell)s/PyIEnumIDList.cpp
                        %(shell)s/PyIEnumObjects.cpp
                        %(shell)s/PyIEnumResources.cpp
                        %(shell)s/PyIEnumShellItems.cpp
                        %(shell)s/PyIEmptyVolumeCache.cpp
                        %(shell)s/PyIEmptyVolumeCacheCallBack.cpp
                        %(shell)s/PyIExplorerBrowser.cpp
                        %(shell)s/PyIExplorerBrowserEvents.cpp
                        %(shell)s/PyIExplorerCommand.cpp
                        %(shell)s/PyIExplorerCommandProvider.cpp
                        %(shell)s/PyIExplorerPaneVisibility.cpp
                        %(shell)s/PyIExtractIcon.cpp
                        %(shell)s/PyIExtractIconW.cpp
                        %(shell)s/PyIExtractImage.cpp
                        %(shell)s/PyIFileOperation.cpp
                        %(shell)s/PyIFileOperationProgressSink.cpp
                        %(shell)s/PyIIdentityName.cpp
                        %(shell)s/PyIInputObject.cpp
                        %(shell)s/PyIKnownFolder.cpp
                        %(shell)s/PyIKnownFolderManager.cpp
                        %(shell)s/PyINameSpaceTreeControl.cpp
                        %(shell)s/PyIObjectArray.cpp
                        %(shell)s/PyIObjectCollection.cpp
                        %(shell)s/PyIPersistFolder.cpp
                        %(shell)s/PyIPersistFolder2.cpp
                        %(shell)s/PyIQueryAssociations.cpp
                        %(shell)s/PyIRelatedItem.cpp
                        %(shell)s/PyIShellBrowser.cpp
                        %(shell)s/PyIShellExtInit.cpp
                        %(shell)s/PyIShellFolder.cpp
                        %(shell)s/PyIShellFolder2.cpp
                        %(shell)s/PyIShellIcon.cpp
                        %(shell)s/PyIShellIconOverlay.cpp
                        %(shell)s/PyIShellIconOverlayIdentifier.cpp
                        %(shell)s/PyIShellIconOverlayManager.cpp
                        %(shell)s/PyIShellItem.cpp
                        %(shell)s/PyIShellItem2.cpp
                        %(shell)s/PyIShellItemArray.cpp
                        %(shell)s/PyIShellItemResources.cpp
                        %(shell)s/PyIShellLibrary.cpp
                        %(shell)s/PyIShellLink.cpp
                        %(shell)s/PyIShellLinkDataList.cpp
                        %(shell)s/PyIShellView.cpp
                        %(shell)s/PyITaskbarList.cpp
                        %(shell)s/PyITransferAdviseSink.cpp
                        %(shell)s/PyITransferDestination.cpp
                        %(shell)s/PyITransferMediumItem.cpp
                        %(shell)s/PyITransferSource.cpp
                        %(shell)s/PyIUniformResourceLocator.cpp
                        %(shell)s/shell.cpp
                        """ % dirs).split()),

    WinExt_win32com('propsys', libraries='propsys', delay_load_libraries='shell32',
                    unicode_mode=True,
                    sources=("""
                        %(propsys)s/propsys.cpp
                        %(propsys)s/PyIInitializeWithFile.cpp
                        %(propsys)s/PyIInitializeWithStream.cpp
                        %(propsys)s/PyINamedPropertyStore.cpp
                        %(propsys)s/PyIPropertyDescription.cpp
                        %(propsys)s/PyIPropertyDescriptionAliasInfo.cpp
                        %(propsys)s/PyIPropertyDescriptionList.cpp
                        %(propsys)s/PyIPropertyDescriptionSearchInfo.cpp
                        %(propsys)s/PyIPropertyEnumType.cpp
                        %(propsys)s/PyIPropertyEnumTypeList.cpp
                        %(propsys)s/PyIPropertyStore.cpp
                        %(propsys)s/PyIPropertyStoreCache.cpp
                        %(propsys)s/PyIPropertyStoreCapabilities.cpp
                        %(propsys)s/PyIPropertySystem.cpp
                        %(propsys)s/PyPROPVARIANT.cpp
                        %(propsys)s/PyIPersistSerializedPropStorage.cpp
                        %(propsys)s/PyIObjectWithPropertyKey.cpp
                        %(propsys)s/PyIPropertyChange.cpp
                        %(propsys)s/PyIPropertyChangeArray.cpp
                        """ % dirs).split(),
                    implib_name="pypropsys",
                    ),

    WinExt_win32com('taskscheduler', libraries='mstask',
                    sources=("""
                        %(taskscheduler)s/taskscheduler.cpp
                        %(taskscheduler)s/PyIProvideTaskPage.cpp
                        %(taskscheduler)s/PyIScheduledWorkItem.cpp
                        %(taskscheduler)s/PyITask.cpp
                        %(taskscheduler)s/PyITaskScheduler.cpp
                        %(taskscheduler)s/PyITaskTrigger.cpp

                        """ % dirs).split()),
    WinExt_win32com('bits', libraries='Bits', pch_header="bits_pch.h",
                    sources=("""
                        %(bits)s/bits.cpp
                        %(bits)s/PyIBackgroundCopyManager.cpp
                        %(bits)s/PyIBackgroundCopyCallback.cpp
                        %(bits)s/PyIBackgroundCopyError.cpp
                        %(bits)s/PyIBackgroundCopyJob.cpp
                        %(bits)s/PyIBackgroundCopyJob2.cpp
                        %(bits)s/PyIBackgroundCopyJob3.cpp
                        %(bits)s/PyIBackgroundCopyFile.cpp
                        %(bits)s/PyIBackgroundCopyFile2.cpp
                        %(bits)s/PyIEnumBackgroundCopyJobs.cpp
                        %(bits)s/PyIEnumBackgroundCopyFiles.cpp

                        """ % dirs).split()),
    WinExt_win32com('ifilter', libraries='ntquery',
                    sources=("%(ifilter)s/PyIFilter.cpp" % dirs).split(),
                    depends=(
                        "%(ifilter)s/PyIFilter.h %(ifilter)s/stdafx.h" %
                        dirs).split(),
                    ),
    WinExt_win32com('directsound', pch_header='directsound_pch.h',
                    sources=("""
                        %(directsound)s/directsound.cpp     %(directsound)s/PyDSBCAPS.cpp
                        %(directsound)s/PyDSBUFFERDESC.cpp  %(directsound)s/PyDSCAPS.cpp
                        %(directsound)s/PyDSCBCAPS.cpp      %(directsound)s/PyDSCBUFFERDESC.cpp
                        %(directsound)s/PyDSCCAPS.cpp       %(directsound)s/PyIDirectSound.cpp
                        %(directsound)s/PyIDirectSoundBuffer.cpp %(directsound)s/PyIDirectSoundCapture.cpp
                        %(directsound)s/PyIDirectSoundCaptureBuffer.cpp
                        %(directsound)s/PyIDirectSoundNotify.cpp
                        """ % dirs).split(),
                    depends=("""
                        %(directsound)s/directsound_pch.h   %(directsound)s/PyIDirectSound.h
                        %(directsound)s/PyIDirectSoundBuffer.h %(directsound)s/PyIDirectSoundCapture.h
                        %(directsound)s/PyIDirectSoundCaptureBuffer.h %(directsound)s/PyIDirectSoundNotify.h
                        """ % dirs).split(),
                    optional_headers=['dsound.h'],
                    libraries='user32 dsound dxguid'),
    WinExt_win32com('authorization', libraries='aclui advapi32',
                    sources=("""
                        %(authorization)s/authorization.cpp
                        %(authorization)s/PyGSecurityInformation.cpp
                        """ % dirs).split()),
]

pythonwin_extensions = [
    WinExt_pythonwin("win32ui",
                     sources=[
                         "Pythonwin/dbgthread.cpp",
                         "Pythonwin/dibapi.cpp",
                         "Pythonwin/dllmain.cpp",
                         "Pythonwin/pythondoc.cpp",
                         "Pythonwin/pythonppage.cpp",
                         "Pythonwin/pythonpsheet.cpp",
                         "Pythonwin/pythonRichEditCntr.cpp",
                         "Pythonwin/pythonRichEditDoc.cpp",
                         "Pythonwin/pythonview.cpp",
                         "Pythonwin/stdafx.cpp",
                         "Pythonwin/win32app.cpp",
                         "Pythonwin/win32assoc.cpp",
                         "Pythonwin/win32bitmap.cpp",
                         "Pythonwin/win32brush.cpp",
                         "Pythonwin/win32cmd.cpp",
                         "Pythonwin/win32cmdui.cpp",
                         "Pythonwin/win32context.cpp",
                         "Pythonwin/win32control.cpp",
                         "Pythonwin/win32ctledit.cpp",
                         "Pythonwin/win32ctrlList.cpp",
                         "Pythonwin/win32ctrlRichEdit.cpp",
                         "Pythonwin/win32ctrlTree.cpp",
                         "Pythonwin/win32dc.cpp",
                         "Pythonwin/win32dlg.cpp",
                         "Pythonwin/win32dlgbar.cpp",
                         "Pythonwin/win32dll.cpp",
                         "Pythonwin/win32doc.cpp",
                         "win32/src/win32dynamicdialog.cpp",
                         "Pythonwin/win32font.cpp",
                         "Pythonwin/win32gdi.cpp",
                         "Pythonwin/win32ImageList.cpp",
                         "Pythonwin/win32menu.cpp",
                         "Pythonwin/win32notify.cpp",
                         "Pythonwin/win32pen.cpp",
                         "Pythonwin/win32prinfo.cpp",
                         "Pythonwin/win32prop.cpp",
                         "Pythonwin/win32rgn.cpp",
                         "Pythonwin/win32RichEdit.cpp",
                         "Pythonwin/win32RichEditDocTemplate.cpp",
                         "Pythonwin/win32splitter.cpp",
                         "Pythonwin/win32template.cpp",
                         "Pythonwin/win32thread.cpp",
                         "Pythonwin/win32toolbar.cpp",
                         "Pythonwin/win32tooltip.cpp",
                         "Pythonwin/win32ui.rc",
                         "Pythonwin/win32uimodule.cpp",
                         "Pythonwin/win32util.cpp",
                         "Pythonwin/win32view.cpp",
                         "Pythonwin/win32virt.cpp",
                         "Pythonwin/win32win.cpp",
                     ],
                     extra_compile_args=['-DBUILD_PYW'],
                     pch_header="stdafx.h", base_address=dll_base_address,
                     depends=[
                         "Pythonwin/stdafx.h",
                         "Pythonwin/win32uiExt.h",
                         "Pythonwin/dibapi.h",
                         "Pythonwin/pythoncbar.h",
                         "Pythonwin/pythondoc.h",
                         "Pythonwin/pythonframe.h",
                         "Pythonwin/pythonppage.h",
                         "Pythonwin/pythonpsheet.h",
                         "Pythonwin/pythonRichEdit.h",
                         "Pythonwin/pythonRichEditCntr.h",
                         "Pythonwin/pythonRichEditDoc.h",
                         "Pythonwin/pythonview.h",
                         "Pythonwin/pythonwin.h",
                         "Pythonwin/Win32app.h",
                         "Pythonwin/win32assoc.h",
                         "Pythonwin/win32bitmap.h",
                         "Pythonwin/win32brush.h",
                         "Pythonwin/win32cmd.h",
                         "Pythonwin/win32cmdui.h",
                         "Pythonwin/win32control.h",
                         "Pythonwin/win32ctrlList.h",
                         "Pythonwin/win32ctrlTree.h",
                         "Pythonwin/win32dc.h",
                         "Pythonwin/win32dlg.h",
                         "Pythonwin/win32dlgbar.h",
                         "win32/src/win32dynamicdialog.h",
                         "Pythonwin/win32dll.h",
                         "Pythonwin/win32doc.h",
                         "Pythonwin/win32font.h",
                         "Pythonwin/win32gdi.h",
                         "Pythonwin/win32hl.h",
                         "Pythonwin/win32ImageList.h",
                         "Pythonwin/win32menu.h",
                         "Pythonwin/win32pen.h",
                         "Pythonwin/win32prinfo.h",
                         "Pythonwin/win32prop.h",
                         "Pythonwin/win32rgn.h",
                         "Pythonwin/win32RichEdit.h",
                         "Pythonwin/win32RichEditDocTemplate.h",
                         "Pythonwin/win32splitter.h",
                         "Pythonwin/win32template.h",
                         "Pythonwin/win32toolbar.h",
                         "Pythonwin/win32ui.h",
                         "Pythonwin/Win32uiHostGlue.h",
                         "Pythonwin/win32win.h",
                     ],
                     optional_headers=['afxres.h']),
    WinExt_pythonwin("win32uiole",
                     sources=[
                         "Pythonwin/stdafxole.cpp",
                         "Pythonwin/win32oleDlgInsert.cpp",
                         "Pythonwin/win32oleDlgs.cpp",
                         "Pythonwin/win32uiole.cpp",
                         "Pythonwin/win32uioleClientItem.cpp",
                         "Pythonwin/win32uioledoc.cpp",
                     ],
                     depends=[
                         "Pythonwin/stdafxole.h",
                         "Pythonwin/win32oleDlgs.h",
                         "Pythonwin/win32uioledoc.h",
                     ],
                     pch_header="stdafxole.h",
                     windows_h_version=0x500,
                     optional_headers=['afxres.h']),
    WinExt_pythonwin("dde",
                     sources=[
                         "Pythonwin/stddde.cpp",
                         "Pythonwin/ddetopic.cpp",
                         "Pythonwin/ddeconv.cpp",
                         "Pythonwin/ddeitem.cpp",
                         "Pythonwin/ddemodule.cpp",
                         "Pythonwin/ddeserver.cpp",
                     ],
                     pch_header="stdafxdde.h",
                     depends=["win32/src/stddde.h", "pythonwin/ddemodule.h"],
                     optional_headers=['afxres.h']),
]
# win32ui is large, so we reserve more bytes than normal
dll_base_address += 0x100000

other_extensions = []
other_extensions.append(
    WinExt_ISAPI('PyISAPI_loader',
                 sources=[os.path.join("isapi", "src", s) for s in
                          """PyExtensionObjects.cpp PyFilterObjects.cpp
                             pyISAPI.cpp pyISAPI_messages.mc
                             PythonEng.cpp StdAfx.cpp Utils.cpp
                          """.split()],
                 # We keep pyISAPI_messages.h out of the depends list, as it is
                 # generated and we aren't smart enough to say *only* the .cpp etc
                 # depend on it - so the generated .h says the .mc needs to be
                 # rebuilt, which re-creates the .h...
                 depends=[os.path.join("isapi", "src", s) for s in
                          """ControlBlock.h FilterContext.h PyExtensionObjects.h
                             PyFilterObjects.h pyISAPI.h
                             PythonEng.h StdAfx.h Utils.h
                          """.split()],
                 pch_header="StdAfx.h",
                 is_regular_dll=1,
                 export_symbols="""HttpExtensionProc GetExtensionVersion
                           TerminateExtension GetFilterVersion
                           HttpFilterProc TerminateFilter
                           PyISAPISetOptions WriteEventLogMessage
                           """.split(),
                 libraries='advapi32',
                 )
)

W32_exe_files = [
    WinExt_win32_Executable("pythonservice",
                            sources=[os.path.join("win32", "src", s) for s in
                          "PythonService.cpp PythonService.rc".split()],
                            unicode_mode=True,
                            extra_link_args=["/SUBSYSTEM:CONSOLE"],
                            libraries="user32 advapi32 ole32 shell32"),
    WinExt_pythonwin_Executable("Pythonwin",
                                sources=[
                         "Pythonwin/pythonwin.cpp",
                         "Pythonwin/pythonwin.rc",
                         "Pythonwin/stdafxpw.cpp",
                     ],
                                extra_link_args=["/SUBSYSTEM:WINDOWS", "/ENTRY:wWinMainCRTStartup"],
                                optional_headers=['afxres.h']),
]

# Special definitions for SWIG.
swig_interface_parents = {
    # source file base,     "base class" for generated COM support
    'mapi': None,  # not a class, but module
    'PyIMailUser': 'IMAPIContainer',
    'PyIABContainer': 'IMAPIContainer',
    'PyIAddrBook': 'IMAPIProp',
    'PyIAttach': 'IMAPIProp',
    'PyIDistList': 'IMAPIContainer',
    'PyIMailUser': 'IMAPIContainer',
    'PyIMAPIContainer': 'IMAPIProp',
    'PyIMAPIFolder': 'IMAPIContainer',
    'PyIMAPIProp': '',  # '' == default base
    'PyIMAPISession': '',
    'PyIMAPIStatus': 'IMAPIProp',
    'PyIMAPITable': '',
    'PyIMessage': 'IMAPIProp',
    'PyIMsgServiceAdmin': '',
    'PyIMsgStore': 'IMAPIProp',
    'PyIProfAdmin': '',
    'PyIProfSect': 'IMAPIProp',
    'PyIConverterSession': '',
    # exchange and exchdapi
    'exchange': None,
    'exchdapi': None,
    'PyIExchangeManageStore': '',
    # ADSI
    'adsi': None,  # module
    'PyIADsContainer': 'IDispatch',
    'PyIADsDeleteOps': 'IDispatch',
    'PyIADsUser': 'IADs',
    'PyIDirectoryObject': '',
    'PyIDirectorySearch': '',
    'PyIDsObjectPicker': '',
    'PyIADs': 'IDispatch',
}

# .i files that are #included, and hence are not part of the build.  Our .dsp
# parser isn't smart enough to differentiate these.
swig_include_files = "mapilib adsilib".split()


# Helper to allow our script specifications to include wildcards.
def expand_modules(module_dir):
    flist = FileList()
    flist.findall(module_dir)
    flist.include_pattern("*.py", anchor=0)
    return [os.path.splitext(name)[0] for name in flist.files]


# NOTE: somewhat counter-intuitively, a result list a-la:
#  [('Lib/site-packages\\pythonwin', ('pythonwin/license.txt',)),]
# will 'do the right thing' in terms of installing licence.txt into
# 'Lib/site-packages/pythonwin/licence.txt'.  We exploit this to
# get 'com/win32com/whatever' installed to 'win32com/whatever'
def convert_data_files(files):
    ret = []
    for file in files:
        file = os.path.normpath(file)
        if file.find("*") >= 0:
            flist = FileList()
            flist.findall(os.path.dirname(file))
            flist.include_pattern(os.path.basename(file), anchor=0)
            # We never want CVS
            flist.exclude_pattern(
                re.compile(".*\\\\CVS\\\\"), is_regex=1, anchor=0)
            flist.exclude_pattern("*.pyc", anchor=0)
            flist.exclude_pattern("*.pyo", anchor=0)
            # files with a leading dot upset bdist_msi, and '.*' doesn't
            # work - it matches from the start of the string and we have
            # dir names.  So any '\.' gets the boot.
            flist.exclude_pattern(re.compile(".*\\\\\."), is_regex=1, anchor=0)
            if not flist.files:
                raise RuntimeError("No files match '%s'" % file)
            files_use = flist.files
        else:
            if not os.path.isfile(file):
                raise RuntimeError("No file '%s'" % file)
            files_use = (file,)
        for fname in files_use:
            path_use = os.path.dirname(fname)
            if path_use.startswith("com/") or path_use.startswith("com\\"):
                path_use = path_use[4:]
            ret.append((path_use, (fname,)))
    return ret


def convert_optional_data_files(files):
    ret = []
    for file in files:
        try:
            temp = convert_data_files([file])
        except RuntimeError as details:
            if not str(details.args[0]).startswith("No file"):
                raise
            log.info('NOTE: Optional file %s not found - skipping' % file)
        else:
            ret.append(temp[0])
    return ret


################################################################
if len(sys.argv) == 1:
    # distutils will print usage - print our docstring first.
    print(__doc__)
    print("Standard usage information follows:")

package_dir = {
    "win32com": "com/win32com",
    "win32comext": "com/win32comext",
    "pythonwin": "pythonwin"
}

packages = ['win32com',
            'win32com.client',
            'win32com.demos',
            'win32com.makegw',
            'win32com.server',
            'win32com.servers',
            'win32com.test',

            'win32comext.adsi',

            'win32comext.axscript',
            'win32comext.axscript.client',
            'win32comext.axscript.server',

            'win32comext.axdebug',

            'win32comext.propsys',
            'win32comext.shell',
            'win32comext.mapi',
            'win32comext.ifilter',
            'win32comext.internet',
            'win32comext.axcontrol',
            'win32comext.taskscheduler',
            'win32comext.directsound',
            'win32comext.directsound.test',
            'win32comext.authorization',
            'win32comext.bits',

            'pythonwin.pywin',
            'pythonwin.pywin.debugger',
            'pythonwin.pywin.dialogs',
            'pythonwin.pywin.docking',
            'pythonwin.pywin.framework',
            'pythonwin.pywin.framework.editor',
            'pythonwin.pywin.framework.editor.color',
            'pythonwin.pywin.idle',
            'pythonwin.pywin.mfc',
            'pythonwin.pywin.scintilla',
            'pythonwin.pywin.tools',
            'isapi',
            ]

py_modules = expand_modules("win32\\lib")
ext_modules = win32_extensions + com_extensions + pythonwin_extensions + \
              other_extensions + W32_exe_files

# Build a map of DLL base addresses.  According to Python's PC\dllbase_nt.txt,
# we start at 0x1e200000 and go up in 0x00020000 increments.  A couple of
# our modules just go over this limit, so we use 30000.  We also do it sorted
# so each module gets the same addy each build.
# Note: If a module specifies a base address it still gets a slot reserved
# here which is unused.  We can live with that tho.
names = sorted([ext.name for ext in ext_modules])
dll_base_addresses = {}
for name in names:
    dll_base_addresses[name] = dll_base_address
    dll_base_address += 0x30000

cmdclass = {
    'build': my_build,
    'build_ext': my_build_ext,
    'install_data': my_install_data,
    'build_scintilla': build_scintilla, }

dist = setup(name="pywin32",
             version=str(build_id),
             description="Python for Window Extensions",
             long_description="Python extensions for Microsoft Windows\n"
                              "Provides access to much of the Win32 API, the\n"
                              "ability to create and use COM objects, and the\n"
                              "Pythonwin environment.",
             author="Mark Hammond (et al)",
             author_email="mhammond@users.sourceforge.net",
             url="http://sourceforge.net/projects/pywin32/",
             license="PSF",
             install_requires=['adodbapi'],
             cmdclass=cmdclass,
             options={"bdist_wininst":
                          {"install_script": "pywin32_postinstall.py",
                           "title": "pywin32-%s" % (build_id,),
                           "user_access_control": "auto",
                           },
                      "bdist_msi":
                          {"install_script": "pywin32_postinstall.py",
                           },
                      },

             scripts=["scripts/pywin32_postinstall.py"],

             ext_modules=ext_modules,

             package_dir=package_dir,
             packages=packages,
             py_modules=py_modules,
             setup_requires=['setuptools>=24.0'],

             data_files=[('', (os.path.join(gettempdir(), 'pywin32.version.txt'),))] +
             convert_optional_data_files([
                 'PyWin32.chm',
             ]) +
             convert_data_files([
                 'pythonwin/pywin/*.cfg',
                 'pythonwin/pywin/Demos/*.py',
                 'pythonwin/pywin/Demos/app/*.py',
                 'pythonwin/pywin/Demos/ocx/*.py',
                 'pythonwin/license.txt',
                 'win32/license.txt',
                 'win32/scripts/*.py',
                 'win32/test/*.py',
                 'win32/test/win32rcparser/test.rc',
                 'win32/test/win32rcparser/test.h',
                 'win32/test/win32rcparser/python.ico',
                 'win32/test/win32rcparser/python.bmp',
                 'win32/Demos/*.py',
                 'win32/Demos/images/*.bmp',
                 'com/win32com/readme.htm',
                 # win32com test utility files.
                 'com/win32com/test/*.idl',
                 'com/win32com/test/*.js',
                 'com/win32com/test/*.sct',
                 'com/win32com/test/*.txt',
                 'com/win32com/test/*.vbs',
                 'com/win32com/test/*.xsl',
                 'com/win32com/test/*.py',
                 # win32com docs
                 'com/win32com/HTML/*.html',
                 'com/win32com/HTML/image/*.gif',
                 'com/win32comext/adsi/demos/*.py',
                 # Active Scripting test and demos.
                 'com/win32comext/axscript/test/*.html',
                 'com/win32comext/axscript/test/*.py',
                 'com/win32comext/axscript/test/*.pys',
                 'com/win32comext/axscript/test/*.vbs',
                 'com/win32comext/axscript/Demos/*.pys',
                 'com/win32comext/axscript/Demos/*.htm*',
                 'com/win32comext/axscript/Demos/*.gif',
                 'com/win32comext/axscript/Demos/*.asp',
                 'com/win32comext/mapi/demos/*.py',
                 'com/win32comext/propsys/test/*.py',
                 'com/win32comext/shell/test/*.py',
                 'com/win32comext/shell/demos/servers/*.py',
                 'com/win32comext/shell/demos/*.py',
                 'com/win32comext/taskscheduler/test/*.py',
                 'com/win32comext/ifilter/demo/*.py',
                 'com/win32comext/authorization/demos/*.py',
                 'com/win32comext/bits/test/*.py',
                 'isapi/*.txt',
                 'isapi/samples/*.py',
                 'isapi/samples/*.txt',
                 'isapi/doc/*.html',
                 'isapi/test/*.py',
                 'isapi/test/*.txt',
             ]) +
             # The headers and .lib files
             [
                 ('win32/include', ('win32/src/PyWinTypes.h',)),
                 ('win32com/include', ('com/win32com/src/include/PythonCOM.h',
                                       'com/win32com/src/include/PythonCOMRegister.h',
                                       'com/win32com/src/include/PythonCOMServer.h'))
             ] +
             # And data files convert_data_files can't handle.
             [
                 ('win32com', ('com/License.txt',)),
                 # pythoncom.py doesn't quite fit anywhere else.
                 # Note we don't get an auto .pyc - but who cares?
                 ('', ('com/pythoncom.py',)),
                 ('', ('pywin32.pth',)),
             ],
             )
