#!/usr/bin/env ruby

require 'xmmsclient'

class Xmms::Client::Async
	attr_reader(:real)

	def initialize(name)
		@real = Xmms::Client.new(name)
	end

	def method_missing(id, *args, &block)
		ret = @real.send(id, *args)
		if(block_given? && (ret.is_a?(Xmms::Result) ||
		   ret.is_a?(Xmms::BroadcastResult) || ret.is_a?(Xmms::SignalResult)))
			ret.notifier(&block)
		elsif(ret.is_a?(Xmms::Client))
			self
		elsif(ret.is_a?(Xmms::Collection))
			Xmms::Collection::Async.new(ret)
		elsif(ret.is_a?(Xmms::Playlist))
			Xmms::Playlist::Async.new(ret)
		else
			ret
		end
	end
end

class Xmms::Collection::Async
	attr_reader(:real)

	def initialize(coll)
		@real = coll
	end

	def method_missing(id, *args, &block)
		ret = @real.send(id, *args)
		if(block_given? && ret.is_a?(Xmms::Result))
			ret.notifier(&block)
		elsif(ret.is_a?(Xmms::Collection))
			self
		else
			ret
		end
	end
end

class Xmms::Playlist::Async
	attr_reader(:real)

	def initialize(plist)
		@real = plist
	end

	def method_missing(id, *args, &block)
		ret = @real.send(id, *args)
		if(block_given? && ret.is_a?(Xmms::Result))
			ret.notifier(&block)
		elsif(ret.is_a?(Xmms::Collection))
			self
		else
			ret
		end
	end
end
