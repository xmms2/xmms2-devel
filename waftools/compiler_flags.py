class compiler_flags(object):
    def __init__(self, conf):
        self.conf = conf
        self.extra = { "CFLAGS": [], "CXXFLAGS": [] }
        if self.conf.check_cc(cflags="-Werror=unknown-warning-option", mandatory=False):
            self.extra["CFLAGS"] = ["-Werror=unknown-warning-option"]
        if self.conf.check_cxx(cxxflags="-Werror=unknown-warning-option", mandatory=False):
            self.extra["CXXFLAGS"] = ["-Werror=unknown-warning-option"]

    def _check(self, func, key, enable_prefix, disable_prefix, name):
        enable_flag = enable_prefix + name
        disable_flag = disable_prefix + name

        # Fork the environment so we can add some temporary compiler
        # flags that might be needed during the detection.
        self.conf.env.stash()
        self.conf.env[key] += self.extra[key]

        kw = { 'mandatory': False, key.lower(): enable_flag }
        result = func(**kw)

        self.conf.env.revert()

        if not result:
            return False, False

        uselib = name.replace("-", "").replace("=", "").upper()
        self.conf.env.append_unique("%s_ENABLE_%s" % (key, uselib), [enable_flag])
        self.conf.env.append_unique("%s_DISABLE_%s" % (key, uselib), [disable_flag])

        return enable_flag, disable_flag

    def check_cc(self, *args):
        return self._check(self.conf.check_cc, "CFLAGS", *args)

    def check_cxx(self, *args):
        return self._check(self.conf.check_cxx, "CXXFLAGS", *args)

    def enable_c_error(self, error_name):
        enable, disable = self.check_cc("-Werror=", "-Wno-error=", error_name)
        if enable:
            self.conf.env.append_unique("CFLAGS", [enable])

    def disable_c_error(self, error_name):
        enable, disable = self.check_cc("-Werror=", "-Wno-error=", error_name)
        if disable:
            self.conf.env.append_unique("CFLAGS", [disable])

    def enable_c_warning(self, warn_name):
        enable, disable = self.check_cc("-W", "-Wno-", warn_name)
        if enable:
            self.conf.env.append_unique("CFLAGS", [enable])

    def disable_c_warning(self, warn_name):
        enable, disable = self.check_cc("-W", "-Wno-", warn_name)
        if disable:
            self.conf.env.append_unique("CFLAGS", [disable])

    def enable_c_feature(self, feature_name):
        enable, disable = self.check_cc("-f", "-fno-", feature_name)
        if enable:
            self.conf.env.append_unique("CFLAGS", [enable])

    def disable_c_feature(self, feature_name):
        enable, disable = self.check_cc("-f", "-fno-", feature_name)
        if disable:
            self.conf.env.append_unique("CFLAGS", [disable])

    def enable_cxx_feature(self, feature_name):
        enable, disable = self.check_cxx("-f", "-fno-", feature_name)
        if enable:
            self.conf.env.append_unique("CXXFLAGS", [enable])

    def disable_cxx_feature(self, feature_name):
        enable, disable = self.check_cxx("-f", "-fno-", feature_name)
        if disable:
            self.conf.env.append_unique("CXXFLAGS", [disable])

    def enable_feature(self, feature_name):
        self.enable_c_feature(feature_name)
        self.enable_cxx_feature(feature_name)
