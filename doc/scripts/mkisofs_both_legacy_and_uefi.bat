set inputdir=C:\Dev\Temp\wim\windows10-17763-107-LTSC
set outputiso=C:\Dev\Temp\wim\mkisofs_iso\windows10-17763-107-LTSC-Virtio-Gfx.iso
set label="WIN10_17763_107_LTSC_VIRTIO_GFX"
set biosboot=boot/etfsboot.com
set efiboot=efi/microsoft/boot/efisys.bin
C:\Dev\Temp\wim\cdrtools-3.01.a23-bootcd.ru-mkisofs\mingw\mkisofs.exe -iso-level 4 -l -R -UDF -D -volid %label% -b %biosboot% -no-emul-boot -boot-load-size 8 -hide boot.catalog -eltorito-alt-boot -eltorito-platform efi -no-emul-boot -b %efiboot%  -o %outputiso% %inputdir%
