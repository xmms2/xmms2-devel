#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Execute the tasks"

import sys, time, random
import Params, Task, Utils
from Params import debug, error

dostat=0
"""
output a stat file (data for gnuplot) when running tasks in parallel

#! /usr/bin/gnuplot -persist
set terminal png
set output "output.png"
set yrange [-1:6]
plot 'test.dat' using 1:3 with linespoints
"""

g_initial = time.time()
"time since the start of the build"

g_quiet = 0
"do not output anything"


class CompilationError(Exception):
	pass

exetor = None
"subprocess"
try:
	import subprocess
	exetor = subprocess
except:
	# this is provided for python < 2.4, will be removed in the future
	import pproc
	exetor = pproc

def write_progress(s):
	if Params.g_options.progress_bar == 1:
		sys.stderr.write(s + '\r')
	elif Params.g_options.progress_bar == 2:
		# do not display anything if there is nothing to display
		if s:
			print s
			sys.stdout.flush()
	else:
		if s: print s

def progress_line(s, t, col1, task, col2):
	"do not print anything if there is nothing to display"
	disp = task.get_display()
	if not disp: return ''
	global g_initial
	if Params.g_options.progress_bar == 1:
		pc = (100.*s)/t
		eta = time.strftime('%H:%M:%S', time.gmtime(time.time() - g_initial))

		left = "[%d/%d] %s%d%%%s |" % (s, t, col1, pc, col2)
		right = '| %s%s%s' % (col1, eta, col2)

		cols = Utils.get_term_cols() - len(left) - len(right) + 15
		if cols < 7: cols = 7

		bar = ('='*int(((cols+1.)*s)/t-1)+'>').ljust(cols)

		disp = '%s%s%s' % (left, bar, right)
		return disp

		return '[%d/%d] %s%d%%%s |%s| %s%s%s' % (s, t, col1, pc, col2, bar, col1, eta, col2)
	elif Params.g_options.progress_bar == 2:
		eta = time.strftime('%H:%M:%S', time.gmtime(time.time() - g_initial))

		ins  = ','.join(map(lambda n: n.m_name, task.m_inputs))
		outs = ','.join(map(lambda n: n.m_name, task.m_outputs))

		return '|Total %s|Current %s|Inputs %s|Outputs %s|Time %s|' % (t, s, ins, outs, eta)

	return '[%d/%d] %s%s%s' % (s, t, col1, disp, col2)

def process_cmd_output(cmd_stdout, cmd_stderr):
	stdout_eof = stderr_eof = 0
	while not (stdout_eof and stderr_eof):
		if not stdout_eof:
			str = cmd_stdout.read()
			if not str: stdout_eof = 1
			elif not g_quiet: sys.stdout.write(str)
		if not stderr_eof:
			str = cmd_stderr.read()
			if not str: stderr_eof = 1
			elif not g_quiet:
				sys.stderr.write('\n')
				sys.stderr.write(str)
		#time.sleep(0.1)

def exec_command_normal(str):
	"run commands in a portable way the subprocess module backported from python 2.4 and should work on python >= 2.2"
	debug("system command -> "+ str, 'runner')
	if Params.g_verbose==1: print str
	proc = exetor.Popen(str, shell=1, stdout=exetor.PIPE, stderr=exetor.PIPE)
	process_cmd_output(proc.stdout, proc.stderr)
	stat = proc.wait()
	if stat & 0xff: return stat | 0x80
	return stat >> 8

def exec_command_interact(str):
	"this one is for the latex output, where we cannot capture the output while the process waits for stdin"
	debug("system command (interact) -> "+ str, 'runner')
	if Params.g_verbose==1: print str
	proc = exetor.Popen(str, shell=1)
	stat = proc.wait()
	if stat & 0xff: return stat | 0x80
	return stat >> 8

exec_command = exec_command_interact # python bug on stdout overload
def set_exec(mode):
	global exec_command
	if mode == 'normal': exec_command = exec_command_normal
	elif mode == 'noredir': exec_command = exec_command_interact
	else: error('set_runner_mode')

