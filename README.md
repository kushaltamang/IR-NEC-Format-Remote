# IR-NEC-Format-Remote-
A TM4C123GHCPM Microcontroller(RedBoard) is used to learn and play NEC format commands.

-  When a remote button is pressed, the IR signal sent by the remote is captured by the sensor and sent to the RedBoard through input PA2. 
-  The captured signal is then analyzed at different time periods using edge triggered interrupt and Timer interrupt on the RedBoard to recognize which button on the remotewas pressed. 
-  The data and address bits of the signal are recorded in EEPROM memory of the RedBoard. 
-  To send an IR signal that matches the signal sent by a remote button press, an alternate function uses “pulse width modulation” to control the PB5 output  pin. 
-  The output signal sent by IR led is then modulated to match the signal sent when a button is pressed on the remote. 
-  A speaker circuit is also connected for convenience. If an IR signal is received with any errors, lower pitched tone is played that indicates an error, whereas if IR signal received has no errors, a higher pitch tone is enabled to indicate success.


A Putty Terminal at 115200 Baud Rate is used to send commands to the board. The following commands are supported:

1. decode - Waits to receive NEC remote signal press and then displays the address and data of the remote button pressed. (Optional) plays a “good”/ “bad” tone when NEC alert is enabled.
2. learn NAME - Receives an NEC IR command and stores as NAME with the associated address and data in EEPROM (e.g. learn ch-)
3. erase NAME - Erases command NAME (e.g. erase 1)
4. info NAME - Displays the address and data of the command NAME (e.g. info eq)
5. list commands - Lists the stored commands
6. play NAME1, NAME2, … NAME5 - Plays up to 5 learned commands (e.g. play ch-, 1, eq)
7. alert good|bad on|off - Turns the control the alert tone on and off for good and bad received IR commands (e.g., alert good on, alert bad off)
