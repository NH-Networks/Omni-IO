(function () {
    function findLinkedDeviceNames(app, remote) {
        if (!remote.devices || !remote.devices.length) {
            return "0 devices";
        }

        return remote.devices.map(function (deviceRef) {
            const device = app.state.devicesCache.find(function (candidate) {
                return candidate.id === deviceRef ||
                    candidate.name === deviceRef ||
                    candidate.description === deviceRef;
            });
            return device ? device.name : deviceRef;
        }).join(", ");
    }

    async function fetchAndDisplayRemotes(app) {
        const tbody = document.querySelector("#remote-table tbody");

        try {
            const remotes = await window.OmniIoApi.requestJson("/api/remotes");
            tbody.textContent = "";

            if (!remotes.length) {
                const row = document.createElement("tr");
                const cell = document.createElement("td");
                cell.colSpan = 4;
                cell.textContent = "No remotes available.";
                row.appendChild(cell);
                tbody.appendChild(row);
                return;
            }

            const fragment = document.createDocumentFragment();
            remotes.forEach(function (remote) {
                const row = document.createElement("tr");

                const idCell = document.createElement("td");
                const idTag = document.createElement("div");
                idTag.className = "remote-id";
                idTag.textContent = remote.id;
                idCell.appendChild(idTag);

                const nameCell = document.createElement("td");
                nameCell.textContent = remote.name;

                const devicesCell = document.createElement("td");
                devicesCell.textContent = findLinkedDeviceNames(app, remote);

                const actionsCell = document.createElement("td");
                const editButton = document.createElement("button");
                editButton.className = "btn edit bg";
                editButton.textContent = app.i18nText("button.edit", "Edit");
                editButton.addEventListener("click", function () {
                    editRemote(app, remote.id, remote.name);
                });
                actionsCell.appendChild(editButton);

                row.appendChild(idCell);
                row.appendChild(nameCell);
                row.appendChild(devicesCell);
                row.appendChild(actionsCell);
                fragment.appendChild(row);
            });

            tbody.appendChild(fragment);
        } catch (error) {
            console.error("Error fetching remotes:", error);
        }
    }

    function editRemote(app, remoteId, remoteName) {
        app.openPopup(
            app.i18nText("popup.edit_remote_title", "Edit Remote"),
            app.i18nText("popup.adjust_name_devices", "Adjust the name/devices:"),
            [
                app.i18nText("popup.remote_id", "Remote ID: {value}").replace("{value}", remoteId),
                app.i18nText("popup.remote_name", "Name: {value}").replace("{value}", remoteName)
            ],
            [""],
            {
                showInput: true,
                showDevicePopup: true,
                btnShowDelete: true,
                showSave: true,
                defaultValue: remoteName,
                pairLabel: app.i18nText("popup.link_unlink_remote", "link / Unlink the remote"),
                pairBtnName: app.i18nText("button.link", "Link"),
                unpairBtnName: app.i18nText("button.unlink", "Unlink"),
                onPair: async function (deviceId) {
                    try {
                        const result = await window.OmniIoApi.postJson("/api/command", {
                            command: "linkRemote " + remoteId + " " + deviceId
                        });
                        await fetchAndDisplayRemotes(app);
                    } catch (error) {
                    }
                },
                onUnpair: async function (deviceId) {
                    try {
                        const result = await window.OmniIoApi.postJson("/api/command", {
                            command: "unlinkRemote " + remoteId + " " + deviceId
                        });
                        await fetchAndDisplayRemotes(app);
                    } catch (error) {
                    }
                },
                onSave: async function (newName) {
                    if (!newName.trim() || newName === remoteName) {
                        return;
                    }

                    try {
                        const result = await window.OmniIoApi.postJson("/api/command", {
                            RemoteId: remoteId,
                            command: "editRemote " + newName
                        });
                        await fetchAndDisplayRemotes(app);
                    } catch (error) {
                        console.error("Error renaming remote:", error);
                    }
                },
                onDelete: async function () {
                    try {
                        const result = await window.OmniIoApi.postJson("/api/command", {
                            command: "delRemote " + remoteId
                        });
                        await fetchAndDisplayRemotes(app);
                    } catch (error) {
                    }
                }
            }
        );
    }

    function openAddRemotePopup(app) {
        app.openPopup(
            app.i18nText("popup.add_remote_title", "Add Remote"),
            app.i18nText("popup.remote_id_label", "Remote ID:"),
            [app.i18nText("popup.here_add_remote", "here add your remote")],
            [""],
            {
                showSave: true,
                showInput: true,
                showTiming: true,
                btnShowDelete: false,
                btnShowCancel: false,
                timingLabel: app.i18nText("popup.remote_name_label", "Remote Name:"),
                defaultValue: (app && app.elements && app.elements.lastAddrInput && app.elements.lastAddrInput.value) ? app.elements.lastAddrInput.value.trim() : "",
                onSave: async function (addr, newName) {
                    const id = addr.trim();
                    if (!id) {
                        return;
                    }
                    if (!newName.trim()) {
                        return;
                    }

                    try {
                        const result = await window.OmniIoApi.postJson("/api/command", {
                            command: "newRemote " + id + " " + newName
                        });
                        await fetchAndDisplayRemotes(app);
                    } catch (error) {
                    }
                }
            }
        );
    }

    function init(app) {
        app.fetchAndDisplayRemotes = function () {
            return fetchAndDisplayRemotes(app);
        };
        app.editRemote = function (remoteId, remoteName) {
            editRemote(app, remoteId, remoteName);
        };
        app.openAddRemotePopup = function () {
            openAddRemotePopup(app);
        };
        window.editRemote = app.editRemote;
    }

    window.OmniIoRemotes = {
        init: init
    };
    window.MiOpenRemotes = window.OmniIoRemotes;
})();
