# Copyright (C) 2003-2017 XMMS2 Team
#
# As sphinx doesn't detect constants in modules we have to
# generate parts of the documentation. To keep it all in the
# same place, all of the documentation sources are generated.

from __future__ import with_statement
import ast
import os
import sys
import subprocess
import re
from datetime import datetime

try:
    sys.path.remove(os.path.dirname(os.path.abspath(__file__)))
    import xmmsclient
    import xmmsclient.collections
    import xmmsclient.service
except:
    print("Set PYTHONPATH to a path containing xmmsclient.")
    raise SystemExit

def write_xmmsapi(filename):
    with open(filename, "w") as dst:
        dst.write("xmmsclient\n")
        dst.write("----------\n")
        dst.write(".. py:module:: xmmsclient\n")
        dst.write("\nFunctions\n")
        dst.write("^^^^^^^^^\n")
        dst.write(".. py:function:: %s\n" % xmmsclient.userconfdir_get.__doc__)
        dst.write("\nClasses\n")
        dst.write("^^^^^^^\n")
        dst.write(".. autoclass:: xmmsclient.XmmsSync\n")
        dst.write(".. autoclass:: xmmsclient.XmmsLoop\n")
        dst.write(".. py:class:: %s\n\n" % xmmsclient.xmmsapi.XmmsCore.__doc__.replace("Core", ""))
        entries = xmmsclient.Xmms.__dict__.items() + xmmsclient.xmmsapi.XmmsCore.__dict__.items()
        for name, func in sorted(entries):
            if type(func).__name__ in ('getset_descriptor'):
                dst.write("   .. py:attribute:: %s\n" % name)
        for name, func in sorted(entries):
            if name.startswith("_"):
                continue
            if type(func).__name__ in ('method_descriptor', 'cython_function_or_method'):
                docstring = func.__doc__.replace("XmmsApi.", "").replace("XmmsCore.", "")
                match = re.findall("-> (.+)", docstring, re.MULTILINE)
                if match:
                    docstring = re.sub(" -> (.+)", "", docstring, re.MULTILINE)
                    docstring += "\n\n\t\t:rtype: %s\n" % match[0]
                dst.write("   .. py:method:: %s\n" % docstring)
        dst.write(".. autoclass:: xmmsclient.XmmsValue\n")
        dst.write(".. autoclass:: xmmsclient.XmmsValueC2C\n")
        dst.write(".. autoclass:: xmmsclient.XmmsLoop\n")
        dst.write(".. autoclass:: xmmsclient.xmmsapi.XmmsResult\n")
        dst.write(".. autoclass:: xmmsclient.xmmsapi.XmmsVisChunk\n")
        dst.write("\nExceptions\n")
        dst.write("^^^^^^^^^^\n")
        dst.write(".. autoexception:: xmmsclient.xmmsvalue.XmmsError\n")
        dst.write(".. autoexception:: xmmsclient.xmmsapi.VisualizationError\n")
        dst.write(".. autoexception:: xmmsclient.xmmsapi.XmmsDisconnectException\n")

def write_service_clients(filename):
    with open(filename, "w") as dst:
        dst.write("xmmsclient.service\n")
        dst.write("------------------\n\n")
        dst.write("See `samples` on how to register and/or consume service clients.\n\n")
        dst.write("\nClasses\n")
        dst.write("^^^^^^^\n")
        dst.write(".. autoclass:: xmmsclient.service.XmmsServiceClient\n")
        dst.write(".. py:class:: xmmsclient.service.XmmsServiceNamespace\n\n")
        dst.write("   .. py:method:: register()\n\n")
        dst.write(".. py:class:: xmmsclient.service.%s\n" %
                  xmmsclient.service.method_arg.__init__.__doc__.replace(".__init__", "").replace("self, ", ""))
        dst.write(".. py:class:: xmmsclient.service.%s\n" %
                  xmmsclient.service.method_varg.__init__.__doc__.replace(".__init__", "").replace("self, ", ""))
        dst.write(".. py:class:: xmmsclient.service.%s\n" %
                  xmmsclient.service.service_constant.__init__.__doc__.replace(".__init__", "").replace("self, ", ""))
        dst.write(".. py:class:: xmmsclient.service.%s\n" %
                  xmmsclient.service.service_broadcast.__doc__)
        dst.write("\nDecorators\n")
        dst.write("^^^^^^^^^^\n")
        dst.write(".. py:decorator:: xmmsclient.service.%s\n" %
                  xmmsclient.service.service_method.__init__.im_func.func_doc.replace(".__init__", ""))


