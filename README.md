# OpenAstroTech-HandController
Firmware for a handcontroller that plugs into the OAT/OAM and can be plugged into a PC as well.

##Status
Early stages of development, still designing and making a prototype/proof-of-concept.

##Concept

- Firmware reads LX200 commands from one Serial port (from PC) and queues them.  
- Firmware processes input devices and controller logic creates jobs that are also queued.
- When no messages are in flight, a job is dequeued and processed:
    - If the job command does not require a reply, the firmware is free to process the next job.
    - If it requires a reply, the firmware waits for the reply and sends it to the source of the job, either the PC connection or the controller logic.
- While messages are in flight, the controller logic still gets time slices to do controller things.

Currently the plan is to support an analog joystick and two buttons (Enter and Back) as well as a 128x64 OLED display screen (as supported by OAT firmware).
The board that will be used for the prototype (and the only one I have found that supports a USB-A Host port) is the [Adafruit Feather RP2040 with USB Type A Host](https://www.adafruit.com/product/5723)

If anyone has industrial design chops, the joystick, two buttons, and screen need a housing (even just a design). 
I was thinking similar to a video game controller, joystick on the left, two buttons on the right, RP2040 inside somewhere, screen in the middle maybe higher. Some kind of ergonomic grip?
