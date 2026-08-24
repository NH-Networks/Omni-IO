(function () {
    function setSettingsStatus(app, message, isError, timeoutMs) {
        if (typeof window.showToast === "function") {
            window.showToast(message, isError, timeoutMs);
        }
    }

    function hideSettingsStatus(_app) {
        if (typeof window.hideToast === "function") {
            window.hideToast();
        }
    }


    function setDisplayStatus(app, message, isError) {
        if (!app.elements.displayStatus) {
            return;
        }

        app.elements.displayStatus.textContent = message;
        app.elements.displayStatus.classList.toggle("error", !!isError);
        if (isError && typeof window.showToast === "function") {
            window.showToast(message, true);
        }
    }

    async function loadLastAddress(app) {
        try {
            const data = await window.OmniIoApi.requestJson("/api/lastaddr");
            app.elements.lastAddrInput.value = data.address || "";
        } catch (error) {
            console.error("Error fetching last address", error);
        }
    }

    async function loadMqttConfig(app) {
        try {
            const config = await window.OmniIoApi.requestJson("/api/mqtt");
            app.elements.mqttUserInput.value = config.user || "";
            app.elements.mqttServerInput.value = config.server || "";
            app.elements.mqttPasswordInput.value = config.password || "";
            app.elements.mqttPortInput.value = config.port || "";
            app.elements.mqttDiscoveryInput.value = config.discovery || "";
        } catch (error) {
            console.error("Error fetching MQTT config", error);
        }
    }

    async function updateMqttConfig(app) {
        setSettingsStatus(
            app,
            app.i18nText("status.settings_saving", "Saving settings...")
        );
        try {
            const result = await window.OmniIoApi.postJson("/api/mqtt", {
                user: app.elements.mqttUserInput.value,
                server: app.elements.mqttServerInput.value,
                password: app.elements.mqttPasswordInput.value,
                port: app.elements.mqttPortInput.value,
                discovery: app.elements.mqttDiscoveryInput.value
            });
            setSettingsStatus(
                app,
                app.i18nText("status.mqtt_saved", "MQTT settings saved")
            );
        } catch (error) {
            console.error("Error updating MQTT config", error);
            setSettingsStatus(
                app,
                app.i18nText("status.mqtt_save_error", "Saving MQTT settings failed"),
                true
            );
        }
    }

    function setWifiStatus(app, message, isError) {
        if (app.elements.wifiConfigStatus) {
            app.elements.wifiConfigStatus.textContent = message || "";
            app.elements.wifiConfigStatus.classList.toggle("error", !!isError);
        }
        if (message) {
            setSettingsStatus(app, message, isError);
        }
    }

    async function loadWifiConfig(app) {
        if (!app.elements.wifiSsidInput) {
            return;
        }
        try {
            const config = await window.OmniIoApi.requestJson("/api/wifi");
            app.elements.wifiSsidInput.value = config.ssid || config.currentSsid || "";
            if (app.elements.wifiPasswordInput) {
                app.elements.wifiPasswordInput.value = "";
            }
            setWifiStatus(app, config.connected ? "WiFi connected" : "WiFi not connected", !config.connected);
        } catch (error) {
            console.error("Error fetching WiFi config", error);
            setWifiStatus(app, error.message || "WiFi settings load failed", true);
        }
    }

    function setNetworkStatus(app, message, isError) {
        if (!app.elements.networkStatus) {
            return;
        }
        app.elements.networkStatus.textContent = message || "";
        app.elements.networkStatus.classList.toggle("error", !!isError);
        app.elements.networkStatus.style.color = isError ? "#e74c3c" : "";
    }

    async function loadNetworkConfig(app) {
        if (!app.elements.networkHostnameInput) {
            return;
        }
        try {
            const config = await window.OmniIoApi.requestJson("/api/network");
            app.elements.networkHostnameInput.value = config.hostname || "";
            app.elements.networkDhcpInput.checked = config.dhcp !== false;
            app.elements.networkIpInput.value = config.ip || "";
            app.elements.networkMaskInput.value = config.mask || "";
            app.elements.networkGatewayInput.value = config.gateway || "";
            app.elements.networkDns1Input.value = config.dns1 || "";
            app.elements.networkDns2Input.value = config.dns2 || "";
            app.elements.networkSntpInput.value = config.sntp || "";
            const tz = config.tz || "CET-1CEST,M3.5.0,M10.5.0/3";
            if (app.elements.networkTzInput) {
                app.elements.networkTzInput.value = tz;
            }
            if (app.elements.networkTzSelect) {
                let matched = false;
                for (let i = 0; i < app.elements.networkTzSelect.options.length; i++) {
                    if (app.elements.networkTzSelect.options[i].value === tz) {
                        app.elements.networkTzSelect.selectedIndex = i;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    app.elements.networkTzSelect.value = "custom";
                    if (app.elements.networkTzCustomWrap) app.elements.networkTzCustomWrap.style.display = "block";
                } else {
                    if (app.elements.networkTzCustomWrap) app.elements.networkTzCustomWrap.style.display = "none";
                }
            }
            app.elements.networkTimeInput.value = config.time || "Not synchronized";
            setNetworkStatus(app, config.connected ? "Network config loaded" : "Network config loaded, WiFi not connected", !config.connected);
        } catch (error) {
            console.error("Error fetching network config", error);
            setNetworkStatus(app, error.message || "Network config load failed", true);
        }
    }
    async function saveNetworkConfig(app) {
        if (!app.elements.networkSaveButton || !app.elements.networkHostnameInput) {
            return;
        }
        app.elements.networkSaveButton.disabled = true;
        setNetworkStatus(app, "Saving network config...");
        try {
            const result = await window.OmniIoApi.postJson("/api/network", {
                hostname: app.elements.networkHostnameInput.value,
                dhcp: app.elements.networkDhcpInput.checked,
                ip: app.elements.networkIpInput.value,
                mask: app.elements.networkMaskInput.value,
                gateway: app.elements.networkGatewayInput.value,
                dns1: app.elements.networkDns1Input.value,
                dns2: app.elements.networkDns2Input.value,
                sntp: app.elements.networkSntpInput.value,
                tz: app.elements.networkTzInput.value
            });
            setNetworkStatus(app, result.message || "Network config saved, rebooting");
            setSettingsStatus(app, result.message || "Network config saved, rebooting");
        } catch (error) {
            console.error("Error saving network config", error);
            setNetworkStatus(app, error.message || "Saving network config failed", true);
            app.elements.networkSaveButton.disabled = false;
        }
    }
    function openWifiScanModal(app, message) {
        if (typeof app.openPopup === "function") {
            app.openPopup("WiFi networks", "", [message || "Scanning WiFi networks..."], [], {
                showSave: false,
                btnShowCancel: true
            });
        }
    }

    function renderWifiScanResults(app, scanResult) {
        const networks = Array.isArray(scanResult) ? scanResult : (scanResult && Array.isArray(scanResult.networks) ? scanResult.networks : []);
        const validNetworks = networks.filter(function (network) { return network && network.ssid; });

        if (app.elements.wifiScanResults) {
            app.elements.wifiScanResults.style.display = "none";
            app.elements.wifiScanResults.textContent = "";
        }

        const content = document.getElementById("popup-content");
        if (!content) {
            return;
        }
        content.textContent = "";

        if (validNetworks.length === 0) {
            const message = document.createElement("p");
            message.textContent = "No WiFi networks found";
            content.appendChild(message);
            setWifiStatus(app, "No WiFi networks found", true);
            return;
        }

        const list = document.createElement("div");
        list.className = "wifi-scan-modal-list";
        validNetworks.forEach(function (network) {
            const button = document.createElement("button");
            button.type = "button";
            button.className = "wifi-scan-result";
            button.textContent = network.ssid + " (" + network.rssi + " dBm" + (network.secure ? ", secure" : ", open") + ")";
            button.addEventListener("click", function () {
                app.elements.wifiSsidInput.value = network.ssid;
                if (typeof app.closePopup === "function") {
                    app.closePopup();
                }
                if (app.elements.wifiPasswordInput) {
                    app.elements.wifiPasswordInput.focus();
                }
            });
            list.appendChild(button);
        });
        content.appendChild(list);
    }

    async function scanWifiNetworks(app) {
        if (!app.elements.wifiScanButton) {
            return;
        }
        app.elements.wifiScanButton.disabled = true;
        openWifiScanModal(app, "Scanning WiFi networks...");
        setWifiStatus(app, "Scanning WiFi networks...");
        try {
            const scanResult = await window.OmniIoApi.requestJson("/api/wifi-scan");
            renderWifiScanResults(app, scanResult);
            const networks = Array.isArray(scanResult) ? scanResult : (scanResult && Array.isArray(scanResult.networks) ? scanResult.networks : []);
            if (networks.length > 0) {
                setWifiStatus(app, networks.length + " WiFi networks found");
            }
        } catch (error) {
            console.error("Error scanning WiFi networks", error);
            setWifiStatus(app, error.message || "WiFi scan failed", true);
        } finally {
            app.elements.wifiScanButton.disabled = false;
        }
    }

    async function saveWifiConfig(app) {
        if (!app.elements.wifiSsidInput || !app.elements.wifiConfigSaveButton) {
            return;
        }
        const ssid = app.elements.wifiSsidInput.value.trim();
        if (!ssid) {
            setWifiStatus(app, "SSID is required", true);
            return;
        }

        app.elements.wifiConfigSaveButton.disabled = true;
        setWifiStatus(app, "Saving WiFi settings...");
        try {
            const result = await window.OmniIoApi.postJson("/api/wifi", {
                ssid: ssid,
                password: app.elements.wifiPasswordInput ? app.elements.wifiPasswordInput.value : ""
            });
            setWifiStatus(app, result.message || "WiFi settings saved, rebooting");
        } catch (error) {
            console.error("Error saving WiFi settings", error);
            setWifiStatus(app, error.message || "Saving WiFi settings failed", true);
            app.elements.wifiConfigSaveButton.disabled = false;
        }
    }

    function setFallbackStatus(app, message, isError) {
        if (!app.elements.fallbackStatus) {
            return;
        }
        app.elements.fallbackStatus.textContent = message || "";
        app.elements.fallbackStatus.classList.toggle("error", !!isError);
        if (message) {
            setSettingsStatus(app, message, isError);
        }
    }

    async function loadFallbackConfig(app) {
        if (!app.elements.fallbackEnabledInput) {
            return;
        }
        try {
            const config = await window.OmniIoApi.requestJson("/api/fallback");
            app.elements.fallbackEnabledInput.checked = config.enabled !== false;
            app.elements.fallbackRetriesBootInput.value = config.retriesBoot || 3;
            app.elements.fallbackRetriesRunningInput.value = config.retriesRunning || 3;
            app.elements.fallbackTimeoutInput.value = config.timeout || 600;
            setFallbackStatus(app, "Fallback AP settings loaded");
        } catch (error) {
            console.error("Error fetching fallback config", error);
            setFallbackStatus(app, error.message || "Fallback AP load failed", true);
        }
    }

    async function saveFallbackConfig(app) {
        if (!app.elements.fallbackSaveButton) {
            return;
        }
        app.elements.fallbackSaveButton.disabled = true;
        try {
            const result = await window.OmniIoApi.postJson("/api/fallback", {
                enabled: app.elements.fallbackEnabledInput.checked,
                retriesBoot: Number(app.elements.fallbackRetriesBootInput.value || 3),
                retriesRunning: Number(app.elements.fallbackRetriesRunningInput.value || 3),
                timeout: Number(app.elements.fallbackTimeoutInput.value || 600)
            });
            setFallbackStatus(app, result.message || "Fallback AP settings saved");
        } catch (error) {
            console.error("Error saving fallback config", error);
            setFallbackStatus(app, error.message || "Fallback AP save failed", true);
        } finally {
            app.elements.fallbackSaveButton.disabled = false;
        }
    }

    async function loadDisplayConfig(app) {
        if (!app.elements.displayEnabledInput) {
            return;
        }

        setDisplayStatus(
            app,
            app.i18nText("status.display_loading", "Display settings loading...")
        );

        try {
            const config = await window.OmniIoApi.requestJson("/api/display");
            const enabled = config.enabled !== false;
            app.elements.displayEnabledInput.checked = enabled;

            if (app.elements.displayScreensaverTimeoutInput) {
                app.elements.displayScreensaverTimeoutInput.value =
                    config.screensaverTimeout !== undefined ? config.screensaverTimeout : 60;
            }
            if (app.elements.displayOffTimeoutInput) {
                app.elements.displayOffTimeoutInput.value =
                    config.screenOffTimeout !== undefined ? config.screenOffTimeout : 3600;
            }
            if (app.elements.displayDimLevelSelect) {
                app.elements.displayDimLevelSelect.value =
                    config.dimLevel !== undefined ? String(config.dimLevel) : "0";
            }
            if (app.elements.displayCpuTempInput) {
                app.elements.displayCpuTempInput.checked = config.showCpuTemp !== false;
            }

            setDisplayStatus(
                app,
                enabled
                    ? app.i18nText("status.display_enabled", "Display is enabled")
                    : app.i18nText("status.display_disabled", "Display is disabled")
            );
        } catch (error) {
            console.error("Error fetching display config", error);
            setDisplayStatus(
                app,
                app.i18nText("status.display_load_error", "Could not load display settings"),
                true
            );
        }
    }

    let displayUpdateInFlight = false;

    async function updateDisplayConfig(app) {
        if (!app.elements.displayEnabledInput || displayUpdateInFlight) {
            return;
        }

        displayUpdateInFlight = true;
        if (app.elements.displayUpdateButton) {
            app.elements.displayUpdateButton.disabled = true;
        }

        const requestedEnabled = app.elements.displayEnabledInput.checked;
        setDisplayStatus(
            app,
            app.i18nText("status.display_saving", "Saving display setting...")
        );
        try {
            const payload = {
                enabled: requestedEnabled
            };

            if (app.elements.displayScreensaverTimeoutInput) {
                const ssVal = parseInt(app.elements.displayScreensaverTimeoutInput.value, 10);
                if (!isNaN(ssVal)) payload.screensaverTimeout = ssVal;
            }
            if (app.elements.displayOffTimeoutInput) {
                const offVal = parseInt(app.elements.displayOffTimeoutInput.value, 10);
                if (!isNaN(offVal)) payload.screenOffTimeout = offVal;
            }
            if (app.elements.displayDimLevelSelect) {
                const dimVal = parseInt(app.elements.displayDimLevelSelect.value, 10);
                payload.dimLevel = isNaN(dimVal) ? 0 : dimVal;
            }
            if (app.elements.displayCpuTempInput) {
                payload.showCpuTemp = app.elements.displayCpuTempInput.checked;
            }

            const result = await window.OmniIoApi.postJson("/api/display", payload);
            const enabled = result.enabled !== false;
            app.elements.displayEnabledInput.checked = enabled;
            setSettingsStatus(
                app,
                enabled
                    ? app.i18nText("status.display_saved_enabled", "Saved: display enabled")
                    : app.i18nText("status.display_saved_disabled", "Saved: display disabled")
            );
            setDisplayStatus(
                app,
                enabled
                    ? app.i18nText("status.display_saved_enabled", "Saved: display enabled")
                    : app.i18nText("status.display_saved_disabled", "Saved: display disabled")
            );
        } catch (error) {
            console.error("Error updating display config", error);
            app.elements.displayEnabledInput.checked = !requestedEnabled;
            setSettingsStatus(
                app,
                app.i18nText("status.display_save_error", "Saving display setting failed"),
                true
            );
            setDisplayStatus(
                app,
                app.i18nText("status.display_save_error", "Saving display setting failed"),
                true
            );
        } finally {
            displayUpdateInFlight = false;
            if (app.elements.displayUpdateButton) {
                app.elements.displayUpdateButton.disabled = false;
            }
        }
    }

    async function loadSyslogConfig(app) {
        if (!app.elements.syslogServerInput) {
            return;
        }
        try {
            const config = await window.OmniIoApi.requestJson("/api/syslog");
            app.elements.syslogEnabledInput.checked = config.enabled !== false;
            app.elements.syslogServerInput.value = config.server || "";
            app.elements.syslogPortInput.value = config.port || "";
            app.elements.syslogTagInput.value = config.tag || "";
        } catch (error) {
            console.error("Error fetching syslog config", error);
        }
    }

    let syslogTestInFlight = false;
    let syslogUpdateInFlight = false;

    async function updateSyslogConfig(app) {
        if (!app.elements.syslogServerInput || syslogUpdateInFlight) {
            return;
        }
        syslogUpdateInFlight = true;
        if (app.elements.syslogUpdateButton) {
            app.elements.syslogUpdateButton.disabled = true;
        }
        try {
            const result = await window.OmniIoApi.postJson("/api/syslog", {
                enabled: app.elements.syslogEnabledInput.checked,
                server: app.elements.syslogServerInput.value,
                port: parseInt(app.elements.syslogPortInput.value, 10),
                tag: app.elements.syslogTagInput.value
            });
            app.elements.syslogEnabledInput.checked = result.enabled !== false;
            app.elements.syslogServerInput.value = result.server || "";
            app.elements.syslogPortInput.value = result.port || "";
            app.elements.syslogTagInput.value = result.tag || "";
        } catch (error) {
            console.error("Error updating syslog config", error);
        } finally {
            syslogUpdateInFlight = false;
            if (app.elements.syslogUpdateButton) {
                app.elements.syslogUpdateButton.disabled = false;
            }
        }
    }

    async function sendSyslogTest(app) {
        if (syslogTestInFlight) return;
        syslogTestInFlight = true;
        if (app.elements.syslogTestButton) app.elements.syslogTestButton.disabled = true;
        try {
            const result = await window.OmniIoApi.postJson("/api/syslog/test", {});
            if (result.success) {
            } else {
            }
        } catch (error) {
            console.error("Error sending syslog test", error);
        } finally {
            syslogTestInFlight = false;
            if (app.elements.syslogTestButton) app.elements.syslogTestButton.disabled = false;
        }
    }

    function normaliseIoKey(value) {
        return (value || "").replace(/[^0-9a-fA-F]/g, "").toLowerCase();
    }
    function uploadFileWithProgress(file, url, onProgress) {
        return new Promise(function (resolve, reject) {
            const xhr = new XMLHttpRequest();
            let uploadComplete = false;
            const formData = new FormData();
            formData.append("file", file);

            xhr.upload.addEventListener("progress", function (event) {
                if (event.lengthComputable && onProgress) {
                    const percent = Math.round((event.loaded / event.total) * 100);
                    uploadComplete = percent >= 100;
                    onProgress(percent);
                }
            });

            xhr.addEventListener("load", function () {
                let result = {};
                try {
                    result = xhr.responseText ? JSON.parse(xhr.responseText) : {};
                } catch (_error) {
                    result = {};
                }

                if (xhr.status >= 200 && xhr.status < 300) {
                    resolve(result);
                    return;
                }
                reject(new Error(result.message || ("HTTP error " + xhr.status)));
            });

            xhr.addEventListener("error", function () {
                if (uploadComplete && (url === "/api/firmware" || url === "/api/filesystem")) {
                    resolve({ message: "Upload sent to device, rebooting..." });
                    return;
                }
                reject(new Error("Upload connection failed"));
            });
            xhr.addEventListener("abort", function () {
                reject(new Error("Upload cancelled"));
            });

            xhr.open("POST", url);
            xhr.send(formData);
        });
    }
    async function uploadSelectedFile(app, input, url, missingMessage, successMessage, refreshFn) {
        const file = input.files[0];
        if (!file) {
            setSettingsStatus(app, missingMessage, true, 8000);
            return;
        }

        setSettingsStatus(app, "Upload started...", false, 20000);
        try {
            const result = await uploadFileWithProgress(file, url, function (percent) {
                const message = percent >= 100
                    ? "Upload sent to device, writing flash..."
                    : "Uploading " + percent + "%...";
                setSettingsStatus(app, message, false, 20000);
            });
            const message = result.message || successMessage;
            setSettingsStatus(app, message, false, 20000);
            input.value = "";
            if (refreshFn) {
                await refreshFn();
            }
        } catch (error) {
            const message = error.message || successMessage;
            setSettingsStatus(app, message, true, 20000);
        }
    }



    function initSettingsTabs() {
        const tabs = Array.from(document.querySelectorAll("[data-settings-tab]"));
        const panels = Array.from(document.querySelectorAll("[data-settings-panel]"));

        function activate(name) {
            tabs.forEach(function (tab) {
                const isActive = tab.dataset.settingsTab === name;
                tab.classList.toggle("active", isActive);
            });
            panels.forEach(function (panel) {
                const isActive = panel.dataset.settingsPanel === name;
                panel.classList.toggle("active", isActive);
                if (isActive) {
                    panel.removeAttribute("hidden");
                    panel.style.setProperty("display", "grid", "important");
                } else {
                    panel.setAttribute("hidden", "hidden");
                    panel.style.setProperty("display", "none", "important");
                }
            });
        }

        tabs.forEach(function (tab) {
            tab.addEventListener("click", function (e) {
                if (e && typeof e.preventDefault === "function") e.preventDefault();
                activate(tab.dataset.settingsTab);
            });
        });

        activate("integration");
    }

    let restartInFlight = false;

    async function restartDevice(app, button) {
        if (restartInFlight) return;
        restartInFlight = true;
        if (button) button.disabled = true;
        setSettingsStatus(app, app.i18nText("status.restarting", "Herstart bezig..."));
        try {
            await window.OmniIoApi.postJson("/api/restart", {});
            setSettingsStatus(app, app.i18nText("status.restarted", "ESP wordt herstart, pagina herlaadt over 8 seconden..."));
            setTimeout(function () {
                window.location.reload();
            }, 8000);
        } catch (error) {
            // fetch throws when ESP drops connection mid-restart — that is expected
            setSettingsStatus(app, app.i18nText("status.restarted", "ESP wordt herstart, pagina herlaadt over 8 seconden..."));
            setTimeout(function () {
                window.location.reload();
            }, 8000);
        } finally {
            restartInFlight = false;
        }
    }

    function initSettingsActions(app) {
        const closeButton = document.getElementById("settings-close");
        if (closeButton) {
            closeButton.addEventListener("click", function () {
                if (typeof window.showPage === "function") {
                    window.showPage("devices");
                }
            });
        }

        if (app.elements.fallbackSaveButton) {
            app.elements.fallbackSaveButton.addEventListener("click", function () {
                app.saveFallbackConfig();
            });
        }

        const restartButton = document.getElementById("settings-restart");
        if (restartButton) {
            restartButton.addEventListener("click", function () {
                restartDevice(app, restartButton);
            });
        }

        if (app.elements.networkTzSelect) {
            app.elements.networkTzSelect.addEventListener("change", function () {
                const val = this.value;
                if (val === "custom") {
                    if (app.elements.networkTzCustomWrap) app.elements.networkTzCustomWrap.style.display = "block";
                    if (app.elements.networkTzInput) app.elements.networkTzInput.focus();
                } else {
                    if (app.elements.networkTzCustomWrap) app.elements.networkTzCustomWrap.style.display = "none";
                    if (app.elements.networkTzInput) app.elements.networkTzInput.value = val;
                }
            });
        }
    }

    let sunScreensCache = {};
    let sunIntervalTimer = null;

    function renderSunScreensList(app, remotes, currentConfig) {
        if (!app.elements.sunScreensList) return;
        const list = app.elements.sunScreensList;
        list.innerHTML = "";

        if (!remotes || remotes.length === 0) {
            list.innerHTML = `<p style="font-size: 0.85em; opacity: 0.7;">${app.i18nText("help.no_screens_found", "Geen schermen gevonden.")}</p>`;
            return;
        }

        const savedScreens = (currentConfig && currentConfig.screens) || sunScreensCache;

        remotes.forEach(function (r) {
            const desc = r.description || r.address || "";
            const name = r.name || desc;
            const isChecked = savedScreens[desc] !== undefined ? savedScreens[desc] : true;

            const row = document.createElement("label");
            row.style.display = "flex";
            row.style.alignItems = "center";
            row.style.gap = "8px";
            row.style.fontSize = "0.9em";
            row.style.cursor = "pointer";

            const cb = document.createElement("input");
            cb.type = "checkbox";
            cb.checked = isChecked;
            cb.dataset.sunScreenDesc = desc;
            cb.style.margin = "0";

            const span = document.createElement("span");
            span.textContent = `${name} (${desc})`;

            row.appendChild(cb);
            row.appendChild(span);
            list.appendChild(row);
        });
    }

    function getWeatherCodeName(code, app) {
        if (code === 0) return app.i18nText("weather.clear", "Helder");
        if (code >= 1 && code <= 3) return app.i18nText("weather.partly_cloudy", "Licht bewolkt");
        if (code >= 45 && code <= 48) return app.i18nText("weather.fog", "Mist");
        if (code >= 51 && code <= 55) return app.i18nText("weather.drizzle", "Motregen");
        if (code >= 61 && code <= 65) return app.i18nText("weather.rain", "Regen");
        if (code >= 71 && code <= 77) return app.i18nText("weather.snow", "Sneeuw");
        if (code >= 80 && code <= 82) return app.i18nText("weather.showers", "Buien");
        if (code >= 95 && code <= 99) return app.i18nText("weather.thunderstorm", "Onweer");
        return app.i18nText("weather.overcast", "Bewolkt");
    }

    function renderSunMetrics(app, data) {
        if (!data || !app.elements.sunConditionBadge) return;
        const met = data.metrics || {};
        const cond = data.condition || "disabled";
        const isActive = data.actionActive || false;
        const countdown = data.countdownSec || 0;
        const lockoutSec = data.lockoutRemainingSec || met.lockoutRemainingSec || 0;
        const lockoutReason = met.lockoutReason || "";

        function getCompassName(deg) {
            if (deg === undefined || isNaN(deg)) return "-";
            const d = (deg % 360 + 360) % 360;
            const dirs = ["N", "NNO", "NO", "ONO", "O", "OZO", "ZO", "ZZO", "Z", "ZZW", "ZW", "WZW", "W", "WNW", "NW", "NNW"];
            const idx = Math.round(d / 22.5) % 16;
            return `${d.toFixed(0)}° (${dirs[idx]})`;
        }

        if (app.elements.sunMetricRadiation) {
            app.elements.sunMetricRadiation.textContent = (met.directRadiation !== undefined ? Math.round(met.directRadiation) : "-") + " W/m²";
        }
        if (app.elements.sunMetricEffective) {
            app.elements.sunMetricEffective.textContent = (met.effectiveRadiation !== undefined ? Math.round(met.effectiveRadiation) : "-") + " W/m²";
        }
        if (app.elements.sunMetricCloud) {
            app.elements.sunMetricCloud.textContent = (met.cloudCover !== undefined ? Math.round(met.cloudCover) : "-") + "%";
        }
        if (app.elements.sunMetricElevation) {
            app.elements.sunMetricElevation.textContent = (met.elevation !== undefined ? met.elevation.toFixed(1) : "-") + "°";
        }
        if (app.elements.sunMetricAzimuth) {
            app.elements.sunMetricAzimuth.textContent = getCompassName(met.azimuth);
        }
        if (app.elements.sunMetricWind) {
            const speed = met.windSpeed !== undefined ? met.windSpeed.toFixed(0) : "-";
            const gusts = met.windGusts !== undefined ? met.windGusts.toFixed(0) : "-";
            app.elements.sunMetricWind.textContent = `${speed} km/h (vlaag: ${gusts})`;
        }
        if (app.elements.sunMetricTemp) {
            const temp = met.temperature !== undefined ? met.temperature.toFixed(1) : "-";
            const maxT = met.forecastMaxTemp !== undefined && met.forecastMaxTemp > -40 ? met.forecastMaxTemp.toFixed(1) : "-";
            app.elements.sunMetricTemp.textContent = `${temp} °C (max: ${maxT}°C)`;
        }
        if (app.elements.sunMetricRain) {
            const precip = met.precipitation !== undefined ? met.precipitation.toFixed(1) : "0.0";
            const wName = getWeatherCodeName(met.weatherCode || 0, app);
            app.elements.sunMetricRain.textContent = `${precip} mm/h (${wName})`;
        }

        const badge = app.elements.sunConditionBadge;
        const desc = app.elements.sunActionDesc;

        let badgeText = "";
        let badgeColor = "#5A6E8C";
        let descText = "";

        switch (cond) {
            case "disabled":
                badgeText = app.i18nText("sun_state.disabled", "Uitgeschakeld");
                badgeColor = "#777";
                descText = app.i18nText("sun_state.disabled_desc", "Zon & weer automatisering staat uit.");
                break;
            case "sunny":
                badgeText = app.i18nText("sun_state.sunny", "☀️ Volle zon op gevel");
                badgeColor = "#d97706";
                if (isActive) {
                    descText = app.i18nText("sun_state.screens_closed", "Schermen zijn automatisch gesloten voor de zon.");
                } else if (countdown > 0) {
                    const min = Math.ceil(countdown / 60);
                    descText = app.i18nText("sun_state.closing_in", `Zon gedetecteerd. Sluiten over ${min} min...`);
                } else {
                    descText = app.i18nText("sun_state.sun_active", "Zon actief.");
                }
                break;
            case "hot_day_precool":
                badgeText = app.i18nText("sun_state.hot_day_precool", "☀️ Hittedag Koeling");
                badgeColor = "#ea580c";
                if (isActive) {
                    descText = app.i18nText("sun_state.precool_closed", "Preventieve hittedag-koeling actief. Schermen gesloten tegen opwarming.");
                } else {
                    descText = app.i18nText("sun_state.precool_closing", "Hete dag voorspeld. Schermen worden preventief gesloten...");
                }
                break;
            case "cold_hold":
                badgeText = app.i18nText("sun_state.cold_hold", "❄️ Passieve Warmte (Koud Weer)");
                badgeColor = "#2563eb";
                descText = app.i18nText("sun_state.cold_hold_desc", "Zon schijnt, maar buitentemperatuur is laag. Schermen blijven open om gratis te verwarmen.");
                break;
            case "cloudy":
                badgeText = app.i18nText("sun_state.cloudy", "⛅ Bewolkt / Weinig straling");
                badgeColor = "#4b5563";
                if (isActive && countdown > 0) {
                    const min = Math.ceil(countdown / 60);
                    descText = app.i18nText("sun_state.opening_in", `Zon weggevallen. Schermen openen over ${min} min...`);
                } else {
                    descText = app.i18nText("sun_state.screens_open", "Schermen geopend (geen zonbelasting).");
                }
                break;
            case "outside_facade":
                badgeText = app.i18nText("sun_state.outside_facade", "🧭 Zon buiten gevelbereik");
                badgeColor = "#6b7280";
                if (isActive && countdown > 0) {
                    const min = Math.ceil(countdown / 60);
                    descText = app.i18nText("sun_state.opening_in", `Zon voorbij gevel. Schermen openen over ${min} min...`);
                } else {
                    descText = app.i18nText("sun_state.outside_desc", "Zon schijnt momenteel niet rechtstreeks op deze gevel.");
                }
                break;
            case "night":
                badgeText = app.i18nText("sun_state.night", "🌙 Nacht / Zonsondergang");
                badgeColor = "#374151";
                descText = app.i18nText("sun_state.night_desc", "Zon is onder de horizon.");
                break;
            case "rain_alert":
                badgeText = app.i18nText("sun_state.rain_alert", "🌧️ Regen / Onweer Alarm");
                badgeColor = "#0284c7";
                descText = app.i18nText("sun_state.rain_desc", "Neerslag gedetecteerd! Schermen zijn direct ingetrokken ter bescherming.");
                break;
            case "wind_alert":
                badgeText = app.i18nText("sun_state.wind_alert", "💨 Wind- & Storm Alarm");
                badgeColor = "#dc2626";
                descText = app.i18nText("sun_state.wind_desc", "Harde wind of zware windstoten gedetecteerd! Schermen zijn beschermd.");
                break;
            case "safety_lockout":
                badgeText = app.i18nText("sun_state.safety_lockout", "🛡️ Veiligheidsvergrendeling");
                badgeColor = "#9333ea";
                const lockMin = Math.ceil(lockoutSec / 60);
                const reasonStr = (lockoutReason === "rain")
                    ? app.i18nText("sun_state.lockout_rain_reason", "droogloop na regen")
                    : app.i18nText("sun_state.lockout_wind_reason", "rusttijd na storm");
                descText = app.i18nText("sun_state.lockout_desc", `Veiligheidsslot actief (${reasonStr}). Vrijgave over ${lockMin} min.`);
                break;
            case "manual_hold":
                badgeText = app.i18nText("sun_state.manual_hold", "✋ Handmatige Pauze");
                badgeColor = "#4f46e5";
                descText = app.i18nText("sun_state.manual_hold_desc", "Handmatige bediening actief; automatisering gepauzeerd.");
                break;
            default:
                badgeText = cond;
                badgeColor = "#5A6E8C";
                descText = "";
        }

        badge.textContent = badgeText;
        badge.style.background = badgeColor;
        if (desc) desc.textContent = descText;
    }

    async function loadSunConfig(app) {
        try {
            const data = await window.OmniIoApi.requestJson("/api/sun");
            const cfg = data.config || {};
            if (app.elements.sunEnabledInput) app.elements.sunEnabledInput.checked = !!cfg.enabled;
            if (app.elements.sunLatInput && cfg.latitude !== undefined) app.elements.sunLatInput.value = cfg.latitude;
            if (app.elements.sunLonInput && cfg.longitude !== undefined) app.elements.sunLonInput.value = cfg.longitude;

            if (app.elements.sunAzimuthStartInput && cfg.azimuthStart !== undefined) app.elements.sunAzimuthStartInput.value = cfg.azimuthStart;
            if (app.elements.sunAzimuthEndInput && cfg.azimuthEnd !== undefined) app.elements.sunAzimuthEndInput.value = cfg.azimuthEnd;
            if (app.elements.sunMinElevationInput && cfg.minElevation !== undefined) app.elements.sunMinElevationInput.value = cfg.minElevation;
            if (app.elements.sunFacadeAzimuthInput && cfg.facadeAzimuth !== undefined) app.elements.sunFacadeAzimuthInput.value = cfg.facadeAzimuth;
            if (app.elements.sunUseIncidenceInput) app.elements.sunUseIncidenceInput.checked = !!cfg.useIncidenceAngle;

            if (app.elements.sunRadiationThreshInput && cfg.radiationThreshold !== undefined) app.elements.sunRadiationThreshInput.value = cfg.radiationThreshold;
            if (app.elements.sunCloudThreshInput && cfg.maxCloudCover !== undefined) app.elements.sunCloudThreshInput.value = cfg.maxCloudCover;

            if (app.elements.sunDelayOnInput && cfg.sunOnDelayMin !== undefined) app.elements.sunDelayOnInput.value = cfg.sunOnDelayMin;
            if (app.elements.sunDelayOffInput && cfg.sunOffDelayMin !== undefined) app.elements.sunDelayOffInput.value = cfg.sunOffDelayMin;
            if (app.elements.sunMinHoldInput && cfg.minHoldDurationMin !== undefined) app.elements.sunMinHoldInput.value = cfg.minHoldDurationMin;

            if (app.elements.sunRainEnabledInput) app.elements.sunRainEnabledInput.checked = cfg.rainSafetyEnabled !== undefined ? !!cfg.rainSafetyEnabled : true;
            if (app.elements.sunRainActionSelect && cfg.rainAction !== undefined) app.elements.sunRainActionSelect.value = cfg.rainAction;
            if (app.elements.sunRainLockoutInput && cfg.rainLockoutMin !== undefined) app.elements.sunRainLockoutInput.value = cfg.rainLockoutMin;

            if (app.elements.sunMaxWindInput && cfg.maxWindSpeed !== undefined) app.elements.sunMaxWindInput.value = cfg.maxWindSpeed;
            if (app.elements.sunMaxWindGustInput && cfg.maxWindGust !== undefined) app.elements.sunMaxWindGustInput.value = cfg.maxWindGust;
            if (app.elements.sunWindActionSelect && cfg.windAction !== undefined) app.elements.sunWindActionSelect.value = cfg.windAction;
            if (app.elements.sunWindLockoutInput && cfg.windLockoutMin !== undefined) app.elements.sunWindLockoutInput.value = cfg.windLockoutMin;

            if (app.elements.sunTempFilterEnabledInput) app.elements.sunTempFilterEnabledInput.checked = cfg.tempFilterEnabled !== undefined ? !!cfg.tempFilterEnabled : true;
            if (app.elements.sunMinTempInput && cfg.minTemperature !== undefined) app.elements.sunMinTempInput.value = cfg.minTemperature;

            if (app.elements.sunHotdayEnabledInput) app.elements.sunHotdayEnabledInput.checked = !!cfg.hotDayForecastEnabled;
            if (app.elements.sunHotdayTempInput && cfg.hotDayThresholdTemp !== undefined) app.elements.sunHotdayTempInput.value = cfg.hotDayThresholdTemp;

            if (app.elements.sunNightAutoOpenInput) app.elements.sunNightAutoOpenInput.checked = cfg.nightAutoOpen !== undefined ? !!cfg.nightAutoOpen : true;

            sunScreensCache = cfg.screens || {};

            // Fetch and render screen list
            const remotes = await window.OmniIoApi.requestJson("/api/remotes").catch(() => []);
            renderSunScreensList(app, remotes, cfg);

            renderSunMetrics(app, data);

            if (!sunIntervalTimer) {
                sunIntervalTimer = setInterval(function () {
                    const sunPanel = document.querySelector('[data-settings-panel="sun"]');
                    if (sunPanel && sunPanel.classList.contains("active")) {
                        window.OmniIoApi.requestJson("/api/sun").then(function (d) {
                            renderSunMetrics(app, d);
                        }).catch(function () {});
                    }
                }, 15000);
            }
        } catch (e) {
            console.error("loadSunConfig error", e);
        }
    }

    async function saveSunConfig(app) {
        const screens = {};
        if (app.elements.sunScreensList) {
            const checkboxes = app.elements.sunScreensList.querySelectorAll("input[data-sun-screen-desc]");
            checkboxes.forEach(function (cb) {
                const desc = cb.dataset.sunScreenDesc;
                if (desc) screens[desc] = cb.checked;
            });
        }

        const payload = {
            enabled: app.elements.sunEnabledInput ? app.elements.sunEnabledInput.checked : false,
            latitude: app.elements.sunLatInput ? parseFloat(app.elements.sunLatInput.value) || 52.3676 : 52.3676,
            longitude: app.elements.sunLonInput ? parseFloat(app.elements.sunLonInput.value) || 4.9041 : 4.9041,
            azimuthStart: app.elements.sunAzimuthStartInput ? parseFloat(app.elements.sunAzimuthStartInput.value) || 120 : 120,
            azimuthEnd: app.elements.sunAzimuthEndInput ? parseFloat(app.elements.sunAzimuthEndInput.value) || 260 : 260,
            minElevation: app.elements.sunMinElevationInput ? parseFloat(app.elements.sunMinElevationInput.value) || 10 : 10,
            facadeAzimuth: app.elements.sunFacadeAzimuthInput ? parseFloat(app.elements.sunFacadeAzimuthInput.value) || 180 : 180,
            useIncidenceAngle: app.elements.sunUseIncidenceInput ? app.elements.sunUseIncidenceInput.checked : false,

            radiationThreshold: app.elements.sunRadiationThreshInput ? parseFloat(app.elements.sunRadiationThreshInput.value) || 200 : 200,
            maxCloudCover: app.elements.sunCloudThreshInput ? parseFloat(app.elements.sunCloudThreshInput.value) || 75 : 75,
            sunOnDelayMin: app.elements.sunDelayOnInput ? parseInt(app.elements.sunDelayOnInput.value, 10) || 5 : 5,
            sunOffDelayMin: app.elements.sunDelayOffInput ? parseInt(app.elements.sunDelayOffInput.value, 10) || 15 : 15,
            minHoldDurationMin: app.elements.sunMinHoldInput ? parseInt(app.elements.sunMinHoldInput.value, 10) || 10 : 10,

            rainSafetyEnabled: app.elements.sunRainEnabledInput ? app.elements.sunRainEnabledInput.checked : true,
            rainAction: app.elements.sunRainActionSelect ? app.elements.sunRainActionSelect.value : "open",
            rainLockoutMin: app.elements.sunRainLockoutInput ? parseInt(app.elements.sunRainLockoutInput.value, 10) || 20 : 20,

            maxWindSpeed: app.elements.sunMaxWindInput ? parseFloat(app.elements.sunMaxWindInput.value) || 35 : 35,
            maxWindGust: app.elements.sunMaxWindGustInput ? parseFloat(app.elements.sunMaxWindGustInput.value) || 45 : 45,
            windAction: app.elements.sunWindActionSelect ? app.elements.sunWindActionSelect.value : "open",
            windLockoutMin: app.elements.sunWindLockoutInput ? parseInt(app.elements.sunWindLockoutInput.value, 10) || 30 : 30,

            tempFilterEnabled: app.elements.sunTempFilterEnabledInput ? app.elements.sunTempFilterEnabledInput.checked : true,
            minTemperature: app.elements.sunMinTempInput ? parseFloat(app.elements.sunMinTempInput.value) || 19.0 : 19.0,

            hotDayForecastEnabled: app.elements.sunHotdayEnabledInput ? app.elements.sunHotdayEnabledInput.checked : false,
            hotDayThresholdTemp: app.elements.sunHotdayTempInput ? parseFloat(app.elements.sunHotdayTempInput.value) || 26.0 : 26.0,

            nightAutoOpen: app.elements.sunNightAutoOpenInput ? app.elements.sunNightAutoOpenInput.checked : true,
            screens: screens
        };

        setSettingsStatus(app, app.i18nText("status.saving", "Opslaan..."));
        try {
            await window.OmniIoApi.postJson("/api/sun", payload);
            setSettingsStatus(app, app.i18nText("status.sun_saved", "Zon & weer automatisering instellingen opgeslagen!"), false, 4000);
            await loadSunConfig(app);
        } catch (e) {
            setSettingsStatus(app, app.i18nText("status.error_saving", "Fout bij opslaan: ") + (e.message || e), true, 6000);
        }
    }

    async function evaluateSunNow(app) {
        setSettingsStatus(app, app.i18nText("status.measuring", "Meting ophalen en evalueren..."));
        try {
            await window.OmniIoApi.postJson("/api/sun/evaluate", {});
            setSettingsStatus(app, app.i18nText("status.measured", "Meting voltooid!"), false, 3000);
            await loadSunConfig(app);
        } catch (e) {
            setSettingsStatus(app, app.i18nText("status.error_measuring", "Fout bij meting: ") + (e.message || e), true, 6000);
        }
    }

    function geolocateUser(app) {
        if (!navigator.geolocation) {
            alert(app.i18nText("error.geolocation_not_supported", "Geolocatie wordt niet ondersteund door deze browser."));
            return;
        }
        setSettingsStatus(app, app.i18nText("status.geolocating", "GPS-locatie bepalen..."));
        navigator.geolocation.getCurrentPosition(function (pos) {
            if (app.elements.sunLatInput) app.elements.sunLatInput.value = pos.coords.latitude.toFixed(4);
            if (app.elements.sunLonInput) app.elements.sunLonInput.value = pos.coords.longitude.toFixed(4);
            setSettingsStatus(app, app.i18nText("status.geolocated", "Locatie succesvol ingevuld!"), false, 3000);
        }, function (err) {
            setSettingsStatus(app, app.i18nText("status.geolocate_failed", "Locatiebepaling mislukt: ") + err.message, true, 5000);
        });
    }

    function init(app) {
        initSettingsTabs();
        initSettingsActions(app);

        app.loadLastAddress = function () {
            return loadLastAddress(app);
        };
        app.loadMqttConfig = function () {
            return loadMqttConfig(app);
        };
        app.updateMqttConfig = function () {
            return updateMqttConfig(app);
        };
        app.loadWifiConfig = function () {
            return loadWifiConfig(app);
        };
        app.loadNetworkConfig = function () {
            return loadNetworkConfig(app);
        };
        app.saveNetworkConfig = function () {
            return saveNetworkConfig(app);
        };
        app.loadFallbackConfig = function () {
            return loadFallbackConfig(app);
        };
        app.saveFallbackConfig = function () {
            return saveFallbackConfig(app);
        };
        app.scanWifiNetworks = function () {
            return scanWifiNetworks(app);
        };
        app.saveWifiConfig = function () {
            return saveWifiConfig(app);
        };
        app.hideSettingsStatus = function () {
            hideSettingsStatus(app);
        };
        app.loadDisplayConfig = function () {
            return loadDisplayConfig(app);
        };
        app.updateDisplayConfig = function () {
            return updateDisplayConfig(app);
        };
        app.loadSyslogConfig = function () {
            return loadSyslogConfig(app);
        };
        app.updateSyslogConfig = function () {
            return updateSyslogConfig(app);
        };
        app.sendSyslogTest = function () {
            return sendSyslogTest(app);
        };
        app.loadSunConfig = function () {
            return loadSunConfig(app);
        };
        app.saveSunConfig = function () {
            return saveSunConfig(app);
        };
        app.evaluateSunNow = function () {
            return evaluateSunNow(app);
        };
        app.geolocateUser = function () {
            return geolocateUser(app);
        };

        app.uploadFirmware = function () {
            return uploadSelectedFile(
                app,
                app.elements.firmwareFileInput,
                "/api/firmware",
                "No firmware file selected",
                "Firmware uploaded"
            );
        };
        app.uploadFilesystem = function () {
            return uploadSelectedFile(
                app,
                app.elements.filesystemFileInput,
                "/api/filesystem",
                "No filesystem file selected",
                "Filesystem uploaded"
            );
        };
        app.uploadBackup = function () {
            return uploadSelectedFile(
                app,
                app.elements.backupFileInput,
                "/api/upload/backup",
                "No backup file selected",
                "Backup uploaded",
                async function () {
                    await app.fetchAndDisplayDevices();
                    await app.fetchAndDisplayRemotes();
                }
            );
        };
        app.uploadDevices = function () {
            return uploadSelectedFile(
                app,
                app.elements.devicesFileInput,
                "/api/upload/devices",
                "No devices file selected",
                "Devices file uploaded",
                async function () {
                    await app.fetchAndDisplayDevices();
                    await app.fetchAndDisplayRemotes();
                }
            );
        };
        app.uploadRemotes = function () {
            return uploadSelectedFile(
                app,
                app.elements.remotesFileInput,
                "/api/upload/remotes",
                "No remotes file selected",
                "Remotes file uploaded",
                function () {
                    return app.fetchAndDisplayRemotes();
                }
            );
        };
    }

    window.OmniIoSettings = {
        init: init
    };
    window.MiOpenSettings = window.OmniIoSettings;
})();
