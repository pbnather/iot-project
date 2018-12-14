/*
 * Project: Willy – Internet Connected Coffee Machine
 * Description: Project for E5IOT course at Aarhus University – Fall 2018
 * Authors: Paweł Belczak & Garrett Shirley
 * Date: 17.11.18
 */

#define MIN_TIME 1
#define MAX_TIME 6
#define ONE_HUNDRED_SEC 120000
#define JUG_WEIGHT 4095
#define TOKEN_MAX_SIZE 255
#define RFC3339Z_SIZE 21
// addresses of data in EEPROM memory
#define REFRESH_TK_ADDR 0
#define REFRESH_TK_SIZE_ADDR 255
#define ACCESS_TK_ADDR 256
#define ACCESS_TK_SIZE_ADDR 511

int relay = D4; // Powers the circuit.

int weight_sensor = A0; // Senses the jug.

int liquid_sensor = D3; // Physical binary switch.

int led = D7; // On-board LED.

int weight_led = D0;  // Signals if there is a jug.

int liquid_led = D1;  // Signals if there is enough water.

int button = D5;  // Physical binary switch.

int weight; // Weight measured by the `weight_sensor`.

bool noWater;  // Property measured by the `liquid_sensor`.

char access_token[TOKEN_MAX_SIZE] = "notset"; // Access token needed for calling Google Calendar API (valid ~1h).

char refresh_token[TOKEN_MAX_SIZE] = "notset";  // Refresh token needed to get new access token.

volatile bool isBrewing = false; // Brewing was initiated.
volatile bool isFinishing = false; // Brewing is finishing.
volatile bool isStarted = false;  // Willy is brewing.
volatile bool hasPendingEvent = false;  // Timer is set to trigger one hour before next event.
volatile bool shouldRefresh = true; // Refresh access token.

Timer next_event_timer(1000, execute_event, true);
Timer event_fetch_timer(1000, set_refresh, true);
Timer stop_brewing_timer(1000, stop_brewing, true);

