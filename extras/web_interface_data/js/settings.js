(function () {
    function showToastIfAvailable(message, type, duration) {
        if (typeof window.showToast === "function") {
            window.showToast(message, type, duration);
        }
    }

    function showSettingsTab(app, tabName) {
        if (!app.elements.settingsTabs || !app.elements.settingsPanels) {
            return;
        }
        app.elements.settingsTabs.forEach(function (tab) {
            const active = tab.dataset.settingsTab === tabName;
            tab.classList.toggle("active", active);
            tab.setAttribute("aria-selected", active ? "true" : "false");
        });
    }

    function scrollToSettingsPanel(app, tabName) {
        showSettingsTab(app, tabName);
        const panel = Array.prototype.find.call(app.elements.settingsPanels, function (item) {
            return item.dataset.settingsPanel === tabName;
        });
        if (panel && typeof panel.scrollIntoView === "function") {
            panel.scrollIntoView({ behavior: "smooth", block: "start" });
        }
    }

    async function restartDevice(app) {
        if (!window.confirm(app.i18nText("confirm.restart", "Restart device now?"))) {
            return;
        }
        try {
            const result = await window.MiOpenApi.postJson("/api/reboot", {});
            const message = result.message || app.i18nText("toast.restarting", "Restarting...");
            app.logStatus(message);
            showToastIfAvailable(message, "info", 8000);
        } catch (error) {
            const message = error.message || app.i18nText("toast.restart_failed", "Restart request failed.");
            app.logStatus(message, true);
            showToastIfAvailable(message, "error", 6000);
        }
    }

    function setDisplayStatus(app, message, isError) {
        if (!app.elements.displayStatus) {
            return;
        }

        app.elements.displayStatus.textContent = message;
        app.elements.displayStatus.classList.toggle("error", !!isError);
    }

    function setGithubUpdateStatus(app, message, isError) {
        if (!app.elements.githubUpdateStatus) {
            return;
        }

        app.elements.githubUpdateStatus.textContent = message;
        app.elements.githubUpdateStatus.classList.toggle("error", !!isError);
    }

    function normalizeVersion(value) {
        return String(value || "")
            .trim()
            .replace(/^v/i, "")
            .replace(/-dirty$/, "")
            .toLowerCase();
    }

    function versionContainsCommit(version, sha) {
        const normalizedVersion = normalizeVersion(version);
        const normalizedSha = normalizeVersion(sha);
        if (!normalizedVersion || !normalizedSha) {
            return false;
        }
        return normalizedVersion.indexOf(normalizedSha.substring(0, 7)) !== -1 ||
            normalizedSha.indexOf(normalizedVersion.substring(0, 7)) === 0;
    }

    function resolveGithubBranch(branch, fallback) {
        const value = String(branch || "").trim();
        if (value.toLowerCase() === "beta") {
            return "Beta";
        }
        if (value === "main") {
            return "master";
        }
        if (value === "master" || value === "dev-main" || value === "Beta") {
            return value;
        }
        return fallback || "Beta";
    }

    async function checkGithubUpdate(app) {
        const repo = "djbenbe/miopen.io";
        if (app.elements.githubUpdateButton) {
            app.elements.githubUpdateButton.disabled = true;
        }
        setGithubUpdateStatus(
            app,
            app.i18nText("status.github_update_checking", "Checking GitHub...")
        );

        try {
            const info = await window.MiOpenApi.requestJson("/api/info");
            const currentVersion = info.version || "unknown";
            const firmwareBranch = info.branch || "Beta";
            const selectedBranch = app.elements.githubUpdateBranchInput
                ? app.elements.githubUpdateBranchInput.value
                : "auto";
            const branch = selectedBranch === "auto"
                ? resolveGithubBranch(firmwareBranch, "Beta")
                : resolveGithubBranch(selectedBranch, "Beta");
            const githubResponse = await fetch("https://api.github.com/repos/" + repo + "/commits/" + branch, {
                cache: "no-store",
                headers: { "Accept": "application/vnd.github+json" }
            });
            if (!githubResponse.ok) {
                throw new Error("GitHub HTTP " + githubResponse.status);
            }
            const latest = await githubResponse.json();
            const latestSha = latest.sha || "";
            const latestShort = latestSha.substring(0, 7);
            const latestDate = latest.commit && latest.commit.committer
                ? latest.commit.committer.date
                : "";

            let message;
            if (versionContainsCommit(currentVersion, latestSha)) {
                message = app.i18nText("status.github_update_current", "Already up to date") +
                    " [" + branch + "] (" + latestShort + ")";
                setGithubUpdateStatus(app, message);
                app.logStatus(message);
                showToastIfAvailable(message, "success");
            } else {
                message = app.i18nText("status.github_update_available", "Update available") +
                    " [" + branch + "]: " + currentVersion + " -> " + latestShort;
                if (latestDate) {
                    message += " (" + latestDate.substring(0, 10) + ")";
                }
                setGithubUpdateStatus(app, message);
                app.logStatus(message, false, "warning");
                showToastIfAvailable(message, "info", 6000);
            }
        } catch (error) {
            console.error("Error checking GitHub update", error);
            const message = app.i18nText("status.github_update_error", "Could not check GitHub update") +
                ": " + error.message;
            setGithubUpdateStatus(app, message, true);
            app.logStatus(message, true);
            showToastIfAvailable(message, "error", 6000);
        } finally {
            if (app.elements.githubUpdateButton) {
                app.elements.githubUpdateButton.disabled = false;
            }
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

    async function loadFirmwareInfo(app) {
        if (!app.elements.githubUpdateBranchInput) {
            return;
        }

        try {
            const info = await window.MiOpenApi.requestJson("/api/info");
            const branch = info.branch || "";
            const hasBranchOption = Array.prototype.some.call(app.elements.githubUpdateBranchInput.options, function (option) {
                return option.value === resolveGithubBranch(branch, "");
            });
            app.elements.githubUpdateBranchInput.value = hasBranchOption ? resolveGithubBranch(branch, "") : "auto";
        } catch (error) {
            console.error("Error fetching firmware info", error);
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
        try {
            const result = await window.MiOpenApi.postJson("/api/mqtt", {
                user: app.elements.mqttUserInput.value,
                server: app.elements.mqttServerInput.value,
                password: app.elements.mqttPasswordInput.value,
                port: app.elements.mqttPortInput.value,
                discovery: app.elements.mqttDiscoveryInput.value
            });
            app.logStatus(result.message || "MQTT settings updated.");
            showToastIfAvailable(result.message || "MQTT settings updated.", "success");
        } catch (error) {
            console.error("Error updating MQTT config", error);
            app.logStatus("Error updating MQTT config", true);
            showToastIfAvailable("Error updating MQTT config", "error", 6000);
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

    async function updateDisplayConfig(app) {
        if (!app.elements.displayEnabledInput) {
            return;
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
            setDisplayStatus(
                app,
                enabled
                    ? app.i18nText("status.display_saved_enabled", "Saved: display enabled")
                    : app.i18nText("status.display_saved_disabled", "Saved: display disabled")
            );
            app.logStatus(result.message || app.i18nText("log.display_updated", "Display settings updated."));
            showToastIfAvailable(result.message || app.i18nText("log.display_updated", "Display settings updated."), "success");
        } catch (error) {
            console.error("Error updating display config", error);
            app.elements.displayEnabledInput.checked = !requestedEnabled;
            setDisplayStatus(
                app,
                app.i18nText("status.display_save_error", "Saving display setting failed"),
                true
            );
            app.logStatus(app.i18nText("log.error_updating_display", "Error updating display config"), true);
            showToastIfAvailable(app.i18nText("log.error_updating_display", "Error updating display config"), "error", 6000);
        }
    }

    async function uploadSelectedFile(app, input, url, missingMessage, successMessage, refreshFn) {
        const file = input.files[0];
        if (!file) {
            app.logStatus(missingMessage, true);
            showToastIfAvailable(missingMessage, "error", 6000);
            return;
        }

        try {
            const result = await window.MiOpenApi.uploadFile(url, file);
            const message = result.message || successMessage;
            app.logStatus(message);
            showToastIfAvailable(message, "success");
            if (refreshFn) {
                await refreshFn();
            }
        } catch (error) {
            const message = error.message || successMessage;
            app.logStatus(message, true);
            showToastIfAvailable(message, "error", 6000);
        }
    }

    function init(app) {
        app.loadLastAddress = function () {
            return loadLastAddress(app);
        };
        app.loadFirmwareInfo = function () {
            return loadFirmwareInfo(app);
        };
        app.loadMqttConfig = function () {
            return loadMqttConfig(app);
        };
        app.updateMqttConfig = function () {
            return updateMqttConfig(app);
        };
        app.loadDisplayConfig = function () {
            return loadDisplayConfig(app);
        };
        app.updateDisplayConfig = function () {
            return updateDisplayConfig(app);
        };
        app.scrollToSettingsPanel = function (tabName) {
            return scrollToSettingsPanel(app, tabName);
        };
        app.restartDevice = function () {
            return restartDevice(app);
        };
        app.checkGithubUpdate = function () {
            return checkGithubUpdate(app);
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
        showSettingsTab(app, "mqtt");
    }

    window.MiOpenSettings = {
        init: init
    };
})();
