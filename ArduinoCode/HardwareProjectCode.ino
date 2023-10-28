#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Servo.h>
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif


int buzzer = 16;//the pin of the active buzzer


//Variables:

Servo servoMotor_front1;
Servo servoMotor_front2;
Servo servoMotorRemLet;

// Ultra Sonic

const int Ultra_Front_Echo=37;
const int Ultra_Front_Trig=35;

int Ultar_Front_duration = 0;
int Ultar_Front_distance = 0;

Servo servoMotor_50;
Servo servoMotor_20;
Servo servoMotor_10;
Servo servoMotor_QR;

const int doorSensor_50 = 27;
const int doorSensor_20 = 28;
const int doorSensor_10 = 29;
const int doorSensor_QR = 30;

//Key pad row and column variables...
const byte ROWS = 4;
const byte COLS = 4;

const int calVal_eepromAdress = 0;
unsigned long t = 0;

bool continueProcess = false;
int stampValueInPost = 0;
int count = 0;



//pins:
int IRSensor_front = 40;//52;
int IRSensor_back = 52;//40;
const int gearMotorPin = 32; // Motor control pin
const int HX711_dout = 14; //mcu > HX711 dout pin
const int HX711_sck = 15; //mcu > HX711 sck pin
const int lightSensorPin1 = 42; //inside the RS.50 stamp
const int lightSensorPin2 = 31;//44; //inside the RS.20 stamp
const int lightSensorPin3 = 48;//33;//46; //inside the RS.10 stamp
const int lightSensorPin4 = 33;//48; //inside the QR box
byte rowPins[ROWS] = {9,8,7,6};
byte colPins[COLS] = {5,4,3,2};

//Constructors:
//Keypad constructor...
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
//I2C constructor...
LiquidCrystal_I2C lcd(0x27, 20, 4);
LiquidCrystal_I2C lcd1(0x26, 16, 2);
//HX711 constructor...
HX711_ADC LoadCell(HX711_dout, HX711_sck);

int InitializingCount=0;
void setup() 
{
  Serial.begin(9600);
  servoMotorRemLet.write(0);
  open_front_gate(false);

  //Ultra Sonic Front
  
  pinMode(Ultra_Front_Trig,OUTPUT);
  pinMode(Ultra_Front_Echo,INPUT);

  //BUzzer
  pinMode(buzzer,OUTPUT);

  //back Door setup
  servoMotor_50.write(90);
  servoMotor_20.write(90);
  servoMotor_10.write(90);
  servoMotor_QR.write(90);
  
  //Setup IR sensor...
  pinMode(IRSensor_front, INPUT);
  pinMode(IRSensor_back, INPUT);
  Serial.println("IR is set");

  //Setup doorSensor...
  pinMode(doorSensor_50, INPUT_PULLUP);
  pinMode(doorSensor_20, INPUT_PULLUP);
  pinMode(doorSensor_10, INPUT_PULLUP);
  pinMode(doorSensor_QR, INPUT_PULLUP);
  Serial.println("Door sensor is set");

  //Setup Gear motor...
  pinMode(gearMotorPin, OUTPUT);
  Serial.println("Gear Motor is set");

  //Setup Servo Motor...
  //servoMotor_front1.attach(16);
  servoMotor_front1.attach(26);//right//24
  servoMotor_front2.attach(24);//left//26
  servoMotorRemLet.attach(25);

  servoMotor_50.attach(10);
  servoMotor_20.attach(11);
  servoMotor_10.attach(12);
  servoMotor_QR.attach(13);
  Serial.println("Servo Motor is set");

  //Setup Loadcell...
  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 8967.80;//8779.29; // uncomment this if you want to set the calibration value in the sketch
  #if defined(ESP8266)|| defined(ESP32)
    //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
  #endif
    //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

    unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
    boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
    LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) 
  {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else 
  {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }

  Serial.println("Loadcell is set");

  //Setup Display...
  lcd.init();
  lcd.backlight();
  Serial.println("User Display is set");

  lcd1.init();
  lcd1.backlight();
  Serial.println("Officer Display is set");
}

