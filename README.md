# IR-NEC-Format-Remote-
A TM4C123GHCPM Microcontroller(RedBoard) is used to learn and play NEC format commands.

-  When a remote button is pressed, the IR signal sent by the remote is captured by the sensor and sent to the RedBoard through input PA2. 
-  The captured signal is then analyzed at different time periods using edge triggered interrupt and Timer interrupt on the RedBoard to recognize which button on the remotewas pressed. 
-  The data and address bits of the signal are recorded in EEPROM memory of the RedBoard. 
-  To send an IR signal that matches the signal sent by a remote button press, an alternate function uses “pulse width modulation” to control the PB5 output  pin. 
-  The output signal sent by IR led is then modulated to match the signal sent when a button is pressed on the remote. 
-  A speaker circuit is also connected for convenience. If an IR signal is received with any errors, lower pitched tone is played that indicates an error, whereas if IR signal received has no errors, a higher pitch tone is enabled to indicate success.
