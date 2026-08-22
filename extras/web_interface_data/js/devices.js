(function () {
    // RAF throttle map: deviceId -> pending RAF id
    var _rafPending = {};

    async function runAction(app, deviceId, action) {
        await window.MiOpenApi.postJson("/api/action", {
            deviceId: deviceId,
            action: action
        });
    }

    function updateDeviceFill(deviceId, percent, durationSeconds) {
        // Cancel any pending RAF for this device — only the latest update matters
        if (_rafPending[deviceId]) {
            cancelAnimationFrame(_rafPending[deviceId]);
        }

        _rafPending[deviceId] = requestAnimationFrame(function () {
            delete _rafPending[deviceId];

            var deviceEl = document.querySelector('.device[data-id="' + deviceId + '"]');
            if (!deviceEl) {
                return;
            }

            if (typeof durationSeconds === "number" && durationSeconds > 0) {
                deviceEl.style.transitionDuration = durationSeconds.toFixed(2) + "s";
            } else {
                deviceEl.style.transitionDuration = "";
            }

            var clamped = Math.max(0, Math.min(100, Number(percent) || 0));
            deviceEl.style.background = "linear-gradient(to top, var(--color-input) " +
                clamped + "%, var(--color-accent3) " + clamped + "%)";
        });
    }

    function setDeviceState(deviceId, state, source) {
        var deviceEl = document.querySelector('.device[data-id="' + deviceId + '"]');
        if (!deviceEl) {
            return;
        }

        var stateEl = deviceEl.querySelector(".device-state");
        if (!stateEl) {
            return;
        }

        var normalizedState = state || "STOP";
        var normalizedSource = source || "gateway";
        stateEl.textContent = normalizedState + " - " + normalizedSource;
        deviceEl.dataset.state = normalizedState;
        deviceEl.dataset.source = normalizedSource;
    }

    function applyDeviceAction(app, data) {
        if (!data || !data.id) {
            return;
        }

        var cached = app.state.devicesCache.find(function (device) {
            return device.id === data.id;
        });
        var action = String(data.action || "").toLowerCase();
        var state = data.state || data.action || "STOP";
        var source = data.source || "gateway";
        var current = typeof data.position !== "undefined" ? data.position : (cached ? cached.position : data.target);

        var animDuration;
        if (action === "stop") {
            animDuration = 0.2;
        } else if (cached && cached.travel_time > 0) {
            animDuration = cached.travel_time;
        }

        updateDeviceFill(data.id, current, animDuration);
        setDeviceState(data.id, state, source);

        if (cached && typeof current !== "undefined") {
            cached.position = Math.max(0, Math.min(100, Number(current) || 0));
            cached.state = state;
            cached.source = source;
        }
    }

    function createDeviceButton(label, className, onClick) {
        var button = document.createElement("button");
        button.textContent = label;
        button.classList.add("btn", className);
        button.addEventListener("click", onClick);
        return button;
    }

    function buildDeviceListItem(app, device) {
        var nameSpan = document.createElement("span");
        nameSpan.textContent = device.name;

        var stateSpan = document.createElement("span");
        stateSpan.classList.add("device-state");
        stateSpan.textContent = device.active === false ? "inactive" : "";

        var listItem = document.createElement("li");
        listItem.classList.add("device");
        listItem.dataset.id = device.id;
        listItem.appendChild(nameSpan);
        listItem.appendChild(stateSpan);

        listItem.appendChild(createDeviceButton("up", "open", function () {
            runAction(app, device.id, "open").catch(function () {});
        }));
        listItem.appendChild(createDeviceButton("stop", "stop", function () {
            runAction(app, device.id, "stop").catch(function () {});
        }));
        listItem.appendChild(createDeviceButton("down", "down", function () {
            runAction(app, device.id, "close").catch(function () {});
        }));

        listItem.appendChild(createDeviceButton(app.i18nText("button.edit", "edit"), "edit", async function () {
            var currentDevice = app.state.devicesCache.find(function (d) {
                return d.id === device.id;
            }) || device;

            try {
                var freshDevices = await window.MiOpenApi.requestJson("/api/devices");
                app.state.devicesCache = freshDevices;
                var freshDevice = freshDevices.find(function (candidate) {
                    return candidate.id === device.id;
                });
                if (freshDevice) {
                    currentDevice = freshDevice;
                }
            } catch (error) {}

            app.openPopup(
                app.i18nText("popup.edit_device_title", "Edit Device"),
                app.i18nText("popup.adjust_name", "Adjust the name:"),
                [
                    app.i18nText("popup.info_id", "ID: {value}").replace("{value}", currentDevice.id),
                    app.i18nText("popup.info_description", "Description: {value}").replace("{value}", currentDevice.description || ""),
                    app.i18nText("popup.info_position", "Position: {value}%").replace("{value}", String(currentDevice.position)),
                    app.i18nText("popup.info_paired", "Paired: {value}").replace(
                        "{value}",
                        currentDevice.paired ? app.i18nText("value.yes", "Yes") : app.i18nText("value.no", "No")
                    )
                ],
                [""],
                {
                    showSave: true,
                    showInput: true,
                    showTiming: true,
                    btnShowDelete: true,
                    defaultValue: currentDevice.name,
                    defaultTiming: currentDevice.travel_time,
                    pairLabel: app.i18nText("popup.pair_label_device", "Add / Remove the device to the physical screen"),
                    deleteInfo: app.i18nText("popup.delete_device_info", "Only use when the device is not linked to a physical screen."),
                    onSave: async function (newName, newTiming) {
                        try {
                            if (newName.trim() && newName !== currentDevice.name) {
                                await window.MiOpenApi.postJson("/api/command", {
                                    deviceId: currentDevice.id,
                                    command: "edit1W " + newName
                                });
                            }
                            var parsedTiming = parseInt(newTiming, 10);
                            if (!isNaN(parsedTiming) && parsedTiming > 0 && parsedTiming !== currentDevice.travel_time) {
                                await window.MiOpenApi.postJson("/api/command", {
                                    deviceId: currentDevice.id,
                                    command: "time1W " + parsedTiming
                                });
                            }
                            await fetchAndDisplayDevices(app);
                        } catch (error) {}
                    },
                    onPair: async function () {
                        try {
                            await window.MiOpenApi.postJson("/api/command", {
                                deviceId: currentDevice.id,
                                command: "add"
                            });
                            await fetchAndDisplayDevices(app);
                        } catch (error) {}
                    },
                    onUnpair: async function () {
                        try {
                            await window.MiOpenApi.postJson("/api/command", {
                                deviceId: currentDevice.id,
                                command: "remove"
                            });
                            await fetchAndDisplayDevices(app);
                        } catch (error) {}
                    },
                    onDelete: async function () {
                        await window.MiOpenApi.postJson("/api/command", {
                            deviceId: currentDevice.id,
                            command: "del1W"
                        });
                        await fetchAndDisplayDevices(app);
                    }
                }
            );
        }));

        return listItem;
    }

    async function fetchAndDisplayDevices(app) {
        var deviceList = app.elements.deviceList;
        var deviceSelect = app.elements.commandDeviceSelect;

        // Show loading placeholder only when list is currently empty
        var isFirstLoad = deviceList.children.length === 0;
        if (isFirstLoad) {
            var placeholder = document.createElement("li");
            placeholder.id = "device-list-placeholder";
            placeholder.textContent = app.i18nText("log.loading_devices", "Loading devices...");
            deviceList.appendChild(placeholder);
        }

        try {
            var devices = await window.MiOpenApi.requestJson("/api/devices");
            app.state.devicesCache = devices;

            // Remove placeholder if present
            var ph = document.getElementById("device-list-placeholder");
            if (ph) {
                ph.parentNode.removeChild(ph);
            }

            // --- Diff-based update ---
            // Build map of incoming devices
            var incomingMap = {};
            devices.forEach(function (d) { incomingMap[d.id] = d; });

            // Remove list items that are no longer in the API response
            var existingItems = Array.from(deviceList.querySelectorAll("li.device"));
            existingItems.forEach(function (li) {
                if (!incomingMap[li.dataset.id]) {
                    deviceList.removeChild(li);
                }
            });

            // Update or insert devices in order
            var existingIds = Array.from(deviceList.querySelectorAll("li.device")).map(function (li) {
                return li.dataset.id;
            });

            if (devices.length === 0) {
                deviceList.textContent = "";
                var empty = document.createElement("li");
                empty.textContent = app.i18nText("list.no_devices_available", "No devices available.");
                deviceList.appendChild(empty);
            } else {
                devices.forEach(function (device, index) {
                    var existingLi = deviceList.querySelector('li.device[data-id="' + device.id + '"]');

                    if (existingLi) {
                        // Patch name if changed
                        var nameSpan = existingLi.querySelector("span:first-child");
                        if (nameSpan && nameSpan.textContent !== device.name) {
                            nameSpan.textContent = device.name;
                        }
                        // Patch state if inactive status changed
                        var stateSpan = existingLi.querySelector(".device-state");
                        if (stateSpan && device.active === false && stateSpan.textContent === "") {
                            stateSpan.textContent = "inactive";
                        } else if (stateSpan && device.active !== false && stateSpan.textContent === "inactive") {
                            stateSpan.textContent = "";
                        }
                        // Ensure correct DOM order
                        var items = deviceList.querySelectorAll("li.device");
                        if (items[index] !== existingLi) {
                            deviceList.insertBefore(existingLi, items[index] || null);
                        }
                    } else {
                        // New device: build and insert at correct position
                        var newLi = buildDeviceListItem(app, device);
                        var refNode = deviceList.querySelectorAll("li.device")[index] || null;
                        deviceList.insertBefore(newLi, refNode);
                    }

                    // Always sync fill/position
                    updateDeviceFill(device.id, device.position || 0);
                });
            }

            // Rebuild select (lightweight — only options, no complex DOM)
            deviceSelect.textContent = "";
            devices.forEach(function (device) {
                var option = document.createElement("option");
                option.value = device.id;
                option.textContent = device.name;
                deviceSelect.appendChild(option);
            });

        } catch (error) {
            var ph2 = document.getElementById("device-list-placeholder");
            if (ph2) {
                ph2.parentNode.removeChild(ph2);
            }
            console.error("Error fetching devices:", error);
        }
    }

    async function sendCommand(app) {
        var selectedDeviceId = app.elements.commandDeviceSelect.value;
        var commandStr = app.elements.commandInput.value.trim();

        if (!selectedDeviceId || !commandStr) {
            return;
        }

        try {
            await window.MiOpenApi.postJson("/api/command", {
                deviceId: selectedDeviceId,
                command: commandStr
            });
        } catch (error) {
            console.error("Error sending command:", error);
        }
    }

    function openAddDevicePopup(app) {
        app.openPopup(
            app.i18nText("popup.add_device_title", "Add Device"),
            app.i18nText("popup.new_device", "new device"),
            [app.i18nText("popup.here_add_device", "here add your device")],
            [""],
            {
                showSave: true,
                showInput: true,
                btnShowDelete: false,
                btnShowCancel: false,
                onSave: async function (newName) {
                    if (!newName.trim()) {
                        return;
                    }
                    try {
                        await window.MiOpenApi.postJson("/api/command", {
                            command: "new1W " + newName
                        });
                        await fetchAndDisplayDevices(app);
                    } catch (error) {}
                }
            }
        );
    }

    function init(app) {
        app.fetchAndDisplayDevices = function () {
            return fetchAndDisplayDevices(app);
        };
        app.sendCommand = function () {
            return sendCommand(app);
        };
        app.updateDeviceFill = updateDeviceFill;
        app.applyDeviceAction = function (data) {
            applyDeviceAction(app, data);
        };
        app.openAddDevicePopup = function () {
            openAddDevicePopup(app);
        };
    }

    window.MiOpenDevices = {
        init: init
    };
})();
