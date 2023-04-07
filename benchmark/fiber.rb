#!/usr/bin/env ruby

def recurse(n)
	# Create an object on the stack to make the GC do a little work:
	object = Object.new
	
	if n == 0
		Fiber.yield while true
	else
		recurse(n - 1)
	end
end

def make_fibers(n = 1000, m = 1000)
	n.times.map do
		Fiber.new{recurse(m)}.tap(&:resume)
	end
end

def benchmark
	15.times do |i|
		count  = 2**i
		puts "Count = #{count}"
		
		fibers = make_fibers(count)
		
		# The first time we do this, we expect all fibers to be fresh, and we should scan them:
		GC.start
		
		# The second time we do this, we would imagine that the fiber state has not changed, in theory it should not require any stack scanning:
		GC.start
	end
end

GC.disable
GC::Profiler.enable

benchmark

GC::Profiler.report
GC::Profiler.disable
