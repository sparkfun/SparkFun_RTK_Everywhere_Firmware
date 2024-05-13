// Given a settingName, and string value, update a given setting
bool updateSettingWithValue(const char *settingName, const char *settingValueStr)
{
  char *ptr;
  double settingValue = strtod(settingValueStr, &ptr);

  bool knownSetting = false;

  if (strcmp(settingValueStr, "true") == 0)
    settingValue = 1;
  if (strcmp(settingValueStr, "false") == 0)
    settingValue = 0;

  if (strcmp(settingName, "elvMask") == 0)
  {
    //settings.elvMask = value
    knownSetting = true;
  }

  return (knownSetting);
}

bool getSettingValue(const char *settingName, char *settingValueStr)
{
  bool knownSetting = false;

  char truncatedName[51];
  char suffix[51];

  if (strcmp(settingName, "elvMask") == 0)
  {
    sprintf(settingValueStr, "%0.2f", 0.25);
    knownSetting = true;
  }

  return (knownSetting);

}
