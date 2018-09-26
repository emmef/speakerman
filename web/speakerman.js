function createCORSRequest(method, url) {
    var xhr = new XMLHttpRequest();
    xhr.open(method, url);
    return xhr;
}

var xHttpRequest = null;
var peakMinimumSignal = 1e-3;
var averagMinimumSignal = 1e-4;
var minimumColorPercentage = 0.02;
var metergroups = undefined;

var RequestMetrics = {
    min_period: 75,
    period: 75,
    max_period: 2000,
    after_failure_period : 1000,
    period_scaling: 2,
    status_count: 0,
    min_status_count: -2,
    max_status_count: 2,
    is_busy: false,
    enter: function (success) {
        var self = RequestMetrics;
        if (self.is_busy === true) {
            return false;
        }
        self.is_busy = success;
        if (self.is_busy) {
            var jump = self.status_count < 0;
            self.status_count = self.status_count <= 0 ? 1 : self.status_count < self.max_status_count ? self.status_count + 1 : self.status_count;
            if (jump) {
                self.period = self.after_failure_period;
            }
        }
        else {
            self.status_count = self.status_count >= 0 ? -1 : self.status_count > self.min_status_count ? self.status_count - 1 : self.status_count;
        }
        if (self.status_count > 1) {
            var period = self.period;
            self.period = Math.max(self.min_period, self.period / self.period_scaling);
            if (period !== self.period) {
                console.log("Speed up refresh");
            }
        }
        else if (self.status_count <= self.min_status_count) {
            var period = self.period;
            self.period = Math.min(self.max_period, self.period * self.period_scaling);
            if (period !== self.period) {
                console.log("Slowing refresh");
            }
        }
        window.setTimeout(function () {
            self.setConnectionMessage(self.period < self.max_period)
        }, self.min_period);
        return self.is_busy;
    },
    release: function () {
        RequestMetrics.is_busy = false;
    },
    setConnectionMessage: function (connection_ok) {
        var element = document.getElementById("connection-message");
        if (element) {
            if (connection_ok) {
                element.setAttribute("style", "display:none;");
            }
            else {
                element.setAttribute("style", "display: inline;");
            }
        }
    }
};

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
    var threshold = (!minNonZero) ? 0 : typeof minNonZero !== 'number' ? minimumColorPercentage : Math.min(0.5, Math.max(0, minNonZero));
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

function ensureMeterGroup(groupElementNumber) {
    var elementName = typeof(groupElementNumber) == 'string' ? groupElementNumber : "meter-group" + groupElementNumber;
    var group = document.getElementById(elementName);
    if (!group) {
        console.log("No group " + "meter-group" + groupElementNumber + " found");
        return null;
    }
    var elem = group.getElementsByClassName("meter-level-pixel");
    var titleElementList = group.getElementsByClassName("meter-title");
    if (elem && elem.length && elem.length > 0) {
        return {
            mainElement: group,
            titleElement:titleElementList && titleElementList.length > 0 ? titleElementList[0] : null,
            groupNumber: groupElementNumber,
            elements: elem,
            integratedPeak: 0,
            getLevelIndex: function (value, minimum) {
                if (!value || !this.elements || !this.elements.length) {
                    return 0;
                }
                var abs = Math.abs(value);
                if (value >= 1.0) {
                    return this.elements.length;
                }
                var percentage = Math.round(this.elements.length * Math.min(1, Math.max(0, getSignalInDbPercent(value, minimum))));

                return percentage;
            },
            getGainIndex: function (value, minimum) {
                if (!value) {
                    return this.elements.length;
                }
                var abs = Math.abs(value);
                if (value >= 0.99) {
                    return this.elements.length;
                }
                var percentage = Math.min(1, Math.max(0, getSignalInDbPercent(value, minimum)));

                return Math.round(percentage * this.elements.length);
            },
            setValues: function (level, sub, name) {
                var peak = 1.0 / Math.max(1.0, level);
                this.integratedPeak = 0.1 * Math.sqrt(level) + 0.9 * this.integratedPeak;
                var average = 1.0 / Math.max(1.0, this.integratedPeak);

                var averageIndex = Math.round(this.elements.length * average);
                var peakIndex = Math.round(this.elements.length * peak);

                for (i = 0; i < this.elements.length; i++) {
                    var r = 0;
                    var g = 0;
                    var b = sub ? 64 : 0;
                    if (i == peakIndex) {
                        r = 255;
                        g = 255;
                        b = 255;
                    }
                    else if (i >= averageIndex) {
                        r = 255;
                    }
                    else if (i >= peakIndex) {
                        r = 144;
                        g = 128;
                    }
                    this.elements[i].style.backgroundColor = "rgb(" + r + "," + g + "," + b + ")";
                }
                if (!sub && this.titleElement) {
                   this.titleElement.innerText = name ? name : "Group " + (1 + this.groupNumber);
                }
            },
            hide: function () {
                this.mainElement.style.visibility = "hidden";
                this.mainElement.style.display = "none";
            }
        };
    }
    return null;
}

