# One full-ROM read after paired qualification

`qualified_rom_readback.py` is a separate explicit version3 workflow. It leaves
the legacy collector's synthetic and spacing prerequisites unchanged. For an
operator-authorized single full read, it instead verifies actual paired PDB and
counter evidence for both patterns at4/100/10000 on the same firmware/ROM/PRC.
The authorization and proof hashes are stored; no synthetic history is invented.

```sh
python3 tools/qualified_rom_readback.py prepare SESSION \
  --paired-session PAIRED_SESSION --firmware-sha256 FROZEN_UF2_SHA \
  --authorization 'User instruction authorizing this one readback'
python3 tools/qualified_rom_readback.py connect SESSION --timeout 1800
# One serial HotSync: read installed FNVD PRC, install only ROMProbeControl,
# retrieve control, compare all PRC resources and exact control record.
python3 tools/qualified_rom_readback.py run SESSION --rp /dev/ttyACM0 \
  --read-timeout 7200 --timeout 1800
# RAM checker main page: None + Burst64, Run/Read ONCE, saved tone, HotSync.
```

No firmware/PRC installation, Palm reset or clock setter is used. Existing
paired and full-ROM result databases remain in place. A pending read supports
retrieval-only retry with `collect SESSION`; a completed session cannot arm a
second read. The collector stays quiet on UART until the user starts HotSync,
then stops the responder and verifies exact full-image counters. The saved
payload must match every reference byte, CRC and SHA256; `readback.rom` is exported.

After a PASS, `continuous_rom.py` accepts explicit `--engine persistent
--marker preload --readback-session SESSION --firmware-sha256 FROZEN_UF2_SHA`.
It validates the completed matching readback before opening/arming the RP2350.
Version3 recognition service does not poll UART or stop on HotSync; coordinated
Ctrl-C stops it. Recognition remains a manual action, separate from normal ROM
application launch. Never stop it automatically once PalmOS depends on the ROM.
