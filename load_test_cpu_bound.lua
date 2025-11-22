-- CPU-Bound Workload: Hot Cache GETs
-- Accesses a small set of keys (10 keys) that fit in the cache (capacity 200)
-- High cache hit ratio (>95%) -> CPU becomes bottleneck

math.randomseed(os.time())

-- 10 hot keys that will fit in the cache
local hot_keys = {}
for i = 1, 10 do
    hot_keys[i] = "/hot" .. i
end

request = function()
    local idx = math.random(1, #hot_keys)
    return wrk.format("GET", hot_keys[idx])
end

done = function(summary, latency, requests)
    local throughput = summary.requests / (summary.duration / 1e6)
    
    io.write("\n===== CPU-Bound Workload (Hot Cache GETs) =====\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", throughput))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write(string.format("P50 Latency: %.2f ms\n", latency:percentile(50) / 1000))
    io.write(string.format("P99 Latency: %.2f ms\n", latency:percentile(99) / 1000))
    io.write("Expected: High cache hit ratio (>95%), CPU bottleneck\n")
    io.write("==============================================\n\n")
end
