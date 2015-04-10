from waflib.TaskGen import before_method,feature
from waflib import Utils

"""
Propagate 'uselib' from local 'object' targets to avoid
having to specify the 'uselib' dependencies at place
that depends on some 'object'.
"""

@feature('cprogram')
@before_method('process_use')
def propagate_uselibs(self):
    lst = []
    def add_use(tg):
        for x in Utils.to_list(getattr(tg, 'uselib', '')):
            if x not in lst:
                lst.append(x)
        for x in Utils.to_list(getattr(tg, 'use', '')):
            add_use(self.bld.get_tgen_by_name(x))

    add_use(self)
    self.uselib = lst
