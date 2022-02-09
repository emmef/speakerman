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
    after_failure_period: 1000,
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
        } else {
            self.status_count = self.status_count >= 0 ? -1 : self.status_count > self.min_status_count ? self.status_count - 1 : self.status_count;
        }
        if (self.status_count > 1) {
            var period = self.period;
            self.period = Math.max(self.min_period, self.period / self.period_scaling);
            if (period !== self.period) {
                console.log("Speed up refresh");
            }
        } else if (self.status_count <= self.min_status_count) {
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
            } else {
                element.setAttribute("style", "display: inline;");
            }
        }
    }
};

class Volume {
    static minimumVolume = 0.1;
    static maximumVolume = 10;
    volume = 0;
    isMuted = false;

    static determineMuted(volume) {
        return Math.abs(volume) < 1e-6;
    }

    static getClampedVolume(volume) {
        if (volume < Volume.minimumVolume) {
            return Volume.minimumVolume
        } else if (volume > Volume.maximumVolume) {
            return Volume.maximumVolume;
        }
        return volume;
    }

    static toDecibels(volume) {
        return Math.round(Math.log10(Volume.getClampedVolume(volume)) * 200) / 10;
    }


    constructor(initialVolume) {
        this.isMuted = Volume.determineMuted(initialVolume);
        this.volume = this.isMuted ? 1.0 : Volume.getClampedVolume(initialVolume);
    }

    getDecibelValue() {
        return Volume.toDecibels(this.volume);
    }

    getValue() {
        return this.isMuted ? 0 : this.volume;
    }

    setVolume(newVolume) {
        this.isMuted = Volume.determineMuted(newVolume);
        if (!this.isMuted) {
            this.volume = Volume.getClampedVolume(newVolume);
            return true;
        }
        return false;
    }

    setVolumeOnly(newVolume) {
        this.volume = Volume.getClampedVolume(newVolume);
    }

    setDecibelsOnly(decibels) {
        let myDecibels = this.getDecibelValue();
        if (Math.abs(myDecibels - decibels) > 0.05) {
            this.setVolumeOnly(Math.pow(10, decibels / 20));
            return true;
        }
        return false;
    }
}

function handleClassNameAddOrReplace(element, classToRemove, classToAdd) {
    element.classList.remove(classToRemove);
    element.classList.add(classToAdd);
}

function findUniqueByClass(element, className) {
    let elements = element.getElementsByClassName(className);
    if (elements.length == 0) {
        throw "Could not find element by class-name \"" + className + "\"";
    }
    if (elements.length > 1) {
        throw "Could not find UNIQUE element by class-name \"" + className + "\"";
    }
    return elements.item(0);
}

class LogicalInputElement {
    controller = undefined;
    index = undefined;
    volume = undefined;
    volumeValue = undefined;
    previousVolumeValue = undefined;
    label = undefined;
    muted = false;
    element = {
        group: undefined,
        mute: undefined,
        label: undefined,
        volume: undefined,
        slider: undefined,
        setMuted: function (value) {
            if (value) {
                handleClassNameAddOrReplace(this.group, "logical-input-unmuted", "logical-input-muted");
                handleClassNameAddOrReplace(this.mute, "logical-input-mute-disabled", "logical-input-mute-enabled");
            } else {
                handleClassNameAddOrReplace(this.group, "logical-input-muted", "logical-input-unmuted");
                handleClassNameAddOrReplace(this.mute, "logical-input-mute-enabled", "logical-input-mute-disabled");
            }
        }
    };

    constructor(inputController, element, index, logicalInput) {
        this.controller = inputController;
        this.index = index;
        this.volume = new Volume(logicalInput.volume);
        this.volumeValue = this.volume.getValue();
        this.previousVolumeValue = this.volumeValue;
        this.label = logicalInput.name;

        this.element.group = element;
        this.element.label = findUniqueByClass(element, "logical-input-label");
        this.element.mute = findUniqueByClass(element, "logical-input-mute");
        this.element.slider = findUniqueByClass(element, "logical-input-slider");
        this.element.volume = findUniqueByClass(element, "logical-input-volume");

        this.element.label.innerHTML = this.label;
        this.element.slider.setAttribute("min", Volume.toDecibels(Volume.minimumVolume));
        this.element.slider.setAttribute("max", Volume.toDecibels(Volume.maximumVolume));
        this.element.slider.setAttribute("value", this.volume.getDecibelValue());
        this.element.volume.innerHTML = "" + this.volume.getDecibelValue();
        this.element.setMuted(this.volume.isMuted);
        let self = this;
        this.element.mute.addEventListener("click", function () {
            self.toggleMute();
        });
        this.element.slider.addEventListener("input", function () {
            self.setSliderVolume();
        });
    }

