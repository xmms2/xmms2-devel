import sys

class Indenter:
	indent = 0

	@classmethod
	def enter(cls, s = None):
		if s != None:
			Indenter.printline(s)

		cls.indent += 1

	@classmethod
	def leave(cls, s = None):
		cls.indent -= 1

		Indenter.printline(s)

	# print the given string without a trailing newline
	@classmethod
	def printx(cls, s):
		sys.stdout.write('\t' * cls.indent)
		sys.stdout.write(s)

	# print the given string including a trailing newline
	@classmethod
	def printline(cls, s = ''):
		if not s:
			print ''
		else:
			sys.stdout.write('\t' * cls.indent)
			print s
