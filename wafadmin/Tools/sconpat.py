#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

import md5
import Utils, Configure, Action, Task, Params
from Params import error, fatal

class sconpat_error:
	pass

class Builder_class:
	def __init__(self):
		self.action = None
		self.generator = None
	def init(self, **kw):
		if kw.has_key('generator') and kw.has_key('action'):
			raise sconpat_error, 'do not mix action and generator in a builder'

		if kw.has_key('action'):

			a = kw['action'].replace('$SOURCES', '${SRC}')
			a = a.replace('$TARGETS', '${TGT}')
			a = a.replace('$TARGET', '${TGT[0].abspath(env)}')
			a = a.replace('$SOURCE', '${SRC[0].abspath(env)}')

			m = md5.new()
			m.update(a)
			key = m.hexdigest()

			Action.simple_action(key, a, kw.get('color', 'GREEN'))
			self.action=key
	def apply(self, target, source, **kw):
		#print "Builder_class apply called"
		#print kw['env']
		#print target
		#print source

		curdir = Params.g_build.m_curdirnode

		t = Task.Task(self.action, kw['env'], 10)
		t.set_inputs(curdir.find_source(source, create=1))
		t.set_outputs(curdir.find_build(target, create=1))

def Builder(**kw):
	ret = Builder_class()
	ret.init(**kw)
	return ret

def Environment(**kw):
	import Environment
	ret = Environment.Environment()
	if kw.has_key('BUILDERS'):
		bd = kw['BUILDERS']
		for k in bd:
			# store the builder name on the builder
			bd[k].name = k

			def miapply(self, *lst, **kw):
				if not 'env' in kw: kw['env']=ret
				bd[k].apply(*lst, **kw)

			ret.__class__.__dict__[k]=miapply
	return ret

def setup(env):
	pass

def detect(conf):
	"attach the checks to the conf object"
	return 1

