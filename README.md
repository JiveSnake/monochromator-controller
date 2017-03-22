# monochromator-controller
Control program for a Monochromator

The program is for two Arduino's in a I2C Master-Slave set up.
The Master is also connected to a 2x16 LCD screen, a 4x4 keypad, a switch, and a gap wheel encoder.
The Slave is connected to a stepper motor.

The Master takes a starting wavelength, ending wavelength (both in nanometers) and a sweep time (in seconds). It converts the inputs into an encoder count and a velocity, and transmits the velocity to the Slave, which drives the stepper motor. When the encoder has counted the appropriate number of counts, the Master sends a halt command to the Slave and stops the stepper motor.
