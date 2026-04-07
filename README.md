![Alt text](https://i.imgur.com/XESMs7T.png "Tarascii Madness")

This mod was created by: Stéphane "Milamber" Roblin. 
Email: milamber.nr@gmail.com

TarasciiMadness is a code modification of the game Jedi Knight Jedi Academy, based on the OpenJK project by the JACoders.


The mod offers a new gametype based on the Team Free For All gametype in Jedi Academy and was inspiried by "Suicide Barrels", a modification for Garry's Mod.

# The mod

* Two teams competing against eachother. A Barrel player team and a Human player team.

* Barrel team
	* Upon spawning as a barrel you possess one of many randomly placed barrels spread across the map.
	* Because these barrels are spread across the map you have the ability to hide in plain sight.
	* They attack using an explosive ability that detonates the barrel player one second after being triggered.
	* Has the goal to blow up every human player, turning them into barrel players, before the round timelimit has been reached.

* Human team
	* Fights using an instant shot sniper rifle with an increased reload time and zoom disabled.
	* Has the goal to survive until the round timer is reached. The round time is announced at the start of a round or can be found in the menu by pressing Escape -> About.
	* When they manage to shoot an enemy barrel the resulting explosion is smaller in range and damage.

* This mod does not required premade maps made specifically for the mod. The mod finds the size of the map and places barrels onto the maps valid surfaces. This happens for as long as there are available spots for it to find, and the entity limit of the engine itself has not been reached.

* These barrels relocate themselves between each round making it impossible to memorize each fake barrel's location.


## New admin commands
* **tm_debugAllowJoin** defaults to 0 valid value 1. Allows you to join a game without having a minimum of 2 players. Will interfere with normal gameplay so should be turned off when not running tests.
* **tm_barrelDensity** 1-100 valid values defaults to 25. This represents the percentage concentration of barrels on the map. If you feel the map has too many or too few barrels you can increase or decrease this number and restart the round using the default command map_restart. It's recommended to stay away from the higher percentages on smaller maps because a high concentration of barrels can be stressful for the game engine.
* **tm_rounds** defaults to 5. This is how many rounds you play on the current map before moving on to the next map in the map cycle, if no map cycle has been added to the server it will repeat the same map.
* **tm_reloadSpeed** defaults to 1500. This is reload speed in miliseconds, can be adjusted for balance in the gameplay.






#Installation:

##Windows:
In your Jedi Academy's GameData folder, create a TarasciiMadness folder and place the jampgamex86.dll in the folder. Add the commandline "+set fs_game TarasciiMadness" to load the mod on your server.

To install the clientplugin, which is required to avoid seeing lag in your own movement as a barrel player, create the same folder structure as mentioned above and place the cgamex86.dll in there. The mod will warn you if you're using an outdated version of the plugin compared to the server version, or if you don't have it installed at all.

##Linux:
Same procedure as Windows but use the jampgamei386.so and cgamei386.so files.