class JobGenerator:
	"kind of iterator - the data structure is a bit complicated (price to pay for flexibility)"
	def __init__(self, tree):
		self.m_tree = tree

		self.curgroup = 0
		self.curprio = -1
		self.m_outstanding = [] # list of tasks in the current priority

		self.priolst = []

		# progress bar
		self.m_total = Task.g_tasks.total()
		self.m_processed = 0

		self.m_switchflag = 1 # postpone
		#Task.g_tasks.debug()

	# warning, this one is recursive ..
	def get_next(self):
		if self.m_outstanding:
			t = self.m_outstanding[0]
			self.m_outstanding=self.m_outstanding[1:]
			self.m_processed += 1
			return t

		# handle case where only one wscript exist
		# that only install files
		if not Task.g_tasks.groups:
			return None

		# stop condition
		if self.curgroup >= len(Task.g_tasks.groups):
			return None

		# increase the priority value
		self.curprio += 1

		# there is no current list
		group = Task.g_tasks.groups[self.curgroup]
		if self.curprio >= len(group.prio.keys()):
			self.curprio = -1
			self.curgroup += 1
			return self.get_next()

		# sort keys if necessary
		if self.curprio == 0:
			self.priolst = group.prio.keys()
			self.priolst.sort()

		# now fill m_outstanding
		id = self.priolst[self.curprio]
		self.m_outstanding = group.prio[id]

		return self.get_next()

	def progress(self):
		return (self.m_processed, self.m_total)

	def postpone(self, task):
		self.m_processed -= 1
		# shuffle the list - some fanciness of mine (ita)
		self.m_switchflag=-self.m_switchflag
		if self.m_switchflag>0: self.m_outstanding = [task]+self.m_outstanding
		else:                   self.m_outstanding.append(task)
		#self.m_current_task_lst = [task]+self.m_current_task_lst

	# TODO FIXME
	def debug(self):
		debug("debugging a task: something went wrong:", 'runner')
		s=""
		for t in Task.g_tasks:
			s += str(t.m_idx)+" "
		debug(s, 'runner')

	# skip a group and report the failure
	def skip_group(self, reason):
		Task.g_tasks.groups[self.curgroup].info = reason
		self.curgroup += 1
		self.curprio = -1
		self.m_outstanding = []
		try: Task.g_tasks.groups[self.curgroup].prio.sort()
		except: pass

class Serial:
	def __init__(self, generator):
		self.m_generator = generator
	def start(self):
		debug("Serial start called", 'runner')
		#self.m_generator.debug()
		while 1:
			# get next Task
			proc = self.m_generator.get_next()
			if proc is None: break

			debug("retrieving #"+str(proc.m_idx), 'runner')

			# # =======================
			#if proc.m_hasrun:
			#	error("task has already run! "+str(proc.m_idx))

			if not proc.may_start():
				debug("delaying   #"+str(proc.m_idx), 'runner')
				self.m_generator.postpone(proc)
				#self.m_generator.debug()
				#proc = None
				continue
			# # =======================

			proc.prepare()
			#proc.debug()

			#debug("m_sig is "+str(proc.m_sig), 'runner')
			#debug("obj output m_sig is "+str(proc.m_outputs[0].get_sig()), 'runner')

			#continue
			if not proc.must_run():
				proc.m_hasrun=2
				#debug("task is up-to_date "+str(proc.m_idx), 'runner')
				continue

			debug("executing  #"+str(proc.m_idx), 'runner')

			# display the command that we are about to run
			if not Params.g_commands['configure']:
				(s, t) = self.m_generator.progress()
				col1=''
				col2=''
				try:
					col1=Params.g_colors[proc.color()]
					col2=Params.g_colors['NORMAL']
				except: pass
				write_progress(progress_line(s, t, col1, proc, col2))

			# run the command
			ret = proc.run()

			# non-zero means something went wrong
			if ret:
				if Params.g_options.keep:
					self.m_generator.skip_group('non-zero return code\n' + proc.debug_info())
					continue
				else:
					if Params.g_verbose:
						error("task failed! (return code %s for #%s)"%(str(ret), str(proc.m_idx)))
						proc.debug(1)
					return ret

			try:
				proc.update_stat()
			except:
				if Params.g_options.keep:
					self.m_generator.skip_group('missing nodes\n' + proc.debug_info())
					continue
				else:
					if Params.g_verbose:
						error('the nodes have not been produced !')
					raise CompilationError()
			proc.m_hasrun=1

			# register the task to the ones that have run - useful for debugging purposes
			Task.g_tasks_done.append(proc)

			"""try:
				proc.apply()
			except KeyboardInterrupt:
				raise
			except:
				print "hum hum .. task failed!"
			"""

		debug("Serial end", 'runner')
		return 0

