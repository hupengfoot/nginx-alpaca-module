blockByVid = "false";
blockByVidOnly = "false";

denyIPAddress = {};
acceptIPPrefix = {};
acceptHttpMethod = {};
denyUserAgent = {};
denyUserAgentPrefix = {};
denyIPAddressPrefix = {};
denyIPAddressRate = {};
denyUserAgentContainAnd = {};
denyIPVidRate = {};
denyNOVisitorIDURL = {};
denyVisterID = {};
denyVisterIDRate = {};

tmp_acceptHttpMethod = {};
tmp_string = "{\"post\":[\"all\"],\"get\":[\"all\"]}";
json = require "cjson";
tmp_acceptHttpMethod = json.decode(tmp_string);

function decode(key, str)
	--local str = "{\"1.1.1.1,22222\":{\"tValue\":[\"all\",\"www.dianping.com\"],\"kValue\":\"3333-03-03 03:03:03\"}}";
	--local str = "{\"Himi\":\"himigame.com\"}";
	if(key == "alpaca.filter.blockByVid") then
		blockByVid = str;
	elseif(key == "alpaca.filter.blockByVidOnly") then
		blockByVidOnly = str;
	elseif(key == "alpaca.policy.denyIPAddress") then
		denyIPAddress = json.decode(str);
	elseif(key == "alpaca.policy.acceptIPPrefix") then
		acceptIPPrefix = json.decode(str);
	elseif(key == "alpaca.policy.acceptHttpMethod") then
		acceptHttpMethod = json.decode(str);
	elseif(key == "alpaca.policy.denyUserAgent") then
		denyUserAgent = json.decode(str);
	elseif(key == "alpaca.policy.denyUserAgentPrefix") then
		denyUserAgentPrefix = json.decode(str);
	elseif(key == "alpaca.policy.denyIPAddressPrefix") then
		denyIPAddressPrefix = json.decode(str);
	elseif(key == "alpaca.policy.denyIPAddressRate") then
		denyIPAddressRate = json.decode(str);
	elseif(key == "alpaca.policy.denyUserAgentContainAnd") then
		denyUserAgentContainAnd = json.decode(str);
	elseif(key == "alpaca.policy.denyIPVidRate") then
		denyIPVidRate = json.decode(str);
	elseif(key == "alpaca.policy.denyNoVisitorIdURL.new") then
		denyNOVisitorIDURL = json.decode(str);
	elseif(key == "alpaca.policy.denyVisterID") then
		denyVisterID = json.decode(str);
	elseif(key == "alpaca.policy.denyVisterIDRate") then
		denyVisterIDRate = json.decode(str);
	end
end

function lookupacceptHttpMethod(httpmethod, domain)
	for key, value in pairs(tmp_acceptHttpMethod) do 
		if(string.lower(key) == string.lower(httpmethod)) then
			for n = 1, #value do
				if(string.lower(value[n]) == string.lower(domain) or string.lower(value[n]) == "all") then
					return 1;
				end
			end
		end
	end 
	return 0;
end

function lookupacceptIPPrefix(clientip, domain)
	for key, value in pairs(acceptIPPrefix) do
		if(clientip == key) then
			for n = 1, #value do
				if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
					return 1;
				end	
			end
		end
	end
	return 0;
end

function isUserAgentValid(useragent, domain)
	if(isBlank(useragent) == 1) then
		return 0;
	end
	if(lookupdenyUserAgent(useragent, domain) == 1) then
		return 0;
	end
	if(lookupdenyUserAgentPrefix(useragent, domain) == 1) then
		return 0;
	end
	return 1;
end

function block(clientip, useragent, httpmethod, rawurl, visitid, domain)
	if(lookupacceptIPPrefix(clientip, domain) == 1) then
		return 201;	
	elseif(lookupacceptHttpMethod(httpmethod, domain) == 0) then
		return 406;	
	elseif(isUserAgentValid(useragent, domain) == 0) then
		return 401;
	elseif(isIPBlackList(clientip, domain) == 1) then
		return 403;
	elseif(isVidBlackList(visitid, domain) == 1) then
		return 407;
	else
		if(isBlank(visitid) == 1) then
			if(rawurl ~= nil and lookupdenyNOVisitorIDURL(rawurl, httpmethod, domain) == 1) then
				return 405;
			end
			if(isIpRateBlackList(clientip, domain) == 1) then
	      			return 402;
			end
		else
			if(blockByVid == "true") then
				if(isIpVidRateBlackList(clientip, visitid, domain) == 1) then
					return 404;
				end
			else
				if(isIpRateBlackList(clientip, domain) == 1) then
					return 402;
				end
			end
			if(blockByVidOnly == "true") then
				if(isVidRateBlackList(visitid, domain) == 1) then
					return 408;
				end
			end
		end
	end
	return -1;
end

