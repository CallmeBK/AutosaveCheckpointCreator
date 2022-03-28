# Preface
This project was requested by aliihsanasl.  I saw it as a great opportunity to learn some I/O functionality as well as to understand more about ps vita functionality.

# AutosaveCheckpointCreator
User plugin for PS Vita.  Tested on vita firmware 3.60 and Henkaku Enso.  Also supports PSTV.

# Installation Instructions
1) Download zip from latest release  
2) Copy AutosaveCheckpointCreator.suprx to ur0:/tai folder on ps vita  
3) Put "ur0:/tai/AutosaveCheckpointCreator.suprx" (without quotes) under main section in Config.txt file  
4) Reboot vita  

# What it does
Copies game savedata to ux0:temp folder.  If a game autosaves over progress you did not want overwritten, copy the savedata that was saved to ux0:temp and use it to overwrite the game's savedata in ux0:user/00/savedata to bring you back to your "checkpoint".  

# How to use
Press Select, Square, and Circle simultaneously while playing a game.  
The game will close down so that all savedata can be properly copied.  (Without this step, sce_pfs will not be copied).  
The game will restart itself automatically when the copying process has finished.  Usually takes 5-10 seconds.

# Credits

aliihsanasl  
-for the idea to create this plugin.  
-for help with testing and suggestions.  

TheOfficialFlow  
-for various VitaShell functions related to copying and deleting files and folders  

Rinnegatamante  
-for taipool.  Allowing me to allocate memory in a user plugin.  

Princess of Sleeping  
-for informing me that ScePfsMgr hides the sce_pfs folder if that game's savedata folder is mounted  

gl33ntwine  
-for informing me that system mode apps can run alongside normal apps, but normal apps cannot run alongside each other.
