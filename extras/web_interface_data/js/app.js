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
            mqttDiscoveryInput: document.getElementById("mqtt-discovery"),
            mqttPasswordInput: document.getElementById("mqtt-password"),
            mqttPortInput: document.getElementById("mqtt-port"),
            mqttServerInput: document.getElementById("mqtt-server"),
            mqttUpdateButton: document.getElementById("mqtt-update"),
            mqttUserInput: document.getElementById("mqtt-user"),
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
            themeToggle: document.getElementById("toggle-theme")
        };
    }

    function i18nText(key, fallback) {
        if (typeof window.t === "function") {
            const value = window.t(key);
            if (value && value !== key) {
                return value;
            }
        }
        return fallback || key;
    }

    function logStatus(app, message, isError) {
        if (!app.elements.statusMessages || !message) {
            return;
        }



        const logEntry = document.createElement("p");
        logEntry.textContent = message;
        if (isError) {
            logEntry.style.color = "red";
        }

        app.elements.statusMessages.appendChild(logEntry);
        app.elements.statusMessages.scrollTop = app.elements.statusMessages.scrollHeight;
        while (app.elements.statusMessages.children.length > 300) {
            app.elements.statusMessages.removeChild(app.elements.statusMessages.firstChild);
        }
    }

    async function loadLogBuffer(app) {
        if (!app.elements.statusMessages || !window.MiOpenApi) {
            return;
        }
        try {
            const logs = await window.MiOpenApi.requestJson("/api/logs");
            app.elements.statusMessages.textContent = "";
            if (Array.isArray(logs)) {
                logs.forEach(function (message) {
                    logStatus(app, message);
                });
            }
        } catch (error) {
            logStatus(app, "Could not load log buffer", true);
        }
    }

    function initSuggestions(app) {
        const suggestions = ["add", "remove", "close", "open", "ls", "cat"];
        app.elements.suggestions.textContent = "";

        suggestions.forEach(function (item) {
            const option = document.createElement("option");
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
        const savedTheme = localStorage.getItem("theme");
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

    function initWebSocket(app) {
        const wsScheme = window.location.protocol === "https:" ? "wss" : "ws";
        const ws = new WebSocket(wsScheme + "://" + window.location.host + "/ws");
        app.state.ws = ws;

        ws.onmessage = function (event) {
            const data = JSON.parse(event.data);
            if (data.type === "log") {
                app.logStatus(data.message);
            } else if (data.type === "position") {
                app.updateDeviceFill(data.id, data.position);
            } else if (data.type === "deviceaction") {
                app.applyDeviceAction(data);
            } else if (data.type === "init") {
                app.fetchAndDisplayDevices();
            } else if (data.type === "lastaddr") {
                app.elements.lastAddrInput.value = data.address || "";
            }
        };

        ws.onopen = function () {
            app.state.wsConnected = true;
        };

        ws.onclose = function () {
            app.state.wsConnected = false;
            if (!app.state.wsReconnectTimer) {
                app.state.wsReconnectTimer = setTimeout(function () {
                    app.state.wsReconnectTimer = null;
                    initWebSocket(app);
                }, 2000);
            }
        };
    }

    function bindEvents(app) {
        if (app.elements.sendCommandButton) {
            app.elements.sendCommandButton.addEventListener("click", app.sendCommand);
        }
        if (app.elements.mqttUpdateButton) {
            app.elements.mqttUpdateButton.addEventListener("click", app.updateMqttConfig);
        }
        if (app.elements.wifiScanButton) {
            app.elements.wifiScanButton.addEventListener("click", app.scanWifiNetworks);
        }
        if (app.elements.wifiConfigSaveButton) {
            app.elements.wifiConfigSaveButton.addEventListener("click", app.saveWifiConfig);
        }
        if (app.elements.networkSaveButton) {
            app.elements.networkSaveButton.addEventListener("click", app.saveNetworkConfig);
        }
        if (app.elements.displayUpdateButton) {
            app.elements.displayUpdateButton.addEventListener("click", app.updateDisplayConfig);
        }
        if (app.elements.syslogUpdateButton) {
            app.elements.syslogUpdateButton.addEventListener("click", app.updateSyslogConfig);
        }
        if (app.elements.syslogTestButton) {
            app.elements.syslogTestButton.addEventListener("click", app.sendSyslogTest);
        }
        if (app.elements.ioKeySaveButton) {
            app.elements.ioKeySaveButton.addEventListener("click", app.saveIoSystemKey);
        }
        if (app.elements.ioKeyClearButton) {
            app.elements.ioKeyClearButton.addEventListener("click", app.clearIoSystemKey);
        }
        if (app.elements.firmwareUploadButton) {
            app.elements.firmwareUploadButton.addEventListener("click", app.uploadFirmware);
        }
        if (app.elements.filesystemUploadButton) {
            app.elements.filesystemUploadButton.addEventListener("click", app.uploadFilesystem);
        }
        if (app.elements.backupUploadButton) {
            app.elements.backupUploadButton.addEventListener("click", app.uploadBackup);
        }
        if (app.elements.devicesUploadButton) {
            app.elements.devicesUploadButton.addEventListener("click", app.uploadDevices);
        }
        if (app.elements.remotesUploadButton) {
            app.elements.remotesUploadButton.addEventListener("click", app.uploadRemotes);
        }
        if (app.elements.downloadBackupButton) {
            app.elements.downloadBackupButton.addEventListener("click", function () {
                window.MiOpenApi.downloadFile("/api/download/backup", "miopen-backup.json").catch(function (error) {
                });
            });
        }
        if (app.elements.downloadDevicesButton) {
            app.elements.downloadDevicesButton.addEventListener("click", function () {
                window.MiOpenApi.downloadFile("/api/download/devices", "1W.json").catch(function (error) {
                });
            });
        }
        if (app.elements.downloadRemotesButton) {
            app.elements.downloadRemotesButton.addEventListener("click", function () {
                window.MiOpenApi.downloadFile("/api/download/remotes", "RemoteMap.json").catch(function (error) {
                });
            });
        }
        if (app.elements.addPopupButton) {
            app.elements.addPopupButton.addEventListener("click", app.openAddDevicePopup);
        }
        if (app.elements.remotePopupButton) {
            app.elements.remotePopupButton.addEventListener("click", app.openAddRemotePopup);
        }
    }

    document.addEventListener("DOMContentLoaded", function () {
        const app = {
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

        window.MiOpenPopup.init(app);
        window.MiOpenDevices.init(app);
        window.MiOpenRemotes.init(app);
        window.MiOpenSettings.init(app);
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

        window.MiOpenApi.requestJson("/api/info").then(function (info) {
            const el = document.getElementById("firmware-version");
            if (el && info.version) {
                el.textContent = "Firmware: " + info.version + (info.branch ? " (" + info.branch + ")" : "");
            }
        }).catch(function () {});

        app.loadLogBuffer();
        app.loadMqttConfig();
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

