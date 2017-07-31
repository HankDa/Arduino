/**
 * @author Tripack McLovin (spamforpatrick@gmx.de)
 * @brief  7-segment digital clock arduino program
 *          
 *         setting the clock is done now by just using the terminal (9600baud), sending:
 *         dYY-MM-DD HH:MM:SS - e.g.: "d16-12-19 13:56:09"
 *         or without date
 *         tHH:MM:SS - e.g.: "t18:55:39"
 * 
 * @library any version of FastLED will be fine, thank you guys for the great work!
 * 
 * @library credits to the ds3231 library goes also to Petre Rodan <petre.rodan@simplex.ro>
 *            found here: https://github.com/rodan/ds3231
 */
#include <FastLED.h>
#include <Wire.h>


//chose clk
  #define CLK_DS3102
  #include <DS1302.h>
  #include <ds3231.h>
//******************** CONFIGURATION *************************



//uncomment this for the large version with 6 digits,  let it uncommented for the 4 digits case
//#define HH_MM_SS
//comment this if you are not fine with the automatic DD:MM-HH:MM-MM:SS switching
#define DDMMHHMMSS_TOGGLE

//uncomment this if you use 2 leds per bar
//#define DUALBAR

#define PIN_LED     5   //data pin of the APA104 led
#define PIN_PHOTO   A0  //pin of the photodiode

//for our imperial friends (there is no am/pm indicator)
//#define TYPE_12H

//brigthness adjustments
//value of the photo value in darkest environment
#define BRIGHTNESS_ADC_LOW  20
//value in the brightest environment
#define BRIGHTNESS_ADC_HIGH 600
//max and min brightness
#define BRIGTHNESS_HIGH 255
#define BRIGHTNESS_LOW  20


//start time and date when uploading the code (this will also be used for winter/summer-time adjustment)
//it must be entered here as wintertime (normal),
//and can prevent you from using manual setting of time
#define SD_DD 20
#define SD_MM 12
#define SD_YY 16
#define ST_HH 18
#define ST_MM 00
#define ST_SS 00

//color definitions
//these are the colors followed through the day
//every n'th hour, a new color is reached, between these, the color is interpolated,
//this should help to get the time without precisly read it, especially during the night
//on APA104 this is RGB, please look at your type, it might be GRB or whatever
//starting at midnight, equally spaces through the day (e.g. 4 values will be 0, 6, 12 and 18 o'clock peak of that color)
CRGB cols[]={0xFF0000,0x0000FF,0x00FF00,0xFFFFFF,0xFFFF80,0xFF8000};
CRGB colA;
CRGB colB;
int bfac;//brightness factor
//repeat the first color at the end!
#define COLAMNT 6


//******************** implementation (nothing to change here) *************************
//positions of the numbers and dots along the strip
#ifdef HH_MM_SS
#define POS_SEC     0
#define POS_TENSEC  7
#define POS_DOTS1   28
#define POS_MIN     14
#define POS_TENMIN  21
#define POS_DOTS2   28
#define POS_HOUR    30
#define POS_TENHOUR 37

#else
#define POS_MIN     0
#define POS_TENMIN  7
#define POS_DOTS1   14
#define POS_HOUR    16
#define POS_TENHOUR 23

#endif
#define NUM_LEDS    POS_TENHOUR+7

//date
byte dd_dd;
byte dd_mm;
byte dd_yy;
bool summertime;

//time allready disassembled as digits (more memory but less calculations)
byte ss_dec;
byte ss_dig;
byte mm_dec;
byte mm_dig;
byte hh_dec;
byte hh_dig;


#define MODE_NORMAL     0
#define MODE_CHRISTMAS  1
#define MODE_1STAPRIL   2
byte mode=MODE_1STAPRIL;

#define MINUTES_PER_DAY 60*24
int minuteofday;
int minutesperblock;

#define SYMBOL_DEG 10

CRGB setColor;
CRGB clockleds[NUM_LEDS];
CRGB clockleds_prep[NUM_LEDS];
//colors
#define colnum sizeof(cols)/sizeof(CRGB)
int colindex;

int lastSecUpdate;

int update_run=0;
int update_run_sub=0;

//debugging
#define BUFF_MAX 128

#ifdef CLK_DS3102
// DS1302 rtc([RST], [DAT], [CLOCK]); 三個pin的接法
DS1302 rtc(10, 9, 8); 

Time T_1302;
//T_1302->year =17;
//T_1302->mon =7;
//T_1302->date =27;
//T_1302->hour =15;
//T_1302->min = 20;
//T_1302->sec = 0;

#endif

