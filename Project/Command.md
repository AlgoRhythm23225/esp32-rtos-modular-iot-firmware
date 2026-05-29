flash program
```bash
python -m esptool --chip esp32 -p COM7 -b 460800 --before default-reset --after no-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 40m 0xd000 build\partition_table\partition-table.bin 0x13000 build\ota_data_initial.bin 0x20000 build\Project.bin
```