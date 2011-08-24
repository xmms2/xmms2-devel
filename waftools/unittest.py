from waflib import Task, Logs, Errors, Options, Utils
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

def run(cmd, cwd):
    proc = Utils.subprocess.Popen(cmd, cwd=cwd,
                                  stderr=Utils.subprocess.PIPE,
                                  stdout=Utils.subprocess.PIPE)
    (stdout, stderr) = proc.communicate()
    return (stdout, stderr, proc.returncode)

def generate_coverage(bld):
    if Options.options.generate_coverage:
        if not bld.env.with_profiling:
            raise Errors.WafError("Coverage reports need --with-profiling passed to configure.")

        if not (bld.env.LCOV and bld.env.GENHTML):
            raise Errors.WafError("Could not generate coverage as the tools are missing.")

        cmd = [bld.env.LCOV, "-c", "-b", ".", "-d", ".", "-o", "coverage.info"]
        (stdout, stderr, code) = run(cmd, bld.bldnode.abspath())

        if code != 0:
            err = "stdout: %s\nstderr: %s" % (stdout, stderr)
            raise Errors.WafError("Could not run coverage analysis tool!\n%s" % err)

        cmd = [bld.env.GENHTML, "-o", "coverage", "coverage.info"]
        (stdout, stderr, code) = run(cmd, bld.bldnode.abspath())

        if code != 0:
            err = "stdout: %s\nstderr: %s" % (stdout, stderr)
            raise Errors.WafError("Could not run coverage report tool!\n%s" % err)

def configure(conf):
    conf.load("waf_unit_test")
    conf.find_program("valgrind", var="VALGRIND", mandatory=False)
    conf.find_program("lcov", var="LCOV", mandatory=False)
    conf.find_program("genhtml", var="GENHTML", mandatory=False)

def setup(bld):
    monkey_patch_test_runner()
    bld.add_post_fun(generate_coverage)
    bld.add_post_fun(summary)

def options(opts):
    opts.load("waf_unit_test")
    opts.add_option('--generate-coverage', action='store_true',
                    dest='generate_coverage', default=False,
                    help="Generate coverage report.")
