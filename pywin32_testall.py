"""A test runner for pywin32"""
import distutils.sysconfig
import os
import signal
import subprocess
import sys
import threading
import time
from contextlib import suppress

# locate the dirs based on where this script is - it may be either in the
# source tree, or in an installed Python 'Scripts' tree.
this_dir = os.path.dirname(__file__)
site_packages = distutils.sysconfig.get_python_lib(plat_specific=1)


def print_output(process):
    while process.poll() is None:
        out = process.stdout.read(1)
        sys.stdout.write(out)
        sys.stdout.flush()


def run_test(script, cmdline_rest=""):
    dirname, scriptname = os.path.split(script)
    # some tests prefer to be run from their directory.
    current_dir = os.getcwd()
    os.chdir(os.path.join(this_dir, dirname))
    print("Changed to: ", os.getcwd())
    cmd = [sys.executable, "-u", scriptname] + cmdline_rest.split()
    print("Running: ", ' '.join(cmd))
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        bufsize=1)

    thread = threading.Thread(target=print_output, args=(process,))
    thread.start()
    thread.join(timeout=60 * 3)

    if process.poll() is None:
        for i in range(5):
            os.kill(process.pid, signal.SIGINT)

    time.sleep(10)  # Give ten seconds before we kill everything
    with suppress(Exception):
        process.kill()

    os.chdir(current_dir)


def find_and_run(possible_locations, script, cmdline_rest=""):
    for maybe in possible_locations:
        if os.path.isfile(os.path.join(maybe, script)):
            run_test(
                os.path.abspath(
                    os.path.join(
                        maybe,
                        script)),
                cmdline_rest)
            break
    else:
        raise RuntimeError("Failed to locate the test script '%s' in one of %s"
                           % (script, possible_locations))

if __name__ == '__main__':
    # win32
    maybes = [os.path.join(this_dir, "win32", "test"),
              os.path.join(site_packages, "win32", "test"),
              ]
    find_and_run(maybes, 'testall.py')

    # win32com
    maybes = [os.path.join(this_dir, "com", "win32com", "test"),
              os.path.join(site_packages, "win32com", "test"),
              ]
    find_and_run(maybes, 'testall.py', "2")

    # adodbapi
    maybes = [os.path.join(this_dir, "adodbapi", "tests"),
              os.path.join(site_packages, "adodbapi", "tests"),
              ]
    # find_and_run(maybes, 'adodbapitest.py')
    # This script has a hard-coded sql server name in it, (and markh typically
    # doesn't have a different server to test on) so don't bother trying to
    # run it...
    # find_and_run(maybes, 'test_adodbapi_dbapi20.py')

    if sys.version_info > (3,):
        print("** The tests have some issues on py3k - not all failures are a problem...")
