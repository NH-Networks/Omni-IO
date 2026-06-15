(function () {
    function appendTwoWLog(app, message, isError) {
        if (!app.elements.twowLog) {
            return;
        }

        const line = document.createElement("p");
        line.textContent = message;
        if (isError) {
            line.classList.add("error");
        }
        app.elements.twowLog.appendChild(line);
        app.elements.twowLog.scrollTop = app.elements.twowLog.scrollHeight;
        while (app.elements.twowLog.children.length > 30) {
            app.elements.twowLog.removeChild(app.elements.twowLog.firstChild);
        }
    }

    function setTwoWStatus(app, message, isError) {
        if (!app.elements.twowStatus) {
            return;
        }

        app.elements.twowStatus.textContent = message;
        app.elements.twowStatus.classList.toggle("error", !!isError);
    }

    function applyTwoWStatus(app, status) {
        const rxCounter = Number(status.lastRxCounter || 0);
        const previousRxCounter = app.lastTwoWRxCounter || 0;
        if (app.elements.twowLastTx) {
            app.elements.twowLastTx.textContent = status.lastTxCommand || "-";
        }
        if (app.elements.twowLastResult) {
            app.elements.twowLastResult.textContent = status.lastTxResult || "-";
            app.elements.twowLastResult.classList.toggle("error", !!status.lastTxError);
        }
        if (app.elements.twowLastRx) {
            const rx = status.lastRxCmd
                ? [status.lastRxType || "2W", status.lastRxFrequency || "-", status.lastRxCmd, status.lastRxFrom || "-", status.lastRxTo || "-"].join(" | ")
                : "-";
            app.elements.twowLastRx.textContent = rx;
        }
        if (app.elements.twowLastData) {
            app.elements.twowLastData.textContent = status.lastRxData || "-";
        }
        if (Array.isArray(status.frames) && app.elements.twowLog) {
            const seenFrames = app.seenTwoWFrames || {};
            status.frames.forEach(function (frame) {
                const counter = Number(frame.counter || 0);
                if (!counter || seenFrames[counter]) {
                    return;
                }
                seenFrames[counter] = true;
                const line = [
                    frame.type || "2W",
                    "freq=" + (frame.frequency || "-"),
                    "cmd=" + (frame.cmd || "-"),
                    "from=" + (frame.from || "-"),
                    "to=" + (frame.to || "-"),
                    "data=" + (frame.data || "-")
                ].join(" ");
                appendTwoWLog(app, "< " + line);
            });
            app.seenTwoWFrames = seenFrames;
        } else if (status.lastRxCmd && rxCounter > previousRxCounter) {
            const line = [
                status.lastRxType || "2W",
                "freq=" + (status.lastRxFrequency || "-"),
                "cmd=" + status.lastRxCmd,
                "from=" + (status.lastRxFrom || "-"),
                "to=" + (status.lastRxTo || "-"),
                "data=" + (status.lastRxData || "-")
            ].join(" ");
            appendTwoWLog(app, "< " + line);
        }
        if (rxCounter > previousRxCounter) {
            app.lastTwoWRxCounter = rxCounter;
        }
        if (rxCounter && app.pendingTwoWResponse && rxCounter > app.pendingTwoWResponse.rxCounter) {
            clearTimeout(app.pendingTwoWResponse.timer);
            app.pendingTwoWResponse = null;
            setTwoWStatus(app, app.i18nText("status.twow_response_received", "2W antwoord ontvangen"));
        }
    }

    async function loadStatus(app) {
        try {
            const status = await window.MiOpenApi.requestJson("/api/2w/status");
            applyTwoWStatus(app, status || {});
        } catch (error) {
            appendTwoWLog(app, "Could not load 2W status", true);
        }
    }

    async function sendCommand(app, command) {
        const rxCounterBeforeSend = app.lastTwoWRxCounter || 0;
        setTwoWStatus(
            app,
            app.i18nText("status.twow_sending", "2W command is being sent...")
        );
        appendTwoWLog(app, "> " + command);

        try {
            const result = await window.MiOpenApi.postJson("/api/command", {
                command: command
            });
            const message = result.message || app.i18nText("status.twow_sent", "2W command sent");
            setTwoWStatus(app, message);
            appendTwoWLog(app, message);
            app.logStatus(message);
            if (command.indexOf("discover") === 0 || command === "pair2W" || command === "listen2W" || command === "listen2Wslow" || command === "powerOn" || command === "associate" || command === "midnight") {
                if (app.pendingTwoWResponse && app.pendingTwoWResponse.timer) {
                    clearTimeout(app.pendingTwoWResponse.timer);
                }
                app.pendingTwoWResponse = {
                    rxCounter: rxCounterBeforeSend,
                    timer: setTimeout(function () {
                        if (app.pendingTwoWResponse && app.pendingTwoWResponse.rxCounter === rxCounterBeforeSend) {
                            setTwoWStatus(app, app.i18nText("status.twow_no_response", "Geen 2W antwoord ontvangen"), true);
                            appendTwoWLog(app, app.i18nText("status.twow_no_response", "Geen 2W antwoord ontvangen"), true);
                            app.pendingTwoWResponse = null;
                        }
                    }, (command === "pair2W") ? 45000 : ((command === "listen2W" || command === "listen2Wslow") ? 30000 : 5000))
                };
            }
        } catch (error) {
            const message = error.message || app.i18nText("status.twow_send_error", "Sending 2W command failed");
            setTwoWStatus(app, message, true);
            appendTwoWLog(app, message, true);
            app.logStatus(message, true);
        }
    }

    function bindButton(app, element, commandFactory) {
        if (!element) {
            return;
        }

        element.addEventListener("click", function () {
            const command = commandFactory();
            if (!command) {
                setTwoWStatus(
                    app,
                    app.i18nText("status.twow_missing_value", "Vul eerst een waarde in"),
                    true
                );
                return;
            }
            sendCommand(app, command);
        });
    }

    function init(app) {
        app.sendTwoWCommand = function (command) {
            return sendCommand(app, command);
        };
        app.applyTwoWStatus = function (status) {
            applyTwoWStatus(app, status);
        };

        bindButton(app, app.elements.twowPowerOnButton, function () { return "powerOn"; });
        bindButton(app, app.elements.twowMidnightButton, function () { return "midnight"; });
        bindButton(app, app.elements.twowAssociateButton, function () { return "associate"; });
        bindButton(app, app.elements.twowAckButton, function () { return "ack"; });
        bindButton(app, app.elements.twowPairButton, function () { return "pair2W"; });
        bindButton(app, app.elements.twowListenButton, function () { return "listen2W"; });
        bindButton(app, app.elements.twowListenSlowButton, function () { return "listen2Wslow"; });
        bindButton(app, app.elements.twowDiscover28Button, function () { return "discover28"; });
        bindButton(app, app.elements.twowDiscover2AButton, function () { return "discover2A"; });
        bindButton(app, app.elements.twowFake0Button, function () { return "fake0"; });

        bindButton(app, app.elements.twowSetTempButton, function () {
            const value = (app.elements.twowTempInput.value || "").trim();
            return value ? "setTemp " + value : "";
        });
        bindButton(app, app.elements.twowSetModeButton, function () {
            return "setMode " + app.elements.twowModeInput.value;
        });
        bindButton(app, app.elements.twowSetPresenceButton, function () {
            return "setPresence " + app.elements.twowPresenceInput.value;
        });
        bindButton(app, app.elements.twowSetWindowButton, function () {
            return "setWindow " + app.elements.twowWindowInput.value;
        });
        bindButton(app, app.elements.twowSendCustomButton, function () {
            const value = (app.elements.twowCustomInput.value || "").trim();
            return value ? "custom " + value : "";
        });
        bindButton(app, app.elements.twowSendCustom60Button, function () {
            const value = (app.elements.twowCustom60Input.value || "").trim();
            return value ? "custom60 " + value : "";
        });

        loadStatus(app);
    }

    window.MiOpenTwoW = {
        init: init
    };
})();
