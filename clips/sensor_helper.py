# This is not intended for execution.
# Previously, the system as a whole would refuse to run if it's on windows (annoying for devs)
# This ensures that gpio functionality is disabled as a whole if the operating system is not Linux
import time
import platform

if (platform.system() == "Linux"):
    import RPi.GPIO as gpio
    last_trigger = 0

    def start_up(pin):
        gpio.setwarnings(False) # suppress useless warnings
        gpio.setmode(gpio.BCM)
        gpio.setup(pin, gpio.IN, pull_up_down=gpio.PUD_UP)

    def close_gpio(pin):
        gpio.cleanup(pin)

    def update_time(_):
        global last_trigger
        last_trigger = time.time()

    def check_gpio(pin):
        gpio.setmode(gpio.BCM)
        try: gpio.setup(pin, gpio.IN, pull_up_down=gpio.PUD_UP)
        except: pass
        
        try:
            gpio.add_event_detect(pin, gpio.FALLING, callback=update_time, bouncetime=200)
        except RuntimeError:
            pass 
        
        return (time.time() - last_trigger) < 10
else: 
    print("the system will run with GPIO functionality off, as no GPIO interface is detected")

if __name__ == "__main__":
    raise ImportError("This file is designed for import, not direct execution.")