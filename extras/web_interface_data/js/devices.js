(function () {
    // RAF throttle map: deviceId -> pending RAF id
    var _rafPending = {};

    async function runAction(app, deviceId, action) {
        await window.OmniIoApi.postJson("/api/action", {
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

    // Room / Group Local Storage Helpers
    function getDeviceRooms() {
        try {
            return JSON.parse(localStorage.getItem("omni_device_rooms") || "{}");
        } catch (e) {
            return {};
        }
    }

    function setDeviceRoom(deviceId, room) {
        var rooms = getDeviceRooms();
        if (room && room.trim()) {
            rooms[deviceId] = room.trim();
        } else {
            delete rooms[deviceId];
        }
        localStorage.setItem("omni_device_rooms", JSON.stringify(rooms));
    }

    function getAllUniqueRooms(devices) {
        var rooms = getDeviceRooms();
        var unique = {};
        (devices || []).forEach(function (d) {
            var r = rooms[d.id];
            if (r && r.trim()) {
                unique[r.trim()] = true;
            }
        });
        return Object.keys(unique).sort();
    }

    var activeRoomFilter = "all";

    function runGroupAction(app, deviceList, action) {
        deviceList.forEach(function (d) {
            runAction(app, d.id, action).catch(function () {});
        });
    }

    function renderRoomFilterBar(app, devices) {
        var bar = document.getElementById("room-filter-bar");
        if (!bar) return;

        var rooms = getDeviceRooms();
        var uniqueRooms = getAllUniqueRooms(devices);

        if (uniqueRooms.length === 0) {
            bar.style.display = "none";
            bar.textContent = "";
            return;
        }

        bar.style.display = "flex";
        bar.textContent = "";

        // "All" chip
        var allChip = document.createElement("button");
        allChip.type = "button";
        allChip.className = "room-chip" + (activeRoomFilter === "all" ? " active" : "");
        allChip.innerHTML = "<span>" + app.i18nText("filter.all_rooms", "All Rooms") + "</span> <span class='room-chip-count'>" + devices.length + "</span>";
        allChip.onclick = function () {
            activeRoomFilter = "all";
            fetchAndDisplayDevices(app);
        };
        bar.appendChild(allChip);

        // Room chips
        uniqueRooms.forEach(function (roomName) {
            var count = devices.filter(function (d) { return rooms[d.id] === roomName; }).length;
            var chip = document.createElement("button");
            chip.type = "button";
            chip.className = "room-chip" + (activeRoomFilter === roomName ? " active" : "");
            chip.innerHTML = "<span>" + roomName + "</span> <span class='room-chip-count'>" + count + "</span>";
            chip.onclick = function () {
                activeRoomFilter = roomName;
                fetchAndDisplayDevices(app);
            };
            bar.appendChild(chip);
        });

        // "Unassigned" chip
        var unassignedCount = devices.filter(function (d) { return !rooms[d.id]; }).length;
        if (unassignedCount > 0 && uniqueRooms.length > 0) {
            var unassignedChip = document.createElement("button");
            unassignedChip.type = "button";
            unassignedChip.className = "room-chip" + (activeRoomFilter === "__unassigned__" ? " active" : "");
            unassignedChip.innerHTML = "<span>" + app.i18nText("filter.unassigned", "Unassigned") + "</span> <span class='room-chip-count'>" + unassignedCount + "</span>";
            unassignedChip.onclick = function () {
                activeRoomFilter = "__unassigned__";
                fetchAndDisplayDevices(app);
            };
            bar.appendChild(unassignedChip);
        }
    }

    function buildDeviceListItem(app, device) {
        var nameContainer = document.createElement("div");
        nameContainer.style.display = "flex";
        nameContainer.style.flexDirection = "column";

        var nameSpan = document.createElement("span");
        nameSpan.textContent = device.name;
        nameContainer.appendChild(nameSpan);

        var rooms = getDeviceRooms();
        var roomName = rooms[device.id];
        if (roomName) {
            var roomBadge = document.createElement("span");
            roomBadge.classList.add("device-room-badge");
            roomBadge.textContent = "📍 " + roomName;
            nameContainer.appendChild(roomBadge);
        }

        var stateSpan = document.createElement("span");
        stateSpan.classList.add("device-state");
        stateSpan.textContent = device.active === false ? "inactive" : "";

        var listItem = document.createElement("li");
        listItem.classList.add("device");
        listItem.dataset.id = device.id;
        listItem.appendChild(nameContainer);
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
                var freshDevices = await window.OmniIoApi.requestJson("/api/devices");
                app.state.devicesCache = freshDevices;
                var freshDevice = freshDevices.find(function (candidate) {
                    return candidate.id === device.id;
                });
                if (freshDevice) {
                    currentDevice = freshDevice;
                }
            } catch (error) {}

            var currentRooms = getDeviceRooms();
            var existingRoom = currentRooms[currentDevice.id] || "";

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
                    showRoom: true,
                    btnShowDelete: true,
                    defaultValue: currentDevice.name,
                    defaultTiming: currentDevice.travel_time,
                    defaultRoom: existingRoom,
                    roomSuggestions: getAllUniqueRooms(app.state.devicesCache),
                    pairLabel: app.i18nText("popup.pair_label_device", "Add / Remove the device to the physical screen"),
                    deleteInfo: app.i18nText("popup.delete_device_info", "Only use when the device is not linked to a physical screen."),
                    onSave: async function (newName, newTiming, deviceVal, newRoom) {
                        try {
                            if (typeof newRoom !== "undefined") {
                                setDeviceRoom(currentDevice.id, newRoom);
                            }
                            if (newName && newName.trim() && newName !== currentDevice.name) {
                                await window.OmniIoApi.postJson("/api/command", {
                                    deviceId: currentDevice.id,
                                    command: "edit1W " + newName
                                });
                            }
                            var parsedTiming = parseInt(newTiming, 10);
                            if (!isNaN(parsedTiming) && parsedTiming > 0 && parsedTiming !== currentDevice.travel_time) {
                                await window.OmniIoApi.postJson("/api/command", {
                                    deviceId: currentDevice.id,
                                    command: "time1W " + parsedTiming
                                });
                            }
                            await fetchAndDisplayDevices(app);
                        } catch (error) {}
                    },
                    onPair: async function () {
                        try {
                            await window.OmniIoApi.postJson("/api/command", {
                                deviceId: currentDevice.id,
                                command: "add"
                            });
                            await fetchAndDisplayDevices(app);
                        } catch (error) {}
                    },
                    onUnpair: async function () {
                        try {
                            await window.OmniIoApi.postJson("/api/command", {
                                deviceId: currentDevice.id,
                                command: "remove"
                            });
                            await fetchAndDisplayDevices(app);
                        } catch (error) {}
                    },
                    onDelete: async function () {
                        setDeviceRoom(currentDevice.id, null);
                        await window.OmniIoApi.postJson("/api/command", {
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
            var devices = await window.OmniIoApi.requestJson("/api/devices");
            app.state.devicesCache = devices;

            // Remove placeholder if present
            var ph = document.getElementById("device-list-placeholder");
            if (ph) {
                ph.parentNode.removeChild(ph);
            }

            renderRoomFilterBar(app, devices);

            var rooms = getDeviceRooms();
            var displayedDevices = devices;
            if (activeRoomFilter === "__unassigned__") {
                displayedDevices = devices.filter(function (d) { return !rooms[d.id]; });
            } else if (activeRoomFilter !== "all") {
                displayedDevices = devices.filter(function (d) { return rooms[d.id] === activeRoomFilter; });
            }

            var groupContainer = document.getElementById("room-groups-container");
            if (groupContainer) {
                groupContainer.textContent = "";
                if (activeRoomFilter !== "all" && displayedDevices.length > 0) {
                    var groupBlock = document.createElement("div");
                    groupBlock.className = "room-group-header";

                    var groupTitle = document.createElement("div");
                    groupTitle.className = "room-group-title";
                    var currentTitle = activeRoomFilter === "__unassigned__" ? app.i18nText("filter.unassigned", "Unassigned") : activeRoomFilter;
                    groupTitle.innerHTML = "<span>📍 " + currentTitle + "</span> <span class='room-chip-count'>" + displayedDevices.length + "</span>";

                    var actions = document.createElement("div");
                    actions.className = "room-group-actions";

                    var openAllBtn = document.createElement("button");
                    openAllBtn.className = "btn open";
                    openAllBtn.title = app.i18nText("button.open_all", "Open All");
                    openAllBtn.textContent = "▲";
                    openAllBtn.onclick = function () { runGroupAction(app, displayedDevices, "open"); };

                    var stopAllBtn = document.createElement("button");
                    stopAllBtn.className = "btn stop";
                    stopAllBtn.title = app.i18nText("button.stop_all", "Stop All");
                    stopAllBtn.textContent = "■";
                    stopAllBtn.onclick = function () { runGroupAction(app, displayedDevices, "stop"); };

                    var closeAllBtn = document.createElement("button");
                    closeAllBtn.className = "btn down";
                    closeAllBtn.title = app.i18nText("button.close_all", "Close All");
                    closeAllBtn.textContent = "▼";
                    closeAllBtn.onclick = function () { runGroupAction(app, displayedDevices, "close"); };

                    actions.appendChild(openAllBtn);
                    actions.appendChild(stopAllBtn);
                    actions.appendChild(closeAllBtn);

                    groupBlock.appendChild(groupTitle);
                    groupBlock.appendChild(actions);
                    groupContainer.appendChild(groupBlock);
                }
            }

            // --- Diff-based update ---
            // Build map of incoming displayed devices
            var incomingMap = {};
            displayedDevices.forEach(function (d) { incomingMap[d.id] = d; });

            // Remove list items that are no longer in the displayed devices
            var existingItems = Array.from(deviceList.querySelectorAll("li.device"));
            existingItems.forEach(function (li) {
                if (!incomingMap[li.dataset.id]) {
                    deviceList.removeChild(li);
                }
            });

            if (displayedDevices.length === 0) {
                deviceList.textContent = "";
                var empty = document.createElement("li");
                empty.textContent = app.i18nText("list.no_devices_available", "No devices available.");
                deviceList.appendChild(empty);
            } else {
                displayedDevices.forEach(function (device, index) {
                    var existingLi = deviceList.querySelector('li.device[data-id="' + device.id + '"]');

                    if (existingLi) {
                        // Patch name and room badge
                        var nameSpan = existingLi.querySelector("span:first-child");
                        if (nameSpan && nameSpan.textContent !== device.name) {
                            nameSpan.textContent = device.name;
                        }
                        var roomBadge = existingLi.querySelector(".device-room-badge");
                        var assignedRoom = rooms[device.id];
                        if (assignedRoom && !roomBadge) {
                            var rb = document.createElement("span");
                            rb.className = "device-room-badge";
                            rb.textContent = "📍 " + assignedRoom;
                            var nameContainer = existingLi.querySelector("div");
                            if (nameContainer) nameContainer.appendChild(rb);
                        } else if (!assignedRoom && roomBadge) {
                            roomBadge.parentNode.removeChild(roomBadge);
                        } else if (assignedRoom && roomBadge && roomBadge.textContent !== "📍 " + assignedRoom) {
                            roomBadge.textContent = "📍 " + assignedRoom;
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
            await window.OmniIoApi.postJson("/api/command", {
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
                        await window.OmniIoApi.postJson("/api/command", {
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

    window.OmniIoDevices = {
        init: init
    };
    window.MiOpenDevices = window.OmniIoDevices;
})();
