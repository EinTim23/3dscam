## Goofy ahh camera
So I was using droidcam yesterday and I wondered if the 3ds can achieve the same thing as it has a camera and networking. Turns out: yes. I wrote this shitty code that has a ton of bugs, only achieves around 2fps if your wifi connection is bad and it took way longer than i expected it to take. You can improve the performance of it drastically by making some minor improvements, but I will no longer work on this as I just wanted to get it working and dont need it either way. If you ever want to use this(hell nah) compile the 3dsx binary via devkitpro, put your 3ds ip into the windows client and run it once the homebrew started on your 3ds (it should display a very laggy live stream of your 3ds camera afterwards)


#### If you decide to improve this for some reason please contact me im interested in seeing it




































This code also contains a memory leak because i never free the old directx 11 texture before replacing the frame :hehe:
