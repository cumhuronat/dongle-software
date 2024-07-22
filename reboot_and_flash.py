import serial
import os
import time
try:
    os.system('idf.py build')
    ser = serial.Serial("COM34", 123123, timeout=1)
    ser.close()

except serial.SerialException as e:
    error_message = str(e)
    if "could not open port" in error_message:
        print("Error: COM port not found")
    else:
        print(f"ESP32 rebooted into bootloader mode")
        time.sleep(1)
        build_folder = os.path.join(os.getcwd(), 'build')
        os.chdir(build_folder)
        os.system('python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset --connect-attempts 5 write_flash "@flash_args"')
        os.chdir('..')