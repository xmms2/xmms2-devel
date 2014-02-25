from waflib import Task, Logs, Errors, Options, Utils
from waflib.TaskGen import feature, before_method
import os
import re

class create_test_runner(Task.Task):
    def generate_runner_source_code(self):
        TEST_CASE_PREFIX = "__testcase_wrapper_"

        declare = []
        register = []
        for i, (suite, tests) in enumerate(self.suites):
            register.append("""CU_pSuite suite%d = CU_add_suite ("%s", NULL, NULL);""" % (i, suite))
            for test in sorted(tests):
                register.append("""CU_add_test (suite%d, "%s", %s);""" % (i, test, TEST_CASE_PREFIX + test))
                declare.append("""void %s (void);""" % (TEST_CASE_PREFIX + test))

        return "\n\t".join(register), "\n".join(declare)

    def run(self):
        register, declare = self.generate_runner_source_code()
        code = self.inputs[0].read()
        code = code.replace("@@DECLARE_TEST_CASES@@", declare)
        code = code.replace("@@REGISTER_TEST_SUITES@@", register)
        self.outputs[0].write(code)

scraper = re.compile("^(CASE|SETUP)\s*\(([^)]+)\)")

def scrape_test_cases(node):
    suite = ""
    tests = []
    for line in node.read().split("\n"):
        match = scraper.match(line)
        if not match:
            continue
        typ, name = match.groups()
        if typ == "SETUP":
            suite = name
        elif typ == "CASE":
            tests.append(name)
    if suite and tests:
        return suite, tests
    return False

@feature("test")
@before_method("process_source")
def generate_runner(self):
    self.source = self.to_nodes(self.source)
    try:
        self.add_objects = self.to_list(self.add_objects)
    except (AttributeError):
        self.add_objects = []

    suites = []
    for node in self.source:
        result = scrape_test_cases(node)
        if result:
            suites.append(result)

    if suites:
        # Generate and add runner
        test_runner_template = os.path.join("tests", "runner", "main.c")
        runner = self.bld.srcnode.find_resource(test_runner_template)
        target = self.path.find_or_declare("test_runner_%s.c" % self.target)

        task = self.create_task("create_test_runner", [runner] + self.source, target)
        task.suites = suites

        self.source += [target]

        # link valgrind_object in
        self.add_objects += ["memorystatus"]

def monkey_patch_test_runner():
    original = Task.classes["utest"].run

    def xmms_test_runner(self):
        if self.env.enable_gcov:
            # If profiling is enabled, we don't want to invoke valgrind.
            return original(self)

        if getattr(self.generator, 'use_valgrind', self.env.VALGRIND and True):
            # Disable GLib memory optimizations to avoid Valgrind confusion.
            os.environ["G_SLICE"] = "always-malloc"
            os.environ["G_DEBUG"] = "gc-friendly"

            suppression = os.path.join(os.getcwd(), "utils", "valgrind-suppressions")
            self.generator.ut_exec = [
                "valgrind",
                "--log-file=%s.log" % self.inputs[0].abspath(),
                "--leak-check=full",
                "--suppressions=%s" % suppression,
                "--num-callers=20",
                "--malloc-fill=0xa1",
                "--free-fill=0xa1",
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
            Logs.pprint('NORMAL', out.decode("ascii", "ignore"))

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
        if not bld.env.enable_gcov:
            raise Errors.WafError("Coverage reports need --enable-gcov passed to configure.")

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
