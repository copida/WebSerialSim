#pragma once

const char serial_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Web Serial Monitor (SSE)</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: 'Courier New', Courier, monospace;
            background-color: #1e1e1e;
            color: #33ff33;
            padding: 20px;
            margin: 0;
            display: flex;
            flex-direction: column;
            height: 100vh;
        }

        .header-container {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #33ff33;
            padding-bottom: 10px;
            margin-bottom: 10px;
            flex-shrink: 0;
        }
        h1 {
            color: #ffffff;
            font-size: 1.4rem;
            margin: 0;
        }
        .controls-group {
            display: flex;
            align-items: center;
            gap: 20px;
            font-size: 0.9rem;
            color: #ffffff;
        }
        .checkbox-container {
            display: flex;
            align-items: center;
            gap: 8px;
            cursor: pointer;
            user-select: none;
        }
        .checkbox-container input {
            cursor: pointer;
            width: 16px;
            height: 16px;
        }

        /* Terminale */
        #serial-terminal {
            flex-grow: 1;
            background-color: #000000;
            border: 1px solid #333333;
            border-radius: 4px;
            overflow-y: auto;
            padding: 12px;
            white-space: pre-wrap;
            word-break: break-all;
            margin-bottom: 10px;
        }

        .log-line {
            min-height: 1.2em;
            line-height: 1.3;
        }
        .show-linenum .log-line::before {
            content: "[" attr(data-line) "] ";
            color: #888888;
        }

        /* Scrollbar Custom */
        #serial-terminal::-webkit-scrollbar { width: 8px; }
        #serial-terminal::-webkit-scrollbar-track { background: #0a0a0a; }
        #serial-terminal::-webkit-scrollbar-thumb { background: #333; border-radius: 4px; }
        #serial-terminal::-webkit-scrollbar-thumb:hover { background: #555; }

        /* Quick Action Toolbar per HISTORY */
        .history-bar {
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
            margin-bottom: 10px;
            flex-shrink: 0;
            background-color: #121212;
            padding: 8px;
            border-radius: 4px;
            border: 1px solid #2a2a2a;
            align-items: center;
        }
        .history-label {
            color: #aaa;
            font-size: 0.85rem;
            font-weight: bold;
            margin-right: 5px;
        }

        /* Input e Pulsanti Bar */
        .input-bar {
            display: flex;
            gap: 10px;
            flex-shrink: 0;
        }
        input[type=text] {
            flex-grow: 1;
            background-color: #121212;
            color: #33ff33;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 10px;
            font-family: inherit;
            font-size: 0.95rem;
            outline: none;
        }
        input[type=text]:focus {
            border-color: #33ff33;
        }
        .btn {
            padding: 8px 14px;
            background-color: #2a2a2a;
            color: #fff;
            border: 1px solid #444;
            border-radius: 4px;
            cursor: pointer;
            font-family: inherit;
            font-size: 0.85rem;
            transition: background-color 0.15s ease, border-color 0.15s ease;
        }
        .btn:hover { background-color: #3d3d3d; }
        
        /* Bottoni Azione Specifica */
        .btn-cmd { border-color: #0088cc; color: #66ccff; }
        .btn-cmd:hover { background-color: #004466; }
        
        .btn-danger { border-color: #882222; color: #ff8888; }
        .btn-danger:hover { background-color: #551111; }

        .btn-clear { border-color: #555; }
        .btn-clear:hover { background-color: #444; }
    </style>
</head>
<body>

    <div class="header-container">
        <h1>ESP32 Web Serial & BLE Monitor</h1>

        <div class="controls-group">
            <label class="checkbox-container">
                <input type="checkbox" id="autoscroll-check" checked>
                Auto-scroll
            </label>

            <label class="checkbox-container">
                <input type="checkbox" id="shownum-check" onchange="toggleLineNumbers(this.checked)">
                Mostra Numerazione
            </label>

            <div>Utenti: <span id="counter-val" style="color: #66ff66; font-weight: bold;">1</span></div>
        </div>
    </div>

    <div id="serial-terminal">--- Inizializzazione Monitor ---</div>

    <!-- Toolbar per la gestione rapida della HISTORY -->
    <div class="history-bar">
        <span class="history-label">HISTORY:</span>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY ON')">ON</button>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY OFF')">OFF</button>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY INFO')">INFO</button>
		<!--<button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY VIEW')">VIEW</button>--->
				<button class="btn btn-cmd" onclick="window.open('/view-buffer', '_blank');">VIEW</button>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY FLUSH')">FLUSH (SD)</button>
        <button class="btn btn-danger" onclick="sendDirectCommand('HISTORY CLEAR')">CLEAR</button>
    </div>

    <!-- Barra Input Comandi -->
    <div class="input-bar">
        <input type="text" id="commandInput" placeholder="Inserisci comando (es. HELP, HISTORY INFO)..." autocomplete="off">
        <button class="btn btn-cmd" onclick="sendCommand()">Invia</button>
        <button class="btn btn-clear" onclick="clearMonitor()">Pulisci Schermo</button>
    </div>

<script>
if (!!window.EventSource) {
    const source = new EventSource('/events/serial');
    const terminal = document.getElementById('serial-terminal');
    const autoscrollCheck = document.getElementById('autoscroll-check');
    const counterVal = document.getElementById('counter-val');
    
    let myClientId = null;
    let rigaNumero = 0;
    let scrollInAttesa = false;

    function toggleLineNumbers(show) {
        if (!terminal) return;
        if (show) {
            terminal.classList.add('show-linenum');
        } else {
            terminal.classList.remove('show-linenum');
        }
    }
    
    async function requestNumClients() {
        try {
            const response = await fetch('/get-clientcount');
            if (response.ok) {
                const count = await response.text();
                if (counterVal) counterVal.textContent = count;
            }
        } catch (err) {
            console.warn("Impossibile recuperare il conteggio client via GET:", err);
        }
    }
    
    function appendToTerminal(testo) {
        if (!terminal) return;
        
        const righeInArrivo = testo.split("\n");
        const fragment = document.createDocumentFragment();
        
        righeInArrivo.forEach(function(riga) {
            rigaNumero++;
            
            const nuovaRigaDiv = document.createElement("div");
            nuovaRigaDiv.className = "log-line";
            nuovaRigaDiv.setAttribute("data-line", rigaNumero);
            nuovaRigaDiv.textContent = riga;
            
            fragment.appendChild(nuovaRigaDiv);
        });

        terminal.appendChild(fragment);
                
        // Manutenzione DOM (limite 2000 righe per fluidità del browser)
        while (terminal.children.length > 2000) {
            terminal.removeChild(terminal.firstChild);
        }
        
        requestScrollToBottom();
    }
    
    // --- EVENT LISTENER SSE ---
    
    source.addEventListener('message', function(e) {
        if (e.data.startsWith("ID_ASSEGNATO:")) {
            myClientId = e.data.split(":")[1];
            console.log("ID Client assegnato: " + myClientId);
        }
    });
    
    source.addEventListener('open', function(e) {
        appendToTerminal("[INFO] Connessione seriale stabilita.");
        requestNumClients();
    }, false);
    
    source.addEventListener('error', function(e) {
        if (e.readyState === EventSource.CLOSED) {
            appendToTerminal("[ERRORE] Connessione interrotta.");
        }
    }, false);
    
    source.addEventListener('serial_print', function(e) {
        appendToTerminal(e.data);
    }, false);
    
    source.addEventListener('client_count', function(e) {
        if (counterVal) counterVal.textContent = e.data;
    }, false);
    
    // --- UTILITIES E INVIO COMANDI ---
    
    function requestScrollToBottom() {
        if (!autoscrollCheck || !autoscrollCheck.checked) return;
        if (scrollInAttesa) return;
        
        scrollInAttesa = true;
        requestAnimationFrame(function() {
            terminal.scrollTop = terminal.scrollHeight;
            scrollInAttesa = false;
        });
    }
    
    function clearMonitor() {
        if (terminal) terminal.innerHTML = "";
        rigaNumero = 0;
    }
    
    // Invio da input di testo
    function sendCommand() {
        const inputElement = document.getElementById('commandInput');
        if (!inputElement) return;
        
        const val = inputElement.value.trim();
        if (val === "") return;

        executePost(val);
        inputElement.value = '';
    }

    // Invio diretto tramite pulsanti della toolbar
    function sendDirectCommand(cmdString) {
        executePost(cmdString);
    }

    // Helper per inviare la richiesta POST all'ESP32
    function executePost(commandText) {
        const payload = (myClientId ? myClientId : "0") + ':' + commandText;
        
        fetch('/parsingCmd', {
            method: 'POST',
            headers: { 'Content-Type': 'text/plain' },
            body: payload
        }).catch(error => {
            console.error('Errore invio comando:', error);
        });
    }
    
    document.getElementById('commandInput')?.addEventListener('keydown', function(event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            sendCommand();
        }
    });
    
} else {
    alert("Il tuo browser non supporta Server-Sent Events (SSE).");
}
</script>
</body>
</html>
)rawliteral";
