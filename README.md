## Iot Project Fall 2018

# Internet Connected Coffee Maker

### Project Description

This project will consist of a coffee maker that is controlled by a Particle Photon device to automate when your coffee is made. This will be accomplished by initially linking the users Google Calender data to the Photon device so that it will start brewing their coffee a few minutes before they plan on waking up. Using a weight sensor it will record when the coffee is picked up and record that in a database. Over time data collected by the machine would be analized (e.g. through some service like ThingsSpeak) and displayed on the display located on the machine on or some web page (display on the machine is not a priority).

The particle will also be able to recieve inputs over the web to start brewing if the user decides they want a cup of coffee at some time other than the morning. The coffee maker will be able to function without the particle being connected to the internet either through its normal operation or by having the Photon have a seperate input acting as a hardware override. 

During brewing coffee the machine will indicate that by somesort of LED lights or (if implemented) some sort of display.

### Hardware 
The hardware necessary for this project will most likely be:
* a simple analog weight sensor 
* some sort of linear actuator or servo to turn on the machine
* the Photon board
* all the necessary electrical components to power the board using the same AC power source going to the machine.

### Optional features
If time allows, a display of somesort will be attached to the coffee maker and will display analized data collected by the machine. Also there is a possibility to implement showing weather data and time on that display, as well as 'loading' times for brewing machine.

