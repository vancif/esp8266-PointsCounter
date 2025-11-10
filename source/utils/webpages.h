const char GAME_HTML_PAGE[] PROGMEM = R"rawliteral(
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1.0" /></head>
<body>
<h2>Points Counter Game</h2>
<input maxlength="6" size="10" type="text" id="n0"><input maxlength="2" size="3" type="text" id="p0"><br><br>
<input maxlength="6" size="10" type="text" id="n1"><input maxlength="2" size="3" type="text" id="p1"><br><br>
<input maxlength="6" size="10" type="text" id="n2"><input maxlength="2" size="3" type="text" id="p2"><br><br>
<input maxlength="6" size="10" type="text" id="n3"><input maxlength="2" size="3" type="text" id="p3"><br><br>
<button onclick="send()">Update</button><br><br>
<button onclick="operate('1')" style="width:75px;height:75px;font:15pt Arial;">+</button>
<button onclick="operate('2')" style="width:75px;height:75px;font:15pt Arial;">-</button><br><br>
<button onclick="operate('3')" style="width:75px;height:75px;font:15pt Arial;">CH</button>
<button onclick="operate('4')" style="width:75px;height:75px;font:15pt Arial;">ENT</button>
<button onclick="operate('5')" style="width:75px;height:75px;font:15pt Arial;">OPT</button><br><br>
<a href="/wifi">WiFi Settings</a>
<script>
function operate(act){
  if(window.navigator.vibrate) window.navigator.vibrate(100);
  var data = new FormData();
  data.append("action", act);
  data.append("create", "false");
  fetch("/update", {method: "POST", body: data});
}
function send(){
  if(window.navigator.vibrate) window.navigator.vibrate(100);
  var data = new FormData();
  data.append("create", "true");
  for(let i = 0; i < 4; i++){
    data.append("n" + i, document.getElementById("n" + i).value);
    data.append("p" + i, document.getElementById("p" + i).value);
  }
  fetch("/update", {method: "POST", body: data});
}
</script>
</body>
</html>
)rawliteral";

const char WIFI_HTML_PAGE[] PROGMEM = R"rawliteral(
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>WiFi Configuration</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    .wifi-item { background: #f0f0f0; margin: 10px 0; padding: 10px; border-radius: 5px; }
    .wifi-form { background: #e0e0e0; padding: 15px; border-radius: 5px; margin: 20px 0; }
    input[type="text"], input[type="password"] { width: 200px; padding: 5px; margin: 5px; }
    button { padding: 10px 15px; margin: 5px; }
    .delete-btn { background: #ff4444; color: white; }
  </style>
</head>
<body>
<h2>WiFi Configuration</h2>

<div id="availableNetworks">
  <h3>Available Networks:</h3>
  <!-- Networks will be populated here -->
  <button onclick="scanNetworks()">Scan for Networks</button>
</div>

<div id="savedNetworks">
  <h3>Saved Networks:</h3>
  <!-- Networks will be populated here -->
</div>

<div class="wifi-form">
  <h3>Add New Network:</h3>
  <form id="wifiForm">
    <label>SSID:</label><br>
    <input type="text" id="newSsid" name="ssid" maxlength="32" required><br>
    <label>Password:</label><br>
    <input type="password" id="newPassword" name="password" maxlength="32"><br><br>
    <button type="submit">Add Network</button>
  </form>
</div>

<div>
  <button onclick="window.location.href='/'">Back to Game</button>
  <button onclick="reboot()">Reboot Device</button>
</div>

<script>
// Load saved networks
function loadNetworks() {
  fetch('/wifi/list')
    .then(response => response.json())
    .then(data => {
      const container = document.getElementById('savedNetworks');
      let html = '<h3>Saved Networks:</h3>';
      
      if (data.networks && data.networks.length > 0) {
        data.networks.forEach((network, index) => {
          html += `
            <div class="wifi-item">
              <strong>${network.ssid}</strong>
              <button class="delete-btn" onclick="deleteNetwork(${index})">Delete</button>
            </div>`;
        });
      } else {
        html += '<p>No networks saved</p>';
      }
      container.innerHTML = html;
    });
}

// Add new network
document.getElementById('wifiForm').addEventListener('submit', function(e) {
  e.preventDefault();
  const ssid = document.getElementById('newSsid').value;
  const password = document.getElementById('newPassword').value;
  
  const data = new FormData();
  data.append('ssid', ssid);
  data.append('password', password);
  
  fetch('/wifi/add', {method: 'POST', body: data})
    .then(response => response.json())
    .then(result => {
      if (result.success) {
        document.getElementById('newSsid').value = '';
        document.getElementById('newPassword').value = '';
        loadNetworks();
        alert('Network added successfully!');
      } else {
        alert('Error: ' + result.message);
      }
    });
});

// Delete network
function deleteNetwork(index) {
  if (confirm('Are you sure you want to delete this network?')) {
    const data = new FormData();
    data.append('index', index);
    
    fetch('/wifi/delete', {method: 'POST', body: data})
      .then(response => response.json())
      .then(result => {
        if (result.success) {
          loadNetworks();
          alert('Network deleted successfully!');
        } else {
          alert('Error: ' + result.message);
        }
      });
  }
}

// populate network SSID on click
function populateNetwork(ssid) {
  document.getElementById('newSsid').value = ssid;
}

// Scan networks
function scanNetworks() {
  fetch('/wifi/scan')
    .then(response => response.json())
    .then(data => {
      let ssidList = 'Available Networks:<br>';
      data.networks.forEach(network => {
        ssidList += '<span>' + network.ssid + ' - RSSI: ' + network.rssi + 'dBm</span><button onclick="populateNetwork(\'' + network.ssid + '\')">Connect</button><br>';
      });
      document.getElementById('availableNetworks').innerHTML = '<h3>Available Networks:</h3><pre>' + ssidList + '</pre>';
    });
}

// Reboot device
function reboot() {
  if (confirm('Are you sure you want to reboot the device?')) {
    fetch('/reboot', {method: 'POST'})
      .then(() => {
        alert('Device rebooting... Please wait and reconnect.');
      });
  }
}

// Load networks on page load
loadNetworks();
</script>
</body>
</html>
)rawliteral";