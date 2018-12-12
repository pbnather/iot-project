/*
 * Project: Willy – Internet Connected Coffee Machine
 * Description: Project for E5IOT course at Aarhus University – Fall 2018
 * Authors: Paweł Belczak & Garrett Shirley
 * Date: 17.11.18
 */

#define JUG_WEIGHT 1000
#define TOKEN_MAX_SIZE 255
#define RFC3339Z_SIZE 21
// addresses of data in EEPROM memory
#define REFRESH_TK_ADDR 0
#define REFRESH_TK_SIZE_ADDR 255
#define ACCESS_TK_ADDR 256
#define ACCESS_TK_SIZE_ADDR 511

int relay = D1; // Powers the circuit.

int weight_sensor = A5; // Senses the jug.

int liquid_sensor = D3; // Physical binary switch.

int led = D7; // On-board LED.

int weight_led = D1;

int liquid_led = D4;

int brewing_led = D5;

int button = D2;  // Physical binary switch.

int power = A4; // For powering liquid and weight sensors.

int weight; // Weight measured by the `weight_sensor`.

bool hasWater;  // Property measured by the `liquid_sensor`.

char access_token[TOKEN_MAX_SIZE] = ""; // Access token needed for calling Google Calendar API (valid ~1h).

char refresh_token[TOKEN_MAX_SIZE] = "";  // Refresh token needed to get new access token.

// Coffe machine internal state:
// either `brewing` or `not brewing`.
volatile bool isBrewing = false;

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
  pinMode(weight_led, OUTPUT);
  pinMode(liquid_led, OUTPUT);
  pinMode(brewing_led, OUTPUT);

  // DEBUG ONLY
  Serial.begin(9600);

  // Set power to output HIGH voltage.
  digitalWrite(power,HIGH);

  // Get refresh token from EEPROM
  uint8_t refresh_token_size;
  EEPROM.get(REFRESH_TK_SIZE_ADDR, refresh_token_size);
  EEPROM.get(REFRESH_TK_ADDR, refresh_token);
  if(refresh_token_size < TOKEN_MAX_SIZE) refresh_token[refresh_token_size] = 0;
  Serial.printf("EEPROM - Refresh token %s\nRefresh token size: %d\n", refresh_token, refresh_token_size);

  // Get access token from EEPROM
  uint8_t access_token_size;
  EEPROM.get(ACCESS_TK_SIZE_ADDR, access_token_size);
  EEPROM.get(ACCESS_TK_ADDR, access_token);
  if(access_token_size < TOKEN_MAX_SIZE) access_token[access_token_size] = 0;
  Serial.printf("EEPROM - Access token %s\nAccess token size: %d\n", access_token, access_token_size);

  // Call `changeState` (ISR) on button press.
  attachInterrupt(button, change_state, RISING);

  // DEBUG ONLY
  Particle.function("get_device_code", get_device_code_fun);
  Particle.function("get_days_first_event", get_days_first_event_fun);
  Particle.function("refresh_access_token", refresh_access_token_fun);
  // Calendar API
  Particle.subscribe("device_code_token", authenticate, MY_DEVICES);
  Particle.subscribe("access_refresh_tokens", save_tokens, MY_DEVICES);
  Particle.subscribe("days_first_event", set_first_event_timer, MY_DEVICES);
  Particle.subscribe("new_access_token", save_refreshed_token, MY_DEVICES);
}

Timer next_event_timer(1, change_state, true);
Timer event_fetch_timer(11, get_days_first_event, true);

void loop()
{
  // Read data from sensors.
  weight = analogRead(weight_sensor);
  hasWater = digitalRead(liquid_sensor);

  // Power the circuit if coffe machine:
  // - is in `brewing` mode
  // - has a jug
  // - has water
  if(isBrewing && has_jug() && hasWater) {
    digitalWrite(relay, HIGH);
    digitalWrite(led,HIGH);
  } else {
    digitalWrite(relay, LOW);
    digitalWrite(led,LOW);
  }
}

// Interrupt Service Routine (ISR)
// Toggles `isBrewing` state.
void change_state()
{
  isBrewing = !isBrewing;
}

// Returns `true` if Willy has a jug in place
// by comparing `weight` with `JUG_WEIGHT`;
// returns false otherwise.
bool has_jug()
{
  return weight >= JUG_WEIGHT;
}

// DEBUG ONLY
int get_device_code_fun(String command) { get_device_code(); return 1; }
int get_days_first_event_fun(String command) { get_days_first_event(); return 1; }
int refresh_access_token_fun(String command) { refresh_access_token(); return 1; }

