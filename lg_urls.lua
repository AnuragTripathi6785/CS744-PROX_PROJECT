#!/usr/bin/env lua

math.randomseed(os.time())


paths = {
  "/file1.html", "/file2.html", "/file3.html", "/file4.html",
  "/file5.html", "/file6.html", "/file7.html", "/file8.html",
  "/file9.html", "/file10.html", "/file11.html", "/file12.html",
  "/file13.html", "/file14.html", "/file15.html", "/file16.html",
  "/file17.html", "/file18.html", "/file19.html", "/file20.html",
  "/file21.html", "/file22.html", "/file23.html"
}

request = function()
  local r = math.random()
  local path
  if r < 0.7 then
	path = "/file" .. math.random(1, 4) .. ".html"
  else
	path = "/file" .. math.random(5, 23) .. ".html"  
  end
  return "GET " .. path .. " HTTP/1.1\r\nHost: localhost\r\n\r\n"
end

