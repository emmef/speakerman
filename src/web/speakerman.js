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

function scaledValue(value, scale) {
	var boundValue = Math.min(255, Math.max(0, typeof value === 'number' ? value : 0));
	
	return Math.round(scale * boundValue);
}

function getRgbValue(scale, r, g, b, minNonZero) {
	var boundScale = Math.min(1.0, Math.max(0.0, typeof scale === 'number' ? scale : 0));
	var threshold = (!minNonZero) ? 0 : typeof minNonZero != 'number' ? 0.25 : Math.min(0.5, Math.max(0, minNonZero));
	var usedScale = boundScale < 0.002 ? 0 : threshold + (1.0 - threshold) * boundScale;
	
	return "rgb(" 
		+ scaledValue(r, usedScale) + ","
		+ scaledValue(g, usedScale) + ","
		+ scaledValue(b, usedScale) + ")";
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
			var mainAvg = document.getElementById(prefix + "_main_avg");
			var main = document.getElementById(prefix + "_main");

			var signalPercentage = group.level > 1e-3 ? getSignalInDbPercent(group.level, 1e-2) : 0; 
			
			var subAvgGainPercentage = 1 - getSignalInDbPercent(subGainAverage, 0.25);
			var subGainPercentage = Math.max(subAvgGainPercentage, 1 - getSignalInDbPercent(subGain, 0.25));
			var mainAvgGainPercentage = 1 - getSignalInDbPercent(group.gainAverage, 0.25);
			var mainGainPercentage = Math.max(mainAvgGainPercentage, 1 - getSignalInDbPercent(group.gain, 0.25));
			var mainGainMax = Math.max(subGainPercentage, mainGainPercentage);
			
			if (subAvg) {
				if (subAvgGainPercentage > 0.01) {
					subAvg.style.backgroundColor = getRgbValue(subAvgGainPercentage, 255, 0, 64, 0.4);
				}
				else if (mainGainMax > 0.01) {
					subAvg.style.backgroundColor = "black";
				}
				else {
					subAvg.style.backgroundColor = getRgbValue(signalPercentage, 0, 255, 0, 0.2);
				}
			}
			if (sub) {
				if (subGainPercentage > 0.01) {
					sub.style.backgroundColor = getRgbValue(Math.max(subGainPercentage, subAvgGainPercentage), 255, 0, 64, 0.4);
				}
				else if (subAvgGainPercentage > 0.01) {
					sub.style.backgroundColor = getRgbValue(subAvgGainPercentage, 255, 0, 64, 0.4);
				}
				else if (mainGainMax > 0.01) {
					sub.style.backgroundColor = "black";
				}
				else {
					sub.style.backgroundColor = getRgbValue(signalPercentage, 0, 255, 0, 0.2);
				}
			}
			if (mainAvg) {
				if (mainAvgGainPercentage > 0.01) {
					mainAvg.style.backgroundColor = getRgbValue(mainAvgGainPercentage, 255, 0, 0, 0.4);
				}
				else if (mainGainMax > 0.01) {
					mainAvg.style.backgroundColor = "black";
				}
				else {
					mainAvg.style.backgroundColor = getRgbValue(signalPercentage, 0, 255, 0, 0.2);
				}
			}
			if (main) {
				if (mainGainPercentage > 0.01) {
					main.style.backgroundColor = getRgbValue(mainGainPercentage, 255, 0, 0, 0.4);
				}
				else if (mainAvgGainPercentage > 0.01) {
					main.style.backgroundColor = getRgbValue(mainAvgGainPercentage, 255, 0, 0, 0.4);
				}
				else if (mainGainMax > 0.01) {
					main.style.backgroundColor = "black";
				}
				else {
					main.style.backgroundColor = getRgbValue(signalPercentage, 0, 255, 0, 0.2);
				}
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

