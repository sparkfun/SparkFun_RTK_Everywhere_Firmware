/*
  On the Torch, USB D+/- is connect through a TS3USB30ERSWR switch. Channel 1 is connected
  to the Power Delivery controller, channel 2 is connected to the CH342 IC.

  To use:
    Load the sketch
    Press 'z'
    The USB should disappear for 5 seconds then reconnect
*/

int pin_usbSelect = 21;

void setup()
{
  Serial.begin(115200);
  delay(750);
  Serial.println();
  Serial.println("USB Mux control");

  pinMode(pin_usbSelect, OUTPUT);

  digitalWrite(pin_usbSelect, HIGH);
}

void loop()
{
  if (Serial.available())
  {
    byte incoming = Serial.read();

    if (incoming == 'r')
      ESP.restart();
    else if (incoming == 'z')
    {
      Serial.println("Pushing test pins LOW (will disconnect CH342 serial) for 5 seconds");
      delay(250); //Allow print to finish

      digitalWrite(pin_usbSelect, LOW);
      
      delay(5000);

      digitalWrite(pin_usbSelect, HIGH);

      Serial.println("And we're back");
    }
  }
}
