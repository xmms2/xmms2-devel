#  XMMS2 - X Music Multiplexer System
#  Copyright (C) 2003-2020 XMMS2 Team
#
#  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.

require 'xmmsclient'

class Xmms::Client::Sync
	attr_reader(:real)

	def initialize(name)
		@real = Xmms::Client.new(name)
	end

	def method_missing(id, *args)
		args.push(&block) if(id == 'on_disconnect')
		ret = @real.send(id, *args)
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
	attr_reader(:real)

	def initialize(coll)
		@real = coll
	end

	def method_missing(id, *args)
		ret = @real.send(id, *args)
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
	attr_reader(:real)

	def initialize(plist)
		@real = plist
	end

	def method_missing(id, *args)
		ret = @real.send(id, *args)
		if(ret.is_a?(Xmms::Result))
			ret.wait.value
		elsif(ret.is_a?(Xmms::Collection))
			self
		else
			ret
		end
	end
end
