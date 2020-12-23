#include <REG51.H>

/******************************************************************************
Defines
******************************************************************************/
#define DIPSWITCH_000 	0		
#define DIPSWITCH_001	1		
#define DIPSWITCH_010	2
#define DIPSWITCH_011	3
#define DIPSWITCH_100	4
#define DIPSWITCH_101	5
#define DIPSWITCH_110	6
#define DIPSWITCH_111	7

/****************************************************************************
Global Variables
*****************************************************************************/
unsigned int g_dipswitch_state = 0;
unsigned int g_arr_RPM [3];
unsigned int g_falling_edge_counter = 0;
unsigned int g_avg_RPM = 0;

char g_seg_code[]={0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90}; 

/*****************************************************************************
Pin Names
******************************************************************************/
sbit dipswitch_a = P1^0;		//set P1.0 as dipswitch_a
sbit dipswitch_b = P1^1;		//set P1.1 as dipswitch_b
sbit dipswitch_c = P1^2;		//set P1.2 as dipswitch_c

sbit pwm_output = P2^0;			//set P2.0 as pwm output
sbit sw_s1_7seg = P2^3;			//set P2.3 as s1 of 7 segment
sbit sw_s2_7seg = P2^2;			//set P2.2 as s2 of 7 segment
sbit sw_s3_7seg = P2^1;			//set P2.1 as s3 of 7 segment

/******************************************************************************
Function Declarations
******************************************************************************/

void pwm_output_motor(int);
void check_dipswitch_state();
void motor_RPM();
void edge_counter() ;
void delay(int);
void feedback_control (int);
void display_RPM();

/****************************************************************************
Intrupts
*****************************************************************************/


/***************************Initilize Interrupts*****************************/
/*Following function initilizes the external internal interrupts used in this program*/
void interrupt_init (){
	/*Use external Interrupt (falling edge) to count encoder pulses*/
	IT0 = 1;   // Configure Interrupt 0 for falling edge on /INT0 (P3.2)
	EX0 = 1;   // Enable EX0 Interrupt
	EA = 1;    // Enable Global Interrupt Flag to Start Interrupt
	
	/*Use timer 1 to generate an interupt at every 50ms, and use timer 0 to  create delay*/
	TMOD = 0x11; //t1 used to create interupt of (50ms), and t0 to create delay (0001 0001)
	TH1 = 0x4B;  //Counting from 0x4BFF to 0xFFF to takes 50ms. After 50ms Interrupt routine is called.
	TL1 = 0xFF; 
	TR1 =1; //turn on timer 1 
	ET1 = 1; //enable timer 1 interupt
}

/***************************Falling Edge Interrupt****************************/
/*Following function increments counter whenever falling edge is detected*/
void falling_edge_interrupt () interrupt 0{
	g_falling_edge_counter++; 
}

/***************************50ms Interrupt and RPM calculation****************/
/*Following function is executed every 50ms, which calculates the average RPM*/
void interrupt_50ms() interrupt 3{	
	int RPM; //declare a local variable to store the current RPM
	
	//rotation_per_min = (pulses_recorded_in_0.05s * 60sec/1min)/24 revolution_per_rotation 
	RPM = (g_falling_edge_counter)*50; 	
		
	//add last three RPMs which is store in the array, and divide by 3 to get average
	g_avg_RPM = (g_arr_RPM [0] + g_arr_RPM [1] + g_arr_RPM[2])/3;
		
	/*This keep the last 3 values of the RPM in an array, and uses it calcualte the average RPM*/
	//Discard RPM at 0th index
	//Move RPM at 1st index to 0th index, 
	//Move RPM at 2nd index to 1st index, 
	//Update current calculated RPM at 2nd index; 
	g_arr_RPM [0] = g_arr_RPM [1];
	g_arr_RPM [1] = g_arr_RPM [2];
	g_arr_RPM [2] = RPM;
	
		
	g_falling_edge_counter = 0;	//reset the counter for next calculation
	TH1 = 0x4B;  //reset the timer for 50ms Interrupt
	TL1 = 0xFF;
}

