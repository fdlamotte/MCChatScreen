#include <MicroNMEA.h>
#include <Arduino.h>   // needed for PlatformIO

// To display free memory include the MemoryFree library, see
// https://github.com/maniacbug/MemoryFree and uncomment the line
// below
//#include <MemoryFree.h>

// Refer to serial devices by use
Stream& console = Serial;
HardwareSerial* gps;

char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));
bool ledState = LOW;
volatile bool ppsTriggered = false;

void ppsHandler(void);

void ppsHandler(void)
{
	ppsTriggered = true;
}

void printUnknownSentence(MicroNMEA& nmea)
{
	console.println();
	console.print("Unknown sentence: ");
	console.println(nmea.getSentence());
}

void gpsHardwareReset()
{
	// Empty input buffer
	while (gps->available())
		gps->read();

	// digitalWrite(GPS_RESET, HIGH);
	// delay(50);
	// digitalWrite(GPS_RESET, LOW);

	// Reset is complete when the first valid message is received
	while (1) {
		while (gps->available()) {
			char c = gps->read();
			if (nmea.process(c))
				return;

		}
	}
}

void gps_setup(HardwareSerial& gps_ser)
{
//	console.begin(115200); // console
	//gps.begin(115200); // gps
	gps = &gps_ser;

	nmea.setUnknownSentenceHandler(printUnknownSentence);

	console.println("Resetting GPS module ...");
	gpsHardwareReset();
	console.println("... done");

	// Clear the list of messages which are sent.
	MicroNMEA::sendSentence(*gps, "$PORZB");

	// Send only RMC and GGA messages.
	//MicroNMEA::sendSentence(gps, "$PORZB,RMC,1,GGA,1");

	// Disable compatability mode (NV08C-CSM proprietary message) and
	// adjust precision of time and position fields
	//MicroNMEA::sendSentence(gps, "$PNVGNME,2,9,1");
	// MicroNMEA::sendSentence(gps, "$PONME,2,4,1,0");
	//MicroNMEA::sendSentence(gps, "$PMTK314,5,5,5,5,5,5,0,0,0,0,0,0,10,10");
}

void gps_feed_nmea() {
	while (gps->available()) {
		char c = gps->read();
		//#ifdef GPS_NMEA_DEBUG
		console.print(c);
		//#endif
		nmea.process(c);
	}
}
