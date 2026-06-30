(function () {
    function ensureApiModule() {
        if (window.MiOpenApi) {
            return;
        }

        function ensureJson(response) {
            return response.json().catch(function () {
                return {};
            });
        }

        async function requestJson(url, options) {
            const requestOptions = options || {};
            const method = (requestOptions.method || "GET").toUpperCase();
            const requestUrl = method === "GET"
                ? url + (url.indexOf("?") === -1 ? "?" : "&") + "_=" + Date.now()
                : url;

            if (method === "GET") {
                requestOptions.cache = "no-store";
            }

            let response;
            try {
                response = await fetch(requestUrl, requestOptions);
            } catch (error) {
                throw new Error("ESP niet bereikbaar. Controleer of het toestel online is of net herstart.");
            }
            const data = await ensureJson(response);
            if (!response.ok) {
                throw new Error(data.message || ("HTTP error " + response.status));
            }
            return data;
        }

        window.MiOpenApi = {
            downloadFile: async function (url, filename) {
                const response = await fetch(url);
                if (!response.ok) {
                    throw new Error("Network response was not ok");
                }

                const blob = await response.blob();
                const link = document.createElement("a");
                link.href = window.URL.createObjectURL(blob);
                link.download = filename;
                link.click();
                window.URL.revokeObjectURL(link.href);
            },
            postJson: function (url, payload) {
                return requestJson(url, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify(payload)
                });
            },
            requestJson: requestJson,
            uploadFile: function (url, file) {
                const formData = new FormData();
                formData.append("file", file);
                return requestJson(url, {
                    method: "POST",
                    body: formData
                });
            }
        };
    }

    function createElements() {
        return {
            addPopupButton: document.getElementById("add-popup"),
            commandDeviceSelect: document.querySelector("#help-page #device-select"),
            commandInput: document.getElementById("command-input"),
            deviceList: document.getElementById("device-list"),
            devicesFileInput: document.getElementById("devices-file"),
            devicesUploadButton: document.getElementById("upload-devices"),
            downloadDevicesButton: document.getElementById("download-devices"),
            downloadRemotesButton: document.getElementById("download-remotes"),
            filesystemFileInput: document.getElementById("filesystem-file"),
            filesystemUploadButton: document.getElementById("upload-filesystem"),
            firmwareFileInput: document.getElementById("firmware-file"),
            firmwareUploadButton: document.getElementById("upload-firmware"),
            githubUpdateButton: document.getElementById("github-update-check"),
            githubUpdateBranchInput: document.getElementById("github-update-branch"),
            githubUpdateStatus: document.getElementById("github-update-status"),
            helpDeviceButton: document.getElementById("help-device"),
            helpRemoteButton: document.getElementById("help-remote"),
            lastAddrInput: document.getElementById("last-address"),
            mqttDiscoveryInput: document.getElementById("mqtt-discovery"),
            mqttPasswordInput: document.getElementById("mqtt-password"),
            mqttPortInput: document.getElementById("mqtt-port"),
            mqttServerInput: document.getElementById("mqtt-server"),
            mqttUpdateButton: document.getElementById("mqtt-update"),
            mqttUserInput: document.getElementById("mqtt-user"),
            settingsTabs: document.querySelectorAll("[data-settings-tab]"),
            settingsPanels: document.querySelectorAll("[data-settings-panel]"),
            settingsRestartButton: document.getElementById("settings-restart"),
            settingsCloseButton: document.getElementById("settings-close"),
            selectLog: document.getElementById("select-log"),
            displayEnabledInput: document.getElementById("display-enabled"),
            displayUpdateButton: document.getElementById("display-update"),
            displayStatus: document.getElementById("display-status"),
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

    function applyLogFilter(app) {
        if (!app.elements.selectLog || !app.elements.statusMessages) {
            return;
        }

        const selected = app.elements.selectLog.value || "all";
        Array.prototype.forEach.call(app.elements.statusMessages.children, function (entry) {
            const level = entry.dataset.level || "info";
            entry.hidden = selected !== "all" && level !== selected;
        });
    }

    function logStatus(app, message, isError, level) {
        const logEntry = document.createElement("p");
        logEntry.textContent = message;
        const logLevel = level || (isError ? "error" : "info");
        logEntry.dataset.level = logLevel;
        logEntry.classList.add("log-" + logLevel);
        if (logLevel === "error") {
            logEntry.style.color = "red";
        }

        app.elements.statusMessages.appendChild(logEntry);
        applyLogFilter(app);
        app.elements.statusMessages.scrollTop = app.elements.statusMessages.scrollHeight;
        while (app.elements.statusMessages.children.length > 20) {
            app.elements.statusMessages.removeChild(app.elements.statusMessages.firstChild);
        }

        if (typeof window.showToast === "function" && isError) {
            window.showToast(message, "error", 6000);
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

        ws.onmessage = function (event) {
            const data = JSON.parse(event.data);
            if (data.type === "log") {
                app.logStatus(data.message, data.level === "error", data.level);
            } else if (data.type === "position") {
                app.updateDeviceFill(data.id, data.position);
            } else if (data.type === "init") {
                app.fetchAndDisplayDevices();
            } else if (data.type === "lastaddr") {
                app.elements.lastAddrInput.value = data.address || "";
            }
        };

        ws.onopen = function () {
            app.logStatus("WebSocket connected");
        };

        ws.onclose = function () {
            app.logStatus("WebSocket disconnected", true);
        };

        app.state.ws = ws;
    }

    function bindEvents(app) {
        if (app.elements.sendCommandButton) {
            app.elements.sendCommandButton.addEventListener("click", app.sendCommand);
        }
        if (app.elements.mqttUpdateButton) {
            app.elements.mqttUpdateButton.addEventListener("click", app.updateMqttConfig);
        }
        if (app.elements.displayUpdateButton) {
            app.elements.displayUpdateButton.addEventListener("click", app.updateDisplayConfig);
        }
        if (app.elements.displayEnabledInput) {
            app.elements.displayEnabledInput.addEventListener("change", app.updateDisplayConfig);
        }
        if (app.elements.selectLog) {
            app.elements.selectLog.addEventListener("change", function () {
                applyLogFilter(app);
            });
        }
        if (app.elements.settingsTabs) {
            app.elements.settingsTabs.forEach(function (tab) {
                tab.addEventListener("click", function () {
                    app.scrollToSettingsPanel(tab.dataset.settingsTab);
                });
            });
        }
        if (app.elements.settingsRestartButton) {
            app.elements.settingsRestartButton.addEventListener("click", app.restartDevice);
        }
        if (app.elements.settingsCloseButton) {
            app.elements.settingsCloseButton.addEventListener("click", function () {
                if (window.showPage) {
                    window.showPage("devices");
                }
            });
        }
        if (app.elements.firmwareUploadButton) {
            app.elements.firmwareUploadButton.addEventListener("click", app.uploadFirmware);
        }
        if (app.elements.githubUpdateButton) {
            app.elements.githubUpdateButton.addEventListener("click", app.checkGithubUpdate);
        }
        if (app.elements.filesystemUploadButton) {
            app.elements.filesystemUploadButton.addEventListener("click", app.uploadFilesystem);
        }
        if (app.elements.devicesUploadButton) {
            app.elements.devicesUploadButton.addEventListener("click", app.uploadDevices);
        }
        if (app.elements.remotesUploadButton) {
            app.elements.remotesUploadButton.addEventListener("click", app.uploadRemotes);
        }
        if (app.elements.downloadDevicesButton) {
            app.elements.downloadDevicesButton.addEventListener("click", function () {
                const message = "Devices download started.";
                if (typeof window.showToast === "function") window.showToast(message, "info");
                window.MiOpenApi.downloadFile("/api/download/devices", "1W.json")
                    .catch(function (error) {
                        app.logStatus("Error downloading file: " + error.message, true);
                    });
            });
        }
        if (app.elements.downloadRemotesButton) {
            app.elements.downloadRemotesButton.addEventListener("click", function () {
                const message = "Remotes download started.";
                if (typeof window.showToast === "function") window.showToast(message, "info");
                window.MiOpenApi.downloadFile("/api/download/remotes", "RemoteMap.json")
                    .catch(function (error) {
                        app.logStatus("Error downloading file: " + error.message, true);
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
        ensureApiModule();

        const app = {
            elements: createElements(),
            i18nText: i18nText,
            logStatus: function (message, isError, level) {
                logStatus(app, message, isError, level);
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

        app.logStatus("System started");
        app.logStatus("Loading devices...");
        app.loadMqttConfig();
        app.loadDisplayConfig();
        app.loadFirmwareInfo();
        app.fetchAndDisplayDevices();
        app.fetchAndDisplayRemotes();
        app.loadLastAddress();
    });
})();