    update(logicalInput) {
        if (logicalInput && typeof logicalInput.volume == 'number') {
            let nameChanged = logicalInput.name && logicalInput.name != this.label;
            let volumeChanged = this.previousVolumeValue != logicalInput.volume;
            if (!volumeChanged && !nameChanged) {
                return;
            }
            console.info("Server-side volume change: " + JSON.stringify(logicalInput));
            if (nameChanged) {
                this.element.label.innerHTML = logicalInput.name;
                this.label = logicalInput.name;
            }
            let wasMuted = this.volume.isMuted;
            if (this.volume.setVolume(logicalInput.volume)) {
                this.element.slider.value = this.volume.getDecibelValue();
                this.element.volume.innerHTML = "" + this.volume.getDecibelValue();
            }
            if (this.volume.isMuted != wasMuted) {
                this.element.setMuted(this.volume.isMuted);
            }
            this.volumeValue = this.volume.getValue();
            this.previousVolumeValue = logicalInput.volume;
        }
    }

    toggleMute() {
        let isMuted = !this.volume.isMuted;
        this.volume.isMuted = isMuted;
        this.element.setMuted(isMuted);
        this.controller.setNewVolume(this.index, this.volume.getValue());
    }

    setSliderVolume() {
        if (this.volume.setDecibelsOnly(this.element.slider.value)) {
            let newVolume = this.volume.getValue();
            if (newVolume != this.volumeValue) {
                this.controller.setNewVolume(this.index, newVolume);
                this.volumeValue = newVolume;
            }
            this.element.volume.innerHTML = "" + this.volume.getDecibelValue();
        }
    }
}

class LogicalInputsController {
    static UPDATE_INTERVAL = 150;
    inputGroupControlElements = undefined;
    initialLogicalInputs = undefined;
    userChangeTimeStamp = undefined;
    logicalInputCache = undefined;
    unsetNewValues = undefined;

    constructor(groupsRootElement, logicalInputs) {
        this.initialLogicalInputs = logicalInputs;
        this.inputGroupControlElements = new Array(logicalInputs.length);

        for (let i = 0; i < logicalInputs.length; i++) {
            let element = document.getElementById("logicalInput" + i);
            if (element && element.parentElement == groupsRootElement) {
                this.inputGroupControlElements[i] = new LogicalInputElement(this, element, i, logicalInputs[i]);
                element.style.display = 'block';
            }
        }
        let self = this;
        window.setTimeout(function() {
            LogicalInputsController.subMitUnsetValuesStatic(self);
        }, LogicalInputsController.UPDATE_INTERVAL);

        let resetButton = document.getElementById("reset-button");
        if (resetButton) {
            resetButton.addEventListener("click", function () { LogicalInputsController.submitReset(); });
        }
    }

    static subMitUnsetValuesStatic(self) {
        self.submitUnsetValues();
        window.setTimeout(function() { LogicalInputsController.subMitUnsetValuesStatic(self); }, LogicalInputsController.UPDATE_INTERVAL)
    }

    static validInputVolumes(logicalVolumes){
        if (!(logicalVolumes && logicalVolumes.length && typeof(logicalVolumes.length) == 'number' && logicalVolumes.length > 0)) {
            return false;
        }
        for (let i = 0; i < logicalVolumes.length; i++) {
            if (typeof (logicalVolumes[i].volume) != 'number') {
                return false;
            }
        }
        return true;
    }

    static copyLogicalInput(logicalVolumes, overrideIndex, overrideVolume) {
        if (!LogicalInputsController.validInputVolumes(logicalVolumes)) {
            return null;
        }
        let validOverrides = (typeof overrideIndex == 'number' && typeof overrideVolume == 'number');
        let overrider = validOverrides ? {
            index: overrideIndex,
            volume: overrideVolume
        } : {index: -1, volume: 0};

        let result = new Array();
        for (let i = 0; i < logicalVolumes.length; i++) {
            if (i == overrider.index) {
                result.push({volume: overrider.volume });
            }
            else {
                result.push({volume: logicalVolumes[i].volume});
            }
        }

        return result;
    }

    static submitVolumes(logicalVolumes) {
        if (LogicalInputsController.validInputVolumes(logicalVolumes)) {
            let output = { logicalInput : logicalVolumes };
            console.debug("Submitting: " + JSON.stringify(output));
            try {
                fetch('/config', {
                    method: "PUT",
                    header: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(output)
                });
                return true;
            } catch (e) {
                log.error("Error sending volume data: " + e.description);
            }
        }
        return false;
    }

    static submitReset() {
        console.info("Reset: requesting reload of server-side configuration.");
        let output = { reload: true };
        try {
            fetch('/config', {
                method: "PUT",
                header: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(output)
            });
            return true;
        } catch (e) {
            log.error("Error sending volume data: " + e.description);
        }
    }

    static copyLogicalInputVolumes(target, source) {
        if (!LogicalInputsController.validInputVolumes(target) || !LogicalInputsController.validInputVolumes(source)) {
            return false;
        }
        if (target.length != source.length) {
            return false;
        }
        for (let i = 0; i < target.length; i++) {
            target[i].volume = source[i].volume;
        }
        return true;
    }

