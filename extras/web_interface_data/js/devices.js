(function () {
    async function runAction(app, deviceId, action) {
        const result = await window.MiOpenApi.postJson("/api/action", {
            deviceId: deviceId,
            action: action
        });
    }

    function updateDeviceFill(deviceId, percent, durationSeconds) {
        const deviceEl = document.querySelector('.device[data-id="' + deviceId + '"]');
        if (!deviceEl) {
            return;
        }

        if (typeof durationSeconds === "number" && durationSeconds > 0) {
            deviceEl.style.transitionDuration = durationSeconds.toFixed(2) + "s";
        } else {
            deviceEl.style.transitionDuration = "";
        }

        const clamped = Math.max(0, Math.min(100, Number(percent) || 0));
        deviceEl.style.background = "linear-gradient(to top, var(--color-input) " +
            clamped + "%, var(--color-accent3) " + clamped + "%)";
    }

    function setDeviceState(deviceId, state, source) {
        const deviceEl = document.querySelector('.device[data-id="' + deviceId + '"]');
        if (!deviceEl) {
            return;
        }

        const stateEl = deviceEl.querySelector(".device-state");
        if (!stateEl) {
            return;
        }

        const normalizedState = state || "STOP";
        const normalizedSource = source || "gateway";
        stateEl.textContent = normalizedState + " - " + normalizedSource;
        deviceEl.dataset.state = normalizedState;
        deviceEl.dataset.source = normalizedSource;
    }

    function applyDeviceAction(app, data) {
        if (!data || !data.id) {
            return;
        }

        const cached = app.state.devicesCache.find(function (device) {
            return device.id === data.id;
        });
        const action = String(data.action || "").toLowerCase();
        const state = data.state || data.action || "STOP";
        const source = data.source || "gateway";
        const current = typeof data.position !== "undefined" ? data.position : (cached ? cached.position : data.target);

        // Use travel_time from cache for smooth open/close animation
        let animDuration;
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
        const button = document.createElement("button");
        button.textContent = label;
        button.classList.add("btn", className);
        button.addEventListener("click", onClick);
        return button;
    }

    async function fetchAndDisplayDevices(app) {
        const deviceList = app.elements.deviceList;
        const deviceSelect = app.elements.commandDeviceSelect;

        try {
            const devices = await window.MiOpenApi.requestJson("/api/devices");
            app.state.devicesCache = devices;

            deviceList.textContent = "";
            deviceSelect.textContent = "";

            if (devices.length === 0) {
                const listItem = document.createElement("li");
                listItem.textContent = "No devices available.";
                deviceList.appendChild(listItem);
                return;
            }

            devices.forEach(function (device) {
                const nameSpan = document.createElement("span");
                nameSpan.textContent = device.name;

                // Live state label (Bug 1 fix: aanmaken zodat setDeviceState() het kan vinden)
                const stateSpan = document.createElement("span");
                stateSpan.classList.add("device-state");
                stateSpan.textContent = device.active === false ? "inactive" : "";

                const listItem = document.createElement("li");
                listItem.classList.add("device");
                listItem.dataset.id = device.id;
                listItem.appendChild(nameSpan);
                listItem.appendChild(stateSpan);

                listItem.appendChild(createDeviceButton("up", "open", function () {
                    runAction(app, device.id, "open").catch(function (error) {
                    });
                }));

                listItem.appendChild(createDeviceButton("stop", "stop", function () {
                    runAction(app, device.id, "stop").catch(function (error) {
                    });
                }));

                listItem.appendChild(createDeviceButton("down", "down", function () {
                    runAction(app, device.id, "close").catch(function (error) {
                    });
                }));

                listItem.appendChild(createDeviceButton(app.i18nText("button.edit", "edit"), "edit", async function () {
                    // Bug 5 fix: altijd verse data uit cache lezen, niet de stale closure-variabele
                    let currentDevice = app.state.devicesCache.find(function (d) {
                        return d.id === device.id;
                    }) || device;

                    try {
                        const freshDevices = await window.MiOpenApi.requestJson("/api/devices");
                        app.state.devicesCache = freshDevices;
                        const freshDevice = freshDevices.find(function (candidate) {
                            return candidate.id === device.id;
                        });
                        if (freshDevice) {
                            currentDevice = freshDevice;
                        }
                    } catch (error) {
                    }

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
                                        const renameResult = await window.MiOpenApi.postJson("/api/command", {
                                            deviceId: currentDevice.id,
                                            command: "edit1W " + newName
                                        });
                                    }

                                    const parsedTiming = parseInt(newTiming, 10);
                                    if (!isNaN(parsedTiming) && parsedTiming > 0 && parsedTiming !== currentDevice.travel_time) {
                                        const timeResult = await window.MiOpenApi.postJson("/api/command", {
                                            deviceId: currentDevice.id,
                                            command: "time1W " + parsedTiming
                                        });
                                    }

                                    await fetchAndDisplayDevices(app);
                                } catch (error) {
                                }
                            },
                            onPair: async function () {
                                try {
                                    const result = await window.MiOpenApi.postJson("/api/command", {
                                        deviceId: currentDevice.id,
                                        command: "add"
                                    });
                                    await fetchAndDisplayDevices(app);
                                } catch (error) {
                                }
                            },
                            onUnpair: async function () {
                                try {
                                    const result = await window.MiOpenApi.postJson("/api/command", {
                                        deviceId: currentDevice.id,
                                        command: "remove"
                                    });
                                    await fetchAndDisplayDevices(app);
                                } catch (error) {
                                }
                            },
                            onDelete: async function () {
                                const result = await window.MiOpenApi.postJson("/api/command", {
                                    deviceId: currentDevice.id,
                                    command: "del1W"
                                });
                                await fetchAndDisplayDevices(app);
                            }
                        }
                    );
                }));

                deviceList.appendChild(listItem);
                updateDeviceFill(device.id, device.position || 0);

                const option = document.createElement("option");
                option.value = device.id;
                option.textContent = device.name;
                deviceSelect.appendChild(option);
            });

        } catch (error) {
            console.error("Error fetching devices:", error);
        }
    }

    async function sendCommand(app) {
        const selectedDeviceId = app.elements.commandDeviceSelect.value;
        const commandStr = app.elements.commandInput.value.trim();

        if (!selectedDeviceId) {
            return;
        }
        if (!commandStr) {
            return;
        }

        try {
            const result = await window.MiOpenApi.postJson("/api/command", {
                deviceId: selectedDeviceId,
                command: commandStr
            });

            if (result.success) {
            } else {
            }
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
                        const result = await window.MiOpenApi.postJson("/api/command", {
                            command: "new1W " + newName
                        });
                        await fetchAndDisplayDevices(app);
                    } catch (error) {
                    }
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
