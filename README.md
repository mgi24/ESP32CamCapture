How to use:
1. Open with arduino ide
2. import library ESP32
3. import library ESPAsyncWebServer https://github.com/me-no-dev/ESPAsyncWebServer
4. flash like usual
5. open http://private ip:8000 to start recording
6. don't forget to put sd card before start recording

alert: for some reason the connection is really slow at AP mode, probably because i turn on both access point and client mode at the same time, but recording performance ot affected by this
max recording at 0.5FPS, tested on generic 2GB SD Card, and not affected by change on resolution, maybe speed limitation on writing a new file.

Update: just delete the AP mode if you want faster wifi performance
