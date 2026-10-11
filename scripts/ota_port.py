# The VS Code Upload button passes --upload-port COM9, which would override the
# WiFi address. Force the OTA address back after the command line is applied.
Import("env")

env.Replace(UPLOAD_PORT="ledboard.local")
