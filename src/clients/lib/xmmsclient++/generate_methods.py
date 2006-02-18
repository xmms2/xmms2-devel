
import re

f = open ("src/include/xmmsclient/xmmsclient.h")

trams = ("x", "y", "z", "w")
exclude_fun = ["result_restart", "result_new"]

l = f.readline()
r = re.compile("xmmsc_result_t \*xmmsc_([a-z0-9_]+)\s*\((.*)\).*")
print "#ifndef XMMSCLIENTPP_METHODS_H"
print "#define XMMSCLIENTPP_METHODS_H"
while (l):
	
	m = r.match (l)
	if m:
		(name, args) = m.groups()

		if name in exclude_fun:
			l = f.readline()
			continue;

		a = []
		for i in args.split(",")[1:]:
			i = i.strip()
			if "*" in i:
				i = i[:i.rindex("*")+1]
			elif " " in i:
				i = i[:i.rindex(" ")]
			a.append("%s %s" % (i, trams[len(a)]))
			
		if len(a) > 0:
			args = ",".join(a)
			args2 = "m_xmmsc, %s" % ", ".join(trams[:len(a)])
		else:
			args = "void"
			args2 = "m_xmmsc"

		print "XMMSResult *%s (%s) { return new XMMSResult (xmmsc_%s (%s)); }" %(name, args, name, args2)

	l = f.readline()
	
print "#endif"
