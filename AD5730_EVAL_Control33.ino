/*   
    Control of the AD5372 & AD5370 EVAL board using arduino uno R3/Mega
    Created: 2017/10/9
    Author: Ulrich Kohlrusch

    Cahnges all cahannel Info changed to CHANMAX constant Value
    
    adapted from https://github.com/filiplindau/AD5370 by Filip Lindau & Kaustubh Banerjee
*/
/*
   SPI communication with EVAL-AD5372
   13 - SCLK
   11 - MOSI/SDI
   12 - MISO/SDO //read - unused
   
   8 - CS/SYNC 1
   2 - CS/SYNC 2
   
   07 - LDAC
   06 - CLR
   05 - BUSY // unused
   04 - RESET
*/

#define SYNC1 PB2
#define SYNC2 PB1
#define SCK PB5
#define MISO PB4
#define MOSI PB3
//#define BUSY PB0

#define LDAC PD7
#define CLEAR PD6
#define RESET PD4

#define VMIN          21844
#define VMAX          65535
#define DEBUG
#define AD5370
//#define ONECHIP
//
#ifdef AD5370
 #define CHANMAX       40            //AD5370 - 40 Channels max.
 #define CHANMAX2      80  
#else #ifdef AD5370
 #define CHANMAX       32            //AD5372 - 32 Channels max.
 #define CHANMAX2      64
//#elif #ifdef ONECHIP
//#define CHANMAX2     CHANMAX       //if only one Chip used CHANMAX2 = 32 or 40..
#endif
     
#define RESET_TIME    1290          //~400µS for Reset Time indicated by Busy
#define RESPONSE_TIME 12            //DAC response time after LDAC falling edge ~3µS
//DAC Register
#define MDAC          0xc0          //b11000000
#define MOFFSET       128           //b10000000
#define MGAIN         64            //010000000

enum channelMode_t{MCHAN, MALL};
enum registerMode_t{DAC, OFFSET, GAIN};

uint16_t voltages[CHANMAX2];  //Voltage Buffer Array of 2 chips
uint16_t offset[CHANMAX2];    //Offset Buffer Array of 2 chips
uint16_t gain[CHANMAX2];      //Gain Buffer Array of 2 chips
//uint8_t idx=0;
uint8_t csel=0;

void resetDac() {
  PORTD &= ~(1<<RESET);
  asm volatile ("nop");                                           //~175nS Reset Pulse low minimum is 30nS
  PORTD |= (1<<RESET);
  for(uint16_t i=0; i < RESET_TIME; i++)  asm volatile ("nop");   //~400µS Reset Time after rising edge of Reset Pin
}

void clearDac() {
  static uint8_t state=0;                                         //initial state is low!
  if(state){                                                      //only if not 0
    PORTD &= ~(1<<CLEAR);                                         //set Clear to low
    asm volatile ("nop");                                         //Clear pulse Activation Time 175nS
    state=0;                                                     //toggle state                                              
    Serial.println("Clear on");
  }
  else{
    PORTD |= (1<<CLEAR);                                          //set Clear to high
    asm volatile ("nop");                                         
    state=1;
    Serial.println("Clear off");
  }
}

void loadDac() {
  PORTD &= ~(1<<LDAC);                                              //set LDAC to low
  //asm volatile ("nop");                                           //LDAC Pulse width low 175nS? not needed yet
  PORTD |= (1<<LDAC);
  for(uint8_t i=0; i < RESPONSE_TIME; i++)  asm volatile ("nop");   //~3µS Dac Response Time after falling edge of LDAC Pin
}


//SETUP
void setup() {
  Serial.begin(9600);
  //Setup IO pins
  DDRB |= ( (1<<SCK) | (1<<MOSI) | (1<<SYNC1) | (1<<SYNC2) );            //set SCK, MOSI, SYNC1 & SYNC2 as output
  DDRD |= ( (1<<RESET) | (1<<CLEAR) | (1<<LDAC) );                       //set RESET, Clear, LDAC as output
  //DDRB &= ~(1<<MISO);                                                  //set MISO as input  - not used yet!
  PORTB |= ( (1<<SCK) | (1<<SYNC1) | (1<<SYNC2) );                       //set high/pullup for SCK, SYNC1 & SYNC2 
  //Setup SPI         
  SPCR |= ( (1<<SPE) | (1<<MSTR) | (1<<CPHA) ); // | _BV(SPR0);          //Activate SPI set Master Data Dir MSB(org 0) MODE=1 first Clockrate FCPU/4 eg. 250ns/4MHz
  SPSR |= (1<<SPI2X);                                                    //double Speed! FCPU/2 now... 125nS/8Mhz
  uint8_t spistatus=SPSR;                                                //clear SPSR status register
  
  for(int i=0;i<CHANMAX2;i++) voltages[i] = VMIN;                      //initial buffer
  
  resetDac();
  clearDac();
  loadDac();
}