/****************************************************************************
**********************************MAIN***************************************
*****************************************************************************/
void main(void)
 { 
	 interrupt_init ();		//initilize the interrupts
   dipswitch_a = 1;			//set port 1.0, 1.1, 1.2 as input to read dipswitch value
   dipswitch_b = 1;
   dipswitch_c = 1;
   
  
   pwm_output = 0;			// set port 2.0 as output to write pwm output
	 sw_s1_7seg = 0;
	 sw_s2_7seg	= 0;
	 sw_s3_7seg = 0;
	 P0 = 0x00;						//set port 0 as output
    
    while (1) {
		 switch (g_dipswitch_state){ //dip switch state machine
				case DIPSWITCH_000:
					 feedback_control(0); //0% 
					 break;
				case DIPSWITCH_001:
					 feedback_control(240); //10% 
					 break;
				case DIPSWITCH_010:
					 feedback_control(720); //30% 
					 break;
				case DIPSWITCH_011:
					 feedback_control(1200); //50% 
					 break;
				case DIPSWITCH_100:
					 feedback_control(1440); //60% 
					 break;
				case DIPSWITCH_101:
					 feedback_control(1680); //70% 
					 break;
				case DIPSWITCH_110:
					 feedback_control(1920); //80%
					 break;
				case DIPSWITCH_111:
					 feedback_control(2400); //100% 
					 break;
				default:
					 feedback_control(0);
				}
	   }
}
 
/******************************************************************************
Function Declarations
******************************************************************************/

/***********************Feedback Control Function*****************************/
/*The following functions implements Proportional Control.This function is called in main state machine. 
Depending on the state of the switches, desired RPM is fed into the system. Using the current and desired 
RPM the error is calcuated. Based on the error, the output pwm of the motor is updated, and current RPM is displayed*/
void feedback_control(int desired_RPM){
	int output = (((desired_RPM - g_avg_RPM))*5); //output = error*proportional_gain
	//update motor
	pwm_output_motor (output);
	display_RPM ();
}

/***********************PWM Generating Function*******************************/  
/*The following ouputs the PWM signal base on the error calculated by the Feedback Controller. 
If required dutycylce>=100, it is changed to 100, or if the required dutycyle is <=0, it is
change to 0*/
void pwm_output_motor(int dutycycle){
	
		//check if dutycycle is 0 or less, output 0 pwm
   if (dutycycle<=0){
		 pwm_output = 0;
      }
   //check if dutycycle is 100 or more, output 100 pwm 
   else if (dutycycle>=100){
			pwm_output = 1;
      } 
   else{
      //set pwm_output as high
      pwm_output = 1;
      delay (dutycycle);
      //set pwm output as low
      pwm_output = 0;
      delay(100-dutycycle);
      }
	 	//check dip switch state
  check_dipswitch_state();
}

/***********************STATE CHECK******************************************/  
/*The following fuctions checks the state of each switch, and updates the global variable
g_dipswitch_state to update the statemachine*/
void check_dipswitch_state(){
   // 000 (a= high, b =high , c= high)
   if (dipswitch_a == 1 && dipswitch_b == 1 && dipswitch_c == 1){
      g_dipswitch_state = DIPSWITCH_000;
      }  
   //001 (a= high, b =high , c= low)
   else if (dipswitch_a == 1 && dipswitch_b == 1 && dipswitch_c == 0){
      g_dipswitch_state = DIPSWITCH_001;
      }  
   //010 (a= high, b =low , c= high)
   else if (dipswitch_a == 1 && dipswitch_b == 0 && dipswitch_c == 1){
      g_dipswitch_state = DIPSWITCH_010;
      }  
   //011(a= high, b=low , c= low)
   else if (dipswitch_a == 1 && dipswitch_b == 0 && dipswitch_c == 0){
      g_dipswitch_state = DIPSWITCH_011;
      }  
   //100 (a= low, b =high , c= high)
   else if (dipswitch_a == 0 && dipswitch_b == 1 && dipswitch_c == 1){
      g_dipswitch_state = DIPSWITCH_100;
      }  
   //101 (a= low, b =high , c= low)
   else if (dipswitch_a == 0 && dipswitch_b == 1 && dipswitch_c == 0){
      g_dipswitch_state = DIPSWITCH_101;
      }  
   //110 (a= low, b =low , c= high)
   else if (dipswitch_a == 0 && dipswitch_b == 0 && dipswitch_c == 1){
      g_dipswitch_state = DIPSWITCH_110;
      }  
   //111(a= low, b =low , c=low)
  else if (dipswitch_a == 0 && dipswitch_b == 0 && dipswitch_c == 0){
      g_dipswitch_state = DIPSWITCH_111;
      }  
}

