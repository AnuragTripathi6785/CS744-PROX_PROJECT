-- Simulates continuous PUT requests with unique keys
-- Expected: I/O bottleneck (Database writes)
counter = 0
math.randomseed(os.time())

request = function()
    counter = counter + 1
    local key = string.format("/key_%d", counter)
    local data = string.rep("X", 512)
    return wrk.format("PUT", key, {
        ["Content-Type"] = "application/octet-stream",
        ["Content-Length"] = tostring(#data)
    }, data)
end

done = function(summary, latency, requests)
    io.write("\nPUT Heavy Workload (I/O Bound)\n")
    io.write("..........................................\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", summary.requests / summary.duration * 1e6))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write(string.format("Max Latency: %.2f ms\n", latency.max / 1000))
    io.write("\nExpected Bottleneck: Disk I/O (Database writes)\n")
    io.write("Use: iostat -d 2  (macOS) or iostat -x 2 (Linux)\n")
    io.write("...........................................\n")
end
