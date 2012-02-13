#  XMMS2 - X Music Multiplexer System
#  Copyright (C) 2003-2012 XMMS2 Team
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

require 'xmmsclient_ext'

module Xmms
	class Collection
		# :call-seq:
		#  c.union(other) -> collection
		#
		# Returns a new collection that is the logical OR of
		# _c_ and _other_.
		def union(other)
			c = Xmms::Collection.new(Xmms::Collection::TYPE_UNION)
			c.operands << self
			c.operands << other
			c
		end

		# :call-seq:
		#  c.intersect(other) -> collection
		#
		# Returns a new collection that is the logical AND of
		# _c_ and _other_.
		def intersect(other)
			c = Xmms::Collection.new(Xmms::Collection::TYPE_INTERSECTION)
			c.operands << self
			c.operands << other
			c
		end

		# :call-seq:
		#  c.complement -> collection
		#
		# Returns a new collection that is the logical complement of
		# _c_.
		def complement
			c = Xmms::Collection.new(Xmms::Collection::TYPE_COMPLEMENT)
			c.operands << self
			c
		end

		alias :or :union
		alias :and :intersect
		alias :not :complement
		alias :| :union
		alias :& :intersect
		alias :~@ :complement
	end
end
