from waflib import Task, Logs, Errors
import os

def monkey_patch_test_runner():
    original = Task.classes["utest"].run

    def xmms_test_runner(self):
        if getattr(self.generator, 'use_valgrind', self.env.VALGRIND and True):
            suppression = os.path.join(os.getcwd(), "utils", "valgrind-suppressions")
            self.ut_exec = [
                "valgrind",
                "--log-file=%s.log" % self.inputs[0].abspath(),
                "--leak-check=full",
                "--suppressions=%s" % suppression,
                self.inputs[0].abspath()
            ]
        original(self)

    Task.classes["utest"].run = xmms_test_runner

def summary(bld):
    lst = getattr(bld, 'utest_results', [])
    if lst:
        Logs.pprint('CYAN', 'test summary')

        failed_tests = []

        for (f, code, out, err) in lst:
            if code != 0:
                failed_tests.append(f)
            Logs.pprint('NORMAL', out)

        if len(failed_tests) > 0:
            raise Errors.WafError("Test(s) failed:\n%s" % "\n".join(failed_tests))

def configure(conf):
    conf.load("waf_unit_test")
    conf.find_program("valgrind", var="VALGRIND", mandatory=False)

def setup(bld):
    monkey_patch_test_runner()
    bld.add_post_fun(summary)

def options(opts):
    opts.load("waf_unit_test")
