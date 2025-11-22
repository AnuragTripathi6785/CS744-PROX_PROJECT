-- IO-Bound Workload: Cold Cache GETs
-- Accesses random keys with high cache miss probability
-- Forces frequent database reads -> Disk I/O becomes bottleneck

math.randomseed(os.time())

-- Use a large range to ensure cache misses
-- Keys are from /cold_1 to /cold_10000
local MAX_COLD_KEY = 10000

request = function()
    local k = math.random(1, MAX_COLD_KEY)
    return wrk.format("GET", "/cold_" .. k)
end

done = function(summary, latency, requests)
    local throughput = summary.requests / (summary.duration / 1e6)
    
    io.write("\n===== IO-Bound Workload (Cold Cache GETs) =====\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", throughput))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write(string.format("P50 Latency: %.2f ms\n", latency:percentile(50) / 1000))
    io.write(string.format("P99 Latency: %.2f ms\n", latency:percentile(99) / 1000))
    io.write("Expected: High cache miss ratio (>80%), Disk I/O bottleneck\n")
    io.write("==============================================\n\n")
end