function isBlank(str)
	local tag = 0;
	for w in string.gmatch(str, "%s+") do
		if(string.len(w) == string.len(str)) then
			tag = 1;
		end
		break;
	end
	return tag;
end

function lookupdenyUserAgent(useragent, domain)
	for key, value in pairs(denyUserAgent) do
		if(string.lower(useragent) == string.lower(key)) then
			for n = 1, #value do
				if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
					return 1;
				end
			end
		end
	end
	return 0;
end

function lookupdenyUserAgentPrefix(useragent, domain)
	for key, value in pairs(denyUserAgentPrefix) do
		if(string.lower(string.sub(useragent, 1, string.len(key))) == string.lower(key)) then
			for n = 1, #value do
				if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
					return 1;
				end
			end
		end
	end
	return 0;
end

function isIPBlackList(clientip, domain)
	if(isBlank(clientip) == 1) then
		return 1;
	end
	if(lookupdenyIPAddress(clientip, domain) == 1) then
		return 1;
	end
	if(lookupdenyIPAddressPrefix(clientip, domain) == 1) then
		return 1;
	end
end

function lookupdenyIPAddress(clientip, domain)
	for key, value in pairs(denyIPAddress) do
		if(string.lower(clientip) == string.lower(key)) then
			for n = 1, #value do
				if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
					return 1;
				end
			end
		end
	end
	return 0;
end

function lookupdenyIPAddressPrefix(clientip, domain)
	for key, value in pairs(denyIPAddressPrefix) do
		if(string.lower(string.sub(clientip, 1, string.len(key))) == string.lower(key)) then
			for n = 1, #value do
				if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
					return 1;
				end
			end
		end
	end
	return 0;
end

function isVidBlackList(visitid, domain)
	if(isBlank(visitid) == 0 and lookupdenyVisterID(visitid, domain) == 1) then
		return 1;
	end
	return 0;
end

function lookupdenyVisterID(visitid, domain)
	for key, value in pairs(denyVisterID) do
		if(string.lower(visitid) == string.lower(key)) then
			for n = 1, #value do
				if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
					return 1;
				end
			end
		end
	end
	return 0;
end

function lookupdenyNOVisitorIDURL(rawurl, httpmethod, domain)
	for key, value in pairs(denyNOVisitorIDURL) do
		local s, e = string.find(key, ",");
		if(string.len(rawurl) >= s - 1) then
			if(string.lower(string.sub(key, 1, s - 1)) == string.lower(string.sub(rawurl, 1, s - 1)) and (string.lower(string.sub(key, s + 1, -1)) == "all" or string.lower(string.sub(key, s + 1, -1)) == string.lower(httpmethod))) then
				for n = 1, #value do
					if(string.lower(domain) == string.lower(value[n]) or string.lower(value[n]) == "all") then
						return 1;
					end
				end
			end
		end

	end
end

function isIpRateBlackList(clientip, domain)
	for key, value in pairs(denyIPAddressRate) do
		if(clientip == key) then
			if(isInArray(value["tValue"], domain) == 1 and isExpire(value["kValue"]) == 0) then
				return 1;
			end
		end
	end
	return 0;
end

function isInArray(array, domain)
	for n = 1, #array do
		if(string.lower(domain) == string.lower(array[n]) or string.lower(array[n]) == "all") then
			return 1;
		end
	end
	return 0;
end

function isExpire(data)
	local pattern = "(%d+)%-(%d+)%-(%d+)%s(%d+):(%d+):(%d+)";
	local year, month, day, hour, min, sec = data:match(pattern);
	local tmptab = {};
	tmptab.year = year;
	tmptab.month = month;
	tmptab.day = day;
	tmptab.hour = hour;
	tmptab.min = min;
	tmptab.sec = sec;
	tmptab.isdst = false;
	if(os.time(tmptab) > os.time()) then
		return 0;
	end
	return 1;
end

function lookupdenyIPVidRate(clientip, visitid, domain)
	if(clientip == nil or visitid == nil) then
		return 0;
	end
	local tmpkey = string.lower(clientip)..","..visitid;
	for key, value in pairs(denyIPVidRate) do
		if(key == tmpkey) then
			if(isInArray(value["tValue"], domain) == 1 and isExpire(value["kValue"]) == 0) then
				return 1;
			end
		end
	end
	return 0;
end

function isIpVidRateBlackList(clientip, visitid, domain)
	if(lookupdenyIPVidRate(clientip, visitid, domain) == 1) then
		return 1;
	end
	return 0;
end

function isVidRateBlackList(visitid, domain)
	if(lookupdenyvisteridrate(visitid, domain) == 1) then
		return 1;
	end
	return 0;
end

function lookupdenyvisteridrate(visitid, domain)
	for key, value in pairs(denyVisterIDRate) do
		if(key == visitid) then
			if(isInArray(value["tValue"], domain) == 1 and isExpire(value["kValue"]) == 0) then
				return 1;
			end
		end
	end
	return 0;
end