void setup() {
#ifdef CLK_DS3102
  DS1302_init(T_1302);
#else  
  Wire.begin(); //A4->SDA A5->SCL
  DS3231_init(DS3231_INTCN);                                 //Niho DS1302 rtc(10, 9, 8);
  
#endif

  FastLED.addLeds<APA104, PIN_LED, GRB>(clockleds, NUM_LEDS);//Niho APA104 -> ??
  colindex=1;

  Serial.begin(9600);
  sync_from_rtc(); //Niho (get time for RTC->Y,M,D-H,M,S)

  minutesperblock=240;

  lastSecUpdate=millis()-1000;
  update_run=0;
  update_run_sub=0;

  setMode();
  
}

void DS1302_init(struct Time t){
    // 設定時鐘為正常執行模式
  rtc.halt(false);
  
  //取消寫入保護，設定日期時要這行
  rtc.writeProtect(false);

    // 以下是設定時間的方法，在電池用完之前，只要設定一次就行了
  rtc.setDOW(FRIDAY);        // 設定週幾，如FRIDAY
  rtc.setTime(t.hour, t.min, t.sec);     // 設定時間 時，分，秒 (24hr format)
  rtc.setDate(t.date, t.mon, t.year);   // 設定日期 日，月，年
  }


void loop() {

  proc_serial(); //Niho

  int now = millis();
  if (now-lastSecUpdate>1000){
    lastSecUpdate=now;
    internal_increments();
    calcColor();
    calcBrightness();
  }
  
  switch(mode){
    case MODE_NORMAL:    
      showHhMm();
      dot_processing();
      updateFromPrep();
      break;
      
    case MODE_CHRISTMAS:
      showHhMm_ce();
      updateFromPrep();
      break;

    case MODE_1STAPRIL:
      proc_april();
      updateFromPrep();
      break;
    
    default:
      mode=MODE_NORMAL;
      break;
  }
  FastLED.show();
  delay(40); //all calculations needs around one milisecond, so this should be fine to not rely on interrupts

}


void setMode(){
  if ( dd_mm==12 && (dd_dd==24) ){
    mode=MODE_CHRISTMAS;
  } else if (dd_mm==4 && dd_dd==1){
    mode=MODE_1STAPRIL;
  } else {
    mode=MODE_NORMAL;
  }
  
}

void showMmSs(){
  setNum(POS_MIN,ss_dig);
  setNum(POS_TENMIN,ss_dec);
  setNum(POS_HOUR,mm_dig); 
  setNum(POS_TENHOUR,mm_dec);
}


void showHhMm(){
  if ( ss_dig+ss_dec==0 ){
    update_run=0;
    update_run_sub=0;
  }

  if ( update_run<=0 ){
    //setting numbers to cockleds_prep
    setNum(POS_MIN,mm_dig);
    setNum(POS_TENMIN,mm_dec);
    setNum(POS_HOUR,hh_dig); 
    setNum(POS_TENHOUR,hh_dec);
  }

}

byte rwgc;
CRGB rwg[]={0xB0B0B0,0xFF0000,0x30A000};
byte rwgStartOffset=0;
void showHhMm_ce(){
  if ( ss_dig+ss_dec==0 ){
    update_run=0;
    update_run_sub=0;
  }

  if ( update_run<=0 ){
    //setting numbers to cockleds_prep
    rwgc=rwgStartOffset;  
    rwgStartOffset++;
    if ( rwgStartOffset>2 ) rwgStartOffset=0;

    setNum_ce(POS_MIN,mm_dig);
    setNum_ce(POS_TENMIN,mm_dec);
    setNum_ce(POS_HOUR,hh_dig); 
    setNum_ce(POS_TENHOUR,hh_dec);

    dot_processing_ce();
  }

}

void updateFromPrep(){
  
  if( update_run<NUM_LEDS ){
    update_run_sub++;
    if ( update_run_sub>1){
      update_run_sub=0;
      clockleds[update_run] = clockleds_prep[update_run];
      clockleds[update_run].nscale8(bfac);
      update_run++;
      if ( update_run==POS_DOTS1) update_run+=2;
      if (update_run>=NUM_LEDS) update_run=0;
    }
  }
  
}


void calcColor(){
  //interpolating colors over the day
  int i = minuteofday/minutesperblock;
  int a = minuteofday-(i*minutesperblock);
  int b = minutesperblock-a;
  float fa=(float)a/(float)minutesperblock;
  float fb=(float)b/(float)minutesperblock;

  colA = cols[i];
  colA.nscale8(fb*255);
  i++;
  if (i>=COLAMNT) i=0;
  colB = cols[i];
  colB.nscale8(fa*255);
  
  setColor = colA+colB;
  
}