//---------------------------------------------------------------------------------LOOP START------------------------------------------------------------------------------------------

void loop() 
{

  StampCheck();
  open_front_gate(false);
  //Ultra sonic Detect User

  //Clear the Trig pin
  digitalWrite(Ultra_Front_Trig, LOW);
  delayMicroseconds(2);


  digitalWrite(Ultra_Front_Trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(Ultra_Front_Trig, LOW);

  Ultar_Front_duration = pulseIn(Ultra_Front_Echo, HIGH);
  Ultar_Front_distance = (Ultar_Front_duration/2)/28.5;
  int Amount=0;
  int Front_Count=0;

  //Have to Display the user that it is Initializing()...
  display_ToUser("Initializing...");

  // To setup the initial weight value.
  float initial_weight_value = setup_initial_weight_value();

  // To clear the display()
  endLcdProcess();

  display_ToUser("Welcome");



  while(Ultar_Front_distance>40)  // Detect user within 40cm.
  {
    digitalWrite(Ultra_Front_Trig, LOW);
    delayMicroseconds(2);

    digitalWrite(Ultra_Front_Trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(Ultra_Front_Trig, LOW);

    Ultar_Front_duration = pulseIn(Ultra_Front_Echo, HIGH);
    Ultar_Front_distance = (Ultar_Front_duration/2)/28.5;
    Serial.println(Ultar_Front_distance);
    delay(20);
  }

  //................................ Real Process starts from here ...............................
  
  endLcdProcess();
  // Have to Display the user to insert the letter().
  display_ToUser("Insert the letter...");

  // Open the front gate using the Servo Motor().
  open_front_gate(true);

  // Look for the letter through the IR sensor().
  //After detect user check 20seconds letter insert or not
  while (digitalRead(IRSensor_front)!=0 && Front_Count<=3500)
  {
    Front_Count++;
    Serial.println(Front_Count);  
  }

  if(Front_Count==3501) //Not insert within 20second Front door will close
  {
    open_front_gate(false);
    endLcdProcess();
  }
  else // Start process when insert the letter within 20 seconds
  {
    // Start the Gear Motor().
    start_gear_motor(true);

    //Officer and User Display Clear
    endLcdProcess();
    endLcdProcessOfficerr();

    // To clear the display()
    display_ToOfficer("Processing");

    // To calculate the weight of the letter.
    // Have to close the front gate.
    // Have to stop the gear motor.
    // Have to display calc total amount.
    float actual_weight_value = calculate_weight_of_the_letter(initial_weight_value);

    if(digitalRead(IRSensor_back)!=0) // Get the Back IR Reading for Letter Go through Out side
    {
      // calculate_the_total_amount();
      int total_amount = calculate_amount_for_weight(actual_weight_value);

      //Clase the Front get;
      open_front_gate(false);

      // To continue selecting the required information until a valid post method is being selected.
      continueProcess = true;

      int selected_post_method = 0;
      
      while (continueProcess)
      {
        // selected_post_method : 1 => Normal Post
        // selected_post_method : 2 => Speed Post
        // selected_post_method : 3 => Stamped Post
        selected_post_method = selectpost();
      }

      // To clear the User display()
      endLcdProcess();

      //To open the required boxes based on the availability of those boxes and display the amount to the officer.
      // logic_for_selelcting_boxes_based_on_availability;
      // display_stamp_values_for_officer;

      // Normal post method.
      if (selected_post_method == 1)
      {
        Amount=open_stamp_boxes(total_amount);
      }
      // Speed post method.
      else if (selected_post_method == 2)
      {
        Amount=open_QR_box(total_amount);
      }
      // Stamped post method.
      else if (selected_post_method == 3)
      {
        
        if (total_amount > stampValueInPost)
        {      
          Amount=open_stamp_boxes(total_amount-stampValueInPost);
        } 
        else 
        {

        // Have to display user no additional amount is needed.
        display_ToUser("Don't need stamp");
        display_ToOfficer("Don't need stamp");
          
        }
        
      }

      Serial.println("Stamp box has been selecedted...");
      
      // check_all_the_boxes_have_closed_using_magnet();
      check_all_stampBox_closed(Amount);
      
      endLcdProcessOfficerr();

      //Diaplay officer side All boxes are closed
      lcd1.setCursor(0,0);
      lcd1.print("All Boxes Closed");
      
      // The letter have to be removed using Servo Motor().
      // IR sensor to detect the letter().
      // Stop the motor once the letter is detetcted().
      remove_letter();


      //Display Officer Precess Finished
      endLcdProcessOfficerr();
      Finnal_display_ToOfficer(selected_post_method,Amount,actual_weight_value);
      

      // To clear the display()
      endLcdProcess();
      // Display that the process has finished.
      Finnal_display_ToUser("Process finished",Amount,actual_weight_value);


      //Wait for Officer take the letter
      while(digitalRead(IRSensor_back)==0)
        {
          delay(1000);
        }
      endLcdProcess();
      endLcdProcessOfficerr();
    }
    else  //If Letter Go to the out side when inserting.
    {
      endLcdProcess();
      start_gear_motor(false);
      display_ToUser("Insert the letter correctly Again");
      display_ToOfficer("Insert Correctly");
      while(digitalRead(IRSensor_back)==0)
      {

        //Buzzer for Incorrect insert.
        BuzzerAlert();

      }
      endLcdProcess();
      endLcdProcessOfficerr();
    }
    
  }

  //Delay for after take the letter 2 seconds
  delay(2000);
}


void Finnal_display_ToOfficer(int selected_post_method,int Amount,float actual_weight_value)
{

  if(selected_post_method == 2)// Display to Officer QR weight and amount
      { 
        lcd1.setCursor(0,0);
        lcd1.print("Take the letter");
        if(actual_weight_value>0.0)
          {
            lcd1.setCursor(0,1);
            lcd1.print("QR:R."+ String(Amount)+"," +String(int(actual_weight_value))+"g");
          }
        else
          {
            lcd1.setCursor(0,1);
            lcd1.print("QR:R."+ String(Amount));          
          }
        
      }
    else// Display to Officer Stamp weight and amount
      {
        lcd1.setCursor(0,0);
        lcd1.print("Take the letter");
        if(actual_weight_value>0.0)
          {
            lcd1.setCursor(0,1);
            lcd1.print("Stamp:R."+ String(Amount)+"," +String(int(actual_weight_value))+"g");
          }
         else
          {
            lcd1.setCursor(0,1);
            lcd1.print("Stamp:R."+ String(Amount));
          }
        
      }

}

//---------------------------------------------------------------------------------LOOP END------------------------------------------------------------------------------------------

void check_all_stampBox_closed(int Amount){

  int OpenCheck=0;

  Serial.println("IN the loop");
  if(digitalRead(doorSensor_50) == 0 && digitalRead(doorSensor_20) == 0 && digitalRead(doorSensor_10) == 0 && digitalRead(doorSensor_QR) == 0)
  {
    BuzzerForOfficer();
    endLcdProcessOfficerr();
    lcd1.setCursor(0,0);
    lcd1.print("Boxes are empty");
    lcd1.setCursor(0,1);
    lcd1.print("ManualStamp:R"+String(Amount));
    delay(5000);
  }
  while(digitalRead(doorSensor_50) != 0 || digitalRead(doorSensor_20) != 0 || digitalRead(doorSensor_10) != 0 || digitalRead(doorSensor_QR) != 0)//Check All boex close(0-close)
  {
    BuzzerForOfficer();
    Serial.println("Door checking");
    if (digitalRead(doorSensor_50) == 0) {
      Serial.println("Door 50 closed");
      open_close_servo(servoMotor_50, false);
    }
    else
    {
      // lcd1.setCursor(0,0);
      // lcd1.print("R.50 ");
      OpenCheck++;
    }

    if (digitalRead(doorSensor_20) == 0) {
      Serial.println("Door 20 closed");
      // lcd1.setCursor(5,0);
      // lcd1.print("     ");
      open_close_servo(servoMotor_20, false);
    }
    else
    {
      // lcd1.setCursor(5,0);
      // lcd1.print("R.20 ");
      OpenCheck++;
    }

    if (digitalRead(doorSensor_10) == 0) {
      Serial.println("Door 10 closed");
      // lcd1.setCursor(10,0);
      // lcd1.print("     ");
      open_close_servo(servoMotor_10, false);
    }
    else
    {
      // lcd1.setCursor(10,0);
      // lcd1.print("R.10 ");
      OpenCheck++;
    }

    if (digitalRead(doorSensor_QR) == 0) {
      Serial.println("Door QR closed");
      // lcd1.setCursor(1,1);
      // lcd1.print("     ");
      open_close_servo(servoMotor_QR, false);
    }
    else
    {
      // lcd1.setCursor(1,1);
      // lcd1.print("QR ");
      OpenCheck++;
    }

    if(OpenCheck>0)
    {
      // lcd1.setCursor(5,1);
      // lcd1.print("Open"); 
    }
    
  }
  
}



int open_QR_box(float weight_value){
  endLcdProcessOfficerr();
  int total_amount;
  bool availability_QR_box = box_availability(lightSensorPin4);

  bool servo_QR_box = false;

    if (weight_value <= 50){
      total_amount = 80;
    } else if (weight_value <= 100){
      total_amount = 120;
    } else if (weight_value <= 150){
      total_amount = 160;
    } else {
      total_amount = 200;
    }

  if (availability_QR_box == true){

    Serial.println("Box for QR is opened");
    servo_QR_box = true;
    open_close_servo(servoMotor_QR, servo_QR_box);

    // Display user Get the QR
    display_ToUser("Pay for QR:Rs."+String(total_amount));
    lcd1.setCursor(0, 0);
    lcd1.print("PayFor QR:R."+String(total_amount));

  } else {

    // Have to show a message that NO QR is available().
    display_ToUser("Manual QR Checking:    Rs."+String(total_amount));
    lcd1.setCursor(0, 0);
    lcd1.print("Box QR Empty");
    lcd1.setCursor(0, 1);
    lcd1.print("Check Manually");

  }
  return total_amount;

}



int open_stamp_boxes(int amount){
  endLcdProcessOfficerr();
  int temp_amount = amount;

  // Counts for the stamps...
  int val_50 = 0;
  int val_20 = 0;
  int val_10 = 0;

  bool availability_box1 = box_availability(lightSensorPin1);
  bool availability_box2 = box_availability(lightSensorPin2);
  bool availability_box3 = box_availability(lightSensorPin3);


  // To check which boxes should be open.
  bool servo_50_box = false;
  bool servo_20_box = false;
  bool servo_10_box = false;

  if (temp_amount >= 50 && availability_box1 == true)
  {
    val_50 = temp_amount / 50;
    temp_amount = temp_amount - (val_50*50);
    Serial.print("Box for Rs.50 is opened : ");
    Serial.println(val_50);
    lcd1.setCursor(0, 0);
    lcd1.print("50:"+ String(val_50)+",");
    servo_50_box = true;
    //open_close_servo(servoMotor_50, servo_50_box);
  }

  if (temp_amount >= 20 && availability_box2 == true)
  {
    val_20 = temp_amount / 20;
    temp_amount = temp_amount - (val_20*20);
    Serial.print("Box for Rs.20 is opened : ");
    lcd1.setCursor(5, 0);
    lcd1.print("20:"+ String(val_20)+",");
    servo_20_box = true;
    //open_close_servo(servoMotor_20, servo_20_box);
  }

  if (temp_amount >= 10 && availability_box3 == true)
  {
    val_10 = temp_amount / 10;
    temp_amount = temp_amount - (val_10*10);
    Serial.print("Box for Rs.10 is opened : ");
    Serial.println(val_10);
    lcd1.setCursor(11, 0);
    lcd1.print("10:"+ String(val_10));
    servo_10_box = true;
  //open_close_servo(servoMotor_10, servo_10_box);
  }

  if (temp_amount != 0 && (availability_box1 == false || availability_box2 == false || availability_box3 == false)){

    // Have to show the message that the stamps can't be distributed.
    display_ToUser("Stamps Not Avail...");
    lcd1.setCursor(0, 1);
    lcd1.print("Stamps Not Avail...");
         

  } 
  else {

    // Once we have the stamp available in boxes to distribute the required total_amount then only the stamp box will be opened.
    open_close_servo(servoMotor_50, servo_50_box);
    open_close_servo(servoMotor_20, servo_20_box);
    open_close_servo(servoMotor_10, servo_10_box);

    // Have to display the total amount to the user.
    display_ToUser("Total Amount :Rs." + String(amount));
    lcd1.setCursor(0, 1);
    lcd1.print("Total:Rs." + String(amount));
    
    // Here the code for displaying to the officer the  values for  val_50, val_20, val_10 will come.

  }
  return amount;

}



void open_close_servo(Servo motor, bool check){
  
  if (check == true){
    motor.write(0);
    delay(500);

    motor.write(90);
    delay(500);

  } else {
    motor.write(90);
  }

}



// checking_which_boxes_available_based_on_light_sensor;
bool box_availability(int lightSensorPin){
    
  int lightLevel = digitalRead(lightSensorPin);

  if (lightLevel == 1) {
    return true;
  } else { 
    return false;
  }

}



int calculate_amount_for_weight(float weight_value){

  int total_amount = 0;

  if(weight_value <= 0)
  {
    total_amount = 0;
  }
  else if (weight_value <= 50){
    total_amount = 30;
  } else if (weight_value <= 100){
    total_amount = 80;
  } else if (weight_value <= 150){
    total_amount = 100;
  } else {
    total_amount = 150;
  }

  Serial.print("The Total amount is : ");
  Serial.println(total_amount);

  return total_amount;

}




int selectpost() {
  String posttype = "";

  // make sure that this value is 0 unless stamped post option is selected...
  stampValueInPost = 0;

  lcd.setCursor(0, 0);
  lcd.print("Select the post type");
  lcd.setCursor(0, 1);
  lcd.print("[A] Normal Post");
  lcd.setCursor(0, 2);
  lcd.print("[B] Speed Post");
  lcd.setCursor(0, 3);
  lcd.print("[C] Stamped Post");

  char key = keypad.getKey();
  if (key != NO_KEY) {
  
    if (key == 'A' || key == 'B') {
      if (key == 'A') {
        posttype = "Normal Post";
        displaySelectedChoice(posttype);
        recheckChoice();
        return 1;
      }
      else if (key == 'B') {
        posttype = "Speed Post";
        displaySelectedChoice(posttype);
        recheckChoice();
        return 2;
      }
    }
    else if (key == 'C') {
      posttype = "Stamped Post";
      displaySelectedChoice(posttype);
      stampedpost();
      return 3;
    }
    else if (key == 'D') {
      //resetChoices();
      lcd.clear();
      selectpost(); // Return to the first step
    }
    else {
      displayErrorMessage("Invalid");
      delay(1000);
      lcd.clear();
    }
  }
  
}

void stampedpost() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select post value:");
  lcd.setCursor(0, 1);
  lcd.print("1 - Rs.10");
  lcd.setCursor(0, 2);
  lcd.print("2 - Rs.50");
  lcd.setCursor(0, 3);
  lcd.print("3 - Rs.100");

  while (stampValueInPost == 0) { // Wait until a choice is made
    char key = keypad.getKey();
    if (key != NO_KEY) {
      switch (key) {
        case '1':
          stampValueInPost = 10;
          break;
        case '2':
          stampValueInPost = 50;
          break;
        case '3':
          stampValueInPost = 100;
          break;
        case 'D':
          //resetChoices();
          lcd.clear();
          selectpost(); // Return to the first step
          return;
        default:
          displayErrorMessage("Invalid Value");
          delay(1000);
          lcd.clear();
          stampedpost();
          return;
      }

      if (stampValueInPost != 0) {
        displaySelectedChoice(String(stampValueInPost));
      }
    }
  }
  recheckChoice();
}


void recheckChoice() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Recheck your choice");
  lcd.setCursor(0,1);
  lcd.print("[A] Confirm");
  lcd.setCursor(0,2);
  lcd.print("[B] Cancel");

  while (true) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      if (key == 'A') {
        //endProcess();
        endSelectProcess();
        return;
      }
      else if (key == 'B') {
        //resetChoices();
        lcd.clear();
        selectpost(); // Return to the first step
        return;
      }
    }
  }
}



