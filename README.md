This is a classic simon game where you are given a random sequence of buttons and have to match it

This game utilizes an LCD for instructions and displaying score,
4 buttons tied to 4 leds for the input,
a piezo buzzer using pwm to make diff pitches per button (currently a Bb7 chord),
on chip eeprom memory to score highscore

This is done with non blocking code with timers and buffers for ease of access

Multiple states include: Welcome screen, watching(displaying) the sequence, playing for storing user input, 
buffer function between ui and displaying sequence, countdown sequence, game over to display score and set high score if reached,
and sound testing state
