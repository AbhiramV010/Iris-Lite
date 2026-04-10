# This is not intended for execution.
import RPi.GPIO as gpio
import time

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
    gpio.setup(pin, gpio.IN, pull_up_down=gpio.PUD_UP)
    
    try:
        gpio.add_event_detect(pin, gpio.FALLING, callback=update_time, bouncetime=200)
    except RuntimeError:
        pass 
    
    return (time.time() - last_trigger) < 10

if __name__ == "__main__":
    raise ImportError("This file is designed for import, not direct execution.")