void endSelectProcess() {
  lcd.clear();
  continueProcess = false;
}



void displayErrorMessage(const char* message) {
  lcd.clear();
  lcd.print(message);
}



void displaySelectedChoice(const String& type) {
  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print(type);
  lcd.setCursor(5, 2);
  lcd.print("Selected!");
  delay(2000);
  lcd.clear();
}



void resetChoices() {
  //posttype = "";
  //ValueChoice = "";
  //continueProcess = true;
  lcd.clear();
  //lcd.print("Select the post type");
}


float calculate_weight_of_the_letter(float initial_weight_value){

  float undetected_weight_value = initial_weight_value;
  float actual_weight_value = 0;
  
  //Checking whether a weight has been arrived to the load cell.
  while (undetected_weight_value > (initial_weight_value - 10.0) && undetected_weight_value < (initial_weight_value + 10.0)){

    static boolean newDataReady = 0;
    const int serialPrintInterval = 0; //increase value to slow down serial print activity

    // check for new data/start next conversion:
    if (LoadCell.update()) newDataReady = true;

    // get smoothed value from the dataset:
    if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
        undetected_weight_value = LoadCell.getData();
        Serial.print("Load_cell undetected weight value : ");
        Serial.println(undetected_weight_value);
        newDataReady = 0;
        t = millis();
      }
    }
  }

  // Close the front door using the Servo motor.
  open_front_gate(false);

  Centering_letter();

  // Stop the Gear Motor.
  start_gear_motor(false);

  // Display the user that it is Processing.
  display_ToUser("Processing...");

  // Once the letter is detected on the loadcell then it calculate the letter value (Actual letter value).
  while (count < 100){
    
    static boolean newDataReady = 0;
    const int serialPrintInterval = 0; //increase value to slow down serial print activity

    // check for new data/start next conversion:
    if (LoadCell.update()) newDataReady = true;

    // get smoothed value from the dataset:
    if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
        float weight_val = LoadCell.getData();
        Serial.print("Load_cell actual weight value : ");
        Serial.println(weight_val);
        newDataReady = 0;
        t = millis();
        if (count >= 50){
          actual_weight_value = actual_weight_value + weight_val;
        }
        count = count + 1;
      }
    }
  }

  actual_weight_value = actual_weight_value/50;
  count = 0;

  Serial.print("The actual weighgt is : ");
  Serial.println(actual_weight_value);

  // To clear the display()
  endLcdProcess();

  return actual_weight_value;

}

