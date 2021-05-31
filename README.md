# Chicken-Coop-Minder

Here you will find basic documentation for an extremely basic chicken coop environmental monitoring and control system.

A basic design notes text file and a template Arduino sketch file are provided. The arduino sketch file has been scrubbed of personal information so be sure to fill in appropriate info for your configuration. Commentary in the sketch file should be helpful for this too.

The basic idea is that the chicken coop is provided with two heat lamps which automatically turn on when temperature measured inside the coop falls below a set threshold, and automatically turn off when it is above that threshold. A Chicken Guard brand pop door was installed and is controlled via a custom stepper-motor driven winch system to open at sunrise and close after sunset. Door closure state is determined via reed switch mounted between pop door and door frame. Sunrise and sunset information are determined first by obtaining Network Time Protocol info (get current time from the internet) and then feeding your chicken coop latitude and longitude into a sunrise/sunset determination algorithm. Finally, another temperature sensor monitors ambient air temperature outside of the coop.

Interior temperature, exterior temperature, door closure state, and current time determination are then fed to a Blynk app (https://blynk.io).