function ensureMeterGroups() {
    if (metergroups) {
        return metergroups;
    }
    var subMeters = ensureMeterGroup("meter-sub");
    if (!subMeters) {
        return;
    }
    var groups = [];
    for (i = 0; i < 4; i++) {
        var groupMeters = ensureMeterGroup(i);
        if (groupMeters) {
            groups[i] = groupMeters;
        }
    }

    metergroups = {};
    metergroups.subMeters = subMeters;
    metergroups.groups = groups;
    metergroups.mixMode = "DEF";
    metergroups.mixModeDefault = document.getElementById("mix-mode-default");
    metergroups.mixModeAll = document.getElementById("mix-mode-all");
    metergroups.mixModeOwn = document.getElementById("mix-mode-own");
    metergroups.mixModeFirst = document.getElementById("mix-mode-first");

    return metergroups;
}

function setMeters(levels) {
    var meterGroups = ensureMeterGroups();
    if (!meterGroups) {
        return;
    }
    meterGroups.subMeters.setValues(levels.subLevel, true);
    var groupCount = levels.group && levels.group.length ? levels.group.length : 0;
    var i = 0;
    for (i = 0; i < groupCount; i++) {
        var grp = levels.group[i];
        meterGroups.groups[i].setValues(grp.level, false, grp.group_name);
    }
    for (; i < meterGroups.groups.length; i++) {
        meterGroups.groups[i].hide();
    }
    var element = document.getElementById("threshold-message");
    if (element) {
        var level;
        if (levels.thresholdScale && levels.thresholdScale > 1.0) {
            level = "+" + Math.round(200 * Math.log(levels.thresholdScale) / Math.log(10)) / 10;
        }
        else {
            level = "0";
        }
        element.innerHTML = level + " dB";
    }
    if (levels.mixMode) {
        var selected = null;
        switch (levels.mixMode) {
            case "all" :
                selected = meterGroups.mixModeAll;
                break;
            case "own" :
                selected = meterGroups.mixModeOwn;
                break;
            case "first" :
                selected = meterGroups.mixModeFirst;
                break;
            default:
                selected = meterGroups.mixModeDefault;
        }
        var modeElements = [meterGroups.mixModeAll, meterGroups.mixModeOwn, meterGroups.mixModeDefault, meterGroups.mixModeFirst];
        for (var i = 0; i < modeElements.length; i++) {
            modeElements[i].setAttribute("class", modeElements[i] == selected ? "mix-mode-message-enabled" : "mix-mode-message-disabled");
        }
    }
}

function setMixMode(newMode) {
    var meterGroups = ensureMeterGroups()
    var url = null;
    switch (newMode) {
        case "own" :
            url = "/mix-mode-own";
            break;
        case "all":
            url = "/mix-mode-all";
            break;
        case "first":
            url = "/mix-mode-first";
            break;
        default:
            url = "/mix-mode-default"
    }
    var request = createCORSRequest('PUT', url);
    try {
        request.send();
    }
    catch (exception) {
        console.log("Set mix mode request failed")
    }
}

function handleRequest() {
    if (xHttpRequest && xHttpRequest.status == 200) {
        if (RequestMetrics.enter(true)) {
            var levels = JSON.parse(xHttpRequest.responseText);
            setMeters(levels);
            RequestMetrics.release();
        }
    }
    else {
        RequestMetrics.enter(false);
    }
}


function sendLevelRequest() {
    if (xHttpRequest) {
        console.log("Postpone");
    }
    else {
        var url = "/levels.json";
        xHttpRequest = createCORSRequest('GET', url);
        try {
            xHttpRequest.onload = handleRequest;
            xHttpRequest.onloadend = function () {
                xHttpRequest = null;
                window.setTimeout(function () {
                    sendLevelRequest();
                }, RequestMetrics.period);
            };
            xHttpRequest.onerror = function () {
                RequestMetrics.enter(false);
                return true;
            };
            xHttpRequest.send();
        }
        catch (exception) {
            console.log("Something went wrong!");
            RequestMetrics.enter(false);
        }
    }
}

