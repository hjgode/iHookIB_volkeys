Installation

Copy iHookIB_volkeys.exe to \Windows on the device
Copy iHookIB_volkeys.lnk to \Windows\StartUp on the device
Reboot the device to have iHookIB_volkeys.exe started
There will be a notification icon on the start screen and a beep on startup

When using VolUp or VolDn keys, the device volume will be increased or decreased

Intermec Browser will unregister F6/F7 on start and does not restore 
their assigments on exit.

iHookIB_volkeys.exe will restore the unregistered F6/F7 keys by registering 
F6/F7 to HHTaskBar, if RestoreVolKeys is 1.

If you press F6/F7 before Intermec Browser has been run one time after reboot, 
the standard Volume Control will be displayed when pressing F6/F7 
(aka VolUp/VolDn)). iHookIB_volkeys.exe does not catch and process the key 
presses if Intermec Browser is not running.

When Intermec Browser is running, iHookIB_volkeys.exe captures the VolUp/Dn keys
and changes the Volume setting.

After Intermec Browser is ended, iHookIB_volkeys.exe does not any more capture
the keys. The Volume can then be controlled outside Intermec Browser if 
RestoreVolKeys=1.

# Configuration

REGEDIT4

[HKEY_LOCAL_MACHINE\Software\Intermec\iHookIB_volkeys]
"RestoreVolKeys"=dword:00000001
"ForwardKeys"=dword:00000000
"ClassName"="IntermecBrowser"

- RestoreVolKeys: controls if Volume Key Assignment is restored to HHTaskBar on
Intermec Browser exit

- ForwardKeys: controls if captured keys are consumed by the app or forwarded to
OS after internal processing

- ClassName: Window Class name to watch for. May be changed to 
EnterpriseBrowser for newer Intermec Browser versions
