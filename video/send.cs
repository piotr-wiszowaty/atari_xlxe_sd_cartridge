using System;
using System.IO;
using System.IO.Ports;

public class Sender
{
	public static void Main(string[] args)
	{
		if (args.Length != 2)
			throw new Exception("Usage: send FILE COMn");

		using (SerialPort sp = new SerialPort(args[1], 115200)) {
			sp.Open();
			sp.Write(new byte[] { (byte) 'a', 0x00, 0xbf }, 0, 3);
			byte[] a = new byte[] { (byte) 'w', 0 };
			using (FileStream fs = File.OpenRead(args[0])) {
				for (;;) {
					int b = fs.ReadByte();
					if (b < 0)
						break;
					a[1] = (byte) b;
					sp.Write(a, 0, 2);
				}
			}
		}
	}
}