def write_constants(filename):
    with open(xmmsclient.consts.__file__) as src, open(filename, "w") as dst:
        class ConstsDocumenter(ast.NodeVisitor):
            def visit_ImportFrom(self, node):
                for name in node.names:
                    value = eval("xmmsclient.%s" % name.name)
                    dst.write(".. py:data:: xmmsclient.%s\n" % name.name)
                    dst.write("   :annotation: = %s\n\n" % value)
        dst.write("Constants\n")
        dst.write("---------\n")
        tree = ast.parse(src.read())
        ConstsDocumenter().visit(tree)

def write_collections(filename):
    with open(xmmsclient.collections.__file__) as src, open(filename, "w") as dst:
        class CollectionsDocumenter(ast.NodeVisitor):
            def visit_ImportFrom(self, node):
                for name in node.names:
                    if name.name == "coll_parse":
                        dst.write("\nMethods\n")
                        dst.write("^^^^^^^\n")
                        dst.write(".. autofunction:: xmmsclient.collections.coll_parse\n")
                    else:
                        dst.write(".. autoclass:: xmmsclient.collections.%s\n" % name.name)
                        dst.write("   :inherited-members:\n\n")
                        dst.write("   .. automethod:: xmmsclient.collections.%s.__init__\n" % name.name)
        dst.write("xmmsclient.collections\n")
        dst.write("----------------------\n")
        dst.write("\nClasses\n")
        dst.write("^^^^^^^\n")
        tree = ast.parse(src.read())
        CollectionsDocumenter().visit(tree)

def write_samples(filename, samples_dir):
    with open(filename, "w") as dst:
        dst.write("\n")
        dst.write("Examples\n")
        dst.write("--------\n")
        dst.write("\nBrowse and track available service clients\n")
        dst.write("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n")
        dst.write(".. literalinclude:: %s\n" % os.path.join(samples_dir, "scwatch.py"))
        dst.write("   :language: python\n")
        dst.write("   :tab-width: 2\n\n")
        dst.write("\nRegister a service client\n")
        dst.write("^^^^^^^^^^^^^^^^^^^^^^^^^\n")
        dst.write(".. literalinclude:: %s\n" % os.path.join(samples_dir, "service.py"))
        dst.write("   :language: python\n")
        dst.write("   :tab-width: 2\n")

def write_index(filename):
    with open(filename, "w") as dst:
        dst.write("Welcome to XMMS2's documentation!\n")
        dst.write("=================================\n")

        dst.write(".. toctree::\n")
        dst.write("   :maxdepth: 1\n\n")
        dst.write("   xmmsapi\n")
        dst.write("   collections\n")
        dst.write("   serviceclients\n")
        dst.write("   constants\n")
        dst.write("   samples\n")

def write_conf(filename, year, version):
    with open(filename, "w") as dst:
        dst.write("""
project = 'XMMS2'
copyright = '2016, XMMS2 Team'
version = '0.8+WiP'
master_doc = 'index'
html_theme = 'sphinx_py3doc_enhanced_theme'

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.autosummary'
]

autodoc_default_flags = ['members', 'undoc-members']
default_role = 'any'
""" % {
    "year": year,
    "version": version
})

if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit("ERROR: Need an output directory argument.")

    basedir = sys.argv[1]
    script_dir = os.path.dirname(os.path.abspath(__file__))
    samples_dir = os.path.relpath(os.path.join(script_dir, "xmmsclient", "samples"), basedir)

    version = sys.argv[2]

    try:
        os.makedirs(basedir)
    except:
        pass
    write_xmmsapi(os.path.join(basedir, "xmmsapi.rst"))
    write_constants(os.path.join(basedir, "constants.rst"))
    write_collections(os.path.join(basedir, "collections.rst"))
    write_service_clients(os.path.join(basedir, "serviceclients.rst"))
    write_samples(os.path.join(basedir, "samples.rst"), samples_dir)
    write_index(os.path.join(basedir, "index.rst"))
    write_conf(os.path.join(basedir, "conf.py"), datetime.now().year, version)
