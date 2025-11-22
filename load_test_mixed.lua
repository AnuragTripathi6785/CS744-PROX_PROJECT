-- Mixed Workload: Combination of GETs and PUTs
-- 70% GET requests (mix of hot and cold)
-- 30% PUT requests
-- Creates mixed CPU and IO pressure

math.randomseed(os.time())

local hot_keys = {}
for i = 1, 10 do
    hot_keys[i] = "/hot" .. i
end

local put_counter = 0
local MAX_COLD_KEY = 10000

request = function()
    local rand = math.random(1, 100)
    
    if rand <= 70 then
        -- 70% GET requests
        if math.random(1, 100) <= 50 then
            -- 50% of GETs are hot (within the 70%)
            local idx = math.random(1, #hot_keys)
            return wrk.format("GET", hot_keys[idx])
        else
            -- 50% of GETs are cold (within the 70%)
            local k = math.random(1, MAX_COLD_KEY)
            return wrk.format("GET", "/cold_" .. k)
        end
    else
        -- 30% PUT requests
        put_counter = put_counter + 1
        local key = string.format("/mixed_put_%d", put_counter)
        local data = string.rep("X", 512)
        return wrk.format("PUT", key, {
            ["Content-Type"] = "application/octet-stream",
            ["Content-Length"] = tostring(#data)
        }, data)
    end
end

done = function(summary, latency, requests)
    local throughput = summary.requests / (summary.duration / 1e6)
    
    io.write("\n===== Mixed Workload (GET + PUT) =====\n")
    io.write(string.format("Total Requests: %d\n", summary.requests))
    io.write(string.format("Throughput: %.2f req/s\n", throughput))
    io.write(string.format("Avg Latency: %.2f ms\n", latency.mean / 1000))
    io.write(string.format("P50 Latency: %.2f ms\n", latency:percentile(50) / 1000))
    io.write(string.format("P99 Latency: %.2f ms\n", latency:percentile(99) / 1000))
    io.write("Expected: Mixed CPU and IO pressure\n")
    io.write("====================================\n\n")
end