float setup_initial_weight_value(){
  InitializingCount++;
  float initial_weight_value = 0;

  while (count < 100){
    
    static boolean newDataReady = 0;
    const int serialPrintInterval = 0; //increase value to slow down serial print activity

    // check for new data/start next conversion:
    if (LoadCell.update()) newDataReady = true;

    // get smoothed value from the dataset:
    if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
        float ini_weight_val = LoadCell.getData();
        Serial.print("Load_cell initial weight value : ");
        Serial.println(ini_weight_val);
        newDataReady = 0;
        t = millis();
        if(InitializingCount>1 && count<=20)
        {
          count = count + 1;
        }
        else
        {
          initial_weight_value = initial_weight_value + ini_weight_val;
          count = count + 1;
        }
        
        
      }
    }
  }

  initial_weight_value = initial_weight_value/count;
  count = 0;

  Serial.print("The threshold weighgt value is: ");
  Serial.println(initial_weight_value);

  return initial_weight_value;  

}

void remove_letter(){
  int Ir_Read=digitalRead(IRSensor_back);
  while (Ir_Read!=0) {

    servoMotorRemLet.write(0);
    delay(1000);

    Ir_Read=digitalRead(IRSensor_back);
    if(Ir_Read==0)break;
    servoMotorRemLet.write(60);
    delay(500);
  }
  servoMotorRemLet.write(0);
  delay(500);
  
}

