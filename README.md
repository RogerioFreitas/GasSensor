# GasSensor
Gas, Smoke and Temperature Sensors in a arduino system. 


The system have a 5V Relay that can be used to turn off a solenoide to interrupt the gas supply (when the gas Natural, Butane or Metane had detected).

The system turn ON a Red LED when GAS above 30% was detected in the environment / room. A buzzer Sounds for 10 secs too.

the system send  MQTT messages that can be consumed by Home-Assistant or other device.

*Improve TO DO: Change the logical to the sensor turn on the supply of GAS. In this case of use, when the sensor don't identify gas in the enviromnent / room, it's turn On the solenoide and the gas. 

