class compiler_flags(object):
    def __init__(self, conf):
        self.conf = conf

    def _check(self, func, key, enable_prefix, disable_prefix, name):
        enable_flag = enable_prefix + name
        disable_flag = disable_prefix + name
        kw = { 'mandatory': False, key.lower(): enable_flag }
        if not func(**kw):
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
