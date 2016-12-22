local rs232 = require("luars232")
local socket = require("socket")

-- Linux
port_name = "/dev/ttyUSB0"

-- (Open)BSD
-- port_name = "/dev/cua00"

-- Windows
-- port_name = "COM3"

-- Config
local config = {
	[0] = { -- theta2
		min = 800,
		max = 1500,
		default = 1425
	},
	[1] = { -- theta1
		min = 775, -- z1 = 90
		max = 2325, -- z1 = -90
		default = 1550 -- z1 = 0
	},
	[2] = { -- cap
		min = 2100,
		max = 2350,
		default = 2250
	},
	[16] = { -- theta3
		min = 600,
		max = 1600,
		default = 900
	},
	[17] = { -- useless
		min = 1550,
		max = 1550,
		default = 1550
	},
	[18] = { -- theta4
		min = 500,
		max = 1500,
		default = 500
	}
}

local status = {}

local timeout = 100 -- in miliseconds
local delay = 0
local speed = 600 -- per second
local lengthA, lengthB, lengthC, height = 16, 14, 17.5, 10 -- in cm

-- convert z1
config[1].convert = function (degree)
	if degree >= 90 then 
		return config[1].min
	elseif degree <= -90 then
		return config[1].max
	end
	if degree >= 0 then
		return config[1].default - (config[1].default - config[1].min) / 90 * degree
	else
		return config[1].default - (config[1].max - config[1].default) / 90 * degree
	end
end

-- convert z2
config[0].convert = function (degree)
	if degree <= 30 then 
		return config[0].min
	elseif degree >= 90 then
		return config[0].default
	end
	return config[0].default - (config[0].default - config[0].min) / 60 * (90 - degree)
end

-- convert z3
config[16].convert = function (degree)
	if degree >= 30 then 
		return config[16].min
	elseif degree <= -60 then
		return config[16].max
	end
	if degree >= 0 then
		return config[16].default - (config[16].default - config[16].min) / 30 * degree
	else
		return config[16].default - (config[16].max - config[16].default) / 60 * degree
	end
end

-- convert z4
config[18].convert = function (degree)
	if degree >= 0 then 
		return config[18].max
	elseif degree <= -90 then
		return config[18].min
	end
	return config[18].max + (config[18].max - config[18].default) / 90 * degree
end

function read(p)
	-- read with timeout
	local read_len = 1 -- read one byte
	local err, data_read, size = p:read(read_len, timeout)
	assert(err == rs232.RS232_ERR_NOERROR)
	return data_read
end

function write(p, cmd)
	-- write with timeout
	local err, len_written = p:write(cmd .. "\r\n", timeout)
	assert(err == rs232.RS232_ERR_NOERROR)
end

function exit(p)
	assert(p:close() == rs232.RS232_ERR_NOERROR)
end

function control(p, id, value, force)
	if config[id] then
		if ((config[id].min <= value and value <= config[id].max) or force) then
			write(p, string.format("#%d P%d S%d", id, value, speed))
			delay = math.max(math.abs(value - status[id]) / speed, delay)
			status[id] = value
		else
			print(string.format("[warning] out of range[%d, %d].", config[id].min, config[id].max))
		end
	else
		print("[warning] not found.")
	end
end

local function up(p)
	control(p, 16, 700)
end

local function relax(p)
	control(p, 2, config[2].max)
end

local function catch(p)
	control(p, 2, config[2].min)
end

function init(p)
	for id, conf in pairs(config) do
		status[id] = conf.default
		control(p, id, conf.default)
	end
end

function query(id)
	if status[id] then
		print(string.format("[info] #%d is %d, [%d, %d].", id, status[id], config[id].min, config[id].max))
	else 
		print("[warning] not found.")
	end
end

function set(id, min, max)
	if config[id] then
		config[id].min = min
		config[id].max = max
		query(id)
	else
		print("[warning] not found.")
	end
end

function sleep(ext)	
	if type(ext) == "number" then
		delay = delay + ext
	end
	socket.sleep(delay)
	delay = 0
end

function calculate(z1, z2, z3)
	print(
		"[calculate]",
		lengthA * math.cos(z1) * math.sin(z2) + lengthB * math.cos(z1) * math.sin(z3),
		lengthA * math.cos(z1) * math.cos(z2) + lengthB * math.cos(z1) * math.cos(z3),
		lengthA * math.sin(z2) + lengthB * math.sin(z3)
	)
end

