#!/usr/bin/env ruby

def recurse(n)
	# Create an object on the stack to make the GC do a little work:
	object = Object.new
	
	if n == 0
		Fiber.yield
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
		GC.start(full_mark: true, immediate_sweep: true)
		
		puts "Debug counters: "RubyVM.stat.slice(:fiber_full_stack_scan, :thread_full_stack_scan, :stack_scan_bytes)
		puts "GC stat: "GC.stat.slice(:count, :heap_marked_slots, :minor_gc_count, :major_gc_count)

		#puts GC.latest_gc_info
		# The second time we do this, we would imagine that the fiber state has not changed, in theory it should not require any stack scanning:
		GC.start(full_mark: false, immediate_sweep: true)
		
		puts "Debug counters: "RubyVM.stat.slice(:fiber_full_stack_scan, :thread_full_stack_scan, :stack_scan_bytes)
		puts "GC stat: "GC.stat.slice(:count, :heap_marked_slots, :minor_gc_count, :major_gc_count)
		# fibers.each do |fiber|
		# 	fiber.resume
		# end
		
		# fibers = nil
		
		# # The third time we do this, we expect all fibers to be dead, and we should not scan them:
		# GC.start(full_mark: true, immediate_sweep: true)
		
		# # The forth time we do this, there should be no state remaining and it should be O(1) time:
		# GC.start(full_mark: true, immediate_sweep: true)
	end
end

GC.disable
GC.start

GC::Profiler.enable

benchmark

GC::Profiler.report
GC::Profiler.disable
