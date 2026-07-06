(function () {
    function setSettingsStatus(app, message, isError) {
        if (!app.elements.settingsStatus) {
            return;
        }

        if (app.elements.settingsStatusText) {
            app.elements.settingsStatusText.textContent = message;
        } else {
            app.elements.settingsStatus.textContent = message;
        }
        app.elements.settingsStatus.hidden = false;
        app.elements.settingsStatus.classList.toggle("error", !!isError);
        if (typeof window.showToast === "function") {
            window.showToast(message, isError);
        }
    }

    function hideSettingsStatus(app) {
        if (!app.elements.settingsStatus) {
            return;
        }

        app.elements.settingsStatus.hidden = true;
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
            const data = await window.MiOpenApi.requestJson("/api/lastaddr");
            app.elements.lastAddrInput.value = data.address || "";
        } catch (error) {
            console.error("Error fetching last address", error);
        }
    }

    async function loadMqttConfig(app) {
        try {
            const config = await window.MiOpenApi.requestJson("/api/mqtt");
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
            const result = await window.MiOpenApi.postJson("/api/mqtt", {
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
            app.logStatus(result.message || "MQTT settings updated.");
        } catch (error) {
            console.error("Error updating MQTT config", error);
            setSettingsStatus(
                app,
                app.i18nText("status.mqtt_save_error", "Saving MQTT settings failed"),
                true
            );
            app.logStatus("Error updating MQTT config", true);
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
            const config = await window.MiOpenApi.requestJson("/api/display");
            const enabled = config.enabled !== false;
            app.elements.displayEnabledInput.checked = enabled;
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
            app.logStatus(app.i18nText("log.error_fetching_display", "Error fetching display config"), true);
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
            const result = await window.MiOpenApi.postJson("/api/display", {
                enabled: requestedEnabled
            });
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
            app.logStatus(result.message || app.i18nText("log.display_updated", "Display settings updated."));
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
            app.logStatus(app.i18nText("log.error_updating_display", "Error updating display config"), true);
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
            const config = await window.MiOpenApi.requestJson("/api/syslog");
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
            const result = await window.MiOpenApi.postJson("/api/syslog", {
                enabled: app.elements.syslogEnabledInput.checked,
                server: app.elements.syslogServerInput.value,
                port: parseInt(app.elements.syslogPortInput.value, 10),
                tag: app.elements.syslogTagInput.value
            });
            app.elements.syslogEnabledInput.checked = result.enabled !== false;
            app.elements.syslogServerInput.value = result.server || "";
            app.elements.syslogPortInput.value = result.port || "";
            app.elements.syslogTagInput.value = result.tag || "";
            app.logStatus(result.message || app.i18nText("log.syslog_updated", "Syslog settings updated."));
        } catch (error) {
            console.error("Error updating syslog config", error);
            app.logStatus(app.i18nText("log.error_updating_syslog", "Error updating syslog config"), true);
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
            const result = await window.MiOpenApi.postJson("/api/syslog/test", {});
            if (result.success) {
                app.logStatus(app.i18nText("log.syslog_test_sent", "Test message sent â€” check your syslog server."));
            } else {
                app.logStatus(app.i18nText("log.syslog_test_failed", "Test failed: ") + (result.message || ""), true);
            }
        } catch (error) {
            console.error("Error sending syslog test", error);
            app.logStatus(app.i18nText("log.error_syslog_test", "Error sending syslog test message"), true);
        } finally {
            syslogTestInFlight = false;
            if (app.elements.syslogTestButton) app.elements.syslogTestButton.disabled = false;
        }
    }

    function normaliseIoKey(value) {
        return (value || "").replace(/[^0-9a-fA-F]/g, "").toLowerCase();
    }

    function setIoKeyStatus(app, message, isError) {
        if (app.elements.ioKeyStatus) {
            app.elements.ioKeyStatus.textContent = message;
            app.elements.ioKeyStatus.classList.toggle("error", !!isError);
        }
    }

    async function loadIoSystemKey(app) {
        if (!app.elements.ioSystemKeyInput) {
            return;
        }
        try {
            const config = await window.MiOpenApi.requestJson("/api/io-key");
            app.elements.ioSystemKeyInput.value = config.key || "";
            setIoKeyStatus(
                app,
                config.configured
                    ? app.i18nText("status.io_key_loaded", "IO system key is configured")
                    : app.i18nText("status.io_key_empty", "No IO system key configured")
            );
        } catch (error) {
            console.error("Error fetching IO system key", error);
            setIoKeyStatus(app, app.i18nText("status.io_key_load_error", "Could not load IO system key"), true);
        }
    }

    async function saveIoSystemKey(app) {
        if (!app.elements.ioSystemKeyInput) {
            return;
        }
        const key = normaliseIoKey(app.elements.ioSystemKeyInput.value);
        if (key.length !== 32) {
            setIoKeyStatus(app, app.i18nText("status.io_key_invalid", "Use exactly 32 hexadecimal characters"), true);
            return;
        }
        try {
            const result = await window.MiOpenApi.postJson("/api/io-key", { key: key });
            app.elements.ioSystemKeyInput.value = result.key || key;
            setSettingsStatus(app, app.i18nText("status.io_key_saved", "IO system key saved"));
            setIoKeyStatus(app, app.i18nText("status.io_key_loaded", "IO system key is configured"));
        } catch (error) {
            console.error("Error saving IO system key", error);
            setIoKeyStatus(app, error.message || app.i18nText("status.io_key_save_error", "Saving IO system key failed"), true);
        }
    }

    async function clearIoSystemKey(app) {
        if (!app.elements.ioSystemKeyInput) {
            return;
        }
        try {
            await window.MiOpenApi.postJson("/api/io-key", { key: "" });
            app.elements.ioSystemKeyInput.value = "";
            setSettingsStatus(app, app.i18nText("status.io_key_cleared", "IO system key cleared"));
            setIoKeyStatus(app, app.i18nText("status.io_key_empty", "No IO system key configured"));
        } catch (error) {
            console.error("Error clearing IO system key", error);
            setIoKeyStatus(app, error.message || app.i18nText("status.io_key_save_error", "Saving IO system key failed"), true);
        }
    }
    async function uploadSelectedFile(app, input, url, missingMessage, successMessage, refreshFn) {
        const file = input.files[0];
        if (!file) {
            app.logStatus(missingMessage, true);
            return;
        }

        try {
            const result = await window.MiOpenApi.uploadFile(url, file);
            app.logStatus(result.message || successMessage);
            if (refreshFn) {
                await refreshFn();
            }
        } catch (error) {
            app.logStatus(error.message || successMessage, true);
        }
    }

    function initSettingsTabs() {
        const tabs = Array.from(document.querySelectorAll("[data-settings-tab]"));
        const panels = Array.from(document.querySelectorAll("[data-settings-panel]"));

        function activate(name) {
            tabs.forEach(function (tab) {
                tab.classList.toggle("active", tab.dataset.settingsTab === name);
            });
            panels.forEach(function (panel) {
                const isActive = panel.dataset.settingsPanel === name;
                panel.classList.toggle("active", isActive);
                panel.hidden = !isActive;
            });
        }

        tabs.forEach(function (tab) {
            tab.addEventListener("click", function () {
                activate(tab.dataset.settingsTab);
            });
        });

        const activeTab = tabs.find(function (tab) {
            return tab.classList.contains("active");
        });
        activate(activeTab ? activeTab.dataset.settingsTab : "integration");
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

        const restartButton = document.getElementById("settings-restart");
        if (restartButton) {
            restartButton.addEventListener("click", function () {
                setSettingsStatus(
                    app,
                    app.i18nText("status.restart_unavailable", "Restart is not available from this firmware build"),
                    true
                );
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
        app.loadIoSystemKey = function () {
            return loadIoSystemKey(app);
        };
        app.saveIoSystemKey = function () {
            return saveIoSystemKey(app);
        };
        app.clearIoSystemKey = function () {
            return clearIoSystemKey(app);
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

    window.MiOpenSettings = {
        init: init
    };
})();


