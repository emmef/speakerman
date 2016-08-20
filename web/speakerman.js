function createCORSRequest(method, url) {
  var xhr = new XMLHttpRequest();
  xhr.open(method, url);
  return xhr;
}

var xHttpRequest = null;
var subOldValues = { previousValue: 0 };
var limitMinGain = 0.25;
var limitPower = 0.99;
var signalMinGain = 3e-4;
var signalPower = 2;
var minimumColorPercentage = 0.02;

function getSignalInDbPercent(level, minLvl, pow) {
	var minLevel = Math.max(minLvl, 1e-6);
	var absLevel = Math.abs(level);
	var level = Math.max(minLevel, absLevel);
	var minLog = Math.abs(Math.log(minLevel));
	var levelLog = Math.log(level);
	if (pow) {
		pow = Math.max(0.25, Math.min(4, pow));
	}
	else {
		pow = 1
	}
	var result = (levelLog + minLog) / minLog;
	if (pow < 1) {
		result = 1 - result;
	}

	return Math.pow(result, pow);
}

function scaledValue(value, scale) {
	var boundValue = Math.min(255, Math.max(0, typeof value === 'number' ? value : 0));
	
	return Math.round(scale * boundValue);
}

function getRgbValue(scale, r, g, b, minNonZero) {
	var boundScale = Math.min(1.0, Math.max(0.0, typeof scale === 'number' ? scale : 0));
	var threshold = (!minNonZero) ? 0 : typeof minNonZero != 'number' ? minimumColorPercentage : Math.min(0.5, Math.max(0, minNonZero));
	var usedScale = boundScale < 0.002 ? 0 : threshold + (1.0 - threshold) * boundScale;
	
	return "rgb(" 
		+ scaledValue(r, usedScale) + ","
		+ scaledValue(g, usedScale) + ","
		+ scaledValue(b, usedScale) + ")";
}

function integrate(element, val, perc) {
	var percentage = Math.max(0, Math.min(1, perc));
	var value = Math.max(0, Math.min(1, val));
	if (element) {
		if (element.previousValue) {
			if (value > element.previousValue) {
				element.previousValue = value;
			}
			else {
				element.previousValue = percentage * value + (1 - percentage) * element.previousValue;
			}
		}
		else {
			element.previousValue = value;
		}
		return element.previousValue;
	}
	return val;
}

function setMeters(levels) 
{
	var i;
	for (i = 0; i < 4; i++) {
		var subGainAverage = integrate(subOldValues, levels.subGainAverage, 0.1);
		var subGain = Math.min(levels.subGain, subGainAverage);
		var subAvgGainPercentage = getSignalInDbPercent(subGainAverage, limitMinGain, limitPower);
		var subGainPercentage = getSignalInDbPercent(subGain, limitMinGain, limitPower);
		
		if (levels.group && levels.group[i]) {
			var group = levels.group[i];
			var prefix = "group_" + i;
			var subAvg = document.getElementById(prefix);
			var sub = document.getElementById(prefix + "_sub");
			var mainAvg = document.getElementById(prefix + "_main_avg");
			var main = document.getElementById(prefix + "_main");
			var signal = document.getElementById(prefix + "_signal");
			
			var mainGainAverage = integrate(mainAvg, group.gainAverage, 0.1);
			var mainGain = Math.min(group.gain, mainGainAverage);
			var signalValue = integrate(signal, group.level, 0.8) * Math.min(subGainAverage, mainGainAverage);
			
			var signalPercentage = getSignalInDbPercent(signalValue, signalMinGain, signalPower); 
			
			if (subAvg) {
				subAvg.style.backgroundColor = getRgbValue(subAvgGainPercentage, 255, 0, 64, minimumColorPercentage);
			}
			if (sub) {
				sub.style.backgroundColor = getRgbValue(subGainPercentage, 255, 0, 64, minimumColorPercentage);
			}
			if (mainAvg) {
				var mainAvgGainPercentage = getSignalInDbPercent(mainGainAverage, limitMinGain, limitPower);
				mainAvg.style.backgroundColor = getRgbValue(mainAvgGainPercentage, 255, 0, 0, minimumColorPercentage);
			}
			if (main) {
				var mainGainPercentage = getSignalInDbPercent(mainGain, limitMinGain, limitPower);
				main.style.backgroundColor = getRgbValue(mainGainPercentage, 255, 0, 0, minimumColorPercentage);
			}
			if (signal) {
				signal.style.backgroundColor = getRgbValue(signalPercentage, 0, 255, 0, minimumColorPercentage);
			}
		}
		else {
			var prefix = "group_" + i;
			var group = document.getElementById(prefix);
			group.style.display = "none";
		}
	}
}

function handleRequest() 
{
	if (!xHttpRequest) {
		//console.log("No request");
	} // xHttpRequest.readyState == 4 && 
	else if (xHttpRequest && xHttpRequest.status == 200) {
		var levels = JSON.parse(xHttpRequest.responseText);
		setMeters(levels);
	}
	else {
		console.log("No correct response");
	}
}

	
function sendLevelRequest() 
{
	//try {
		if (xHttpRequest) {
			console.log("Postpone");
		}
		else {
			var url = "/levels.json";
			xHttpRequest = createCORSRequest('GET', url);
			xHttpRequest.onload = handleRequest;
			xHttpRequest.onloadend = function() {
				xHttpRequest = null;
				window.setTimeout(function() {sendLevelRequest(); }, 75);
			}
			xHttpRequest.send();
		}
	//}
}

