REM VirtIO Driver Injection by Derek Seaman
REM www.derekseaman.com
REM Version 1.0
REM Place Windows boot.wim and install.wim in C:\Wim
REM Create a directory C:\Mount and leave it empty
REM Mount the VirtIO ISO to the D drive

REM Echo off

Set IDX=1

REM Modify boot.wim file to inject drivers
dism /Mount-Wim /WimFile:C:\Wim\boot.wim /Index:%IDX% /MountDir:C:\mount
dism /image:C:\mount /Add-Driver "/driver:d:\balloon\w10\amd64\balloon.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\NetKVM\w10\amd64\netkvm.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\viorng\w10\amd64\viorng.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\vioscsi\w10\amd64\vioscsi.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\vioserial\w10\amd64\vioser.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\viostor\w10\amd64\viostor.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\vioinput\w10\amd64\vioinput.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\cui_dch.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\HdBusExt.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\iigd_dch.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\IntcDAud.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\msdk.inf"
dism /unmount-wim /mountdir:c:\mount /commit

REM Modify install.wim to inject drivers
dism /Mount-Wim /WimFile:C:\WIM\install.wim /Index:%IDX% /MountDir:C:\mount
dism /image:C:\mount /Add-Driver "/driver:d:\balloon\w10\amd64\balloon.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\NetKVM\w10\amd64\netkvm.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\viorng\w10\amd64\viorng.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\vioscsi\w10\amd64\vioscsi.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\vioserial\w10\amd64\vioser.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\viostor\w10\amd64\viostor.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:d:\vioinput\w10\amd64\vioinput.inf" /forceunsigned
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\cui_dch.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\HdBusExt.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\iigd_dch.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\IntcDAud.inf"
dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\dch_win64_25.20.100.6444\Graphics\msdk.inf"
dism /unmount-wim /mountdir:c:\mount /commit



