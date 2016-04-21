function createCORSRequest(method, url) {
  var xhr = new XMLHttpRequest();
  xhr.open(method, url);
  return xhr;
}

var xHttpRequest = null;

function getSignalInDbPercent(level, minLvl) {
	var minLevel = Math.max(minLvl, 1e-6);
	var absLevel = Math.abs(level);
	var level = Math.max(minLevel, absLevel);
	var minLog = Math.abs(Math.log(minLevel));
	var levelLog = Math.log(level);

	return (levelLog + minLog) / minLog;
}

function getRgbValue(percentage, selector, useStartLevel) {
	var usedPercentage = percentage;
	if (useStartLevel) {
		usedPercentage = percentage > 0.01 ? 0.3 + 0.7 * percentage : 0;
	}
	var value = Math.max(0, Math.min(255, Math.round(usedPercentage * 255)));
	switch (selector) {
		case 1: 
			return "rgb(0," + value + ",0)";
		case 2:
			return "rgb(0,0," + value + ")";
		default:
			return "rgb(" + value + ",0,0)";
	}
}

function setMeters(levels) 
{
	var i;
	for (i = 0; i < 4; i++) {
		var subGain = levels.subGain;
		var subGainAverage = levels.subGainAverage;
		if (levels.group && levels.group[i]) {
			var group = levels.group[i];
			var prefix = "group_" + i;
			var subAvg = document.getElementById(prefix);
			var sub = document.getElementById(prefix + "_sub");
			var signal = document.getElementById(prefix + "_signal");
			var mainAvg = document.getElementById(prefix + "_main_avg");
			var main = document.getElementById(prefix + "_main");

			var signalPercentage = group.level > 2e-3 ? 0.3 + 0.7 * getSignalInDbPercent(group.level, 1e-1) : 0; 
			var subGainPercentage = 1 - getSignalInDbPercent(subGain, 0.25);
			var subAvgGainPercentage = 1 - getSignalInDbPercent(subGainAverage, 0.25);
			var mainGainPercentage = 1 - getSignalInDbPercent(group.gain, 0.25)
			var mainAvgGainPercentage = 1 - getSignalInDbPercent(group.gainAverage, 0.25);
			/*
			if (console) {
				console.log("SIG=" + signalPercentage + 
					"; SUB=" + subGainPercentage + "; AVG=" + subAvgGainPercentage + 
					"; MAIN=" + mainGainPercentage + "; AVG=" + mainAvgGainPercentage);
			}
			*/
			if (signal) {
				var limiting = Math.max(subGainPercentage,mainGainPercentage);
				var greenAttenuate = 0.2 + 0.8 * (1 - limiting);
				
				signal.style.backgroundColor = getRgbValue(greenAttenuate * signalPercentage, 1);
			}					
			if (subAvg) {
				subAvg.style.display = "block";
				subAvg.style.backgroundColor = getRgbValue(subAvgGainPercentage, 0, true);
			}
			if (sub) {
				sub.style.backgroundColor = getRgbValue(subGainPercentage, 0, true);
			}
			if (mainAvg) {
				mainAvg.style.backgroundColor = getRgbValue(mainAvgGainPercentage, 0, true);
			}
			if (main) {
				main.style.backgroundColor = getRgbValue(mainGainPercentage, 0, true);
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
				window.setTimeout(function() {sendLevelRequest(); }, 50);					
			}
			xHttpRequest.send();
		}
	//}
}

