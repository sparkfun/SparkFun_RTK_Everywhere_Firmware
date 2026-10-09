//----------------------------------------
// Based on the platform, put the GNSS receiver into run mode
//----------------------------------------
void gpioGnssBoot()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell LG290P to boot
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderGnssBoot(); // Drive the GNSS reset pin high
    }
    else if (productVariant == RTK_POSTCARD)
    {
        //digitalWrite(pin_GNSS_Reset, HIGH); // Tell LG290P to boot
    }
    else
        systemPrintln("Uncaught gnssBoot()");
}

//----------------------------------------
// Based on the platform, put the GNSS receiver into reset
//----------------------------------------
void gpioGnssReset()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell LG290P to reset
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderGnssReset(); // Drive the GNSS reset pin low
    }
    else if (productVariant == RTK_POSTCARD)
    {
        //digitalWrite(pin_GNSS_Reset, LOW); // Tell LG290P to reset
    }
    else
        systemPrintln("Uncaught gpioGnssReset()");
}
