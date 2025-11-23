-- IO-Bound Workload: PUT Heavy
-- Generates continuous unique PUT requests with larger payloads
-- Forces every request to write to database -> Disk I/O becomes bottleneck

counter = 0
math.randomseed(os.time())

local payload_len = 1024

local function random_payload()
    local t = {}
    for i = 1, payload_len do
        -- A-Z random character per byte
        t[i] = string.char(math.random(65, 90))
    end
    return table.concat(t)
end

request = function()
    counter = counter + 1
    local key = string.format("/put_%d", counter)
    -- Use ~4KB of random data to drive higher write throughput
    local data = random_payload()
    
    return wrk.format("PUT", key, {
        ["Content-Type"] = "application/octet-stream",
        ["Content-Length"] = tostring(#data)
    }, data)
end

done = function(summary, latency, requests)
    local throughput = summary.requests / (summary.duration / 1e6)
    
    io.write("\n===== IO-Bound Workload (PUT Heavy) =====\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", throughput))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write(string.format("P50 Latency: %.2f ms\n", latency:percentile(50) / 1000))
    io.write(string.format("P99 Latency: %.2f ms\n", latency:percentile(99) / 1000))
    io.write("Expected: Disk I/O bottleneck (DB writes, larger payloads)\n")
    io.write("========================================\n\n")
end
