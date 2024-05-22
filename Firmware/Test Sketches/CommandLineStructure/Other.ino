// Given a settingName, and string value, update a given setting
SettingValueResponse updateSettingWithValue(const char *settingName, const char *settingValueStr)
{
  bool knownSetting = false;
  bool settingIsString = false;

  if (strcmp(settingName, "elvMask") == 0)
  {
    //settings.elvMask = value
    knownSetting = true;
  }
  else if (strcmp(settingName, "ntripClientCasterUserPW") == 0)
  {
    //Serial.printf("New password: '%s'\r\n", settingValueStr);
    settingIsString = true;
    knownSetting = true;
  }

  if (knownSetting == true && settingIsString == true)
    return (SETTING_KNOWN_STRING);
  else if (knownSetting == true)
    return (SETTING_KNOWN);

  return (SETTING_UNKNOWN);
}

SettingValueResponse getSettingValue(const char *settingName, char *settingValueStr)
{
  bool knownSetting = false;
  bool settingIsString = false;

  if (strcmp(settingName, "elvMask") == 0)
  {
    sprintf(settingValueStr, "%0.2f", 0.25);
    knownSetting = true;
  }
  else if (strcmp(settingName, "ntripClientCasterUserPW") == 0)
  {
    sprintf(settingValueStr, "%s", (char*)"pwWith\"quote");
    settingIsString = true;
    knownSetting = true;
  }

  if (knownSetting == true && settingIsString == true)
    return (SETTING_KNOWN_STRING);
  else if (knownSetting == true)
    return (SETTING_KNOWN);

  return (SETTING_UNKNOWN);
}
