ZBT WE826-16M Wireless issues fix

## Disclaimer

Warranty void if you do anything with your device. I'm not responsible for any damage you do to your device.

## 1. History

I got box of free ZBT WE826-16M routers from a customer. They were flashed by very old OpenWRT had issue with wifi signal (too weak 2Ghz and 5Ghz not working at all) and reflashing with latest OpenWRT didn't help.

## 2. Hardware details

* 2Ghz wireless provided by onboard Mediatek MT7620.
* 5Ghz wireless provided by Mediatek MT76x2(?) miniPCIe card.

## 3. Fixes for 2Ghz wireless

2Ghz might have invalid eeprom data, you can find eeprom backups various projects. I got one from padavan-ng: MT7620_EEPROM_layout_20131101.bin or wive-ng: MT7620_AP_2T2R-4L_internal_LNA_internal_PA_V15.bin
You might need to edit default MAC address in it, it starts at offset 0x0004.
scp it to router /tmp directory and write it to eeprom with following commands:
```
opkg update
opkg install kmod-mtd-rw
insmod mtd-rw i_want_a_brick=1
# backup current eeprom
dd if=/dev/mtdblock2 of=eeprom.bin
# overwrite initial block (MT7620A) with eeprom data
dd if=MT7620_AP_2T2R-4L_internal_LNA_internal_PA_V15.bin of=eeprom.bin conv=notrunc
mtd write eeprom.bin factory
```
After reboot you should see 2Ghz wireless working.

## 4. Fixes for 5Ghz wireless

5Ghz wireless card shows max TX power 3dBm, Android wifi scanner confirm that proprietary firmware 5Ghz router giving much better signal at same distance.
You need 2 things, first is DTS patch to fix it:
```diff
diff --git a/target/linux/ramips/dts/mt7620a_zbtlink_zbt-we826-16m.dts b/target/linux/ramips/dts/mt7620a_zbtlink_zbt-we826-16m.dts
index ecad0d6582..54cbd8bd97 100644
--- a/target/linux/ramips/dts/mt7620a_zbtlink_zbt-we826-16m.dts
+++ b/target/linux/ramips/dts/mt7620a_zbtlink_zbt-we826-16m.dts
@@ -8,3 +8,13 @@
 &firmware {
        reg = <0x50000 0xfb0000>;
 };
+
+&pcie0 {
+    mt76@0,0 {
+       reg = <0x0000 0 0 0 0>;
+       mediatek,mtd-eeprom = <&factory 0x8000>;
+       ieee80211-freq-limit = <5000000 6000000>;
+       mtd-mac-address = <&factory 0x8004>;
+       //mtd-mac-address-increment = <(-1)>;
+        };
+};
```
Second you need working 512bytes of EEPROM data for 5Ghz card. I got it from padavan-ng: MT7612E3_EEPROM_layout_20131022_2G5G_iPAiLNA_wTSSI_default_slope_offset.bin
To write it you need to do following sequence:
```
# make sure you have internet at your OpenWRT box
opkg update
# install mtd-rw module that allows writing to eeprom
opkg install kmod-mtd-rw
# enable writing to eeprom
insmod mtd-rw i_want_a_brick=1
# backup current eeprom
dd if=/dev/mtdblock2 of=eeprom.bin
# skip first 32768 bytes, write 512 bytes from MT7612E3_EEPROM_layout_20131022_2G5G_iPAiLNA_wTSSI_default_slope_offset.bin
dd if=MT7612E3_EEPROM_layout_20131022_2G5G_iPAiLNA_wTSSI_default_slope_offset.bin of=eeprom.bin conv=notrunc bs=1 seek=32768
# write back to eeprom
mtd write eeprom.bin factory
```

## 5. Conclusion

After all this you should have working 2Ghz and 5Ghz wireless. I'm not sure if 5Ghz is working at full power, as it's not proper factory calibration data, but it's much better than before.