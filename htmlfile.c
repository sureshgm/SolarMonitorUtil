#include <sys/pgmspace.h>
static const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Solar Utility Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #4B1D3F; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .card.PVVoltage { color: #0e7c7b; }
    .card.PVCurrent { color: #17bebb; }
    .card.PVPower { color: #3fca6b; }
    .card.PVEnergy { color: #d62246; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>Solar Utility Monitor</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card PVVoltage">
        <h4><i class="fas fa-bolt"></i> PV_VOLTAGE</h4><p><span class="reading"><span id="temp">%PV_VOLTAGE%</span> V</span></p>
      </div>
      <div class="card PVCurrent">
        <h4><i class="fas fa-bolt"></i> PV_CURRENT</h4><p><span class="reading"><span id="hum">%PV_CURRENT%</span> A</span></p>
      </div>
      <div class="card PVPower">
        <h4><i class="fas fa-sun"></i> PV_POWER</h4><p><span class="reading"><span id="pres">%PV_POWER%</span> W</span></p>
      </div>
      <div class="card PVEnergy">
        <h4><i class="fas fa-solar-panel"></i> PV_ENERGY</h4><p><span class="reading"><span id="gas">%PV_ENERGY%</span> Wh</span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('PVVoltage', function(e) {
  console.log("PVVoltage", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('PVCurrent', function(e) {
  console.log("PVCurrent", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('PVPower', function(e) {
  console.log("PVPower", e.data);
  document.getElementById("pres").innerHTML = e.data;
 }, false);
 
 source.addEventListener('PVEnergy', function(e) {
  console.log("PVEnergy", e.data);
  document.getElementById("gas").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";