void spi_write(const uint8_t *write_list){ //[3]) {
  switch(csel){
               case 0: PORTB &= ~(1<<SYNC1); break;
               case 1: PORTB &= ~(1<<SYNC2); break;
               case 2: PORTB &= ~( (1<<SYNC1) | (1<<SYNC2) ); break;
               default: PORTB &= ~(1<<SYNC1);            
              }
  SPDR=201;
  while (!(SPSR & (1<<SPIF)));
  SPDR=255;
  while (!(SPSR & (1<<SPIF)));
  SPDR=255;
  while (!(SPSR & (1<<SPIF)));
  PORTB |= ( (1<<SYNC1) | (1<<SYNC2) );
}

uint8_t write_value_int(registerMode_t reg, channelMode_t cmod, uint8_t channel, uint8_t bothchip, uint16_t value){ 
  // Write DAC value to specific output(MCHAN) or to all channels(MALL)from voltage Array  - returns the selected chip Number
  uint8_t x=0;
  uint8_t a=0;                                                                  //Mode - all channels
  switch(reg){
              case DAC:     x=MDAC; break;                                      //192  - Dac X1A or X1B Register select
              case OFFSET:  x=MOFFSET; break;                                   //128  - Offset register select
              case GAIN:    x=MGAIN; break;                                     //64   - Gain Register select
              default:      x=MDAC;                                             // default is Dac Register selected
            }                     
  //--
  if(channel<CHANMAX && bothchip==0) {chipselect(0);}                           //Single Mode channel from 0 CHANMAX
  else if(channel>=CHANMAX && bothchip==0) {channel-=CHANMAX; chipselect(1);}   //Single Mode channel over CHANMAX maped to 0
  else if(channel<CHANMAX && bothchip==1) {chipselect(2);Serial.println(channel); Serial.println("ok");}                      //Dual Mode - both chips selected channel from 0 to CHANMAX
  else if(channel>=CHANMAX && bothchip==1){return(100); Serial.println("error 100");}                       //error in dual Mode only channels up to CHANMAX alowed 
  //--
  
  if(cmod == MCHAN) a=channel+8;                                                //if Mode MCHAN a specific channel is selected - add group offset and channel number 
  
  //char str[]="                     ";
  //sprintf(str,"Channel-%d Chip-%d", channel, csel);
  //Serial.println(str);                
  
  
  uint8_t write_list[3];
  write_list[0] = x + a;                                                        //summary Dac Register + Channel
  write_list[1] = uint8_t(value >> 8);                                          //seperate high byte
  write_list[2] = value & 0xff;                                                 //seperate low byte

  spi_write(write_list);                                                        //write list(Array) to spi write routine
  return(csel);
}

void InitRegister(registerMode_t reg, uint8_t root, uint8_t target, uint16_t value ){    //NOTE die Frage, ob überhaupt die Schleife durchlaufen werden muss 
  char str[]="                                                 ";
  char str2[]="      ";
  uint16_t *pArray;
  channelMode_t mode=MALL;
  if(root < CHANMAX2 || target < CHANMAX2) mode=MCHAN; 
  // /*
  if(mode==MALL){Serial.println("All");} else {Serial.println("Chan");}
  Serial.println("test");
  if(reg==DAC) Serial.println("DAC");
  if(reg==OFFSET) Serial.println("OFFSET");
  if(reg==GAIN) Serial.println("GAIN");
  //Serial.println(bothchip);
  Serial.println(root);
  Serial.println(target);//*/
  
  switch(reg){
              case DAC:     pArray=voltages; strcpy(str2, "Dac"); break;                                      
              case OFFSET:  pArray=offset; strcpy(str2, "Offset"); break;                                   
              case GAIN:    pArray=gain; strcpy(str2, "Gain"); break;
              default:      pArray=voltages; strcpy(str2, "Dac");                                           
            }   
  if(mode==MALL){
    write_value_int(reg, MALL, 0, 2, value);              //simply write one time only a value to all channels/groups @group0/all Channels
    for(uint8_t i=0; i<CHANMAX2; i++) pArray[i]=value;    //update complete register Array
    sprintf(str, "writing %s %i to all Channels", str2, value);
  }
  else if(mode==MCHAN){
    for(uint8_t i=root; i<=target; i++){
      pArray[i]=value;                                    //update register Array from root to target channel
      write_value_int(reg, MCHAN, i, 0, value);           //write value from root to target channel in disired Register
    }
    sprintf(str, "Initial Channels from %d to %d in %s Register with %i", root, target, str2, value);
  }
  loadDac();
  Serial.println(str);
}


