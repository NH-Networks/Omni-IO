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
            if (app.elements.mqttEnabledInput) {
                app.elements.mqttEnabledInput.checked = config.enabled !== false;
            }
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
                enabled: app.elements.mqttEnabledInput ? app.elements.mqttEnabledInput.checked : true,
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

    async function loadEspHomeConfig(app) {
        try {
            const config = await window.OmniIoApi.requestJson("/api/esphome");
            if (app.elements.esphomeEnabledInput) {
                app.elements.esphomeEnabledInput.checked = !!config.enabled;
            }
            if (app.elements.esphomeNameInput) {
                app.elements.esphomeNameInput.value = config.name || "omni-io";
            }
            if (app.elements.esphomePortInput) {
                app.elements.esphomePortInput.value = config.port || 6053;
            }
            if (app.elements.esphomePasswordInput) {
                app.elements.esphomePasswordInput.value = config.password || "";
            }
            if (app.elements.esphomeStatus) {
                const statusText = config.running
                    ? `${app.i18nText("status.esphome_running", "Running on port")} ${config.port} (${config.clients || 0} ${app.i18nText("status.esphome_clients", "connected")})`
                    : app.i18nText("status.esphome_stopped", "Stopped");
                app.elements.esphomeStatus.textContent = statusText;
                app.elements.esphomeStatus.className = "status-indicator " + (config.running ? "success" : "idle");
            }
            if (typeof app.updateEspHomeStatus === "function") {
                app.updateEspHomeStatus(config);
            }
        } catch (error) {
            console.error("Error fetching ESPHome config", error);
        }
    }

    async function updateEspHomeConfig(app) {
        setSettingsStatus(
            app,
            app.i18nText("status.settings_saving", "Saving settings...")
        );
        try {
            await window.OmniIoApi.postJson("/api/esphome", {
                enabled: app.elements.esphomeEnabledInput ? app.elements.esphomeEnabledInput.checked : true,
                name: app.elements.esphomeNameInput ? app.elements.esphomeNameInput.value : "omni-io",
                port: app.elements.esphomePortInput ? parseInt(app.elements.esphomePortInput.value, 10) : 6053,
                password: app.elements.esphomePasswordInput ? app.elements.esphomePasswordInput.value : ""
            });
            setSettingsStatus(
                app,
                app.i18nText("status.esphome_saved", "ESPHome settings saved")
            );
            loadEspHomeConfig(app);
        } catch (error) {
            console.error("Error updating ESPHome config", error);
            setSettingsStatus(
                app,
                app.i18nText("status.esphome_save_error", "Saving ESPHome settings failed"),
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
        app.loadEspHomeConfig = function () {
            return loadEspHomeConfig(app);
        };
        app.updateEspHomeConfig = function () {
            return updateEspHomeConfig(app);
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
