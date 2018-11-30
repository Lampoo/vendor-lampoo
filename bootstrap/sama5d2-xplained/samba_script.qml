import SAMBA 3.2
import SAMBA.Connection.Serial 3.2
import SAMBA.Device.SAMA5D2 3.2

SerialConnection {
	//port: "ttyACM0"
	//port: "COM85"
	//baudRate: 57600

	device: SAMA5D2Xplained {
		// to use a custom config, replace SAMA5D2Xplained by SAMA5D2 and
		// uncomment the following lines, or see documentation for
		// custom board creation.
		//config {
		//	qspiflash {
		//		instance: 0
		//		ioset: 3
		//		freq: 66
		//	}
		//}
	}

	onConnectionOpened: {
		// initialize QSPI applet
		initializeApplet("qspiflash")

		// erase all memory
		applet.erase(0, applet.memorySize)

		// write files
		// sam-ba -p serial -d sama5d2 -a qspiflash:0:3:66 -c erase:0:0x4000 -c writeboot:at91bootstrap.bin
		applet.write(0x00000, "at91bootstrap.bin", true)
		// sam-ba -p serial -d sama5d2 -a qspiflash:0:3:66 -c erase:0x60000:0xc000 -c write:kernel-dtb:0x60000
		applet.write(0x60000, "kernel-dtb")
		// sam-ba -p serial -d sama5d2 -a qspiflash:0:3:66 -c erase:0x6c000:0x394000 -c write:kernel:0x6c000
		applet.write(0x6c000, "kernel")
		// sam-ba -p serial -d sama5d2 -a qspiflash:0:3:66 -c erase:0x400000:0xc00000 -c write:system.img:0x400000
		applet.write(0x400000, "system.img")
		// sam-ba -p serial -d sama5d2 -a qspiflash:0:3:66 -c erase:0x1000000:0x1000000 -c write:userdata.img:0x1000000
		applet.write(0x1000000, "userdata.img")

		// initialize boot config applet
		initializeApplet("bootconfig")

		// Use BUREG0 as boot configuration word
		applet.writeBootCfg(BootCfg.BSCR, BSCR.fromText("VALID,BUREG0"))

		// Enable external boot only on QSPI0 IOSET3
		applet.writeBootCfg(BootCfg.BUREG0,
			BCW.fromText("EXT_MEM_BOOT,UART1_IOSET1,JTAG_IOSET1," +
			             "SDMMC1_DISABLED,SDMMC0_DISABLED,NFC_DISABLED," +
			             "SPI1_DISABLED,SPI0_DISABLED," +
			             "QSPI1_DISABLED,QSPI0_IOSET3"))
	}
}