import threading
import Queue


lock = None
condition = None
count = 0
stop = 0
running = 0
failed = 0


stat = []
class TaskPrinter(threading.Thread):
	def __init__(self, id, master):
		threading.Thread.__init__(self)
		self.setDaemon(1)
		self.m_master = master
		self.start()

	def run(self):
		global count, lock, running, stat
		while 1:
			lock.acquire()
			stat.append( (time.time(), count, running) )
			lock.release()
			time.sleep(0.1)

class TaskConsumer(threading.Thread):
	def __init__(self, id, master):
		threading.Thread.__init__(self)
		self.setDaemon(1)
		self.m_master = master
		self.m_id     = id
		self.start()

		self.m_count = 0
		self.m_stop  = 0

	def notify(self):
		global condition
		condition.acquire()
		condition.notify()
		condition.release()

	def do_stat(self, num):
		global running
		lock.acquire()
		running += num
		lock.release()

	def run(self):
		global lock, count, stop, running, failed
		while 1:
			lock.acquire()
			self.m_stop  = stop
			lock.release()

			if self.m_stop:
				while 1:
					if failed > 0: count = 0 # force the scheduler to check for failure
					time.sleep(1)

			# take the next task
			proc = self.m_master.m_ready.get(block=1)

			self.do_stat(1)

			# display the label for the command executed
			write_progress(proc.get_display())

			# run the command
			ret = proc.run()

			self.do_stat(-1)

			if ret:
				lock.acquire()
				if Params.g_verbose:
					error("task failed! (return code %s and task id %s)"%(str(ret), str(proc.m_idx)))
					proc.debug(1)
				count -= 1
				stop   = 1
				failed = 1
				self.notify()
				lock.release()
				continue

			try:
				proc.update_stat()
			except:
				lock.acquire()
				if Params.g_verbose:
					error('the nodes have not been produced !')
				count -= 1
				stop   = 1
				failed = 1
				self.notify()
				lock.release()

			proc.m_hasrun = 1

			lock.acquire()
			count -= 1
			lock.release()
			self.notify()