// Run only once at the power on/reset.
void setup()
{
  // Set pins modes.
  pinMode(liquid_sensor, INPUT);
  pinMode(weight_sensor, INPUT);
  pinMode(button, INPUT);
  pinMode(relay, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(weight_led, OUTPUT);
  pinMode(liquid_led, OUTPUT);

  // For printing useful info/debugging.
  Serial.begin(9600);

  // Get refresh token from EEPROM.
  uint8_t refresh_token_size;
  EEPROM.get(REFRESH_TK_SIZE_ADDR, refresh_token_size);
  EEPROM.get(REFRESH_TK_ADDR, refresh_token);
  if(refresh_token_size < TOKEN_MAX_SIZE) refresh_token[refresh_token_size] = 0;
  Serial.printf("EEPROM - Refresh token %s\nRefresh token size: %d\n", refresh_token, refresh_token_size);

  // Get access token from EEPROM.
  uint8_t access_token_size;
  EEPROM.get(ACCESS_TK_SIZE_ADDR, access_token_size);
  EEPROM.get(ACCESS_TK_ADDR, access_token);
  if(access_token_size < TOKEN_MAX_SIZE) access_token[access_token_size] = 0;
  Serial.printf("EEPROM - Access token %s\nAccess token size: %d\n", access_token, access_token_size);

  // Call `changeState` (ISR) on button press.
  attachInterrupt(button, change_state, RISING);

  // For controlling WIlly through Particle console.
  Particle.function("print_info", print_info_fun);
  Particle.function("get_device_code", get_device_code_fun);
  Particle.function("get_days_first_event", get_days_first_event_fun);
  Particle.function("refresh_access_token", refresh_access_token_fun);
  // Calendar API
  Particle.subscribe("device_code_token", authenticate, MY_DEVICES);
  Particle.subscribe("access_refresh_tokens", save_tokens, MY_DEVICES);
  Particle.subscribe("days_first_event", set_first_event_timer, MY_DEVICES);
  Particle.subscribe("new_access_token", save_refreshed_token, MY_DEVICES);
}

void loop()
{
  if(shouldRefresh) {
    refresh_access_token();
    shouldRefresh = false;
  }
  // Read data from sensors.
  weight = analogRead(weight_sensor);
  noWater = digitalRead(liquid_sensor);

  // Light led if needed.
  if(has_jug()) {
    digitalWrite(weight_led, HIGH);
  } else {
    digitalWrite(weight_led, LOW);
  }
  if(noWater) {
    digitalWrite(liquid_led, LOW);
  } else {
    digitalWrite(liquid_led, HIGH);
  }
  // When started brewing but stopped sensing water
  // brew for two minutes to finish water in the tank.
  if(isStarted) {
    digitalWrite(relay, HIGH);
    digitalWrite(led,HIGH);
    if(noWater) {
      if(isFinishing == false) {
        isFinishing = true;
        Serial.printf("Finishing brewing in 2 minutes.\n");
        stop_brewing_timer.changePeriod(ONE_HUNDRED_SEC);
      } else {
        digitalWrite(liquid_led, HIGH);
      }
    }
  } else {
    digitalWrite(relay, LOW);
    digitalWrite(led,LOW);
  }

  if(isBrewing && has_jug() && !noWater) {
    isStarted = true;
  } else if(isBrewing && has_jug() && noWater && isFinishing) {
    isStarted = true;
  } else {
    if(isStarted) {
      Serial.printf("stopped brewing.\n");
    }
    isStarted = false;
  }
}

int print_info_fun(String command)
{
  Serial.printf("isBrewing: %d\n", isBrewing);
  Serial.printf("isFinishing: %d\n", isFinishing);
  Serial.printf("isStarted: %d\n", isStarted);
  Serial.printf("hasWater: %d\n", !noWater);
  Serial.printf("has_jug: %d\n", has_jug());
  Serial.printf("weight: %d\n", weight);
  return 1;
}

void stop_brewing()
{
  isStarted = false;
  isFinishing = false;
  isBrewing = false;
}

void set_refresh() {
  shouldRefresh = true;
}

// Interrupt Service Routine (ISR)
// Toggles `isBrewing` state.
void change_state()
{
  isBrewing = !isBrewing;
}

//
void execute_event() {
  isBrewing = true;
  hasPendingEvent = false;
  set_fetch_event_timer();
}

// Returns `true` if Willy has a jug in place
// by comparing `weight` with `JUG_WEIGHT`;
// returns false otherwise.
bool has_jug()
{
  return weight == JUG_WEIGHT;
}

bool should_fetch_event()
{
  if(!hasPendingEvent) {
    if(MIN_TIME <= Time.hour() && Time.hour() < MAX_TIME) {
      return true;
    }
  }
  return false;
}

void set_fetch_event_timer() {
  // Call Particle process for safety.
  Particle.process();
  if(!hasPendingEvent) {
    if(MIN_TIME <= Time.hour() && Time.hour() < MAX_TIME) {
      Serial.printf("Fetching events started.");
      event_fetch_timer.changePeriod(1000);
    } else {
      int next_hour = 24 - Time.hour() + 1;
      unsigned int next_fetch = next_hour * 3600000;
      Serial.printf("Fetching events starts in %d hours.", next_hour);
      event_fetch_timer.changePeriod(next_fetch);
    }
  }
}

// For controlling WIlly through Particle console
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
  Particle.process();
  Particle.publish("new_user_code", token, PRIVATE);
  int i;
  for(i=0; i<60; i++) {
    Serial.printf("counting %d/60\n", i);
    if(i % 9 == 0) Particle.process();
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

  // Call Particle process for safety.
  Particle.process();

  // Save tokens to EEPROM memory.
  EEPROM.put(REFRESH_TK_ADDR, refresh_token);
  EEPROM.put(REFRESH_TK_SIZE_ADDR, refresh_token_size);
  EEPROM.put(ACCESS_TK_ADDR, access_token);
  EEPROM.put(ACCESS_TK_SIZE_ADDR, access_token_size);

  // DEBUG ONLY
  Serial.printf("acces_token: %s\n", access_token);
  Serial.printf("refresh_token: %s\n", refresh_token);

  if(should_fetch_event()) {
    get_days_first_event();
  } else {
    set_fetch_event_timer();
  }
}

void set_first_event_timer(const char *event, const char *data)
{
  Serial.printf("Setting timer for first event in the day:\n");
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

  // Call Particle process for safety.
  Particle.process();

  if(hour < cur_hour) hour += 24;
  hour -= cur_hour;

  Serial.printf("Time to event: %d:%d:%d\n", hour, min, sec);
  // Set up brewing one hour before event starts.
  if(hour >= 1) {
    hour -= 1;
    Serial.printf("Time to brew: %d:%d:%d\n", hour, min, sec);
    time_to_brew += hour * 3600000;
    time_to_brew += min * 60000;
    time_to_brew += sec * 1000;
    Serial.printf("In millis to event: %u\n", time_to_brew);
    next_event_timer.changePeriod(time_to_brew);
    hasPendingEvent = true;
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
  Serial.printf("Access token: %s\nAccess token size: %d\n", access_token, token_size);
  // Call Particle process for safety
  Particle.process();
  if(should_fetch_event()) {
    get_days_first_event();
  } else {
    set_fetch_event_timer();
  }
}

void rfc3339Time(char* now)
{
  // RFC3339 date example
  // 2018-12-11T11:30:00Z
  time_t time = Time.now();
  String timestr = Time.format(time, "%Y-%m-%dT%H:%M:%SZ");
  timestr.toCharArray(now, RFC3339Z_SIZE);
}
