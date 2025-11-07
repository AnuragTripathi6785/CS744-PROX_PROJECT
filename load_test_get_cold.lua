-- Simulates random GET requests for non-existent or uncached keys
-- Expected: High disk I/O and cache misses
math.randomseed(os.time())

request = function()
    local key = string.format("/cold_key_%d", math.random(1, 10000))
    return wrk.format("GET", key)
end

done = function(summary, latency, requests)
    io.write("\n=============================================\n")
    io.write("GET Cold Workload (I/O Bound)\n")
    io.write("=============================================\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", summary.requests / summary.duration * 1e6))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write(string.format("Cache Miss Rate: High (cold keys)\n"))
    io.write("Expected Bottleneck: Disk I/O\n")
    io.write("=============================================\n")
end
