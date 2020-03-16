# RSGC ACES: Message Board

![Message Board](images/DESMessageBoard.jpg)

## Beginnings
As a Grade 10 student, O. Logush agreed to develop the code for a message board to be located over the door of the DES. I would develop the hardware.
Here's what we came up with in the Spring of 2016: [Message Board](http://darcy.rsgc.on.ca/ACES/PCBs/index.html#ScrollingMessageBoard).

## Native SPI Version
With time to spare over the COVID-19 MArch Break of 2020, I had time to revisit the firmware of the board with a view to developing a fast, library-free, native SPI version.

### Transitions  
A MAX7219 register level fade-to-black, coupled with software-based rise and fall shifts, provides a few transition options, randomly selected.