    submitUnsetValues() {
        let volumes = this.unsetNewValues;
        if (volumes) {
            this.unsetNewValues = null;
            LogicalInputsController.submitVolumes(volumes);
        }
    }

    update(logicalInputs) {
        if (logicalInputs && logicalInputs.length > 0) {
            if (!this.userChangeTimeStamp || Date.now() - this.userChangeTimeStamp > 1000) {
                let length = Math.min(logicalInputs.length, this.initialLogicalInputs.length);
                for (let i = 0; i < length; i++) {
                    this.inputGroupControlElements[i].update(logicalInputs[i]);
                }
                this.logicalInputCache = logicalInputs;
            } else if (!this.logicalInputCache) {
                this.logicalInputCache = logicalInputs;
            }
        }
    }

    setNewVolume(index, volume) {
        if (!(
            this.logicalInputCache && this.logicalInputCache.length &&
            typeof index == 'number' && typeof volume == 'number' &&
            this.logicalInputCache.length > index)) {
            return;
        }

        this.unsetNewValues = LogicalInputsController.copyLogicalInput(this.logicalInputCache, index, volume);
        let now = Date.now();
        let interval = now - this.userChangeTimeStamp;
        this.userChangeTimeStamp = now;
        if (interval < LogicalInputsController.UPDATE_INTERVAL) {
            return;
        }
        let setValues = this.unsetNewValues;
        this.unsetNewValues = null;
        LogicalInputsController.copyLogicalInputVolumes(this.logicalInputCache, setValues);
        LogicalInputsController.submitVolumes(setValues);
    }
}

var logicalInputs = {
    logicalGroupElements: undefined,

    update: function (logicalInputsUpdate) {
        if (typeof this.logicalGroupElements == 'object') {
            this.logicalGroupElements.update(logicalInputsUpdate);
        } else if (typeof this.logicalGroupElements == 'boolean') {
            // Not initialized.
            return;
        } else if (logicalInputsUpdate && logicalInputsUpdate.length > 0) {
            let groupsRootElement = document.getElementById("logicalInputs");
            if (!groupsRootElement) {
                this.logicalGroupElements = false;
                console.error("Missing root element for logical input groups");
                return;
            }
            try {
                this.logicalGroupElements = new LogicalInputsController(groupsRootElement, logicalInputsUpdate);
            } catch (e) {
                console.error("Error when initializing logical input groups!" + e.description);
                this.logicalGroupElements = false;
            }
        } else {
            this.logicalGroupElements = false;
            console.error("No logical input groups defined.");
        }
    }
}

function getSignalInDbPercent(level, minLvl, pow) {
    var minLevel = Math.max(minLvl, 1e-6);
    var absLevel = Math.abs(level);
    var level = Math.max(minLevel, absLevel);
    var minLog = Math.abs(Math.log(minLevel));
    var levelLog = Math.log(level);
    if (pow) {
        pow = Math.max(0.25, Math.min(4, pow));
    } else {
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
            } else {
                element.previousValue = percentage * value + (1 - percentage) * element.previousValue;
            }
        } else {
            element.previousValue = value;
        }
        return element.previousValue;
    }
    return val;
}

function ensureMeterGroup(groupElementNumber) {
    var elementName = typeof (groupElementNumber) == 'string' ? groupElementNumber : "meter-group" + groupElementNumber;
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
            titleElement: titleElementList && titleElementList.length > 0 ? titleElementList[0] : null,
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
                this.mainElement.style.display = "block";
                this.mainElement.style.visibility = "visible";
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
                    } else if (i >= averageIndex) {
                        r = 255;
                    } else if (i >= peakIndex) {
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
    if (levels && levels.logicalInput) {
        logicalInputs.update(levels.logicalInput);
    }
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
        } else {
            level = "0";
        }
        element.innerHTML = level + " dB";
    }
    var cpuElement = document.getElementById("cpu-usage-stats");
    if (cpuElement) {
        let now = Date.now();
        if (!meterGroups.lastCpuStamp || (now - meterGroups.lastCpuStamp > 1000)) {
            var message = "CPU: ";
            message += Math.round(levels.cpuShortTerm * 10) / 10;
            message += "%";
            cpuElement.innerText = message;
            meterGroups.lastCpuStamp = now;
        }
    }
}

function handleRequest() {
    if (xHttpRequest && xHttpRequest.status == 200) {
        if (RequestMetrics.enter(true)) {
            var levels = JSON.parse(xHttpRequest.responseText);
            setMeters(levels);
            RequestMetrics.release();
        }
    } else {
        RequestMetrics.enter(false);
    }
}


function sendLevelRequest() {
    if (xHttpRequest) {
        console.log("Postpone");
        return;
    }
    var url = "/levels";
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
    } catch (exception) {
        console.log("Something went wrong!");
        RequestMetrics.enter(false);
    }
}

