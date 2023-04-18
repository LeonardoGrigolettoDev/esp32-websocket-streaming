![headline](/images/headline.jpg)


# esp32_camera_webstream
Bringing the ESP32 camera video stream to the web!


The Arduino ESP32-Camera test sketch only lets you use the stream on your local network. To get the stream to the web, you need a bit more...

This collection of scripts consists of:
 - Arduino code for ESP32 camera module (AI Thinker CAM) `websocket_camera_stream.ino`
 - Python code to receive the images via websockets with `receive_stream.py`
 - Python code to push the most recent image to a website with `send_image_stream.py`


 # Why is this cool?
I havent found a working repository that streams ESP32 camera images in real time to a web backend. This sovles this issue.

# How to run?
1. Open the ESP32 code in your Arduino IDE, install all missing libraries, change the `ssid`, `password` and `websockets_server_host`.
Upload the code to you ESP32 AI Thinker Cam board. Please test the Arduino camera example before you test this code!

2. Install the missing python requirements using pip: `pip install pillow websockets flask asyncio`

3. Run `python receive_stream.py`
You should get a constant stream of numbers (sizes of images). The image.jpg in the directory is always the latest received image.

4. Open a second terminal and run `python send_image_stream.py`
You should get a response by flask with an IP and port to enter in your browser.

Now enjoy your fresh live stream! 📺



# Known Issues
### Browsers don't like broken images.
Sometimes the stream stops and the preview freezes. This only happend to me on a Raspberry Pi with low processing power. Normal cloud server seem to work fine. If you encounter this problem, create a placeholder.jpg in the directory of send_image_stream.py by `cp image.jpg placeholder.jpg` after you received an image from the ESP32 camera.
This issus is caused, because your browser don't like to get a broken images.

### You have to have the right board.
There are many ESP32 Camera modules. The defined pins in `websocket_camera_stream.ino` only work with the AI Thinker Cam. Change this, if you have a different board. The only tested camera is currently the OV2640.