int readout=512;
void calcBrightness(){

  //lowpass filtering
  int r = analogRead(A0);
  if (abs(r-readout)> 90) { //jump if the input changes in large steps
    readout = r;
  } else {
    readout=(readout*7+r)>>3; // 7/8 old and 1/8 new value
  }
  //translate ADC to brightness range
  bfac=map( BRIGHTNESS_ADC_LOW, BRIGHTNESS_ADC_HIGH, BRIGHTNESS_LOW, BRIGTHNESS_HIGH, readout);
  //clamp
  if (bfac<BRIGHTNESS_LOW) bfac=BRIGHTNESS_LOW;
  else if (bfac>BRIGTHNESS_HIGH) bfac=BRIGTHNESS_HIGH;

}

void internal_increments(){
  //incrementing numbers (least effords regarding computations
  ss_dig++;
  if (ss_dig>9){ 
    ss_dig=0;
    ss_dec++;
    if(ss_dec>5){ //new minute
      minuteofday++;
      ss_dec=0;
      mm_dig++;
      if(mm_dig>9){ //new ten minute
        mm_dig=0;
        mm_dec++;
        if (mm_dec>5){ //new hour
          mm_dec=0;
          hh_dig++;
          if ( hh_dig>9 || (hh_dec>=2 && hh_dig>3)){
            hh_dig=0;
            hh_dec++;
            if (hh_dec>2) {//new day
              hh_dec=0;
            }
          }
        }
        //resync every 10 minutes (after incrementing the other numbers)
        sync_from_rtc();
      }
    }
  }
}

byte ucount=0;
void dot_processing(){
  ucount++;
  if (ucount>25) ucount=0;
  
  if (ucount==0) {
    clockleds[POS_DOTS1  ]=setColor; 
    clockleds[POS_DOTS1  ].nscale8(bfac);
  }
  clockleds[POS_DOTS1  ].nscale8(0xF8);
  clockleds[POS_DOTS1+1]=clockleds[POS_DOTS1  ];
}

void dot_processing_ce(){
  clockleds[POS_DOTS1  ]=rwg[rwgc++];
  clockleds[POS_DOTS1  ].nscale8(bfac);
  if (rwgc>2) rwgc=0;
  clockleds[POS_DOTS1+1]=rwg[rwgc++];
  clockleds[POS_DOTS1+1].nscale8(bfac);
  if (rwgc>2) rwgc=0;
}

void proc_april(){
  setNum(POS_MIN,10);
  setNum(POS_TENMIN,0);
  setNum(POS_HOUR,0);
  setNum(POS_TENHOUR,10);
}

void sync_from_rtc(){
  
    Serial.println("sync_from_rtc()");
    
#ifdef CLK_DS1302
  struct Time rtc;
  rtc = getTime();
  
  dd_yy = rtc.year;
    dd_mm = rtc.mon;
    dd_dd = rtc.date;
#else 
  struct ts rtc; //from the ds3231 lib
    DS3231_get(&rtc);
   
    dd_yy = rtc.year;
    dd_mm = rtc.mon;
    dd_dd = rtc.mday;
#endif  
    //try to check if we are in summer or wintertime, time is always stored to rtc as wintertime
    set_stwt();

    char buff[BUFF_MAX];
    snprintf(buff, BUFF_MAX, "%d.%02d.%02d %02d:%02d:%02d dst:%d", rtc.year,
             rtc.mon, rtc.mday, rtc.hour, rtc.min, rtc.sec, summertime);   
    Serial.println(buff);
//Niho    
    hh_dec = rtc.hour*0.1;
    hh_dig = rtc.hour-hh_dec*10;
    mm_dec = rtc.min*0.1;
    mm_dig = rtc.min-mm_dec*10;
    ss_dec = rtc.sec*0.1;
    ss_dig = rtc.sec-ss_dec*10;

    //apply summertime addition
    if (summertime){
      hh_dig++;
      if ( hh_dig>9 || (hh_dec>=2 && hh_dig>3)){
        hh_dig=0;
        hh_dec++;
        if (hh_dec>2) {
          hh_dec=0;  
        }
      }
    }
   
    minuteofday=mm_dig+10*mm_dec+60*hh_dig+600*hh_dec;
    setMode();
    
}


/** 
 * @brief assigning a number to the specified position in the strip 
 *  if you not fine how the numbers look
 *  (e.g. the bottom bar on the 9, or the hook on the 7)
 *  just change digit_bits[]
 *  pattern: 
 *  -2-
 * 1   3
 *  -0-
 * 6   4
 *  -5-
 */
byte digit_bits[]={  0b01111110, //0
                     0b00011000, //1
                     0b01101101, //2
                     0b00111101, //3
                     0b00011011, //4
                     0b00110111, //5
                     0b01110111, //6
                     0b00011100, //7
                     0b01111111, //8
                     0b00111111, //9
                     0b01001111}; //P
