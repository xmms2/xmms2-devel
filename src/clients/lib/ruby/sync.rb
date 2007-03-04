#!/usr/bin/env ruby

require 'xmmsclient'

class Xmms::Client::Sync
	def initialize(name)
		@x = Xmms::Client.new(name)
	end

	def method_missing(id, *args)
		ret = @x.send(id, *args)
		if(ret.is_a?(Xmms::Result) || ret.is_a?(Xmms::BroadcastResult) ||
		   ret.is_a?(Xmms::SignalResult))
			ret.wait.value
		elsif(ret.is_a?(Xmms::Client))
			self
		elsif(ret.is_a?(Xmms::Collection))
			Xmms::Collection::Sync.new(ret)
		elsif(ret.is_a?(Xmms::Playlist))
			Xmms::Playlist::Sync.new(ret)
		else
			ret
		end
	end
end

class Xmms::Collection::Sync
	def initialize(coll)
		@c = coll
	end

	def method_missing(id, *args)
		ret = @c.send(id, *args)
		if(ret.is_a?(Xmms::Result))
			ret.wait.value
		elsif(ret.is_a?(Xmms::Collection))
			self
		else
			ret
		end
	end
end

class Xmms::Playlist::Sync
	def initialize(plist)
		@p = plist
	end

	def method_missing(id, *args)
		ret = @p.send(id, *args)
		if(ret.is_a?(Xmms::Result))
			ret.wait.value
		elsif(ret.is_a?(Xmms::Collection))
			self
		else
			ret
		end
	end
end
