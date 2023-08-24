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
	fiber_full_stack_scan = Array.new
	thread_full_stack_scan = Array.new
	stack_scan_bytes = Array.new

	15.times do |i|
		count  = 2**i
		puts "Count = #{count}"
		
		fibers = make_fibers(count)
		
		# The first time we do this, we expect all fibers to be fresh, and we should scan them:
		GC.start(full_mark: true, immediate_sweep: true)

		
		fiber_full_stack_scan.push(RubyVM.stat.slice(:fiber_full_stack_scan).to_a)
		thread_full_stack_scan.push(RubyVM.stat.slice(:thread_full_stack_scan).to_a)
		stack_scan_bytes.push(RubyVM.stat.slice(:stack_scan_bytes).to_a)

		# The second time we do this, we would imagine that the fiber state has not changed, in theory it should not require any stack scanning:
		GC.start(full_mark: false, immediate_sweep: true)

		fiber_full_stack_scan.push(RubyVM.stat.slice(:fiber_full_stack_scan).to_a)
		thread_full_stack_scan.push(RubyVM.stat.slice(:thread_full_stack_scan).to_a)
		stack_scan_bytes.push(RubyVM.stat.slice(:stack_scan_bytes).to_a)

		fibers.each do |fiber|
			fiber.resume
		end
		
		fibers = nil
		
		# # The third time we do this, we expect all fibers to be dead, and we should not scan them:
		# GC.start(full_mark: true, immediate_sweep: true)
		
		# # The forth time we do this, there should be no state remaining and it should be O(1) time:
		# GC.start(full_mark: true, immediate_sweep: true)
	end
	
	a = fiber_full_stack_scan.zip(thread_full_stack_scan).map(&:flatten)
	a = a.zip(stack_scan_bytes).map(&:flatten)

	count = 1

	puts "Index      fiber_scan       thread_scan	stack_scan_bytes"
	a.each do |pair|
		index = count.to_s.rjust(5)
		fiber_scan = pair[1].to_s.rjust(15)
		thread_scan = pair[3].to_s.rjust(15)
		stack_scan_bytes = pair[5].to_s.rjust(15)
		puts "#{index}#{fiber_scan}#{thread_scan}#{stack_scan_bytes}"
		count += 1
	end

	fiber_full_stack_scan = nil
	thread_full_stack_scan = nil
	stack_scan_bytes = nil
	a = nil

end

GC.disable
GC.start

GC::Profiler.enable

benchmark

GC::Profiler.report
GC::Profiler.disable