void start_gear_motor(bool check){

  if (check == true){
    // start the gear motor to rotate at maximum speed of 255.
    analogWrite(gearMotorPin, 255);

  } else {
    analogWrite(gearMotorPin, 0);

  }

}

void open_front_gate(bool check){

  if (check == true){
    // For opening the front gate.
    servoMotor_front1.write(180);
    servoMotor_front2.write(0);

  } else {
    // For closing the front gate.
    servoMotor_front1.write(0);
    servoMotor_front2.write(180);

  }
  
}

void display_ToUser(const String& message){

  lcd.setCursor(0, 0);
  lcd.print(message);
}

void Finnal_display_ToUser(const String& message,int Amount, float weight){

  lcd.setCursor(0, 0);
  lcd.print(message);
  lcd.setCursor(0,1);
  if(weight>0.0)
  {
    lcd.print("Rs:"+ String(Amount)+", " +String(int(weight))+"g");
  }
  else
  {
    lcd.print("Rs:"+ String(Amount));
  }
  

}

void display_ToOfficer(const String& message){

  lcd1.setCursor(0, 0);
  lcd1.print(message);

}

void endLcdProcess() {
  lcd.clear();
}

void endLcdProcessOfficerr() {
  lcd1.clear();
}


void BuzzerForOfficer()
{
      digitalWrite(buzzer,HIGH);
      delay(500);
      digitalWrite(buzzer,LOW);
      delay(1500);

}


