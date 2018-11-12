

if(!inited){
var previous = null;
var current_state = null;
var previous_state = null;
var inited;
    chrome.runtime.onMessage.addListener(function(message, sender, sendResponse) {

           var x = message.content[0] - 220, y = message.content[1] - 220
            elementMouseIsOver = document.elementFromPoint(x, y);
            if(elementMouseIsOver != null) {
            if(previous != null){
            previous.style.border = previous_state
            }
            previous_state = elementMouseIsOver.style.border
            elementMouseIsOver.style.border  = "thick solid #0000FF";
            previous = elementMouseIsOver
            }
    })
}

function init() {
    inited=true;
}

init()