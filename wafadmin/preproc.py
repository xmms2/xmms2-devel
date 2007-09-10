#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"""Waf preprocessor for finding dependencies because of the includes system, it is necessary to do the preprocessing in at least two steps:
  - filter the comments and output the preprocessing lines
  - interpret the preprocessing lines, jumping on the headers during the process
"""

import re, sys, os, string
import Params
from Params import debug, error, warning

reg_define = re.compile('^\s*(#|%:)')
reg_nl = re.compile('\\\\\r*\n', re.MULTILINE)
reg_cpp = re.compile(r"""/\*[^*]*\*+([^/*][^*]*\*+)*/|//[^\n]*|("(\\.|[^"\\])*"|'(\\.|[^'\\])*'|.[^/"'\\]*)""", re.MULTILINE)
def repl(m):
	s = m.group(2)
	if s is None: return ''
	return s

def filter_comments(filename):
	f = open(filename, "r")
	code = f.read()
	f.close()

	code = reg_nl.sub('', code)
	code = reg_cpp.sub(repl, code)
	code = code.split('\n')

	return [reg_define.sub("", line).replace('\r', '') for line in code if reg_define.search(line)]

strict_quotes = 0
"Keep <> for system includes (do not search for those includes)"

alpha = string.letters + '_' + string.digits

accepted  = 'a'
ignored   = 'i'
undefined = 'u'
skipped   = 's'

num = 'number'
op = 'operator'
ident = 'ident'
stri = 'string'
chr = 'char'

trigs = {
'=' : '#',
'-' : '~',
'/' : '\\',
'!' : '|',
'\'': '^',
'(' : '[',
')' : ']',
'<' : '{',
'>' : '}',
}

punctuators_table = [
{'!': 43, '#': 45, '%': 22, '&': 30, ')': 50, '(': 49, '+': 11, '*': 18, '-': 14,
 ',': 56, '/': 20, '.': 38, ';': 55, ':': 41, '=': 28, '<': 1, '?': 54, '>': 7,
 '[': 47, ']': 48, '^': 36, '{': 51, '}': 52, '|': 33, '~': 53},
{'=': 6, ':': 5, '%': 4, '<': 2, '$$': '<'},
{'$$': '<<', '=': 3},
{'$$': '<<='},
{'$$': '<%'},
{'$$': '<:'},
{'$$': '<='},
{'$$': '>', '=': 10, '>': 8},
{'$$': '>>', '=': 9},
{'$$': '>>='},
{'$$': '>='},
{'$$': '+', '+': 12, '=': 13},
{'$$': '++'},
{'$$': '+='},
{'=': 17, '-': 15, '$$': '-', '>': 16},
{'$$': '--'},
{'$$': '->'},
{'$$': '-='},
{'$$': '*', '=': 19},
{'$$': '*='},
{'$$': '/', '=': 21},
{'$$': '/='},
{'$$': '%', ':': 23, '=': 26, '>': 27},
{'$$': '%:', '%': 24},
{':': 25},
{'$$': '%:%:'},
{'$$': '%='},
{'$$': '%>'},
{'$$': '=', '=': 29},
{'$$': '=='},
{'$$': '&', '=': 32, '&': 31},
{'$$': '&&'},
{'$$': '&='},
{'$$': '|', '=': 35, '|': 34},
{'$$': '||'},
{'$$': '|='},
{'$$': '^', '=': 37},
{'$$': '^='},
{'$$': '.', '.': 39},
{'.': 40},
{'$$': '...'},
{'$$': ':', '>': 42},
{'$$': ':>'},
{'$$': '!', '=': 44},
{'$$': '!='},
{'#': 46, '$$': '#'},
{'$$': '##'},
{'$$': '['},
{'$$': ']'},
{'$$': '('},
{'$$': ')'},
{'$$': '{'},
{'$$': '}'},
{'$$': '~'},
{'$$': '?'},
{'$$': ';'},
{'$$': ','}
]

preproc_table = [
{'e': 21, 'd': 31, 'i': 1, 'p': 42, 'u': 37, 'w': 51},
{'f': 13, 'm': 8, 'n': 2},
{'c': 3},
{'l': 4},
{'u': 5},
{'d': 6},
{'e': 7},
{'$$': 'include'},
{'p': 9},
{'o': 10},
{'r': 11},
{'t': 12},
{'$$': 'import'},
{'$$': 'if', 'd': 14, 'n': 17},
{'e': 15},
{'f': 16},
{'$$': 'ifdef'},
{'d': 18},
{'e': 19},
{'f': 20},
{'$$': 'ifndef'},
{'r': 58, 'l': 22, 'n': 27},
{'i': 25, 's': 23},
{'e': 24},
{'$$': 'else'},
{'f': 26},
{'$$': 'elif'},
{'d': 28},
{'i': 29},
{'f': 30},
{'$$': 'endif'},
{'e': 32},
{'b': 48, 'f': 33},
{'i': 34},
{'n': 35},
{'e': 36},
{'$$': 'define'},
{'n': 38},
{'d': 39},
{'e': 40},
{'f': 41},
{'$$': 'undef'},
{'r': 43},
{'a': 44},
{'g': 45},
{'m': 46},
{'a': 47},
{'$$': 'pragma'},
{'u': 49},
{'g': 50},
{'$$': 'debug'},
{'a': 52},
{'r': 53},
{'n': 54},
{'i': 55},
{'n': 56},
{'g': 57},
{'$$': 'warning'},
{'r': 59},
{'o': 60},
{'r': 61},
{'$$': 'error'}
]

def parse_token(stuff, table):
	c = stuff.next()
	stuff.back(1)
	if not (c in table[0].keys()):
		#print "error, character is not in table", c
		return 0
	pos = 0
	while stuff.good():
		c = stuff.next()
		if c in table[pos].keys():
			pos = table[pos][c]
		else:
			stuff.back(1)
			try: return table[pos]['$$']
			except KeyError: return 0
			# lexer error
	return table[pos]['$$']

def get_punctuator_token(stuff):
	return parse_token(stuff, punctuators_table)

def get_preprocessor_token(stuff):
	return parse_token(stuff, preproc_table)

def subst(lst, defs):
	if not lst: return []

	a1_t = lst[0][0]
	a1 = lst[0][1]
	if len(lst) == 1:
		if a1_t == ident:
			if a1 in defs:
				return defs[a1]
		return lst

	# len(lst) > 1 : search for macros
	a2_type = lst[1][0]
	a2 = lst[1][1]
	if a1_t == ident:
		if a1 == 'defined':
			if a2_type == ident:
				if a2 in defs:
					return [[num, '1']] + subst(lst[2:], defs)
				else:
					return [[num, '0']] + subst(lst[2:], defs)
			if a2_type == op and a2 == '(':
				if len(lst) < 4:
					raise ValueError, "expected 4 tokens defined(ident)"
				if lst[2][0] != ident:
					raise ValueError, "expected defined(ident)"
				if lst[2][1] in defs:
					return [[num, '1']] + subst(lst[4:], defs)
				else:
					return [[num, '0']] + subst(lst[4:], defs)
		if a1 in defs:
			#print a2
			if a2_type == op and a2 == '(':
				# beginning of a macro function - ignore for now
				args = []
				i = 2
				while 1:
					if lst[i][1] == ')':
						return subst(lst[i+1:], defs)
					args += lst[i]
				# TODO
				#print 'macro subst'
			else:
				# not a '(', try to substitute now
				if a1 in defs:
					return defs[a1] + subst(lst[1:], defs)
				else:
					return [lst[0]] + subst(lst[1:], defs)
	return [lst[0]] + subst(lst[1:], defs)

def comp(lst):
	if not lst: return [stri, '']

	if len(lst) == 1:
		return lst[0]

	#print "lst len is ", len(lst)
	#print "lst is ", str(lst)

	a1_type = lst[0][0]
	a1 = lst[0][1]

	a2_type = lst[1][0]
	a2 = lst[1][1]

	if a1_type == ident:
		if a2 == '#':
			return comp( [[stri, a1]] + lst[2:] )
	if a1 == '#':
		if a2_type == ident:
			return comp( [[stri, a2]] + lst[2:] )
	if a1_type == op:
		if a2_type == num:
			if a1 == '-':
				return [num, - int(a2)]
			if a1 == '!':
				if int(a2) == 0:
					return [num, 1]
				else:
					return [num, 0]
			raise ValueError, "cannot compute %s (1)" % str(lst)
		raise ValueError, "cannot compute %s (2)" % str(lst)
	if a1_type == stri:
		if a2_type == stri:
			if lst[2:]:
				return comp( [[stri, a1+a2], comp(lst[2:])] )
			else:
				return [[stri, a1+a2]]

	## we need to scan the third argument given
	try:
		a3_type = lst[2][0]
		a3 = lst[2][1]
	except:
		raise ValueError, "cannot compute %s (3)" % str(lst)

	if a1_type == ident:
		#print "a1"
		if a2 == '#':
			#print "a2"
			if a3_type == stri:
				#print "hallo"
				return comp([[stri, a1 + a3]] + lst[3:])

	if a1_type == num:
		if a3_type == num:
			a1 = int(a1)
			a3 = int(a3)
			if a2_type == op:
				val = None
				if a2 == '+':    val = a1+a3
				elif a2 == '-':  val = a1-a3
				elif a2 == '/':  val = a1/a3
				elif a2 == '*':  val = a1 * a3
				elif a2 == '%':  val = a1 % a3

				if not val is None:
					return comp( [[num, val]] + lst[3:] )

				elif a2 == '|':  val = a1 | a3
				elif a2 == '&':  val = a1 & a3
				elif a2 == '||': val = a1 or a3
				elif a2 == '&&': val = a1 and a3

				if val: val = 1
				else: val = 0
				return comp( [[num, val]] + lst[3:] )

	raise ValueError, "could not parse the macro %s " % str(lst)


class cparse:
	def __init__(self, nodepaths=None, strpaths=None, defines=None):
		#self.lines = txt.split('\n')
		self.lines = []
		self.i     = 0
		self.txt   = ''
		self.max   = 0
		self.buf   = []

		if defines is None:
			self.defs  = {}
		else:
			self.defs  = dict(defines) # make a copy
		self.state = []

		self.env   = None # needed for the variant when searching for files

		# include paths
		if strpaths is None:
			self.strpaths = []
		else:
			self.strpaths = strpaths
		self.pathcontents = {}

		self.deps  = []
		self.deps_paths = []

		# waf uses
		if nodepaths is None:
			self.m_nodepaths = []
		else:
			self.m_nodepaths = nodepaths
		self.m_nodes = []
		self.m_names = []

		# dynamic cache
		try:
			self.parse_cache = Params.g_build.parse_cache
		except AttributeError:
			Params.g_build.parse_cache = {}
			self.parse_cache = Params.g_build.parse_cache

	def tryfind(self, filename):
		if self.m_nodepaths:
			found = 0
			for n in self.m_nodepaths:
				found = n.find_source(filename, create=0)
				if found:
					self.m_nodes.append(found)
					# screw Qt
					if filename[-4:] != '.moc': self.addlines(found.abspath(self.env))
					break
			if not found:
				if not filename in self.m_names:
					self.m_names.append(filename)
		else:
			found = 0
			for p in self.strpaths:
				if not p in self.pathcontents.keys():
					self.pathcontents[p] = os.listdir(p)
				if filename in self.pathcontents[p]:
					#print "file %s found in path %s" % (filename, p)
					np = os.path.join(p, filename)
					# screw Qt two times
					if filename[-4:] != '.moc': self.addlines(np)
					self.deps_paths.append(np)
					found = 1
			if not found:
				pass
				#error("could not find %s " % filename)

	def addlines(self, filepath):
		pc = self.parse_cache
		if filepath in pc.keys():
			self.lines = pc[filepath] + self.lines
			return

		try:
			lines = filter_comments(filepath)
			pc[filepath] = lines # memorize the lines filtered
			self.lines = lines + self.lines
		except IOError:
			raise
		except:
			if Params.g_verbose > 0: warning("parsing %s failed" % filepath)
			raise

	def start2(self, node, env):
		debug("scanning %s (in %s)" % (node.m_name, node.m_parent.m_name), 'preproc')

		self.env = env
		variant = node.variant(env)

		self.addlines(node.abspath(env))
		if env['DEFLINES']:
			self.lines = env['DEFLINES'] + self.lines

		while self.lines:
			line = self.lines.pop(0)
			if not line: continue
			self.txt = line
			self.i   = 0
			self.max = len(line)
			try:
				self.process_line()
			except Exception, ex:
				if Params.g_verbose:
					warning("line parsing failed (%s): %s" % (str(ex), line))

	# debug only
	def start(self, filename):
		self.addlines(filename)

		while self.lines:
			line = self.lines.pop(0)
			if not line: continue
			self.txt = line
			self.i   = 0
			self.max = len(line)
			try:
				self.process_line()
			except Exception, ex:
				if Params.g_verbose:
					warning("line parsing failed (%s): %s" % (str(ex), line))
				raise
	def back(self, c):
		self.i -= c

	def next(self):
		car = self.txt[self.i]
		self.i += 1
		return car

	def good(self):
		return self.i < self.max

	def skip_spaces(self):
		# skip the spaces
		while self.good():
			c = self.next()
			if c == ' ' or c == '\t': continue
			else:
				self.i -= 1
				break

	def isok(self):
		if not self.state: return 1
		for tok in self.state:
			if tok == skipped or tok == ignored: return None
		return 1

	def process_line(self):
		type = ''
		l = len(self.txt)
		token = get_preprocessor_token(self)
		if not token:
			debug("line %s has no preprocessor token" % self.txt, 'preproc')
			return

		if token == 'endif':
			self.state.pop(0)
		elif token[0] == 'i' and token != 'include':
			self.state = [undefined] + self.state

		#print "token before ok is ", token

		# skip lines when in a dead block
		# wait for the endif
		if not token in ['else', 'elif']:
			if not self.isok():
				return

		#print "token is ", token

		debug("line is %s state is %s" % (self.txt, self.state), 'preproc')

		if token == 'if':
			ret = self.comp(self.get_body())
			if ret: self.state[0] = accepted
			else: self.state[0] = ignored
		elif token == 'ifdef':
			ident = self.get_name()
			if ident in self.defs.keys(): self.state[0] = accepted
			else: self.state[0] = ignored
		elif token == 'ifndef':
			ident = self.get_name()
			if ident in self.defs.keys():
				self.state[0] = ignored
			else: self.state[0] = accepted
		elif token == 'include' or token == 'import':
			(type, body) = self.get_include()
			if self.isok():
				debug("include found %s    (%s) " % (body, type), 'preproc')
				if type == '"':
					if not body in self.deps:
						self.deps.append(body)
						self.tryfind(body)
				elif type == '<':
					if not strict_quotes:
						if not body in self.deps:
							self.deps.append(body)
							self.tryfind(body)
				else:
					res = self.comp(body)
					#print 'include body is ', res
					if res and (not res in self.deps):
						self.deps.append(res)
						self.tryfind(res)

		elif token == 'elif':
			if self.state[0] == accepted:
				self.state[0] = skipped
			elif self.state[0] == ignored:
				if self.comp(self.get_body()):
					self.state[0] = accepted
				else:
					# let another 'e' treat this case
					pass
				pass
			else:
				pass
		elif token == 'else':
			if self.state[0] == accepted: self.state[0] = skipped
			elif self.state[0] == ignored: self.state[0] = accepted
		elif token == 'endif':
			pass
		elif token == 'define':
			name = self.get_name()
			args = self.get_args()
			body = self.get_body()
			#print "define %s (%s) { %s }" % (name, str(args), str(body))
			if not args:
				self.defs[name] = body
			else:
				# TODO handle macros
				pass
		elif token == 'undef':
			name = self.get_name()
			if name:
				if name in self.defs.keys():
					self.defs.__delitem__(name)
				#print "undef %s" % name

	def get_include(self):
		self.skip_spaces()
		delimiter = self.next()
		if delimiter == '"':
			buf = []
			while self.good():
				c = self.next()
				if c == delimiter: break
				buf.append(c)
			return (delimiter, "".join(buf))
		elif delimiter == "<":
			buf = []
			while self.good():
				c = self.next()
				if c == '>': break
				buf.append(c)
			return (delimiter, "".join(buf))
		else:
			self.i -= 1
			return ('', self.get_body())

	def get_name(self):
		ret = []
		self.skip_spaces()
		# get the first word found
		while self.good():
			c = self.next()
			if c != ' ' and c != '\t' and c != '(': ret.append(c)
			else:
				self.i -= 1
				break
		return "".join(ret)

	def get_args(self):
		ret = []
		self.skip_spaces()
		if not self.good(): return None

		c = self.next()
		if c != '(':
			self.i -= 1
			return None
		buf = []
		while self.good():
			c = self.next()
			if c == ' ' or c == '\t': continue
			elif c == ',':
				ret.append("".join(buf))
				buf = []
			elif c == '.':
				if self.txt[self.i:self.i+2]=='..':
					buf += ['.', '.', '.']
					ret.append("".join(buf))
					self.i += 2
			elif c == ')':
				break
			else:
				buf.append(c)
		return ret

	def get_body(self):
		buf = []
		self.skip_spaces()
		while self.good():
			c = self.next()
			self.back(1)

			#print "considering ", c

			if c == ' ' or c == '\t':
				self.i += 1
				continue
			elif c == '"':
				self.i += 1
				r = self.get_string()
				buf.append( [stri, r] )
			elif c == '\'':
				self.i += 1
				r = self.get_char()
				buf.append( [chr, r] )
			elif c in string.digits:
				res = self.get_number()
				buf.append( [num, res] )
			elif c in alpha:
				r = self.get_ident()
				buf.append( [ident, r] )
			else:
				r = get_punctuator_token(self)
				if r:
					#print "r is ", r
					buf.append( [op, r])
				#else:
				#	print "NO PUNCTUATOR FOR ", c

		#def end(l):
		#	return l[1]
		#print buf
		#return "".join( map(end, buf) )
		return buf

	def get_char(self):
		buf = []
		c = self.next()
		buf.append(c)
		# skip one more character if there is a backslash '\''
		if c == '\\':
			c = self.next()
			buf.append(c)
		c = self.next()
		#buf.append(c)
		if c != '\'': error("uh-oh, invalid character"+str(c))

		return ''.join(buf)

	def get_string(self):
		buf = []
		c=''
		while self.good():
			p = c
			c = self.next()
			if c == '"':
				cnt = 0
				while 1:
					#print "cntcnt = ", str(cnt), self.txt[self.i-2-cnt]
					if self.txt[self.i-2-cnt] == '\\': cnt+=1
					else: break
				#print "cnt is ", str(cnt)
				if (cnt%2)==0: break
				else: buf.append(c)
			else:
				buf.append(c)

		return ''.join(buf)

	def get_number(self):
		buf =[]
		while self.good():
			c = self.next()
			if c in string.digits:
				buf.append(c)
			else:
				self.i -= 1
				break
		return ''.join(buf)
	def get_ident(self):
		buf = []
		while self.good():
			c = self.next()
			if c in alpha:
				buf.append(c)
			else:
				self.i -= 1
				break
		return ''.join(buf)

	def comp(self, stuff):
		clean = subst(stuff, self.defs)
		res = comp(clean)
		#print res
		if res:
			if res[0] == num: return int(res[1])
			return res[1]
		return 0

# quick test #
if __name__ == "__main__":
	Params.g_verbose = 2
	Params.g_zones = ['preproc']
	class dum:
		def __init__(self):
			self.parse_cache = {}
	Params.g_build = dum()

	try: arg = sys.argv[1]
	except: arg = "file.c"

	paths = ['.']
	gruik = cparse(strpaths = paths)
	gruik.start(arg)
	print "we have found the following dependencies"
	print gruik.deps
	print gruik.deps_paths

