/*
 * Project: Willy – Internet Connected Coffee Machine
 * Description: Project for E5IOT course at Aarhus University – Fall 2018
 * Authors: Paweł Belczak & Garrett Shirley
 * Date: 17.11.18
 */

int relay = D1; // Powers the circuit.

int weight_sensor = A5; // Senses the jug.

int liquid_sensor = D3; // Physical binary switch.

int led = D7; // On-board LED.

int button = D2;  // Physical binary switch.

int power = A4; // For powering liquid and weight sensors.

const int JUG_WEIGHT = 1000;  // Weight treshhold for sensing a jug.

int weight; // Weight measured by the `weight_sensor`.

bool hasWater;  // Property measured by the `liquid_sensor`.

char device_code[255] = "";

char access_token[255] = "";

char refresh_token[255] = "";

// Coffe machine internal state:
// either `brewing` or `not brewing`.
bool isBrewing = false;

// Run only once at the power on/reset.
void setup()
{
  // Set pins modes.
  pinMode(liquid_sensor, INPUT);
  pinMode(weight_sensor, INPUT);
  pinMode(button, INPUT);
  pinMode(relay, OUTPUT);
  pinMode(power, OUTPUT);
  pinMode(led, OUTPUT);

  Serial.begin(9600);

  // Set power to output HIGH voltage.
  digitalWrite(power,HIGH);

  // Get refresh token from EEPROM
  uint8_t refresh_token_size;
  EEPROM.get(255, refresh_token_size);
  EEPROM.get(0, refresh_token);
  if(refresh_token_size < 255) refresh_token[refresh_token_size] = 0;
  Serial.printf("EEPROM - Refresh token %s\nRefresh token size: %d\n", refresh_token, refresh_token_size);

  // Get access token from EEPROM
  uint8_t access_token_size;
  EEPROM.get(511, access_token_size);
  EEPROM.get(256, access_token);
  if(access_token_size < 255) access_token[access_token_size] = 0;
  Serial.printf("EEPROM - Access token %s\nAccess token size: %d\n", access_token, access_token_size);

  // Call `changeState` (ISR) on button toggle.
  // attachInterrupt(button, changeState, CHANGE);

  // DEBUG ONLY
  Particle.function("get_device_code", get_device_code);
  Particle.function("get_days_first_event", get_days_first_event);
  Particle.function("refresh_token", refresh_token_calendar);
  // Calendar API
  Particle.subscribe("receive_device_code", authenticate, MY_DEVICES);
  Particle.subscribe("CalendarAPI-token2", myHandler, MY_DEVICES);
  Particle.subscribe("events-get-them", mineHandler, MY_DEVICES);
  Particle.subscribe("refreshed-token", save_refreshed_token, MY_DEVICES);
}

Timer next_event_timer(10, refreshxd, true);
Timer next_calendar_fetch(10, get_first_event, true);

void loop()
{
  delay(10000);
  // rfc3339Time();
  // delay(3000);
  // Read data from sensors.
  weight = 2000;//analogRead(weight_sensor);
  hasWater = true;//digitalRead(liquid_sensor);

  // Power the circuit if coffe machine:
  // - is in `brewing` mode
  // - has a jug
  // - has water
  if(isBrewing && hasJug() && hasWater) {
    digitalWrite(relay, HIGH);
    digitalWrite(led,HIGH);
  } else {
    digitalWrite(relay, LOW);
    digitalWrite(led,LOW);
  }
}

// Interrupt Service Routine (ISR)
// Toggles `isBrewing` state.
void changeState()
{
  // isBrewing = !isBrewing;
}

// Returns `true` if Willy has a jug in place
// by comparing `weight` with `JUG_WEIGHT`;
// returns false otherwise.
bool hasJug()
{
  return weight >= JUG_WEIGHT;
}

// Start authentication process, request Google servers
// for code for the user to approve calendar integration
// at https://www.google.com/device
int get_device_code(String command)
{
  Serial.printf("Get device code webhook called.\n");
  Particle.publish("get_device_code_google_calendar", NULL, PRIVATE);
  return 1;
}

void get_first_event() {
  get_days_first_event("test");
}

int get_days_first_event(String command)
{
  Serial.printf("Get days first event webhook called.\n");
  char data[255];
  char now[21];
  rfc3339Time(now);
  snprintf(data, sizeof(data), "{\"access_token\":\"%s\", \"time\":\"%s\"}", access_token, now);
  Serial.printf("%s\n", data);
  Particle.publish("getEvents", data, PRIVATE);
  return 1;
}

int refresh_token_calendar(String command)
{
  Serial.printf("Refresh token webhook called.\n");
  Particle.publish("refresh-token-calendar-api", refresh_token, PRIVATE);
  return 1;
}

void refreshxd() {
  Serial.printf("Timer called\n");
  digitalWrite(led,HIGH);
  delay(500);
  digitalWrite(led,LOW);
  delay(500);
  // refresh_token_calendar("test");
  get_first_event();
}

