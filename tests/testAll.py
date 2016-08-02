"""Integration test runner for hidl-cpp.

Runs hidl-cpp on a bunch of files and tests the result.
Exit code is 0 iff all tests pass.

Usage (intended to be called from Android.mk):

testAll.py path_to_hidl_cpp output_dir test_dir

The output files will all go in output_dir
The input files come from test_dir
Some output files are compared with files of the same
 name in test_dir/canonical

All tests are called from Tester::RunTests(). Available
 checks include the value of the return code,
 any text printed, and whether an output and canonical file
 are equal.
"""

import subprocess
import sys


class Tester(object):
  """The entirety of the program."""

  def __init__(self, hidl_cpp_path, test_dir, output_dir):
    """Stashes the paths the program will need."""
    self.hidl_cpp_path = hidl_cpp_path
    self.output_dir = output_dir + "/"
    self.test_dir = test_dir + "/"
    self.errors = 0  # Number of failed tests
    # Many of these are for convenience in writing small
    # function calls in the RunTests() function.
    self.last_stdout = ""  # Return string from last command
    self.last_stderr = ""  # Return string from last command
    self.last_return = 0  # Exit code from last command
    self.last_out_path = ""  # Last output file generated
    self.last_out_text = ""  # Last output file's text
    self.last_hidl_name = ""  # Last .hal file processed
    self.last_out_type = ""  # last file-type parameter

  def RunTests(self):
    """Runs all the tests."""
    self.ProcessFile("IBadType.hal", "foo", "bn_h")
    self.Test(self.last_return != 0)
    self.Test(Has(self.last_stdout, "int48_t"))
    self.ProcessFile("ITest.hal", "foo", "bn_h")
    self.Test(self.last_return == 0)
    self.TestOutput("vts_gps.hal", "vts_gps.vts", "vts")
     # , True)  # echoes stdout from hidl-gen, for debugging
    self.TestOutput("vts_lights.hal", "vts_lights.vts", "vts")
    self.TestOutput("vts_lights_hal.hal", "vts_lights_hal.vts", "vts")
    self.TestOutput("ITestService.hal", "ITestService.h", "i_h")
    self.TestOutput("ITestService.hal", "BnTestService.h", "bn_h")
    self.TestOutput("ITestService.hal", "BpTestService.h", "bp_h")
    self.TestOutput("ITestService.hal", "TestServiceProxy.cpp", "proxy_cpp")
    self.TestOutput("ITestService.hal", "TestServiceStubs.cpp", "stubs_cpp")
    self.TestOutput("IEverything.hal", "Everything.json", "json")
    self.TestOutput("IExpression.hal", "Expression_all.cpp", "all_cpp")
    self.TestOutput("IExpression.hal", "Expression_i.h", "i_h")
    self.TestOutput("INfc.hal", "NfcAll.cpp", "all_cpp")
    self.TestOutput("INfc.hal", "INfc.h", "i_h")
    self.TestOutput("INfc.hal", "BnNfc.h", "bn_h")
    self.TestOutput("INfc.hal", "BpNfc.h", "bp_h")
    self.TestOutput("INfc.hal", "vts_nfc.vts", "vts")
    self.TestOutput("INfcClientCallback.hal", "vts_nfc_client_callback.vts", "vts")
    self.TestOutput("ITypes.hal", "TypesAll.cpp", "all_cpp")
    self.TestOutput("ITypes.hal", "ITypes.h", "i_h")
    self.TestOutput("ITypes.hal", "BnTypes.h", "bn_h")
    self.TestOutput("ITypes.hal", "BpTypes.h", "bp_h")

  def RunCommand(self, command):
    """Runs a unix command and stashes the result."""
#    print command
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    self.last_stdout, self.last_stderr = proc.communicate()
    self.last_return = proc.returncode
#    print "Ret from RunC is %d" % self.last_return

  def TestOutput(self, hidl_name, out_name, out_type, print_out = None):
    self.ProcessFile(hidl_name, out_name, out_type, print_out)
    if self.last_return != 0:
      self.Error("hidl-gen error on file %s (type %s): %s" % (
          hidl_name, out_type, self.last_stdout + self.last_stderr))
      return
    canonical_path = self.test_dir + "canonical/" + out_name
    with open(canonical_path) as canonical:
      good_text = canonical.read()
    if good_text != self.last_out_text:
      self.Error(("Output file %s (%s) is wrong for hidl file %s\n"
                  "meld %s %s") % (out_name, out_type, hidl_name,
                                     canonical_path,
                                     self.last_out_path))

  def Error(self, string):
    """Prints an error message and increments error count."""
    print string
    self.errors += 1

  def ProcessFile(self, hidl_name, out_name, out_type, print_out=None):
    self.last_hidl_name = hidl_name
    self.last_out_type = out_type
    hidl_path = self.test_dir + hidl_name
    self.last_out_path = self.output_dir + out_name
    self.RunCommand([self.hidl_cpp_path, out_type,
                     hidl_path, self.last_out_path])
    if print_out:
      print self.last_stdout
    if self.last_return == 0:
      with open(self.last_out_path) as out_file:
        self.last_out_text = out_file.read()
    else:
      self.last_out_text = " * * INVALID * * "

  def Test(self, bool_value, text=""):
    if not bool_value:
      self.Error(("File %s (type %s) error: %s\n" % (
          self.last_hidl_name, self.last_out_type, text)) +
                 ("  (stdout: %s\n  stderr: %s\n)" % (
                     self.last_stdout, self.last_stderr)))


def Has(string, substr):
  """Syntactic sugar: true iff str contains substr."""
  return string.find(substr) != -1


if __name__ == "__main__":
  if len(sys.argv) != 4:
    print "Usage: python testAll.py hidl-cpp-path test-dir out-dir"
    sys.exit(1)

  tester = Tester(sys.argv[1], sys.argv[2], sys.argv[3])
  tester.RunTests()
  if tester.errors:
    print "hidl_cpp test: %d errors" % tester.errors
  sys.exit(tester.errors)