void writeFromArray(registerMode_t reg, uint8_t bothchip, uint8_t root, uint8_t target ){    
  
  uint16_t *pArray;
  uint16_t value=0;
  uint8_t errorCheck=0;  //if Error 100 from write_value..
  channelMode_t mode=MALL;
  if(root < CHANMAX2 || target < CHANMAX2) mode=MCHAN;
  //if(mode==MALL){Serial.println("All");} else {Serial.println("Chan");}
  
  //#ifdef DEBUG
  char str[80];
  char str2[]="      ";
  char str3[]="      ";
  char str4[27];
  
  switch(reg){
              case DAC:     pArray=voltages; strcpy(str, "Writing Voltage to all Channel"); strcpy(str2, "Dac"); break;                                      
              case OFFSET:  pArray=offset; strcpy(str, "Writing Offset to all Channel"); strcpy(str2, "Offset"); break;                                   
              case GAIN:    pArray=gain; strcpy(str, "Writing Gain to all Channel"); strcpy(str2, "Gain"); break;
              default:      pArray=voltages; strcpy(str, "Writing Voltage to all Channel"); strcpy(str2, "Dac");                                             
            } 
   if(mode==MALL){
     for(uint8_t i=0; i<CHANMAX2; i++){value=pArray[i]; write_value_int(reg, MCHAN, 0, 0, value); };
     sprintf(str, "Wrote %s Array at all Channels", str2);
    }
    else if(mode==MCHAN){
      uint8_t x;
      if(root>target){
        x=0-(target-root); 
        strcpy(str3, "downto");
        uint8_t dec=root;
        for(uint8_t i=0; i<x+1; i++){ 
          value=pArray[dec]; 
          errorCheck=write_value_int(reg, MCHAN, dec, bothchip, value); 
          dec--;
        } //;
      }
      else if(root<target){
        x=target-root;
        strcpy(str3, "to");
        for(uint8_t i=root; i<=target; i++){
         value=pArray[i]; 
         errorCheck=write_value_int(reg, MCHAN, i, bothchip, value); 
        }
      }
      //Serial.println(x);
      if(root<CHANMAX && target<CHANMAX) {strcpy(str4,"Chip one");}
      else if (root>CHANMAX && target>CHANMAX) {strcpy(str4,"Chip two");}
      else if (root<CHANMAX && target>CHANMAX) {strcpy(str4,"Chip one & two");}
      else if (root>CHANMAX && target<CHANMAX) {strcpy(str4,"Chip two & one");}
      if (bothchip) {strcpy(str4,"Dual");}
      //wrote Offset Array Channels from FF downto TT both Chips at the same time =74 Chars
      
      //for(uint8_t i=root; i<=target; i++){ value=pArray[i]; errorCheck=write_value_int(reg, MCHAN, 0, bothchip, value); };
      
      sprintf(str, "Wrote %s Array Channels from %d %s %d %s", str2, root, str3, target, str4); 
    } 
 if(errorCheck==100) Serial.println("wrong root or target Channel for dual mode");
 else Serial.println(str);
 loadDac();
}
void write_function(uint8_t function, uint16_t value){
  // Write special function
  // function: 6 bit function code
  // value: Value to write. 0-65535
  
  uint8_t x = B00000000;
  uint8_t a = function;
  
  uint8_t write_list[3];
  write_list[0] = x + a;
  write_list[1] = uint8_t(value>> 8);
  write_list[2] = value & 0xff;

  spi_write(write_list);
  loadDac();
}

void loop() {
  // put your main code here, to run repeatedly:
}

void chipselect(uint8_t select){
  switch(select){
                  case 0: csel=0; break;
                  case 1: csel=1; break;
                  case 2: csel=2; break;
                  default:csel=0;
                }
}

