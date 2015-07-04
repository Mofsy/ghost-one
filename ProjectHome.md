INSERT INTO gamelist (botid) VALUES ('1');

NOTICE: The SVN source is pretty dated, it is old as v1.45, few small glitches which are fixed in v1.46,v1.47 were not updated with source code to the SVN. There maybe a lot of works for you if you want to have it like v1.49 or latest. However some important files like ghost.exe, ghost.cfg, language.cfg, etc. are up-to-date.

USAGE & REGULATION:

If you are modifying the code based on my source to compile & make it to your own bot.
PLEASE AT LEAST ASK FOR COOPERATION OPPORTUNITY AND GIVE CREDITS TO THE DEVELOPER by doing the following:

- keep fakeplayer patch, you can disable if not use

- keep !owner & commands like !fp,!dfs,!df work

- the bot should have signs of the developer, at least !version to show v1.45 or previous ones with link to thegenmaps.tk

- keep the ban for download & leavers in X seconds.

PLEASE DON'T publish & re-distribute your modding version based on my source code, OK?
ALSO PLS BE ADVISED we shall not help your issues in your modification of the code and NOTICED by making it to your own bot, you'll have no more support from us.

PLS make Questions for Supports in http://thegenmaps.tk

Patches:
!votestart

!ff

!wim

!ready

Spam control

Garena Client Broadcast (GCBI patch)

mrotate + autohost addons (fixed: 06212012)

gamelist patch

GenPatch fakeplayers in lobby

GenPatch Display Garena RoomName

GenPatch Ban working on realms + Garena

GenPatch Temporary Owner

Game Display on bnet realms with refresh hacks

Basic fixes:

-+$PLAYER$ to start game spamming (bot\_playerbeforestartprintdelay)

-m\_GameOverTime >= 12s (Rehost after gameover in 12 sec instead of 30 by default)

-m\_LastAutoHostTime >= 15

-Specific admins from LAN

-Garena auto-hooking setup

- and much more

#notes for latest revision:
bnet.cpp
ghost.exe works
gamelist patched
mrotate fully works
GCBI works