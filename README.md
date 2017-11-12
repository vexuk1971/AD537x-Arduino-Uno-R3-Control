# AD537x-Arduino-Uno-R3-Control
Control an AD537x Eval Board with an Arduino - only Uno R3 supported yet

Created: 2017/10/9
Author: Ulrich.Kohlrusch

changed & improved from https://github.com/filiplindau/AD5370 by Filip Lindau & Kaustubh Banerje https://bitbucket.org/lamdacore/ad5372-eval-arduino-control

Changes:
- Dual operation on two boards is no longer possible, but the channel range has been increased from 32 (40) to 64 (80) if 
  two boards / chips are to be used 
- The Arduino SPI class is not included, I use the direct access
  ok, that is not necessarily Arduino compatible, but saves me memory and CPU time.
  Also, i don´t use any delay(n) functions

Improvements:
- It is possible to use both types of chips.
  #define determines which one it is (AD5372 or AD5370) - #define AD5370  or ...AD5372
  
- #define also determines how much boards / chips are used - #define ONECHIP
- all registers can now be addressed, except that only the data returns from the chip

Data Input format:

C = clear toggle on/off

R = reset Chip

L = load Chip Register

D = select Dac Register
O = select Offset Register
G = select Gain Register
F = select Special Function Register
S = Start Channel
E = End Channel
/ = spacer of line value input
n = End of line value input

P = prints selected Register example: DP (Dac Register + Print)
W = write from Array to selected Chip Register(except the SFR register), with or without specifying the start / end channel
    example: DW write the complete dac buffer into the chip or D10S20EW write from Channel 10 to 20 from Array Buffer to the
    Channel Dac Register
I = initial Array and write selected Register to Chip with a specified value (except the SFR register),
    with or without specifying the start / end channel
    example: D1000I write value 1000 in the complete dac buffer & into the chip or D20S10E1000I 
    write from Channel 20 downto 10 (yes, that is also possible :) ) value 1000 in Dac Buffer & Chip
    
Line Value Input format is: 
Register(-start channel optional) num 1 / num 2 / num 3/... and end of line symbol /n   
example: D1000/2000/3000/.../n  - write 1000, 2000, 3000 .. from channel 0 (default Channel) or
D10S1000/2000/3000/..../n - the same but this time from channel 10 upwards.
When the end of the Channel buffer is reached, it will continue automatically from channel 0 onwards.

Special Function Register format is:
F command# value example: F1#3 - command "Select SFR Control Register" with value 3 - means in the case:
soft power up, enable temperature shutdown and select register X1A for input

- todo: clean up any bugs and make the source code more readable, if possible ;)

 