void serialEvent(){
  // listens to serial input
  // "/" is used a delimiter between numbers
  // "W" instructs a write operation to write all 32/40 voltages in voltage array
  // "C" toggle clear state
  // "Z" zero all channels @21844
  // "R" reset all registers
  // "U", "V", "T" select chip 1, chip 2 or both of them respectively
  // "L" pulses LDAC
  // "@" sets single channel and "$" sets voltage at the channel - 31@65000$ sets channel 31/39 at 65000. - doesnt work if channel is not set before
  // "#" resets this loop. But doesnt really do anything.
  // "U34/65000/23/WL" will update only the first 3 postions of the voltage array and update the first dac
  static uint16_t serialdata=0;
  static uint8_t i=0, bothChip=0; //all=0;
  //uint16_t volt=VMIN;
  static uint8_t channel=CHANMAX2, root=CHANMAX2, target=CHANMAX2;
  uint8_t bsout=0;
  char astr[]="                                      ";
  static registerMode_t reg=DAC;
  while (Serial.available()){
    char inChar = Serial.read(); 
    if(inChar > 47 && inChar < 58) {serialdata*=10; serialdata+=(inChar -48); bsout=0; } 
    
    switch(inChar){
                    //case  0:  serialdata=0; bsout=1; break;
                    case 'B': bothChip=1;  break;                                                        //Both Chips selected
                    case 'D': reg=DAC;  break;                                                                  //Register Mode DAC
                    case 'O': reg=OFFSET; break;                                                               //Register Mode Offset
                    case 'G': reg=GAIN; break;                                                               //Register Mode Gain
                    case 'F': if(serialdata < CHANMAX2) {root=serialdata; serialdata=0;} break;         //root Channel Symbol use for Array and Initial operation
                    case 'T': if(serialdata < CHANMAX2) {target=serialdata; serialdata=0;} break;       //target Channel Symbol use for Array and Initial operation
                    case 'I': {
                                InitRegister(reg, root, target, serialdata );
                                reg=DAC;
                                root=CHANMAX2;
                                target=CHANMAX2;
                                serialdata=0;
                                break;
                              }
                    case 'W':  {
                                writeFromArray(reg, bothChip, root, target);
                                reg=DAC;
                                root=CHANMAX2;
                                target=CHANMAX2;
                                serialdata=0;
                                break;
                               }
                    case 'C': clearDac(); serialdata=0; break;                                              //toggle clear
                    case 'L': loadDac(); strcpy(astr, "Load Dac"); bsout=1; break;            //load Dac
                    case 'R': resetDac();strcpy(astr, "Reset to default"); bsout=1; break;    //reset Dac
                    case '@': {                                                               //Channel Symbol for specific Dac Channel
                                channel=serialdata; 
                                if(channel < CHANMAX) chipselect(0);                          //channel 0 to CHANMAX selected chip 0
                                if(channel > CHANMAX && channel < CHANMAX2) chipselect(1);    //channel CHANMAX to CHAN2CHIP selected chip 1
                                serialdata=0; 
                                break;
                              }
                    case '/': {                                                               //Value seperator symbol increments channel and wrote value to Array
                                voltages[i] = serialdata;
                                serialdata = 0;
                                i++;
                                if(i==CHANMAX2) i=0;
                                break;  
                              }
                    case '$': {                                                              //Value symbol for specific Dac Channel                                 
                                if(channel<CHANMAX2){
                                  uint8_t cs=write_value_int(DAC, MCHAN, channel, bothChip, serialdata);
                                  if(cs==100) {Serial.println("wrong Channel for dual mode"); bothChip=0; serialdata=0; break;}
                                  loadDac();
                                  voltages[channel] = serialdata;
                                  char str[]="          ";
                                  switch(cs){
                                                case 0: strcpy(str, "Chip one"); break; 
                                                case 1: strcpy(str, "Chip two"); break; 
                                                case 2: strcpy(str, "both Chips"); break;
                                                default: strcpy(str, "Chip one"); break;                                             
                                              }
                                  sprintf(astr, "wrote %i at Channel-%d %s", serialdata, channel, str);
                                  channel = CHANMAX2;
                                  bothChip=0;
                                  serialdata=0;
                                  bsout=1;
                                  break;
                                }
                                else{
                                      strcpy(astr, "Wrong Channel!!!");
                                      channel = CHANMAX2;
                                      bothChip=0;
                                      serialdata=0;
                                      bsout=1;
                                      break;
                                }                              
                                break;
                              }
                    case 'P': {                                                         //Print all data from voltage Array //later change to desired Register Array
                                for(uint8_t i=0; i<CHANMAX2; i++){
                                 sprintf(astr, "Channel-%d Val-%i", i, voltages[i]);
                                 Serial.println(astr);
                                }
                                break;
                              }
                  }
  } 
  if(bsout) Serial.println(astr); 
}


