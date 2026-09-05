/*
 * Modifications Copyright 2026 CloudAXS.
 * Original upstream portions remain licensed under Apache-2.0.
 */
(function () {
    function createElements() {
        return {
            addPopupButton: document.getElementById("add-popup"),
            commandDeviceSelect: document.querySelector("#help-page #device-select"),
            commandInput: document.getElementById("command-input"),
            deviceList: document.getElementById("device-list"),
            backupFileInput: document.getElementById("backup-file"),
            backupUploadButton: document.getElementById("upload-backup"),
            downloadBackupButton: document.getElementById("download-backup"),
            devicesFileInput: document.getElementById("devices-file"),
            devicesUploadButton: document.getElementById("upload-devices"),
            downloadDevicesButton: document.getElementById("download-devices"),
            downloadRemotesButton: document.getElementById("download-remotes"),
            filesystemFileInput: document.getElementById("filesystem-file"),
            filesystemUploadButton: document.getElementById("upload-filesystem"),
            firmwareFileInput: document.getElementById("firmware-file"),
            firmwareUploadButton: document.getElementById("upload-firmware"),
            helpDeviceButton: document.getElementById("help-device"),
            helpRemoteButton: document.getElementById("help-remote"),
            lastAddrInput: document.getElementById("last-address"),
            mqttEnabledInput: document.getElementById("mqtt-enabled"),
            mqttDiscoveryInput: document.getElementById("mqtt-discovery"),
            mqttPasswordInput: document.getElementById("mqtt-password"),
            mqttPortInput: document.getElementById("mqtt-port"),
            mqttServerInput: document.getElementById("mqtt-server"),
            mqttUpdateButton: document.getElementById("mqtt-update"),
            mqttUserInput: document.getElementById("mqtt-user"),
            esphomeEnabledInput: document.getElementById("esphome-enabled"),
            esphomeNameInput: document.getElementById("esphome-name"),
            esphomePasswordInput: document.getElementById("esphome-password"),
            esphomeUpdateButton: document.getElementById("esphome-update"),
            esphomeStatus: document.getElementById("esphome-status"),
            wifiSsidInput: document.getElementById("wifi-ssid"),
            wifiPasswordInput: document.getElementById("wifi-password"),
            wifiScanButton: document.getElementById("wifi-scan-btn"),
            wifiScanResults: document.getElementById("wifi-scan-results"),
            wifiConfigSaveButton: document.getElementById("wifi-config-save"),
            wifiConfigStatus: document.getElementById("wifi-config-status"),
            networkHostnameInput: document.getElementById("net-hostname"),
            networkDhcpInput: document.getElementById("net-dhcp"),
            networkIpInput: document.getElementById("net-ip"),
            networkMaskInput: document.getElementById("net-mask"),
            networkGatewayInput: document.getElementById("net-gateway"),
            networkDns1Input: document.getElementById("net-dns1"),
            networkDns2Input: document.getElementById("net-dns2"),
            networkSntpInput: document.getElementById("net-sntp"),
            networkTzSelect: document.getElementById("net-tz-select"),
            networkTzCustomWrap: document.getElementById("net-tz-custom-wrap"),
            networkTzInput: document.getElementById("net-tz"),
            networkTimeInput: document.getElementById("net-time"),
            networkStatus: document.getElementById("network-status"),
            networkSaveButton: document.getElementById("network-save"),
            fallbackEnabledInput: document.getElementById("fallback-enabled"),
            fallbackRetriesBootInput: document.getElementById("fallback-retries-boot"),
            fallbackRetriesRunningInput: document.getElementById("fallback-retries-running"),
            fallbackTimeoutInput: document.getElementById("fallback-timeout"),
            fallbackStatus: document.getElementById("fallback-status"),
            fallbackSaveButton: document.getElementById("fallback-save"),
            displayEnabledInput: document.getElementById("display-enabled"),
            displayScreensaverTimeoutInput: document.getElementById("display-screensaver-timeout"),
            displayOffTimeoutInput: document.getElementById("display-off-timeout"),
            displayDimLevelSelect: document.getElementById("display-dim-level"),
            displayCpuTempInput: document.getElementById("display-cpu-temp"),
            displayUpdateButton: document.getElementById("display-update"),
            displayStatus: document.getElementById("display-status"),
            syslogEnabledInput: document.getElementById("syslog-enabled"),
            syslogServerInput: document.getElementById("syslog-server"),
            syslogPortInput: document.getElementById("syslog-port"),
            syslogTagInput: document.getElementById("syslog-tag"),
            syslogUpdateButton: document.getElementById("syslog-update"),
            syslogTestButton: document.getElementById("syslog-test"),
            remotePopupButton: document.getElementById("remote-popup"),
            remotesFileInput: document.getElementById("remotes-file"),
            remotesUploadButton: document.getElementById("upload-remotes"),
            sendCommandButton: document.getElementById("send-command-button"),
            statusMessages: document.getElementById("status-messages"),
            suggestions: document.getElementById("suggestions"),
            themeToggle: document.getElementById("toggle-theme"),
            selectLog: document.getElementById("select-log"),
            clearLogButton: document.getElementById("clear-log-button"),
            copyLogButton: document.getElementById("copy-log-button"),
            copyAddressButton: document.getElementById("copy-address-button"),
            useAddressRemoteButton: document.getElementById("use-address-remote-button"),
            clearAddressButton: document.getElementById("clear-address-button"),
            lastAddressBadge: document.getElementById("last-address-badge"),
            lastAddressProto: document.getElementById("last-address-proto"),
            lastAddressAction: document.getElementById("last-address-action"),
            lastAddressTime: document.getElementById("last-address-time"),
            logStatusDot: document.getElementById("log-status-dot"),
            logStatusText: document.getElementById("log-status-text"),
            logCount: document.getElementById("log-count")
        };
    }

    function i18nText(key, fallback) {
        if (typeof window.t === "function") {
            var value = window.t(key);
            if (value && value !== key) {
                return value;
            }
        }
        return fallback || key;
    }

    function updateLogCount(app) {
        if (app.elements.logCount && app.elements.statusMessages) {
            var count = app.elements.statusMessages.children.length;
            var tmpl = i18nText("log.messages_count", "{count} messages");
            app.elements.logCount.textContent = tmpl.replace("{count}", count);
        }
    }

    function applyLogFilter(app) {
        if (!app.elements.statusMessages || !app.elements.selectLog) return;
        var filter = app.elements.selectLog.value;
        var children = app.elements.statusMessages.children;
        for (var i = 0; i < children.length; i++) {
            var p = children[i];
            var lvl = p.dataset.level || "info";
            if (filter === "all") {
                p.style.display = "";
            } else if (filter === "error") {
                p.style.display = lvl === "error" ? "" : "none";
            } else if (filter === "warning") {
                p.style.display = (lvl === "error" || lvl === "warning") ? "" : "none";
            } else if (filter === "info") {
                p.style.display = "";
            }
        }
    }

    function renderLastAddressTime(app) {
        if (!app.elements.lastAddressTime) return;
        var receivedAt = app.state.lastAddressReceivedAt;
        if (!receivedAt) {
            app.elements.lastAddressTime.textContent = i18nText("label.no_signal_yet", "No signal received yet");
            app.elements.lastAddressTime.classList.remove("time-live");
            return;
        }

        var diffSec = Math.max(0, Math.floor((Date.now() - receivedAt) / 1000));
        var date = new Date(receivedAt);
        var timeStr = date.toTimeString().split(" ")[0];

        var label = "";
        if (diffSec < 5) {
            label = i18nText("label.received_just_now", "Received: Just now") + " (" + timeStr + ")";
            app.elements.lastAddressTime.classList.add("time-live");
        } else if (diffSec < 60) {
            label = i18nText("label.received_ago_sec", "Received: {sec}s ago").replace("{sec}", diffSec) + " (" + timeStr + ")";
            app.elements.lastAddressTime.classList.add("time-live");
        } else if (diffSec < 3600) {
            var min = Math.floor(diffSec / 60);
            label = i18nText("label.received_ago_min", "Received: {min}m ago").replace("{min}", min) + " (" + timeStr + ")";
            app.elements.lastAddressTime.classList.remove("time-live");
        } else {
            var hours = Math.floor(diffSec / 3600);
            label = i18nText("label.received_ago_hours", "Received: > 1 hour ago") + " (" + timeStr + ")";
            app.elements.lastAddressTime.classList.remove("time-live");
        }
        app.elements.lastAddressTime.textContent = label;
    }

    function updateLastAddressBadge(app, address, action, protocol, secondsAgo, isLive) {
        var addr = (address || "").trim().toUpperCase();
        var isBlank = !addr || addr === "------" || addr === "000000";

        if (app.elements.lastAddrInput) {
            app.elements.lastAddrInput.value = isBlank ? "" : addr;
        }

        if (app.elements.lastAddressBadge) {
            app.elements.lastAddressBadge.textContent = isBlank ? "------" : addr;
            if (isLive && !isBlank) {
                app.elements.lastAddressBadge.classList.remove("badge-pulse");
                void app.elements.lastAddressBadge.offsetWidth;
                app.elements.lastAddressBadge.classList.add("badge-pulse");
            }
        }

        if (isBlank) {
            app.state.lastAddressReceivedAt = null;
            if (app.elements.lastAddressProto) app.elements.lastAddressProto.style.display = "none";
            if (app.elements.lastAddressAction) app.elements.lastAddressAction.style.display = "none";
            renderLastAddressTime(app);
            return;
        }

        if (app.elements.lastAddressProto) {
            if (protocol) {
                app.elements.lastAddressProto.textContent = protocol;
                app.elements.lastAddressProto.style.display = "";
            } else if (!app.elements.lastAddressProto.textContent) {
                app.elements.lastAddressProto.style.display = "none";
            }
        }

        if (app.elements.lastAddressAction) {
            if (action && action !== "-" && action !== "unknown") {
                app.elements.lastAddressAction.textContent = action;
                app.elements.lastAddressAction.className = "rf-action-badge action-" + action.toLowerCase();
                app.elements.lastAddressAction.style.display = "";
            } else if (!isLive && !action) {
                app.elements.lastAddressAction.style.display = "none";
            }
        }

        if (isLive) {
            app.state.lastAddressReceivedAt = Date.now();
        } else if (typeof secondsAgo !== "undefined" && secondsAgo !== null && secondsAgo >= 0) {
            app.state.lastAddressReceivedAt = Date.now() - (secondsAgo * 1000);
        }

        renderLastAddressTime(app);

        if (!app.state.lastAddressTimer) {
            app.state.lastAddressTimer = setInterval(function () {
                renderLastAddressTime(app);
            }, 5000);
        }
    }

    function logStatus(app, message, isError) {
        if (!app.elements.statusMessages || !message) {
            return;
        }

        var logEntry = document.createElement("p");
        logEntry.textContent = message;

        var lowerMsg = message.toLowerCase();
        var level = "info";
        if (isError || lowerMsg.indexOf("[e]") !== -1 || lowerMsg.indexOf("error") !== -1 || lowerMsg.indexOf("failed") !== -1) {
            level = "error";
            logEntry.classList.add("log-error");
        } else if (lowerMsg.indexOf("[w]") !== -1 || lowerMsg.indexOf("warning") !== -1 || lowerMsg.indexOf("rejected") !== -1) {
            level = "warning";
            logEntry.classList.add("log-warning");
        } else if (lowerMsg.indexOf("[i]") !== -1 || lowerMsg.indexOf("info") !== -1) {
            level = "info";
            logEntry.classList.add("log-info");
        }
        logEntry.dataset.level = level;

        var currentFilter = app.elements.selectLog ? app.elements.selectLog.value : "all";
        if (currentFilter === "error" && level !== "error") {
            logEntry.style.display = "none";
        } else if (currentFilter === "warning" && level !== "error" && level !== "warning") {
            logEntry.style.display = "none";
        }

        app.elements.statusMessages.appendChild(logEntry);
        app.elements.statusMessages.scrollTop = app.elements.statusMessages.scrollHeight;
        while (app.elements.statusMessages.children.length > 300) {
            app.elements.statusMessages.removeChild(app.elements.statusMessages.firstChild);
        }
        updateLogCount(app);

        if (typeof app.onLogMessage === "function") {
            try {
                app.onLogMessage(message, isError);
            } catch (e) {}
        }
    }

    async function loadLogBuffer(app) {
        if (!app.elements.statusMessages || !window.OmniIoApi) {
            return;
        }
        try {
            var logs = await window.OmniIoApi.requestJson("/api/logs");
            app.elements.statusMessages.textContent = "";
            if (Array.isArray(logs)) {
                logs.forEach(function (message) {
                    logStatus(app, message);
                });
            }
            updateLogCount(app);
        } catch (error) {
            logStatus(app, "Could not load log buffer", true);
        }
    }

    function initSuggestions(app) {
        var suggestions = ["add", "remove", "close", "open", "ls", "cat"];
        app.elements.suggestions.textContent = "";

        suggestions.forEach(function (item) {
            var option = document.createElement("option");
            option.value = item;
            option.textContent = item;
            app.elements.suggestions.appendChild(option);
        });

        app.elements.suggestions.addEventListener("change", function () {
            if (!app.elements.suggestions.value) {
                return;
            }

            if (app.elements.commandInput.value !== "" &&
                !app.elements.commandInput.value.endsWith(" ")) {
                app.elements.commandInput.value += " ";
            }

            app.elements.commandInput.value += app.elements.suggestions.value + " ";
            app.elements.commandInput.focus();
            app.elements.suggestions.selectedIndex = 0;
        });
    }

    function initTheme(app) {
        var savedTheme = localStorage.getItem("theme");
        if (savedTheme === "dark") {
            document.body.classList.add("dark-mode");
        }

        if (app.elements.themeToggle) {
            app.elements.themeToggle.addEventListener("click", function () {
                document.body.classList.toggle("dark-mode");
                localStorage.setItem(
                    "theme",
                    document.body.classList.contains("dark-mode") ? "dark" : "light"
                );
            });
        }
    }

    function initHelpButtons(app) {
        if (app.elements.helpDeviceButton) {
            app.elements.helpDeviceButton.addEventListener("click", function () {
                app.openPopup(
                    app.i18nText("popup.help_title", "Help"),
                    app.i18nText("popup.help_device", "help device"),
                    [],
                    app.i18nText("help.device", "No help text").split("\n"),
                    { showSave: false }
                );
            });
        }

        if (app.elements.helpRemoteButton) {
            app.elements.helpRemoteButton.addEventListener("click", function () {
                app.openPopup(
                    app.i18nText("popup.help_title", "Help"),
                    app.i18nText("popup.help_remote", "help remote"),
                    [],
                    app.i18nText("help.remote", "No help text").split("\n"),
                    { showSave: false }
                );
            });
        }
    }

    function updateMqttStatus(app, statusData) {
        var dot = document.getElementById("mqtt-indicator-dot");
        var pill = document.getElementById("mqtt-status-pill");
        var text = document.getElementById("mqtt-status-text");
        if (!dot || !pill) return;

        var isConnected = !!(statusData && statusData.connected);
        var isEnabled = statusData && typeof statusData.enabled !== "undefined" ? !!statusData.enabled : isConnected;
        var state = statusData && statusData.state ? statusData.state : (isConnected ? "connected" : (isEnabled ? "disconnected" : "disabled"));

        dot.className = "mqtt-dot " + state;
        if (!isEnabled) {
            dot.className = "mqtt-dot disabled";
            pill.title = app.i18nText("mqtt.disabled", "MQTT: Disabled");
        } else if (isConnected) {
            dot.className = "mqtt-dot connected";
            pill.title = app.i18nText("mqtt.connected", "MQTT: Connected");
        } else if (state === "connecting") {
            dot.className = "mqtt-dot connecting";
            pill.title = app.i18nText("mqtt.connecting", "MQTT: Connecting...");
        } else {
            dot.className = "mqtt-dot disconnected";
            pill.title = app.i18nText("mqtt.disconnected", "MQTT: Disconnected");
        }
    }

    function updateEspHomeStatus(app, statusData) {
        var dot = document.getElementById("esphome-indicator-dot");
        var pill = document.getElementById("esphome-status-pill");
        var text = document.getElementById("esphome-status-text");
        if (!dot || !pill) return;

        var isEnabled = statusData && typeof statusData.enabled !== "undefined" ? !!statusData.enabled : true;
        var isRunning = statusData && typeof statusData.running !== "undefined" ? !!statusData.running : false;
        var clients = statusData && typeof statusData.clients !== "undefined" ? Number(statusData.clients) : 0;
        var isConnected = clients > 0;

        if (!isEnabled) {
            dot.className = "mqtt-dot disabled";
            pill.title = "ESPHome: " + app.i18nText("status.esphome_stopped", "Disabled");
        } else if (isConnected) {
            dot.className = "mqtt-dot connected";
            pill.title = "ESPHome: " + app.i18nText("mqtt.connected", "Connected") + " (" + clients + " " + app.i18nText("status.esphome_clients", "connected") + ")";
        } else if (isRunning) {
            dot.className = "mqtt-dot connecting";
            pill.title = "ESPHome: " + app.i18nText("status.esphome_running", "Running on port") + " " + (statusData.port || 6053);
        } else {
            dot.className = "mqtt-dot disconnected";
            pill.title = "ESPHome: " + app.i18nText("mqtt.disconnected", "Disconnected");
        }
    }

    var _wsPositionRaf = {};

    function initWebSocket(app) {
        var wsScheme = window.location.protocol === "https:" ? "wss" : "ws";
        var ws = new WebSocket(wsScheme + "://" + window.location.host + "/ws");
        app.state.ws = ws;

        ws.onmessage = function (event) {
            try {
                var data = JSON.parse(event.data);
                if (data.type === "log") {
                    app.logStatus(data.message);
                } else if (data.type === "position") {
                    // Throttle to one DOM write per animation frame per device
                    if (_wsPositionRaf[data.id]) {
                        cancelAnimationFrame(_wsPositionRaf[data.id]);
                    }
                    var _capturedData = data;
                    _wsPositionRaf[data.id] = requestAnimationFrame(function () {
                        delete _wsPositionRaf[_capturedData.id];
                        app.updateDeviceFill(_capturedData.id, _capturedData.position);
                    });
                } else if (data.type === "deviceaction") {
                    app.applyDeviceAction(data);
                } else if (data.type === "init") {
                    app.fetchAndDisplayDevices();
                } else if (data.type === "mqtt_status") {
                    updateMqttStatus(app, data);
                } else if (data.type === "esphome_status") {
                    updateEspHomeStatus(app, data);
                } else if (data.type === "lastaddr") {
                    updateLastAddressBadge(app, data.address, data.action, data.protocol, 0, true);
                } else if (data.type === "twowstatus") {
                    // twowstatus has no UI in this branch — silently ignored
                }
            } catch (e) {
                // Invalid JSON from WebSocket — ignore
            }
        };

        ws.onopen = function () {
            app.state.wsConnected = true;
            if (app.elements.logStatusDot) app.elements.logStatusDot.className = "log-dot live";
            if (app.elements.logStatusText) app.elements.logStatusText.textContent = i18nText("log.live_connected", "Live");
        };

        ws.onclose = function () {
            app.state.wsConnected = false;
            if (app.elements.logStatusDot) app.elements.logStatusDot.className = "log-dot offline";
            if (app.elements.logStatusText) app.elements.logStatusText.textContent = i18nText("log.disconnected", "Disconnected");
            if (!app.state.wsReconnectTimer) {
                app.state.wsReconnectTimer = setTimeout(function () {
                    app.state.wsReconnectTimer = null;
                    initWebSocket(app);
                }, 2000);
            }
        };
    }

    function bindEvents(app) {
        if (app.elements.selectLog) {
            app.elements.selectLog.addEventListener("change", function () {
                applyLogFilter(app);
            });
        }
        if (app.elements.clearLogButton) {
            app.elements.clearLogButton.addEventListener("click", function () {
                if (app.elements.statusMessages) {
                    app.elements.statusMessages.textContent = "";
                    updateLogCount(app);
                }
            });
        }
        if (app.elements.copyLogButton) {
            app.elements.copyLogButton.addEventListener("click", function () {
                if (!app.elements.statusMessages) return;
                var text = app.elements.statusMessages.innerText || "";
                if (!text) return;
                if (navigator.clipboard && navigator.clipboard.writeText) {
                    navigator.clipboard.writeText(text).then(function () {
                        var orig = app.elements.copyLogButton.textContent;
                        app.elements.copyLogButton.textContent = i18nText("log.copied", "Copied!");
                        setTimeout(function () { app.elements.copyLogButton.textContent = orig; }, 2000);
                    });
                }
            });
        }
        if (app.elements.copyAddressButton) {
            app.elements.copyAddressButton.addEventListener("click", function () {
                var addr = (app.elements.lastAddrInput && app.elements.lastAddrInput.value) || "";
                if (addr && addr !== "------" && navigator.clipboard && navigator.clipboard.writeText) {
                    navigator.clipboard.writeText(addr).then(function () {
                        var orig = app.elements.copyAddressButton.textContent;
                        app.elements.copyAddressButton.textContent = i18nText("log.copied", "Copied!");
                        setTimeout(function () { app.elements.copyAddressButton.textContent = orig; }, 2000);
                    });
                }
            });
        }
        if (app.elements.useAddressRemoteButton) {
            app.elements.useAddressRemoteButton.addEventListener("click", function () {
                if (typeof window.showPage === "function") {
                    window.showPage("devices");
                }
                if (app.elements.remotePopupButton) {
                    app.elements.remotePopupButton.click();
                }
            });
        }
        if (app.elements.clearAddressButton) {
            app.elements.clearAddressButton.addEventListener("click", async function () {
                try {
                    await window.OmniIoApi.postJson("/api/lastaddr/clear", {});
                } catch (e) {}
                updateLastAddressBadge(app, "------");
            });
        }
        if (app.elements.sendCommandButton) {
            app.elements.sendCommandButton.addEventListener("click", function () {
                if (typeof app.sendCommand === "function") app.sendCommand();
            });
        }
        if (app.elements.mqttUpdateButton) {
            app.elements.mqttUpdateButton.addEventListener("click", function () {
                if (typeof app.updateMqttConfig === "function") app.updateMqttConfig();
            });
        }
        if (app.elements.esphomeUpdateButton) {
            app.elements.esphomeUpdateButton.addEventListener("click", function () {
                if (typeof app.updateEspHomeConfig === "function") app.updateEspHomeConfig();
            });
        }
        if (app.elements.wifiScanButton) {
            app.elements.wifiScanButton.addEventListener("click", function () {
                if (typeof app.scanWifiNetworks === "function") app.scanWifiNetworks();
            });
        }
        if (app.elements.wifiConfigSaveButton) {
            app.elements.wifiConfigSaveButton.addEventListener("click", function () {
                if (typeof app.saveWifiConfig === "function") app.saveWifiConfig();
            });
        }
        if (app.elements.networkSaveButton) {
            app.elements.networkSaveButton.addEventListener("click", function () {
                if (typeof app.saveNetworkConfig === "function") app.saveNetworkConfig();
            });
        }
        if (app.elements.displayUpdateButton) {
            app.elements.displayUpdateButton.addEventListener("click", function () {
                if (typeof app.updateDisplayConfig === "function") app.updateDisplayConfig();
            });
        }
        if (app.elements.syslogUpdateButton) {
            app.elements.syslogUpdateButton.addEventListener("click", function () {
                if (typeof app.updateSyslogConfig === "function") app.updateSyslogConfig();
            });
        }
        if (app.elements.syslogTestButton) {
            app.elements.syslogTestButton.addEventListener("click", function () {
                if (typeof app.sendSyslogTest === "function") app.sendSyslogTest();
            });
        }
        if (app.elements.firmwareUploadButton) {
            app.elements.firmwareUploadButton.addEventListener("click", function () {
                if (typeof app.uploadFirmware === "function") app.uploadFirmware();
            });
        }
        if (app.elements.filesystemUploadButton) {
            app.elements.filesystemUploadButton.addEventListener("click", function () {
                if (typeof app.uploadFilesystem === "function") app.uploadFilesystem();
            });
        }
        if (app.elements.backupUploadButton) {
            app.elements.backupUploadButton.addEventListener("click", function () {
                if (typeof app.uploadBackup === "function") app.uploadBackup();
            });
        }
        if (app.elements.devicesUploadButton) {
            app.elements.devicesUploadButton.addEventListener("click", function () {
                if (typeof app.uploadDevices === "function") app.uploadDevices();
            });
        }
        if (app.elements.remotesUploadButton) {
            app.elements.remotesUploadButton.addEventListener("click", function () {
                if (typeof app.uploadRemotes === "function") app.uploadRemotes();
            });
        }
        if (app.elements.downloadBackupButton) {
            app.elements.downloadBackupButton.addEventListener("click", function () {
                window.OmniIoApi.downloadFile("/api/download/backup", "omni-io-backup.json").catch(function () {});
            });
        }
        if (app.elements.downloadDevicesButton) {
            app.elements.downloadDevicesButton.addEventListener("click", function () {
                window.OmniIoApi.downloadFile("/api/download/devices", "1W.json").catch(function () {});
            });
        }
        if (app.elements.downloadRemotesButton) {
            app.elements.downloadRemotesButton.addEventListener("click", function () {
                window.OmniIoApi.downloadFile("/api/download/remotes", "RemoteMap.json").catch(function () {});
            });
        }
        if (app.elements.addPopupButton) {
            app.elements.addPopupButton.addEventListener("click", function () {
                if (typeof app.openAddDevicePopup === "function") app.openAddDevicePopup();
            });
        }
        if (app.elements.remotePopupButton) {
            app.elements.remotePopupButton.addEventListener("click", function () {
                if (typeof app.openAddRemotePopup === "function") app.openAddRemotePopup();
            });
        }
    }

    document.addEventListener("DOMContentLoaded", function () {
        var app = {
            elements: createElements(),
            i18nText: i18nText,
            logStatus: function (message, isError) {
                logStatus(app, message, isError);
            },
            loadLogBuffer: function () {
                return loadLogBuffer(app);
            },
            state: {
                devicesCache: [],
                ws: null
            }
        };

        window.OmniIoPopup.init(app);
        window.OmniIoDevices.init(app);
        window.OmniIoRemotes.init(app);
        window.OmniIoSettings.init(app);
        window.OmniIoApp = app;
        window.MiOpenApp = app;

        initSuggestions(app);
        initTheme(app);
        initHelpButtons(app);
        initWebSocket(app);
        bindEvents(app);

        window.addEventListener("i18n:changed", function () {
            app.fetchAndDisplayDevices();
            app.fetchAndDisplayRemotes();
        });

        var mqttPill = document.getElementById("mqtt-status-pill");
        if (mqttPill) {
            mqttPill.addEventListener("click", function () {
                if (typeof window.showPage === "function") {
                    window.showPage("settings");
                }
                var mqttTab = document.querySelector('[data-settings-target="mqtt"]');
                if (mqttTab) {
                    mqttTab.click();
                }
            });
        }

        var esphomePill = document.getElementById("esphome-status-pill");
        if (esphomePill) {
            esphomePill.addEventListener("click", function () {
                if (typeof window.showPage === "function") {
                    window.showPage("settings");
                }
                var intTab = document.querySelector('[data-settings-target="integration"]');
                if (intTab) {
                    intTab.click();
                }
                var esphomeSec = document.getElementById("esphome-settings-section");
                if (esphomeSec) {
                    esphomeSec.scrollIntoView({ behavior: "smooth" });
                }
            });
        }
        app.updateEspHomeStatus = function (data) {
            updateEspHomeStatus(app, data);
        };
        app.updateLastAddressBadge = function (addr, action, protocol, secondsAgo, isLive) {
            updateLastAddressBadge(app, addr, action, protocol, secondsAgo, isLive);
        };

        window.OmniIoApi.requestJson("/api/info").then(function (info) {
            var el = document.getElementById("firmware-version");
            if (el && info.version) {
                el.textContent = "Firmware: " + info.version + (info.branch ? " (" + info.branch + ")" : "");
            }
            updateMqttStatus(app, {
                connected: info.mqttConnected,
                enabled: info.mqttEnabled,
                state: info.mqttState
            });
        }).catch(function () {});

        app.loadLogBuffer();
        app.loadMqttConfig();
        if (typeof app.loadEspHomeConfig === "function") app.loadEspHomeConfig();
        app.loadWifiConfig();
        app.loadNetworkConfig();
        app.loadFallbackConfig();
        app.loadDisplayConfig();
        app.loadSyslogConfig();
        app.fetchAndDisplayDevices();
        app.fetchAndDisplayRemotes();
        app.loadLastAddress();
    });
})();