void setNum(int digit, byte number){
  if (number>=0 && number<=10){
    byte mask=0x01;
    for (int i=0; i<7;i++){
      clockleds_prep[digit+i]=(mask&digit_bits[number])? setColor : CRGB::Black ;
      mask<<=1;
    }
  }
}



void setNum_ce(int digit, byte number){
  if (number>=0 && number<=9){
    byte mask=0x01;
    for (int i=0; i<7;i++){
      clockleds_prep[digit+i]=(mask&digit_bits[number])?( rwg[rwgc++] ):CRGB::Black;
      if (rwgc>2) rwgc=0;
      mask<<=1;
    }
  }
}


/**
 * @brief this is summer/wintertime-determination
 * 
 * (this is only works for the rules applied for the Mid-European time - change on the last sunday in march
 * from 0200 to 0300 and last sunday in october from 0300 to 0200
 * 
 * one could condense the formulas, but this is as entropic driven search the most efficient (lesser events are deeper)
 * 
 * the summertime-flag is used to compensate the rtc time with the visible time
 * 
 */
void set_stwt(){
  if (dd_mm==3) { //march
    if (dd_dd<25) summertime=false;
    else{
      //check if the sunday allready happend
      byte dow = 0; //get weekday sun=0, mon=1 ... from date-system
      if (dow>0) summertime = true;
      else{
        //so we are at the sunday of switching
        byte hh = hh_dec*10+hh_dig;
        if (hh>=2) summertime = true;
        else summertime = false;
      }
    }
  } else if (dd_mm==10) { //october
    if (dd_dd<25) summertime = true;
    else{
      //check if the sunday allready happend
        byte dow = 0; //get weekday sun=0, mon=1 ... from date-system
      if (dow>0) summertime = false;
        //so we are at the sunday of switching
        byte hh = hh_dec*10+hh_dig;
        if (hh>=2) summertime = false;
        else summertime = true;
    }
  } else if (dd_mm<3 || dd_mm>10) {
    summertime=false; 
  } else {
    summertime=true;
  }
}


char recv[BUFF_MAX];
byte incnt=0;

void proc_serial(){
  if (Serial.available() > 8) {
    delay(10);//wait on all bytes
    while (Serial.available()>0){
        recv[incnt++] = Serial.read();
    }
    Serial.println(recv);
    if (incnt==18) {
      if (recv[0]!='d' || recv[3]!='-' || recv[6]!='-' || recv[9]!=' ' || recv[12]!=':' || recv[15]!=':' ){
        Serial.println("format mismatch");
      } else {
        //easy parsing (not checking number types yet)
        struct ts rtc; //from the ds3231 lib
        
        rtc.year=inp2toi(recv,1)+2000;
        rtc.mon=inp2toi(recv,4);
        rtc.mday=inp2toi(recv,7);

        rtc.hour=inp2toi(recv,10);
        rtc.min=inp2toi(recv,13);
        rtc.sec=inp2toi(recv,16);

        if ( (rtc.mon>12) || (rtc.mday>31) || (rtc.hour>23) || (rtc.min>59) || (rtc.sec>59) ){
            char buff[BUFF_MAX];
            snprintf(buff, BUFF_MAX, "%d.-%02d-%02d %02d:%02d:%02d", rtc.year,
              rtc.mon, rtc.mday, rtc.hour, rtc.min, rtc.sec);   
            Serial.println("numbers not parsable");
            Serial.println(buff);
        } else {
          Serial.println("sucessfully set date and time");
          DS3231_set(rtc);
          sync_from_rtc(); //read it back to our internal format
        }
      }
    } else if (incnt==9) {
       if (recv[0]!='t' || recv[3]!=':' || recv[6]!=':'){
        Serial.println("format mismatch");
      } else {
        //easy parsing (not checking number types yet)
        struct ts rtc; //from the ds3231 lib
        DS3231_get(&rtc);

        rtc.hour=inp2toi(recv,1);
        rtc.min=inp2toi(recv,4);
        rtc.sec=inp2toi(recv,7);

        if ( (rtc.hour>23) || (rtc.min>59) || (rtc.sec>59) ){
            char buff[BUFF_MAX];
            snprintf(buff, BUFF_MAX, "%02d:%02d:%02d", rtc.hour, rtc.min, rtc.sec);   
            Serial.println("numbers not parsable");
            Serial.println(buff);
        } else {
          Serial.println("sucessfully set date and time");
          DS3231_set(rtc);
          sync_from_rtc(); //read it back to our internal format
        }
      }

      
    } else {
      Serial.println("length mismatch");
    }
    incnt=0;
  }
}