class Parallel:
	"""
	The following is a small scheduler, using an agressive scheme
	for making as many tasks available to the consumer threads
	Someone may come with a much better scheme, as i do not have too much
	time to spend on this (ita)
	"""
	def __init__(self, tree, numjobs):
		# the tree we are working on
		self.m_tree = tree

		# number of consumers
		self.m_numjobs = numjobs

		# the container of all tasks: a list of hashtables containing lists of tasks
		self.m_tasks = Task.g_tasks

		# progress bar
		self.m_total = Task.g_tasks.total()
		self.m_processed = 1

		# tasks waiting to be processed - IMPORTANT
		self.m_outstanding = []
		# tasks waiting to be run by the consumers
		self.m_ready = Queue.Queue(150)
		# tasks that are awaiting for another task to complete
		self.m_frozen = []

		# lock for self.m_count - count the amount of tasks active
		self.m_count = 0
		self.m_stop = 0
		self.m_failed = 0
		self.m_running = 0

		self.curgroup = 0
		self.curprio = -1
		self.priolst = []

		global condition
		condition = threading.Condition()

		global lock
		lock = threading.Lock()

		# for consistency
		self.m_generator = self

	def read_values(self):
		#print "read values acquire lock"
		global lock, stop, count, failed
		lock.acquire()
		self.m_stop = stop
		self.m_count = count
		self.m_failed = failed
		self.m_running = running
		lock.release()
		#print "read values release lock"

	def wait_all_finished(self):
		global condition
		condition.acquire()
		while self.m_count>0:
			condition.wait()
			self.read_values()
		condition.release()
		if self.m_failed:
			while 1:
				self.read_values()
				if self.m_running == 0: raise CompilationError()
				time.sleep(0.5)

	def get_next_prio(self):
		# stop condition
		if self.curgroup >= len(Task.g_tasks.groups):
			return (None, None)

		# increase the priority value
		self.curprio += 1

		# there is no current list
		group = Task.g_tasks.groups[self.curgroup]
		if self.curprio >= len(group.prio.keys()):
			self.curprio = -1
			self.curgroup += 1
			return self.get_next_prio()

		# sort keys if necessary
		if self.curprio == 0:
			self.priolst = group.prio.keys()
			self.priolst.sort()

		id = self.priolst[self.curprio]
		return (id, group.prio[id])

	def start(self):
		global count, lock, stop, condition, dostat

		# unleash the consumers
		for i in range(self.m_numjobs): TaskConsumer(i, self)

		if dostat: TaskPrinter(-1, self)

		# the current group
		#group = None

		# current priority
		currentprio = 0

		loop=0

		# add the tasks to the queue
		while 1:
			self.read_values()
			if self.m_stop:
				self.wait_all_finished()
				break

			# if there are no tasks to run, wait for the consumers to eat all of them
			# and then skip to the next priority group
			if (not self.m_frozen) and (not self.m_outstanding):
				self.wait_all_finished()
				(currentprio, self.m_outstanding) = self.get_next_prio()
				if currentprio is None: break

			# for tasks that must run sequentially
			# (linking object files uses a lot of memory for example)
			if (currentprio%2)==1:
				# make sure there is no more than one task in the queue
				condition.acquire()
				while self.m_count>0:
					condition.wait()
					self.read_values()
				condition.release()
			else:
				# wait a little bit if there are enough jobs for the consumer threads
				condition.acquire()
				while self.m_count>self.m_numjobs+10:
					condition.wait()
					self.read_values()
				condition.release()

			loop += 1

			if not self.m_outstanding:
				self.m_outstanding = self.m_frozen
				self.m_frozen = []

			# now we are certain that there are outstanding or frozen threads
			if self.m_outstanding:
				proc = self.m_outstanding.pop(0)
				if not proc.may_start():
					# shuffle
					#print "shuf0"
					#self.m_frozen.append(proc)
					#self.m_frozen = [proc]+self.m_frozen
					if random.randint(0,1):
						#print "shuf1"
						self.m_frozen.append(proc)
					else:
						#print "shuf2"
						self.m_frozen = [proc]+self.m_frozen
					if not self.m_outstanding:
						condition.acquire()
						condition.wait()
						condition.release()

				else:
					proc.prepare()
					if not proc.must_run():
						proc.m_hasrun=2
						self.m_processed += 1
						continue

					# display the command that we are about to run
					col1=''
					col2=''
					try:
						col1=Params.g_colors[proc.color()]
						col2=Params.g_colors['NORMAL']
					except: pass
					proc.set_display(progress_line(self.m_processed, self.m_total, col1, proc, col2))

					lock.acquire()
					count += 1
					self.m_processed += 1
					lock.release()

					self.m_ready.put(proc, block=1)

		debug("amount of loops "+str(loop), 'runner')
		global stat
		if dostat and stat:
			file = open('test.dat', 'w')
			(t1, queue, run) = stat[0]
			for (time, queue, run) in stat:
				file.write("%f %f %f\n" % (time-t1, queue, run))
			file.close()

