var i = 0
var ws;
if ("WebSocket" in window) {
  ws = new WebSocket("ws://localhost:37950");
  ws.onopen = function() {
    alert("connected")
  };
  ws.onmessage = function(evt){
   myarr = evt.data.trim().split(/\s+/);
   msg = myarr[0] +" " +myarr[1];

chrome.tabs.query({active: true, currentWindow: true}, function(tabs) {

chrome.tabs.executeScript(tabs[0].id, {
file: "source.js"
}, function() {

 chrome.tabs.sendMessage(tabs[0].id,{content: myarr});
});
})
}
}
