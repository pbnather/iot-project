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

  // Set power to output HIGH voltage.
  digitalWrite(power,HIGH);

  // Call `changeState` (ISR) on button toggle.
  attachInterrupt(button, changeState, CHANGE);

  // DEBUG ONLY
  Particle.function("change_state", toggleState);
}

void loop()
{
  // Read data from sensors.
  weight = analogRead(weight_sensor);
  hasWater = digitalRead(liquid_sensor);

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
  isBrewing = !isBrewing;
}

// Returns `true` if Willy has a jug in place
// by comparing `weight` with `JUG_WEIGHT`;
// returns false otherwise.
bool hasJug()
{
  return weight >= JUG_WEIGHT;
}

// DEBUG ONLY
int toggleState(String command)
{
  changeState();
}
