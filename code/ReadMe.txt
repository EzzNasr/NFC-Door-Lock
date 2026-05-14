Before you upload your code, you’ll need to install the RFID library which is bundled with the sketch zip file. This is easily done in your Arduino IDE by clicking on Sketch -> Include Library -> Add .ZIP Library and the selecting the zipped library file.

In the code we first include the required libraries and then set up the sensor object and an array to return the read tag serial number.

The next array and its associated size is used to store the serial numbers for all of the tags which you’d like to grant access to. You’ll need to find and update these numbers using the serial monitor by uploading this code and then scanning your tags. The Serial monitor will display the tag’s serial number and then state that access is denied. Copy this number into the array accessGranted, update the array size (number of tags registered) and then re-upload the code. You could also write a short section to enable you to register a new tag by pushing a button inside the sensor component box or inside the door for example.

We then set up the servo object and it’s travel limits. You may need to make adjustments to these limits to get your servo to move through it’s full range without over-travelling in either direction.

In the setup code, we connect to the RFID sensor, define the LED pins and then run through a quick LED flash startup sequence before making sure that the lock is in the locked position. You can remove the Serial monitor output lines in the code in your final version, these are just useful for registering your tags and debugging the system when you first assemble it.

We then run through the loop which waits for a card or tag to be scanned, determines its serial number and then passes this serial number through to a function called checkAccess to verify whether the tag number can grant access or not.

The checkAccess function simply takes the read tag number and then cycles through the array of accepted numbers to see if it is an accepted tag. If a match is found then the green LED is flashed and the lock is either opened or closed, depending on the previous state. If the tag number is not found in the array then the red LED is flashed and the lock is not opened.

There are a couple of ways to make this lock a bit more secure if you’re actually going to be using it to secure a room or cupboard.

Start by replacing the 3D printed lock with a proper metal lock from a hardware store. Make sure that you have a solid connection between the lock and the servo and try to position the servo such that the arm is in line with the push-rod and the head of the slider when it is in the locked position. This will ensure that you can’t slip a thin object through the gap in the door and try to push the slider open, you’ll be pushing against the centre of the servo and not relying on the torque provided by the servo to keep the slider in place.

Next, place as few of the electronic components outside as possible. It is better to have the actual Arduino and servo connection inside the room or box and place only the RFID sensor and LEDs outside. It’s much more difficult to trick the Arduino into opening the lock using the RFID sensor connection than it is to simply provide a PWM signal to the servo to unlock the door.
