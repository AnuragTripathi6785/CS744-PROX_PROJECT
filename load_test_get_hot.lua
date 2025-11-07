-- Simulates repeated reads on small set of cached keys
-- Expected: CPU bottleneck due to in-memory cache hits
keys = {}
for i = 1, 10 do
  keys[i] = string.format("/hot%d", i)
end
math.randomseed(os.time())

request = function()
    local key = keys[math.random(1, #keys)]
    return wrk.format("GET", key)
end

done = function(summary, latency, requests)
    io.write("\n=============================================\n")
    io.write("GET Hot Workload (CPU Bound)\n")
    io.write("=============================================\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", summary.requests / summary.duration * 1e6))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write("\nExpected Bottleneck: CPU (cache hits)\n")
    io.write("Use: top -pid $(pgrep proxy_server)\n")
    io.write("=============================================\n")
end