void BuzzerAlert()
{
      digitalWrite(buzzer,HIGH);
      delay(500);
      digitalWrite(buzzer,LOW);
      delay(500);
}


//Stamp box Empty or not Checking
void StampCheck()
{
  bool availability_box1 = box_availability(lightSensorPin1);
  bool availability_box2 = box_availability(lightSensorPin2);
  bool availability_box3 = box_availability(lightSensorPin3);
  bool availability_box4 = box_availability(lightSensorPin4);

  if(availability_box1==false || availability_box2==false || availability_box3==false || availability_box4==false)
  {
    lcd1.setCursor(0, 0);
    lcd1.print("Empty:");
    if(availability_box1==false)
      {
        lcd1.setCursor(0, 1);
        lcd1.print("Rs50 ");
      }
    if(availability_box2==false)
      {
        lcd1.setCursor(5, 1);
        lcd1.print("Rs20 ");

      }
      if(availability_box3==false)
      {
        lcd1.setCursor(10, 1);
        lcd1.print("Rs10 ");
      }
      if(availability_box4==false)
      {
        lcd1.setCursor(8, 0);
        lcd1.print("QR Box");
      }
    
    
  }
}

void Centering_letter()
{
int i=0;
while (i<2) {

    servoMotorRemLet.write(0);
    delay(500);

    servoMotorRemLet.write(15);
    delay(500);
    i++;
  }
  servoMotorRemLet.write(0);
  delay(500);  
}