// Start authentication process, request Google servers
// for code for the user to approve calendar integration
// at https://www.google.com/device
void get_device_code()
{
  Serial.printf("get_device_code webhook called.\n");
  Particle.publish("get_device_code", NULL, PRIVATE);
}

void get_days_first_event()
{
  Serial.printf("get_days_first_event webhook called.\n");
  char now[RFC3339Z_SIZE];
  rfc3339Time(now);
  String data = "{\"access_token\":\"";
  data += access_token;
  data += "\", \"time\":\"";
  data += now;
  data += "\"}";
  Particle.publish("get_events", data, PRIVATE);
}

void refresh_access_token()
{
  Serial.printf("refresh_access_token webhook called.\n");
  Particle.publish("refresh_access_token", refresh_token, PRIVATE);
}

void authenticate(const char *event, const char *data) {
  // json data template:
  // "device_code": device_code,
  // "user_code": user_code,
  // "expires_in": 1800,
  // "interval": 5,
  // "verification_url": "https://www.google.com/device"

  Serial.printf("Authenticating:\n");
  String response = String(data);
  char responseBuffer[TOKEN_MAX_SIZE];
  char device_code[TOKEN_MAX_SIZE];
  char delimeter[2] = "~";
  char* token;
  response.toCharArray(responseBuffer, TOKEN_MAX_SIZE);
  String tmp = strtok(responseBuffer, delimeter);
  tmp.toCharArray(device_code, TOKEN_MAX_SIZE);
  token = strtok(NULL, delimeter);
  Serial.printf("device_code: %s\n", device_code);
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

  Particle.publish("get_access_refresh_tokens", device_code, PRIVATE);
}

void save_tokens(const char *event, const char *data)
{
  Serial.printf("Saving tokens:\n");
  String response = String(data);
  char responseBuffer[TOKEN_MAX_SIZE] = "";
  char delimeter[2] = "~";
  response.toCharArray(responseBuffer, TOKEN_MAX_SIZE);

  // Get access and refresh token.
  String tmp = strtok(responseBuffer, delimeter);
  tmp.toCharArray(access_token, TOKEN_MAX_SIZE);
  tmp = strtok(NULL, delimeter);
  tmp.toCharArray(refresh_token, TOKEN_MAX_SIZE);

  // Get tokens sizes.
  uint8_t access_token_size = strlen(access_token);
  uint8_t refresh_token_size = strlen(refresh_token);

  // Save tokens to EEPROM memory.
  EEPROM.put(REFRESH_TK_ADDR, refresh_token);
  EEPROM.put(REFRESH_TK_SIZE_ADDR, refresh_token_size);
  EEPROM.put(ACCESS_TK_ADDR, access_token);
  EEPROM.put(ACCESS_TK_SIZE_ADDR, access_token_size);

  // DEBUG ONLY
  Serial.printf("acces_token: %s\n", access_token);
  Serial.printf("refresh_token: %s\n", refresh_token);
}

void set_first_event_timer(const char *event, const char *data)
{
  Serial.printf("Setting timer for first event in the day:\n");
  String timestr = String(data);
  unsigned int time_to_brew = 0;
  int hour, min, sec, zone, day;
  // Parse event time.
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
  // Set up brewing one hour before event starts.
  if(hour >= 1) {
    hour -= 1;
    Serial.printf("Time to brew: %d:%d:%d\n", hour, min, sec);
    time_to_brew += hour*3600000;
    time_to_brew += min*60000;
    time_to_brew += sec*1000;
    Serial.printf("In millis to event: %u\n", time_to_brew);
    next_event_timer.changePeriod(time_to_brew);
  } else {
    Serial.printf("Too late, next time brew faster...");
  }
}

void save_refreshed_token(const char *event, const char *data)
{
  Serial.printf("Saving refreshed access token:\n");
  String response(data);
  response.toCharArray(access_token, TOKEN_MAX_SIZE);
  uint8_t token_size = strlen(access_token);
  EEPROM.put(ACCESS_TK_SIZE_ADDR, token_size);
  EEPROM.put(ACCESS_TK_ADDR, access_token);
  Serial.printf("Access token: %s\n Access token size: %d\n", access_token, token_size);
}

void rfc3339Time(char* now)
{
  // RFC3339 date example
  // 2018-12-11T11:30:00Z
  time_t time = Time.now();
  String timestr = Time.format(time, "%Y-%m-%dT%H:%M:%SZ");
  timestr.toCharArray(now, RFC3339Z_SIZE);
}