/***********************RPM Displaying Function*******************************/
/*The following functions displays the average RPM in three 7 segment displays.
Only first three digits of the RPM>1000 are displayed on 7 segment.Eg 1440 is displayed as 144*/
void display_RPM (){
	int first_digit; 
	int second_digit; 
	int third_digit; 
	
	int temp; 
	int RPM_temp; 
	
	//each index represents binary representation of that respective index 
	//eg: binary representation at 0th index g_seg_code is 11000000 (Xgfedcba) (g of 7 segment is off, therefore displaying 0) 
	//similarly, eg: g_seg_code[7] = 0xF8 ---> 11111000 (xgfedcba) (abc of 7 segment are on, therefore displaying 7)
	
	if (g_avg_RPM<=0){ 
		//if the RPM is <= 0, display 0 in all three 7 segments	
		first_digit = g_seg_code[0];
		second_digit = g_seg_code[0];
		third_digit = g_seg_code[0];
	}
	else if (g_avg_RPM>0 && g_avg_RPM<10){ 
		//if 0<RPM<10, display 0 in first two 7 segments, and display current RPM in third segment 
		first_digit = g_seg_code[0];
		second_digit = g_seg_code[0];
		third_digit = g_seg_code[g_avg_RPM];
	}

	else if (g_avg_RPM>=10 && g_avg_RPM<100){ //10<RPM<100
		//if 10<=RPM<100, display 0 in first 7 segments, and display current RPM in second and third segment
		first_digit = g_seg_code[0];
		second_digit = g_seg_code[g_avg_RPM/10]; 
		third_digit = g_seg_code[g_avg_RPM%10]; 
	}
	else if (g_avg_RPM>=100 && g_avg_RPM<1000){ //100<RPM<1000
		//if 100<=RPM<1000, display current RPM on three 7 segments
		/*the following seperates each digit of the RPM to display on individual segments
		for example: 
		g_avg_RPM = 147
		temp = 147/10 = 14
		first_digit = (14/10) = 1
		second_digit = (14%10) = 4
		third_digit = (147%10) = 7
		*/
		temp = g_avg_RPM/10;
		
		first_digit = g_seg_code[temp/10];
		second_digit = g_seg_code[temp%10];
		third_digit = g_seg_code[g_avg_RPM%10];
	}
	else if (g_avg_RPM>=1000 && g_avg_RPM<10000){
		//if 1000<=RPM<10000, display first three digits of current RPM on three 7 segments
		/*the following seperates each digit of the RPM to display on individual segments
		for example: 
		g_avg_RPM = 1897
		temp = 1897/10 = 189
		RPM_temp = 189
		temp = 189/10 = 18
		first_digit = (18/10) = 1
		second_digit = (18%10) = 8
		third_digit = (189%10) = 9
		*/
		temp = g_avg_RPM/10;
		RPM_temp = temp; 
		temp = RPM_temp/10;
		
		first_digit = g_seg_code[temp/10];
		second_digit = g_seg_code[temp%10];
		third_digit = g_seg_code[RPM_temp%10];
	}
	else {
		//display E on 7 segment displays
		first_digit = 0x06;
		second_digit = 0x06;
		third_digit = 0x06;
	}
		
	//turn on switch 1, turn off switch 2 and 3
		sw_s1_7seg = 1;
		sw_s2_7seg = 0;
		sw_s3_7seg = 0;
	// display the first digit
		P0 = first_digit;
		delay (20);
		
		
	//turn on switch 2, turn off switch 1 and 3
		sw_s1_7seg = 0;
		sw_s2_7seg = 1;
		sw_s3_7seg = 0;
	//display the second digit
		P0 = second_digit;
		delay (20);

	//turn on switch 3, turn off switch 2 and 1
		sw_s1_7seg = 0;
		sw_s2_7seg = 0;
		sw_s3_7seg = 1;
	//display the third digit
		P0 = third_digit;
		delay (20);
}

/***********************100us Delay***************************************/
/*The following function generates the delay of 100usec, in a loop. For example, 
to generate a delay of 500us, call function delay(5)*/
void delay(int num_of_100us_delay){
   int i;
   for (i=0; i<num_of_100us_delay; i++){
	 /*one for loop generates a delay of 100u sec 
		 count from 0xFFA3 to 0xFFFF. Each count takes 1.085u sec, 
		 therefore counting from 0xFFA3 to 0xFFFF will result in 100u sec of delay */
		 TMOD = 0x11; //t1 used to create Interrupt of 50ms, and t0 to create delay of 100us (0001 0001)
		 TH0 = 0xFF;  //set MSB to 1111 1111 (0xFFA3= 1111 1111 1010 0011)
		 TL0 = 0xA3; //set LSB to 1010 0011
		 TR0 =1; //turn on timer zero to count 
		 while(TF0 == 0){
			 //while overflow flag is zero, do nothing 
		 } 
		 TF0= 0; //clear the overflow flag
		 TR0 = 0; //clear timer
	 }
}   
