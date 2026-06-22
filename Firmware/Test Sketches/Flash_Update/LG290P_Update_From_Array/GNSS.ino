// Based on the platform, put the GNSS receiver into run mode
void gnssBoot()
{
#ifdef PLATFORM_FP
    gpioExpanderGnssBoot(); // Drive the GNSS reset pin high
#elif defined(PLATFORM_TX2)
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell LG290P to boot
#endif
}

// Based on the platform, put the GNSS receiver into reset
void gnssReset()
{

#ifdef PLATFORM_FP
    gpioExpanderGnssReset(); // Drive the GNSS reset pin low
#elif defined(PLATFORM_TX2)
    digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell LG290P to reset
#endif
}