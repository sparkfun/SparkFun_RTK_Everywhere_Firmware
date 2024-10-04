//----------------------------------------
void lg290pBoot()
{
    digitalWrite(pin_GNSS_Reset, HIGH); // Tell LG290P to boot
}

void lg290pReset()
{
    digitalWrite(pin_GNSS_Reset, LOW);
}