function solve(p, x, y, z) 
	if math.abs(x) > 25 or y < 0 then
		return print("[warning] out of range.")
	end 
	z = z + lengthC - height
	local z1 = math.asin(x / math.sqrt(x^2 + y^2))
	local len = math.sqrt(x^2 + y^2 + z^2)
	local z2 = math.asin(z / len) + math.acos((lengthA^2 + len^2 - lengthB^2) / (2 * lengthA * len))
	local tx = lengthA * math.sin(z1) * math.cos(z2)
	local ty = lengthA * math.cos(z1) * math.cos(z2)
	local tz = lengthA * math.sin(z2)
	print("[top] ",tx,ty,tz)
	local z3 = math.asin((z - tz) / math.sqrt((x-tx)^2 + (y-ty)^2 + (z-tz)^2))
	z1, z2, z3 = z1 / math.pi * 180, z2 / math.pi * 180, z3 / math.pi * 180
	local z4 = -90 - z3
	--print("[solve] " .. z1 ..", " .. z2 ..", " .. z3 ..", " .. z4)
	--z3 = z3 + (90 - z2)
	print("[solve] " .. z1 ..", " .. z2 ..", " .. z3 ..", " .. z4)
	control(p, 1, config[1].convert(z1))
	control(p, 0, config[0].convert(z2))
	sleep()
	control(p, 16, config[16].convert(z3))
	sleep()
	control(p, 18, config[18].convert(z4))
	sleep()
end

-- open port
local e, p = rs232.open(port_name)

if e ~= rs232.RS232_ERR_NOERROR then
	-- handle error
	io.write(string.format("[error] can't open serial port '%s', error: '%s'\n",
			port_name, rs232.error_tostring(e)))
	return
end

-- set port settings
assert(p:set_baud_rate(rs232.RS232_BAUD_115200) == rs232.RS232_ERR_NOERROR)
assert(p:set_data_bits(rs232.RS232_DATA_8) == rs232.RS232_ERR_NOERROR)
assert(p:set_parity(rs232.RS232_PARITY_NONE) == rs232.RS232_ERR_NOERROR)
assert(p:set_stop_bits(rs232.RS232_STOP_1) == rs232.RS232_ERR_NOERROR)
assert(p:set_flow_control(rs232.RS232_FLOW_OFF)  == rs232.RS232_ERR_NOERROR)

io.write(string.format("[info] port open with values [%s]\n", tostring(p)))
io.write("[note] [z1] #1; [z2] #0; [z3] #16; [z4]#18; [z5] #2; [z6] #17. \n")

init(p)

local preset = {
	["太鼓"] = {-15, 15, 0},
	["仓鼠"] = {15, 15, 0},
	["小狗"] = {15, 0, 0},
	["小牛"] = {-15, 0, 0},
	["小牛上"] = {-15, 0, 5},
	["乌龟上"] = {0, 10, 5},
	["太鼓上"] = {-15, 15, 8},
}
-- command line
while true do 
	cmd = io.read()
	if cmd:find("init") then
		init(p)
		sleep()
	elseif cmd:find("up") then
		up(p)
		sleep()
	elseif cmd:find("relax") then
		relax(p)
		sleep()
	elseif cmd:find("catch") then
		catch(p)
		sleep()
	elseif cmd:find("exit") then
		break
	elseif cmd:find("voice") then
		-- voice control
		local server = socket.tcp()
		server:bind("*", "8877")
		server:listen(8)
		while true do
			local client = server:accept()
			local res = client:receive("*a")
			local ret = res:match("^.-input=(%S+)")
			if ret then
				print("[info] received command: " .. ret)
				if ret == "停止控制" then
					break
				elseif ret == "初始化" then
					init(p)
				elseif ret:find("抓起(%S+)") then
					local obj = ret:match("抓起(%S+)")
					if preset[obj] then
						up(p)
						relax(p)
						solve(p, preset[obj][1], preset[obj][2], preset[obj][3])
						socket.sleep(0.5)
						catch(p)
						sleep()
						up(p)
					else
						print("[warning] unknown position.")
					end
				elseif ret:find("放在(%S+)") then
					local pos = ret:match("放在(%S+)")
					if preset[pos] then
						solve(p, preset[pos][1], preset[pos][2], preset[pos][3])
						sleep(0.5)
						relax(p)
						sleep(1)
						up(p)
					else
						print("[warning] unknown position.")
					end
				else
					print("[warning] not an available command.")
				end
			else
				print("[warning] command not found.")
			end
		end
		server:close()
	elseif cmd:find("%-?%d+ %-?%d+ %-?%d+") then
		local x, y, z = cmd:match("(%-?%d+) (%-?%d+) (%-?%d+)")
		if cmd:find("set") then
			set(tonumber(x), tonumber(y), tonumber(z))
		else 
			solve(p, tonumber(x), tonumber(y), tonumber(z))
		end
	elseif cmd:find("%-?%d+ %-?%d+") then
		local m, v = cmd:match("(%-?%d+) (%-?%d+)")
		control(p, tonumber(m), tonumber(v), cmd:find("force"))
		sleep()
	elseif cmd:find("%-?%d+") then
		local m = cmd:match("(%-?%d+)")
		query(tonumber(m))
	end
end
-- close
exit(p)
