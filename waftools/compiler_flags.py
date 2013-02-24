class compiler_flags(object):
    def __init__(self, conf):
        self.conf = conf
        self.extra = { "CFLAGS": [], "CXXFLAGS": [] }
        if self.conf.check_cc(cflags="-Werror=unknown-warning-option", mandatory=False):
            self.extra["CFLAGS"] = ["-Werror=unknown-warning-option"]
        if self.conf.check_cxx(cxxflags="-Werror=unknown-warning-option", mandatory=False):
            self.extra["CXXFLAGS"] = ["-Werror=unknown-warning-option"]

        langs = (
            (self.conf.check_cc, "CFLAGS", "c"),
            (self.conf.check_cxx, "CXXFLAGS", "cxx")
        )

        kinds = (
            ("feature", "-f", "-fno-"),
            ("warning", "-W", "-Wno-"),
            ("error", "-Werror=", "-Wno-error=")
        )

        # Generate functions like flags.enable_cxx_warning(flag_name)
        for conf_func, flag, lang_name in langs:
            for kind, enable_prefix, disable_prefix in kinds:
                for name, func in self._generate_funcs(conf_func, flag, enable_prefix, disable_prefix):
                    self.__dict__["%s_%s_%s" % (name, lang_name, kind)] = func

        # Generate functions like flags.enable_warning(flag_name) which calls both c and cxx.
        for action in ("check", "enable", "disable"):
            for kind, _, _ in kinds:
                self.__dict__["%s_%s" % (action, kind)] = self._generate_multi_func(action, kind, [lang for _, _, lang in langs])

    def _generate_funcs(self, func, flag, enable_prefix, disable_prefix):
        def check(flag_name):
            return self._check(func, flag, enable_prefix, disable_prefix, flag_name)

        def enable(flag_name):
            enable, disable = check(flag_name)
            if enable:
                self.conf.env.append_unique(flag, [enable])

        def disable(flag_name):
            enable, disable = check(flag_name)
            if disable:
                self.conf.env.append_unique(flag, [disable])

        return (("check", check), ("enable", enable), ("disable", disable))

    def _generate_multi_func(self, action, kind, langs):
        def multi(flag_name):
            for lang in langs:
                self.__dict__["%s_%s_%s" % (action, lang, kind)](flag_name)
        return multi

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
