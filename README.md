# Overview

The general idea of this project is to integrate Tobii Eye Tracker with
internet browser (currently, only Chrome supported). This project is simple POC and there is still room for development and improvement.

The project is built with two components: Desktop application and Chrome plugin.
The use case is very simple: you run the desktop app and open the Chrome browser with plugin enabled.
Now, the plugin is able to catch the eyes' position and process the DOM of any page that is oppened in your tab.
For now, only simple blue border is displayed on element you're looking on.

The long term project that could base on current implementation could include:
- navigation through internet just by your eyes' blinks (one blink is click)
- collect the statistics of how many times we look at advertisements in the internet. What's the best location?
- zoom in on text we're currently looking (triggered by blinks)

## Requirements

- Windows OS
- [Tobii Eye Tracker](https://tobiigaming.com/products/) and [SDK](https://developer.tobii.com/consumer-eye-trackers/core-sdk/)
- [Visual Studio for C++](https://visualstudio.microsoft.com/vs/features/cplusplus/?rr=https%3A%2F%2Fwww.google.pl%2F)
- Chrome browser

## How to
Due to Tobii Eye Tracker's requirements, it's supported only on Windows platform.
1. Connect the Eye Tracker.
2. Just create release in Visual Studio and run it. 
There should be message displaed that it successfully connected to Eye Tracker.
3. Open the browser and [add extension](https://support.google.com/chrome_webstore/answer/2664769?hl=en) located in `plugin` directory.
4. Open new page and look around. HTML element you're looking at should be focused with blue border.

## How it works

Desktop application connects to Eye Tracker using their SDK. 
Additionally, it starts Websocket server on localhost.
Every time there is update from the device (many per second), the locations are sent through websocket's protocol on `http://localhost:37950`. This is all App's responsibility

Chrome plugin opens websocket protocol and listen on the same host, `http://localhost:37950` on incoming messages.
From each message it's able to get the coordinates and based on them, select the element located on those coordinates (normalised) and process it. As previously mentioned, currently only simple border is added.



