This is a sample SAMD20 firmware projects that implements the firmware upgrade functionality via SMBus (as a slave). It supports the following SMBus commands:

- Get Status (command code: 0; SMBus protocol: read byte; data: bit 0 = BUSY, bit 1 = PEC ERROR, bit 2 = UPGRADE ERROR)
- Upgrade Start (command code: 1; SMBus protocol: send byte; data: none)
- Upgrade Send Data (command code: 2; SMBus protocol: block write; data: IHEX line in binary format)
- Upgrade Activate (command code: 3; SMBus protocol: send byte; data: none)

To initiate an upgrade and erase the Flash, the master sends the Upgrade Status command and then repeatedly sends the Get Status command to read back the device status until the BUSY flag is cleared. After that, the master sends the image file data using a series of Send Data commands containing IHEX image data, line by line, in binary format (NOT in text format!), followed by the Upgrade Activate command to activate the new firmware. After each command, the master repeatedly sends the Get Status command until the BUSY flag is cleared. If an error flag is detected, the master re-transmits the previous command. Note that while the BUSY flag is set, any write commands will be ignored, so it is important to wait until this flag is cleared before sending new upgrade commands. However, readback commands (such as Get Status) are supported even when the BUSY flag is set. An example upgrade client is implemented in the "smbusprog" repository.

This implementation supports the Parity Error Checking feature of SMBus and automatically detects if a PEC byte is present in a transaction. If the PEC verification fails, it ignores the command and sets the PEC ERROR flag in the status byte. If a PEC byte is absent, no verification is done. In addition, it automatically appends the PEC byte to every readback transaction, but it is up to the master to verify it (if required). The PEC is calculated using the standard CRC8 algorithm using the polynomial of 0x7.

The SMBus SCL Low Timeout feature is supported using the built-in feature of the SAMD20 I2C controller. If an SCL Low Timeout is detected, the bus is released and the I2C controller is automatically reset.
  