void authenticate(const char *event, const char *data) {
  // "device_code": "AH-1Ng2cbcdTaeFUCzgny_srzOR8j3Wh1S0M_c8q8CmviTS6GGNFTY-wljyNj3XPrTfUeBW_8ft8Weqw0eMAi1tHtoxXmoXXtA",
  // "user_code": "ZMYD-DFDJ",
  // "expires_in": 1800,
  // "interval": 5,
  // "verification_url": "https://www.google.com/device"

  Serial.printf("Authentication started:\n");
  String response = String(data);
  char responseBuffer[255] = "";
  char delimeter[2] = "~";
  char* token;
  response.toCharArray(responseBuffer, 255);
  String tmp = strtok(responseBuffer, delimeter);
  tmp.toCharArray(device_code, 255);
  Serial.printf("device_code: %s\n", device_code);
  token = strtok(NULL, delimeter);
  Serial.printf("user_code: %s\n", token);
  int i;
  // TODO: make while loop and try to request every interval
  for(i=0; i<45; i++) {
    Serial.printf("counting %d/45\n", i);
    digitalWrite(led,HIGH);
    delay(500);
    digitalWrite(led,LOW);
    delay(500);
  }

  // String userCode = strtok(NULL, delimeter);
  // Serial.printf("%s\n", userCode);
  // int expiresIn = atoi(strtok(NULL, delimeter));
  // Serial.printf("%d\n", expiresIn);
  // int interval = atoi(strtok(NULL, delimeter));
  // Serial.printf("%d\n", interval);
  // Trigger the integration
  Particle.publish("CalendarAPI-get-token2", device_code, PRIVATE);
}

void myHandler(const char *event, const char *data)
{
  Serial.printf("Saving credentials started:\n");
  String response = String(data);
  char responseBuffer[255] = "";
  char delimeter[2] = "~";
  char* token;
  response.toCharArray(responseBuffer, 255);
  String tmp = strtok(responseBuffer, delimeter);
  tmp.toCharArray(access_token, 255);
  Serial.printf("acces_token: %s\n", access_token);
  EEPROM.put(256, access_token);
  uint8_t access_token_size = strlen(access_token);
  EEPROM.put(511, access_token_size);
  tmp = strtok(NULL, delimeter);
  tmp.toCharArray(refresh_token, 255);
  Serial.printf("refresh_token: %s\n", refresh_token);
  EEPROM.put(0, refresh_token);
  uint8_t refresh_token_size = strlen(refresh_token);
  EEPROM.put(255, refresh_token_size);
}

void mineHandler(const char *event, const char *data)
{
  Serial.printf("Getting first event in the day:\n");
  Serial.printf("%s\n", data);
  String timestr = String(data);
  unsigned int time_to_brew = 0;
  int hour, min, sec, zone, day;
  char now[21];
  rfc3339Time(now);
  Serial.printf("Current time: %s\n", now);
  // parsing string data
  hour = data[12] - '0';
  if(data[11] != '0') hour += 10 * (data[11]-'0');
  min = data[15] - '0';
  if(data[14] != '0') min += 10 * (data[14]-'0');
  sec = data[18] - '0';
  if(data[17] != '0') sec += 10 * (data[17]-'0');
  zone = data[21] - '0';
  if(data[20] != '0') zone += 10 * (data[20]-'0');
  if(data[19] == '+') {
    hour = (hour - zone);
    if(hour < 0) hour += 24;
  } else {
    hour = (hour + zone) % 24;
  }

  Serial.printf("Event time: %d:%d:%d\n", hour, min, sec);

  int cur_hour = Time.hour();
  int cur_min = Time.minute();
  int cur_sec = Time.second();

  sec -= cur_sec;
  if(sec < 0) {
    sec += 60;
    min -= 1;
  }

  min -= cur_min;
  if(min < 0) {
    min += 60;
    hour -= 1;
  }

  if(hour < cur_hour) hour += 24;
  hour -= cur_hour;

  Serial.printf("Time to event: %d:%d:%d\n", hour, min, sec);
  if(hour >= 1) hour -= 1;
  Serial.printf("Time to brew: %d:%d:%d\n", hour, min, sec);
  time_to_brew += hour*3600000;
  time_to_brew += min*60000;
  time_to_brew += sec*1000;
  if(hour >= 0) {
    Serial.printf("In millis to event: %u\n", time_to_brew);
    next_event_timer.changePeriod(time_to_brew);
  } else {
    Serial.printf("Too late, next time brew faster...");
  }
}

void rfc3339Time(char* now)
{
  //            11 14 17 20
  // 2018-12-11T23:30:00+01:00
  // 2018-12-11T11:30:00Z
  time_t time = Time.now();
  String timestr = Time.format(time, "%Y-%m-%dT%H:%M:%SZ");
  timestr.toCharArray(now, 21);
  // strftime(buf, sizeof(16), "%Y-%m-%dT%H:%M:%SZ", info);
}

void save_refreshed_token(const char *event, const char *data)
{
  Serial.printf("Saving refreshed access token:\n");
  String response(data);
  response.toCharArray(access_token, 255);
  uint8_t token_size = strlen(access_token);
  EEPROM.put(511, token_size);
  EEPROM.put(256, access_token);
  Serial.printf("Access token: %s\n Access token size: %d\n", access_token, token_size);
}
